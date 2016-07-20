/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2016 Red Hat
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Rob Bradford <rob@linux.intel.com> (from cogl-winsys-egl-kms.c)
 *   Kristian Høgsberg (from eglkms.c)
 *   Benjamin Franzke (from eglkms.c)
 *   Robert Bragg <robert@linux.intel.com> (from cogl-winsys-egl-kms.c)
 *   Neil Roberts <neil@linux.intel.com> (from cogl-winsys-egl-kms.c)
 *   Jonas Ådahl <jadahl@redhat.com>
 *
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-renderer-view.h"
#include "backends/native/meta-monitor-manager-kms.h"
#include "backends/native/meta-renderer-native.h"
#include "cogl/cogl.h"
#include "core/boxes-private.h"

enum
{
  PROP_0,

  PROP_KMS_FD,

  PROP_LAST
};

typedef struct _MetaOnscreenNative
{
  struct gbm_surface *surface;
  uint32_t current_fb_id;
  uint32_t next_fb_id;
  struct gbm_bo *current_bo;
  struct gbm_bo *next_bo;
  gboolean pending_swap_notify;

  gboolean pending_set_crtc;

  MetaRendererView *view;
  int pending_flips;
} MetaOnscreenNative;

struct _MetaRendererNative
{
  MetaRenderer parent;

  int kms_fd;
  struct gbm_device *gbm;
  CoglClosure *swap_notify_idle;

  int64_t frame_counter;

  struct gbm_surface *dummy_gbm_surface;
};

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (MetaRendererNative,
                         meta_renderer_native,
                         META_TYPE_RENDERER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init))

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;
static const CoglWinsysVtable *parent_vtable;

static void
meta_renderer_native_disconnect (CoglRenderer *cogl_renderer)
{
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;

  if (egl_renderer->edpy != EGL_NO_DISPLAY)
    eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

static void
flush_pending_swap_notify (CoglFramebuffer *framebuffer)
{
  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      MetaOnscreenNative *onscreen_native = egl_onscreen->platform;

      if (onscreen_native->pending_swap_notify)
        {
          CoglFrameInfo *info =
            g_queue_pop_head (&onscreen->pending_frame_infos);

          _cogl_onscreen_notify_frame_sync (onscreen, info);
          _cogl_onscreen_notify_complete (onscreen, info);

          onscreen_native->pending_swap_notify = FALSE;
          cogl_object_unref (onscreen);

          cogl_object_unref (info);
        }
    }
}

static void
flush_pending_swap_notify_idle (void *user_data)
{
  CoglContext *cogl_context = user_data;
  CoglRendererEGL *egl_renderer = cogl_context->display->renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;
  GList *l;

  /* This needs to be disconnected before invoking the callbacks in
   * case the callbacks cause it to be queued again */
  _cogl_closure_disconnect (renderer_native->swap_notify_idle);
  renderer_native->swap_notify_idle = NULL;

  l = cogl_context->framebuffers;
  while (l)
    {
      GList *next = l->next;
      CoglFramebuffer *framebuffer = l->data;

      flush_pending_swap_notify (framebuffer);

      l = next;
    }
}

static void
free_current_bo (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
  CoglContext *cogl_context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;

  if (onscreen_native->current_fb_id)
    {
      drmModeRmFB (renderer_native->kms_fd,
                   onscreen_native->current_fb_id);
      onscreen_native->current_fb_id = 0;
    }
  if (onscreen_native->current_bo)
    {
      gbm_surface_release_buffer (onscreen_native->surface,
                                  onscreen_native->current_bo);
      onscreen_native->current_bo = NULL;
    }
}

static void
meta_onscreen_native_queue_swap_notify (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
  MetaBackend *backend = meta_get_backend ();
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;

  /* We only want to notify that the swap is complete when the
   * application calls cogl_context_dispatch so instead of
   * immediately notifying we queue an idle callback */
  if (!renderer_native->swap_notify_idle)
    {
      renderer_native->swap_notify_idle =
        _cogl_poll_renderer_add_idle (cogl_renderer,
                                      flush_pending_swap_notify_idle,
                                      cogl_context,
                                      NULL);
    }

  /*
   * The framebuffer will have its own referenc while the swap notify is
   * pending. Otherwise when destroying the view would drop the pending
   * notification with if the destruction happens before the idle callback
   * is invoked.
   */
  cogl_object_ref (onscreen);
  onscreen_native->pending_swap_notify = TRUE;
}

