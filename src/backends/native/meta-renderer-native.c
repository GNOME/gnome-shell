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
#include "backends/native/meta-monitor-manager-kms.h"
#include "backends/native/meta-renderer-native.h"
#include "cogl/cogl.h"

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

  EGLSurface *pending_egl_surface;
  struct gbm_surface *pending_surface;
} MetaOnscreenNative;

struct _MetaRendererNative
{
  MetaRenderer parent;

  int kms_fd;
  struct gbm_device *gbm;
  CoglClosure *swap_notify_idle;

  int width, height;
  gboolean pending_set_crtc;
  struct gbm_surface *dummy_gbm_surface;

  CoglOnscreen *onscreen;
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
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (egl_renderer->edpy != EGL_NO_DISPLAY)
    eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

static void
flush_pending_swap_notify_cb (void *data,
                              void *user_data)
{
  CoglFramebuffer *framebuffer = data;

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

          cogl_object_unref (info);
        }
    }
}

static void
flush_pending_swap_notify_idle (void *user_data)
{
  CoglContext *context = user_data;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;

  /* This needs to be disconnected before invoking the callbacks in
   * case the callbacks cause it to be queued again */
  _cogl_closure_disconnect (renderer_native->swap_notify_idle);
  renderer_native->swap_notify_idle = NULL;

  g_list_foreach (context->framebuffers,
                  flush_pending_swap_notify_cb,
                  NULL);
}

static void
free_current_bo (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
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
queue_swap_notify_for_onscreen (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;

  /* We only want to notify that the swap is complete when the
   * application calls cogl_context_dispatch so instead of
   * immediately notifying we queue an idle callback */
  if (!renderer_native->swap_notify_idle)
    {
      renderer_native->swap_notify_idle =
        _cogl_poll_renderer_add_idle (renderer,
                                      flush_pending_swap_notify_idle,
                                      context,
                                      NULL);
    }

  onscreen_native->pending_swap_notify = TRUE;
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *cogl_renderer,
                               CoglError **error)
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
  _cogl_winsys_renderer_disconnect (cogl_renderer);

  return FALSE;
}

static void
setup_crtc_modes (CoglDisplay *display, int fb_id)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);

  meta_monitor_manager_kms_apply_crtc_modes (monitor_manager_kms, fb_id);
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;

  egl_display->platform = renderer_native;

  /* Force a full modeset / drmModeSetCrtc on
   * the first swap buffers call.
   */
  renderer_native->pending_set_crtc = TRUE;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
}

static CoglBool
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
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

  if (!_cogl_winsys_egl_make_current (display,
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
_cogl_winsys_egl_cleanup_context (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }
}

static void
flip_callback (void *user_data)
{
  CoglOnscreen *onscreen = user_data;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;

  queue_swap_notify_for_onscreen (onscreen);

  free_current_bo (onscreen);

  onscreen_native->current_fb_id = onscreen_native->next_fb_id;
  onscreen_native->next_fb_id = 0;

  onscreen_native->current_bo = onscreen_native->next_bo;
  onscreen_native->next_bo = NULL;

  cogl_object_unref (onscreen);
}

static void
_cogl_winsys_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                                const int *rectangles,
                                                int n_rectangles)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerKms *monitor_manager_kms =
    META_MONITOR_MANAGER_KMS (monitor_manager);
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
  uint32_t handle, stride;
  gboolean fb_in_use;
  uint32_t next_fb_id;

  /* If we already have a pending swap then block until it completes */
  while (onscreen_native->next_fb_id != 0)
    meta_monitor_manager_kms_wait_for_flip (monitor_manager_kms);

  if (onscreen_native->pending_egl_surface)
    {
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (renderer_native->onscreen);

      eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface);
      egl_onscreen->egl_surface = onscreen_native->pending_egl_surface;
      onscreen_native->pending_egl_surface = NULL;

      _cogl_framebuffer_winsys_update_size (fb,
                                            renderer_native->width,
                                            renderer_native->height);
      context->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_BIND;
    }
  parent_vtable->onscreen_swap_buffers_with_damage (onscreen,
                                                    rectangles,
                                                    n_rectangles);

  if (onscreen_native->pending_surface)
    {
      free_current_bo (onscreen);
      if (onscreen_native->surface)
        gbm_surface_destroy (onscreen_native->surface);
      onscreen_native->surface = onscreen_native->pending_surface;
      onscreen_native->pending_surface = NULL;
    }
  /* Now we need to set the CRTC to whatever is the front buffer */
  onscreen_native->next_bo =
    gbm_surface_lock_front_buffer (onscreen_native->surface);

  stride = gbm_bo_get_stride (onscreen_native->next_bo);
  handle = gbm_bo_get_handle (onscreen_native->next_bo).u32;

  if (drmModeAddFB (renderer_native->kms_fd,
                    renderer_native->width,
                    renderer_native->height,
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
  if (renderer_native->pending_set_crtc)
    {
      setup_crtc_modes (context->display, onscreen_native->next_fb_id);
      renderer_native->pending_set_crtc = FALSE;
    }

  next_fb_id = onscreen_native->next_fb_id;

  /* Reference will either be released in flip_callback, or if the fb
   * wasn't used, indicated by the return value below.
   */
  cogl_object_ref (onscreen);
  fb_in_use = meta_monitor_manager_kms_flip_all_crtcs (monitor_manager_kms,
                                                       next_fb_id,
                                                       flip_callback, onscreen);

  if (!fb_in_use)
    {
      drmModeRmFB (renderer_native->kms_fd, onscreen_native->next_fb_id);
      gbm_surface_release_buffer (onscreen_native->surface,
                                  onscreen_native->next_bo);
      onscreen_native->next_bo = NULL;
      onscreen_native->next_fb_id = 0;

      queue_swap_notify_for_onscreen (onscreen);

      g_object_unref (onscreen);
    }
}

static CoglBool
_cogl_winsys_egl_context_init (CoglContext *context,
                               CoglError **error)
{
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_SWAP_BUFFERS_EVENT, TRUE);
  /* TODO: remove this deprecated feature */
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT,
                  TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                  TRUE);

  return TRUE;
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  MetaRendererNative *renderer_native = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen;
  MetaOnscreenNative *onscreen_native;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  if (renderer_native->onscreen)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Cannot have multiple onscreens in the KMS platform");
      return FALSE;
    }

  renderer_native->onscreen = onscreen;

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

  onscreen_native = g_slice_new0 (MetaOnscreenNative);
  egl_onscreen->platform = onscreen_native;

  /* If a kms_fd is set then the display width and height
   * won't be available until meta_renderer_native_set_layout
   * is called. In that case, defer creating the surface
   * until then.
   */
  if (renderer_native->width == 0 ||
      renderer_native->height == 0)
    return TRUE;

  onscreen_native->surface =
    gbm_surface_create (renderer_native->gbm,
                        renderer_native->width,
                        renderer_native->height,
                        GBM_FORMAT_XRGB8888,
                        GBM_BO_USE_SCANOUT |
                        GBM_BO_USE_RENDERING);

  if (!onscreen_native->surface)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (EGLNativeWindowType) onscreen_native->surface,
                            NULL);
  if (egl_onscreen->egl_surface == EGL_NO_SURFACE)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        renderer_native->width,
                                        renderer_native->height);

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  MetaRendererNative *renderer_native = egl_display->platform;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  MetaOnscreenNative *onscreen_native;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;

  renderer_native->onscreen = NULL;

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
  .display_setup = _cogl_winsys_egl_display_setup,
  .display_destroy = _cogl_winsys_egl_display_destroy,
  .context_created = _cogl_winsys_egl_context_created,
  .cleanup_context = _cogl_winsys_egl_cleanup_context,
  .context_init = _cogl_winsys_egl_context_init
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
  renderer_native->pending_set_crtc = TRUE;
}