static CoglBool
meta_renderer_native_connect (CoglRenderer *cogl_renderer,
                              CoglError   **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  CoglRendererEGL *egl_renderer;

  cogl_renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = cogl_renderer->winsys;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;
  egl_renderer->platform = renderer_native;
  egl_renderer->edpy = EGL_NO_DISPLAY;

  if (renderer_native->gbm == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Couldn't create gbm device");
      goto fail;
    }

  egl_renderer->edpy =
    eglGetDisplay ((EGLNativeDisplayType) renderer_native->gbm);
  if (egl_renderer->edpy == EGL_NO_DISPLAY)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Couldn't get eglDisplay");
      goto fail;
    }

  if (!_cogl_winsys_egl_renderer_connect_common (cogl_renderer, error))
    goto fail;

  return TRUE;

fail:
  meta_renderer_native_disconnect (cogl_renderer);

  return FALSE;
}

static CoglBool
meta_renderer_native_setup_egl_display (CoglDisplay *cogl_display,
                                        CoglError  **error)
{
  CoglDisplayEGL *egl_display = cogl_display->winsys;
  CoglRendererEGL *egl_renderer = cogl_display->renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;

  egl_display->platform = renderer_native;

  /* Force a full modeset / drmModeSetCrtc on
   * the first swap buffers call.
   */
  meta_renderer_native_queue_modes_reset (renderer_native);

  return TRUE;
}

static void
meta_renderer_native_destroy_egl_display (CoglDisplay *cogl_display)
{
}

static CoglBool
meta_renderer_native_egl_context_created (CoglDisplay *cogl_display,
                                          CoglError  **error)
{
  CoglDisplayEGL *egl_display = cogl_display->winsys;
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;

  if ((egl_renderer->private_features &
       COGL_EGL_WINSYS_FEATURE_SURFACELESS_CONTEXT) == 0)
    {
      renderer_native->dummy_gbm_surface =
        gbm_surface_create (renderer_native->gbm,
                            16, 16,
                            GBM_FORMAT_XRGB8888,
                            GBM_BO_USE_RENDERING);
      if (!renderer_native->dummy_gbm_surface)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_CREATE_CONTEXT,
                           "Failed to create dummy GBM surface");
          return FALSE;
        }

      egl_display->dummy_surface =
        eglCreateWindowSurface (egl_renderer->edpy,
                                egl_display->egl_config,
                                (EGLNativeWindowType)
                                renderer_native->dummy_gbm_surface,
                                NULL);
      if (egl_display->dummy_surface == EGL_NO_SURFACE)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_CREATE_CONTEXT,
                           "Failed to create dummy EGL surface");
          return FALSE;
        }
    }

  if (!_cogl_winsys_egl_make_current (cogl_display,
                                      egl_display->dummy_surface,
                                      egl_display->dummy_surface,
                                      egl_display->egl_context))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to make context current");
      return FALSE;
    }

  return TRUE;
}

static void
meta_renderer_native_egl_cleanup_context (CoglDisplay *cogl_display)
{
  CoglDisplayEGL *egl_display = cogl_display->winsys;
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }
}

static void
meta_onscreen_native_swap_drm_fb (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;

  free_current_bo (onscreen);

  onscreen_native->current_fb_id = onscreen_native->next_fb_id;
  onscreen_native->next_fb_id = 0;

  onscreen_native->current_bo = onscreen_native->next_bo;
  onscreen_native->next_bo = NULL;
}

static void
on_crtc_flipped (GClosure         *closure,
                 MetaRendererView *view)
{
  ClutterStageView *stage_view = CLUTTER_STAGE_VIEW (view);
  CoglFramebuffer *framebuffer =
    clutter_stage_view_get_framebuffer (stage_view);
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  CoglOnscreenEGL *egl_onscreen =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;

  onscreen_native->pending_flips--;
  if (onscreen_native->pending_flips == 0)
    {
      meta_onscreen_native_queue_swap_notify (onscreen);
      meta_onscreen_native_swap_drm_fb (onscreen);
    }
}

static void
flip_closure_destroyed (MetaRendererView *view)
{
  ClutterStageView *stage_view = CLUTTER_STAGE_VIEW (view);
  CoglFramebuffer *framebuffer =
    clutter_stage_view_get_framebuffer (stage_view);
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  CoglOnscreenEGL *egl_onscreen =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;

  if (onscreen_native->next_fb_id)
    {
      MetaBackend *backend = meta_get_backend ();
      MetaRenderer *renderer = meta_backend_get_renderer (backend);
      MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);

      drmModeRmFB (renderer_native->kms_fd, onscreen_native->next_fb_id);
      gbm_surface_release_buffer (onscreen_native->surface,
                                  onscreen_native->next_bo);
      onscreen_native->next_bo = NULL;
      onscreen_native->next_fb_id = 0;

      meta_onscreen_native_queue_swap_notify (onscreen);
    }

  g_object_unref (view);
}

static void
meta_onscreen_native_flip_crtc (MetaOnscreenNative *onscreen_native,
                                GClosure           *flip_closure,
                                MetaCRTC           *crtc,
                                int                 x,
                                int                 y,
                                gboolean           *fb_in_use)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);

  if (!meta_monitor_manager_kms_is_crtc_active (monitor_manager_kms,
                                                crtc))
    {
      *fb_in_use = FALSE;
      return;
    }

  if (meta_monitor_manager_kms_flip_crtc (monitor_manager_kms,
                                          crtc,
                                          x, y,
                                          onscreen_native->next_fb_id,
                                          flip_closure))
    onscreen_native->pending_flips++;

  *fb_in_use = TRUE;
}

static void
meta_onscreen_native_set_crtc_modes (MetaOnscreenNative *onscreen_native)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);
  MetaRendererView *view = onscreen_native->view;
  uint32_t next_fb_id = onscreen_native->next_fb_id;
  MetaMonitorInfo *monitor_info;

  monitor_info = meta_renderer_view_get_monitor_info (view);
  if (monitor_info)
    {
      int i;

      for (i = 0; i < monitor_info->n_outputs; i++)
        {
          MetaOutput *output = monitor_info->outputs[i];
          int x = output->crtc->rect.x - monitor_info->rect.x;
          int y = output->crtc->rect.y - monitor_info->rect.y;

          meta_monitor_manager_kms_apply_crtc_mode (monitor_manager_kms,
                                                    output->crtc,
                                                    x, y,
                                                    next_fb_id);
        }
    }
  else
    {
      unsigned int i;

      for (i = 0; i < monitor_manager->n_crtcs; i++)
        {
          MetaCRTC *crtc = &monitor_manager->crtcs[i];

          meta_monitor_manager_kms_apply_crtc_mode (monitor_manager_kms,
                                                    crtc,
                                                    crtc->rect.x, crtc->rect.y,
                                                    next_fb_id);
        }
    }
}

static void
meta_onscreen_native_flip_crtcs (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaRendererView *view = onscreen_native->view;
  GClosure *flip_closure;
  MetaMonitorInfo *monitor_info;
  gboolean fb_in_use = FALSE;

  /*
   * Create a closure that either will be invoked or destructed.
   * Invoking the closure represents a completed flip. If the closure
   * is destructed before being invoked, the framebuffer references will be
   * cleaned up accordingly.
   *
   * Each successful flip will each own one reference to the closure, thus keep
   * it alive until either invoked or destructed. If flipping failed, the
   * closure will be destructed before this function goes out of scope.
   */
  flip_closure = g_cclosure_new (G_CALLBACK (on_crtc_flipped),
                                 g_object_ref (view),
                                 (GClosureNotify) flip_closure_destroyed);
  g_closure_set_marshal (flip_closure, g_cclosure_marshal_generic);

  /* Either flip the CRTC's of the monitor info, if we are drawing just part
   * of the stage, or all of the CRTC's if we are drawing the whole stage.
   */
  monitor_info = meta_renderer_view_get_monitor_info (view);
  if (monitor_info)
    {
      int i;

      for (i = 0; i < monitor_info->n_outputs; i++)
        {
          MetaOutput *output = monitor_info->outputs[i];
          int x = output->crtc->rect.x - monitor_info->rect.x;
          int y = output->crtc->rect.y - monitor_info->rect.y;

          meta_onscreen_native_flip_crtc (onscreen_native, flip_closure,
                                          output->crtc, x, y,
                                          &fb_in_use);
        }
    }
  else
    {
      unsigned int i;

      for (i = 0; i < monitor_manager->n_crtcs; i++)
        {
          MetaCRTC *crtc = &monitor_manager->crtcs[i];

          meta_onscreen_native_flip_crtc (onscreen_native, flip_closure,
                                          crtc, crtc->rect.x, crtc->rect.y,
                                          &fb_in_use);
        }
    }

  /*
   * If the framebuffer is in use, but we don't have any pending flips it means
   * that flipping is not supported and we set the next framebuffer directly.
   * Since we won't receive a flip callback, lets just notify listeners
   * directly.
   */
  if (fb_in_use && onscreen_native->pending_flips == 0)
    {
      meta_onscreen_native_queue_swap_notify (onscreen);
      meta_onscreen_native_swap_drm_fb (onscreen);
    }

  g_closure_unref (flip_closure);
}