gboolean
meta_renderer_native_set_layout (MetaRendererNative *renderer_native,
                                 int                 width,
                                 int                 height,
                                 GError            **error)
{
  ClutterBackend *clutter_backend = clutter_get_default_backend ();
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  CoglDisplay *cogl_display = cogl_context_get_display (cogl_context);
  CoglDisplayEGL *egl_display = cogl_display->winsys;
  CoglRenderer *renderer = cogl_display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if ((width != renderer_native->width ||
       height != renderer_native->height) &&
      renderer_native->onscreen)
    {
      CoglOnscreenEGL *egl_onscreen = renderer_native->onscreen->winsys;
      MetaOnscreenNative *onscreen_native = egl_onscreen->platform;
      struct gbm_surface *new_surface;
      EGLSurface new_egl_surface;

      /* Need to drop the GBM surface and create a new one */

      new_surface = gbm_surface_create (renderer_native->gbm,
                                        width, height,
                                        GBM_FORMAT_XRGB8888,
                                        GBM_BO_USE_SCANOUT |
                                        GBM_BO_USE_RENDERING);

      if (!new_surface)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                           "Failed to allocate new surface");
          return FALSE;
        }

      new_egl_surface =
        eglCreateWindowSurface (egl_renderer->edpy,
                                egl_display->egl_config,
                                (EGLNativeWindowType) new_surface,
                                NULL);
      if (new_egl_surface == EGL_NO_SURFACE)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                           "Failed to allocate new surface");
          gbm_surface_destroy (new_surface);
          return FALSE;
        }

      if (onscreen_native->pending_egl_surface)
        eglDestroySurface (egl_renderer->edpy,
                           onscreen_native->pending_egl_surface);
      if (onscreen_native->pending_surface)
        gbm_surface_destroy (onscreen_native->pending_surface);

      /* If there's already a surface, wait until the next swap to switch
       * it out, otherwise, if we're just starting up we can use the new
       * surface right away.
       */
      if (onscreen_native->surface != NULL)
        {
          onscreen_native->pending_surface = new_surface;
          onscreen_native->pending_egl_surface = new_egl_surface;
        }
      else
        {
          CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (renderer_native->onscreen);

          onscreen_native->surface = new_surface;
          egl_onscreen->egl_surface = new_egl_surface;

          _cogl_framebuffer_winsys_update_size (framebuffer, width, height);
        }
    }

  renderer_native->width = width;
  renderer_native->height = height;

  renderer_native->pending_set_crtc = TRUE;

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

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;

      /* The KMS winsys doesn't support swap region */
      vtable.onscreen_swap_region = NULL;
      vtable.onscreen_swap_buffers_with_damage =
        _cogl_winsys_onscreen_swap_buffers_with_damage;

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