static void
meta_onscreen_native_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                               const int    *rectangles,
                                               int           n_rectangles)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);
  CoglContext *cogl_context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
  CoglFrameInfo *frame_info;
  MetaRendererView *view;
  cairo_rectangle_int_t view_layout;
  uint32_t handle, stride;

  frame_info = g_queue_peek_tail (&onscreen->pending_frame_infos);
  frame_info->global_frame_counter = renderer_native->frame_counter;

  /* If we already have a pending swap then block until it completes */
  while (onscreen_native->next_fb_id != 0)
    meta_monitor_manager_kms_wait_for_flip (monitor_manager_kms);

  view = onscreen_native->view;
  clutter_stage_view_get_layout (CLUTTER_STAGE_VIEW (view), &view_layout);

  parent_vtable->onscreen_swap_buffers_with_damage (onscreen,
                                                    rectangles,
                                                    n_rectangles);

  /* Now we need to set the CRTC to whatever is the front buffer */
  onscreen_native->next_bo =
    gbm_surface_lock_front_buffer (onscreen_native->surface);

  stride = gbm_bo_get_stride (onscreen_native->next_bo);
  handle = gbm_bo_get_handle (onscreen_native->next_bo).u32;

  if (drmModeAddFB (renderer_native->kms_fd,
                    view_layout.width,
                    view_layout.height,
                    24, /* depth */
                    32, /* bpp */
                    stride,
                    handle,
                    &onscreen_native->next_fb_id))
    {
      g_warning ("Failed to create new back buffer handle: %m");
      gbm_surface_release_buffer (onscreen_native->surface,
                                  onscreen_native->next_bo);
      onscreen_native->next_bo = NULL;
      onscreen_native->next_fb_id = 0;
      return;
    }

  /* If this is the first framebuffer to be presented then we now setup the
   * crtc modes, else we flip from the previous buffer */
  if (onscreen_native->pending_set_crtc)
    {
      meta_onscreen_native_set_crtc_modes (onscreen_native);
      onscreen_native->pending_set_crtc = FALSE;
    }

  meta_onscreen_native_flip_crtcs (onscreen);
}

static CoglBool
meta_renderer_native_init_egl_context (CoglContext *cogl_context,
                                       CoglError  **error)
{
  COGL_FLAGS_SET (cogl_context->features,
                  COGL_FEATURE_ID_SWAP_BUFFERS_EVENT, TRUE);
  /* TODO: remove this deprecated feature */
  COGL_FLAGS_SET (cogl_context->winsys_features,
                  COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT,
                  TRUE);
  COGL_FLAGS_SET (cogl_context->winsys_features,
                  COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                  TRUE);
  COGL_FLAGS_SET (cogl_context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  return TRUE;
}

static gboolean
meta_renderer_native_create_surface (MetaRendererNative  *renderer_native,
                                     int                  width,
                                     int                  height,
                                     struct gbm_surface **gbm_surface,
                                     EGLSurface          *egl_surface,
                                     GError             **error)
{
  MetaBackend *backend = meta_get_backend ();
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  CoglDisplay *cogl_display = cogl_context->display;
  CoglDisplayEGL *egl_display = cogl_display->winsys;
  CoglRendererEGL *egl_renderer = cogl_display->renderer->winsys;
  struct gbm_surface *new_gbm_surface;
  EGLNativeWindowType egl_native_window;
  EGLSurface new_egl_surface;

  new_gbm_surface = gbm_surface_create (renderer_native->gbm,
                                        width, height,
                                        GBM_FORMAT_XRGB8888,
                                        GBM_BO_USE_SCANOUT |
                                        GBM_BO_USE_RENDERING);

  if (!new_gbm_surface)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  egl_native_window = (EGLNativeWindowType) new_gbm_surface;
  new_egl_surface = eglCreateWindowSurface (egl_renderer->edpy,
                                            egl_display->egl_config,
                                            egl_native_window,
                                            NULL);
  if (new_egl_surface == EGL_NO_SURFACE)
    {
      gbm_surface_destroy (new_gbm_surface);
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  *gbm_surface = new_gbm_surface;
  *egl_surface = new_egl_surface;

  return TRUE;
}

static CoglBool
meta_renderer_native_init_onscreen (CoglOnscreen *onscreen,
                                    CoglError   **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *cogl_context = framebuffer->context;
  CoglDisplay *cogl_display = cogl_context->display;
  CoglDisplayEGL *egl_display = cogl_display->winsys;
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen;
  MetaOnscreenNative *onscreen_native;
  int width;
  int height;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

  onscreen_native = g_slice_new0 (MetaOnscreenNative);
  egl_onscreen->platform = onscreen_native;

  onscreen_native->pending_set_crtc = TRUE;

  /* If a kms_fd is set then the display width and height
   * won't be available until meta_renderer_native_set_layout
   * is called. In that case, defer creating the surface
   * until then.
   */
  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);
  if (width == 0 || height == 0)
    return TRUE;

  if (!meta_renderer_native_create_surface (renderer_native,
                                            width, height,
                                            &onscreen_native->surface,
                                            &egl_onscreen->egl_surface,
                                            error))
    return FALSE;


  _cogl_framebuffer_winsys_update_size (framebuffer, width, height);

  return TRUE;
}

static void
meta_renderer_native_release_onscreen (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *cogl_context = framebuffer->context;
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *egl_renderer = cogl_renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;

  onscreen_native = egl_onscreen->platform;

  /* flip state takes a reference on the onscreen so there should
   * never be outstanding flips when we reach here. */
  g_return_if_fail (onscreen_native->next_fb_id == 0);

  free_current_bo (onscreen);

  if (egl_onscreen->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface);
      egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

  if (onscreen_native->surface)
    {
      gbm_surface_destroy (onscreen_native->surface);
      onscreen_native->surface = NULL;
    }

  g_slice_free (MetaOnscreenNative, onscreen_native);
  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable = {
  .display_setup = meta_renderer_native_setup_egl_display,
  .display_destroy = meta_renderer_native_destroy_egl_display,
  .context_created = meta_renderer_native_egl_context_created,
  .cleanup_context = meta_renderer_native_egl_cleanup_context,
  .context_init = meta_renderer_native_init_egl_context
};

struct gbm_device *
meta_renderer_native_get_gbm (MetaRendererNative *renderer_native)
{
  return renderer_native->gbm;
}

int
meta_renderer_native_get_kms_fd (MetaRendererNative *renderer_native)
{
  return renderer_native->kms_fd;
}

void
meta_renderer_native_queue_modes_reset (MetaRendererNative *renderer_native)
{
  MetaRenderer *renderer = META_RENDERER (renderer_native);
  GList *l;

  for (l = meta_renderer_get_views (renderer); l; l = l->next)
    {
      ClutterStageView *stage_view = l->data;
      CoglFramebuffer *framebuffer =
        clutter_stage_view_get_framebuffer (stage_view);
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      MetaOnscreenNative *onscreen_native = egl_onscreen->platform;

      onscreen_native->pending_set_crtc = TRUE;
    }
}

gboolean
meta_renderer_native_set_legacy_view_size (MetaRendererNative *renderer_native,
                                           MetaRendererView   *view,
                                           int                 width,
                                           int                 height,
                                           GError            **error)
{
  ClutterBackend *clutter_backend = clutter_get_default_backend ();
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  CoglDisplay *cogl_display = cogl_context_get_display (cogl_context);
  CoglRendererEGL *egl_renderer = cogl_display->renderer->winsys;
  ClutterStageView *stage_view = CLUTTER_STAGE_VIEW (view);
  cairo_rectangle_int_t view_layout;

  clutter_stage_view_get_layout (stage_view, &view_layout);

  if (width != view_layout.width || height != view_layout.height)
    {
      MetaBackend *backend = meta_get_backend ();
      MetaMonitorManager *monitor_manager =
        meta_backend_get_monitor_manager (backend);
      MetaMonitorManagerKms *monitor_manager_kms =
        META_MONITOR_MANAGER_KMS (monitor_manager);
      CoglFramebuffer *framebuffer =
        clutter_stage_view_get_framebuffer (stage_view);
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
      CoglDisplayEGL *egl_display = cogl_display->winsys;
      struct gbm_surface *new_surface;
      EGLSurface new_egl_surface;

      /*
       * Ensure we don't have any pending flips that will want
       * to swap the current buffer.
       */
      while (onscreen_native->next_fb_id != 0)
        meta_monitor_manager_kms_wait_for_flip (monitor_manager_kms);

      /* Need to drop the GBM surface and create a new one */

      if (!meta_renderer_native_create_surface (renderer_native,
                                                width, height,
                                                &new_surface,
                                                &new_egl_surface,
                                                error))
        return FALSE;

      if (egl_onscreen->egl_surface)
        {
          _cogl_winsys_egl_make_current (cogl_display,
                                         egl_display->dummy_surface,
                                         egl_display->dummy_surface,
                                         egl_display->egl_context);
          eglDestroySurface (egl_renderer->edpy,
                             egl_onscreen->egl_surface);
        }

      /*
       * Release the current buffer and destroy the associated surface. The
       * kernel will deal with keeping the actual buffer alive until its no
       * longer used.
       */
      free_current_bo (onscreen);
      g_clear_pointer (&onscreen_native->surface, gbm_surface_destroy);

      /*
       * Update the active gbm and egl surfaces and make sure they they are
       * used for drawing the coming frame.
       */
      onscreen_native->surface = new_surface;
      egl_onscreen->egl_surface = new_egl_surface;
      _cogl_winsys_egl_make_current (cogl_display,
                                     egl_onscreen->egl_surface,
                                     egl_onscreen->egl_surface,
                                     egl_display->egl_context);

      _cogl_framebuffer_winsys_update_size (framebuffer, width, height);
    }

  meta_renderer_native_queue_modes_reset (renderer_native);

  return TRUE;
}

static const CoglWinsysVtable *
get_native_cogl_winsys_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The this winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_CUSTOM;
      vtable.name = "EGL_KMS";

      vtable.renderer_connect = meta_renderer_native_connect;
      vtable.renderer_disconnect = meta_renderer_native_disconnect;

      vtable.onscreen_init = meta_renderer_native_init_onscreen;
      vtable.onscreen_deinit = meta_renderer_native_release_onscreen;

      /* The KMS winsys doesn't support swap region */
      vtable.onscreen_swap_region = NULL;
      vtable.onscreen_swap_buffers_with_damage =
        meta_onscreen_native_swap_buffers_with_damage;

      vtable_inited = TRUE;
    }

  return &vtable;
}

static CoglRenderer *
meta_renderer_native_create_cogl_renderer (MetaRenderer *renderer)
{
  CoglRenderer *cogl_renderer;

  cogl_renderer = cogl_renderer_new ();
  cogl_renderer_set_custom_winsys (cogl_renderer,
                                   get_native_cogl_winsys_vtable);

  return cogl_renderer;
}

static void
meta_onscreen_native_set_view (CoglOnscreen     *onscreen,
                               MetaRendererView *view)
{
  CoglOnscreenEGL *egl_onscreen;
  MetaOnscreenNative *onscreen_native;

  egl_onscreen = onscreen->winsys;
  onscreen_native = egl_onscreen->platform;
  onscreen_native->view = view;
}

MetaRendererView *
meta_renderer_native_create_legacy_view (MetaRendererNative *renderer_native)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  CoglOnscreen *onscreen;
  CoglFramebuffer *framebuffer;
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  cairo_rectangle_int_t view_layout = { 0 };
  MetaRendererView *view;
  GError *error = NULL;

  if (!monitor_manager)
    return NULL;

  meta_monitor_manager_get_screen_size (monitor_manager,
                                        &view_layout.width,
                                        &view_layout.height);

  onscreen = cogl_onscreen_new (cogl_context,
                                view_layout.width,
                                view_layout.height);
  cogl_onscreen_set_swap_throttled (onscreen,
                                    _clutter_get_sync_to_vblank ());

  framebuffer = COGL_FRAMEBUFFER (onscreen);
  if (!cogl_framebuffer_allocate (framebuffer, &error))
    meta_fatal ("Failed to allocate onscreen framebuffer: %s\n",
                error->message);

  view = g_object_new (META_TYPE_RENDERER_VIEW,
                       "layout", &view_layout,
                       "framebuffer", framebuffer,
                       NULL);
  cogl_object_unref (framebuffer);

  meta_onscreen_native_set_view (onscreen, view);

  return view;
}

static MetaRendererView *
meta_renderer_native_create_view (MetaRenderer    *renderer,
                                  MetaMonitorInfo *monitor_info)
{
  MetaBackend *backend = meta_get_backend ();
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  CoglOnscreen *onscreen;
  CoglFramebuffer *framebuffer;
  MetaRendererView *view;
  GError *error = NULL;

  onscreen = cogl_onscreen_new (cogl_context,
                                monitor_info->rect.width,
                                monitor_info->rect.height);
  cogl_onscreen_set_swap_throttled (onscreen,
                                    _clutter_get_sync_to_vblank ());

  framebuffer = COGL_FRAMEBUFFER (onscreen);
  if (!cogl_framebuffer_allocate (framebuffer, &error))
    meta_fatal ("Failed to allocate onscreen framebuffer: %s\n",
                error->message);

  view = g_object_new (META_TYPE_RENDERER_VIEW,
                       "layout", &monitor_info->rect,
                       "framebuffer", framebuffer,
                       "monitor-info", monitor_info,
                       NULL);
  cogl_object_unref (framebuffer);

  meta_onscreen_native_set_view (onscreen, view);

  return view;
}

void
meta_renderer_native_finish_frame (MetaRendererNative *renderer_native)
{
  renderer_native->frame_counter++;
}

int64_t
meta_renderer_native_get_frame_counter (MetaRendererNative *renderer_native)
{
  return renderer_native->frame_counter;
}

static void
meta_renderer_native_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (object);

  switch (prop_id)
    {
    case PROP_KMS_FD:
      g_value_set_int (value, renderer_native->kms_fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_renderer_native_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (object);

  switch (prop_id)
    {
    case PROP_KMS_FD:
      renderer_native->kms_fd = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_renderer_native_finalize (GObject *object)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (object);

  g_clear_pointer (&renderer_native->dummy_gbm_surface, gbm_surface_destroy);
  g_clear_pointer (&renderer_native->gbm, gbm_device_destroy);

  G_OBJECT_CLASS (meta_renderer_native_parent_class)->finalize (object);
}

static gboolean
meta_renderer_native_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (initable);
  drmModeRes *resources;

  renderer_native->gbm = gbm_create_device (renderer_native->kms_fd);
  if (!renderer_native->gbm)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create gbm device");
      goto err;
    }

  resources = drmModeGetResources (renderer_native->kms_fd);
  if (!resources)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "drmModeGetResources failed");
      goto err_resources;
    }

  return TRUE;

err_resources:
  g_clear_pointer (&renderer_native->gbm, gbm_device_destroy);

err:
  return FALSE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = meta_renderer_native_initable_init;
}

static void
meta_renderer_native_init (MetaRendererNative *renderer_native)
{
}

static void
meta_renderer_native_class_init (MetaRendererNativeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaRendererClass *renderer_class = META_RENDERER_CLASS (klass);

  object_class->get_property = meta_renderer_native_get_property;
  object_class->set_property = meta_renderer_native_set_property;
  object_class->finalize = meta_renderer_native_finalize;

  renderer_class->create_cogl_renderer = meta_renderer_native_create_cogl_renderer;
  renderer_class->create_view = meta_renderer_native_create_view;

  g_object_class_install_property (object_class,
                                   PROP_KMS_FD,
                                   g_param_spec_int ("kms-fd",
                                                     "KMS fd",
                                                     "The KMS file descriptor",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT_ONLY));
}

MetaRendererNative *
meta_renderer_native_new (int      kms_fd,
                          GError **error)
{
  MetaRendererNative *renderer_native;

  renderer_native = g_object_new (META_TYPE_RENDERER_NATIVE,
                                  "kms-fd", kms_fd,
                                  NULL);
  if (!g_initable_init (G_INITABLE (renderer_native), NULL, error))
    {
      g_object_unref (renderer_native);
      return NULL;
    }

  return renderer_native;
}
