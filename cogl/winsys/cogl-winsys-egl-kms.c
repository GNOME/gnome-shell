/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Rob Bradford <rob@linux.intel.com>
 *   Kristian Høgsberg (from eglkms.c)
 *   Benjamin Franzke (from eglkms.c)
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <glib.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>

#include "cogl-winsys-egl-kms-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-kms-renderer.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

static const CoglWinsysVtable *parent_vtable;

typedef struct _CoglRendererKMS
{
  int fd;
  struct gbm_device *gbm;
  CoglPollFD poll_fd;
} CoglRendererKMS;

typedef struct _CoglDisplayKMS
{
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  drmModeModeInfo mode;
  drmModeCrtcPtr saved_crtc;
  int width, height;
  gboolean pending_swap_notify;
} CoglDisplayKMS;

typedef struct _CoglOnscreenKMS
{
  struct gbm_surface *surface;
  uint32_t current_fb_id;
  uint32_t next_fb_id;
  struct gbm_bo *current_bo;
  struct gbm_bo *next_bo;
  gboolean pending_swap_notify;
} CoglOnscreenKMS;

static const char device_name[] = "/dev/dri/card0";

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererKMS, kms_renderer);
  g_slice_free (CoglRendererEGL, egl_renderer);
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererKMS *kms_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;
  egl_renderer->platform = g_slice_new0 (CoglRendererKMS);
  kms_renderer = egl_renderer->platform;

  kms_renderer->fd = open (device_name, O_RDWR);
  if (kms_renderer->fd < 0)
    {
      /* Probably permissions error */
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't open %s", device_name);
      return FALSE;
    }

  kms_renderer->gbm = gbm_create_device (kms_renderer->fd);
  if (kms_renderer->gbm == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't create gbm device");
      goto close_fd;
    }

  egl_renderer->edpy = eglGetDisplay ((EGLNativeDisplayType)kms_renderer->gbm);
  if (egl_renderer->edpy == EGL_NO_DISPLAY)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't get eglDisplay");
      goto destroy_gbm_device;
    }

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto egl_terminate;

  kms_renderer->poll_fd.fd = kms_renderer->fd;
  kms_renderer->poll_fd.events = COGL_POLL_FD_EVENT_IN;

  return TRUE;

egl_terminate:
  eglTerminate (egl_renderer->edpy);
destroy_gbm_device:
  gbm_device_destroy (kms_renderer->gbm);
close_fd:
  close (kms_renderer->fd);

  _cogl_winsys_renderer_disconnect (renderer);

  return FALSE;
}

static gboolean
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglEGLWinsysFeature surfaceless_feature = 0;
  const char *surfaceless_feature_name = "";
  drmModeRes *resources;
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  int i;

  kms_display = g_slice_new0 (CoglDisplayKMS);
  egl_display->platform = kms_display;

  switch (display->renderer->driver)
    {
    case COGL_DRIVER_GL:
      surfaceless_feature = COGL_EGL_WINSYS_FEATURE_SURFACELESS_OPENGL;
      surfaceless_feature_name = "opengl";
      break;
    case COGL_DRIVER_GLES1:
      surfaceless_feature = COGL_EGL_WINSYS_FEATURE_SURFACELESS_GLES1;
      surfaceless_feature_name = "gles1";
      break;
    case COGL_DRIVER_GLES2:
      surfaceless_feature = COGL_EGL_WINSYS_FEATURE_SURFACELESS_GLES2;
      surfaceless_feature_name = "gles2";
      break;
    case COGL_DRIVER_ANY:
      g_return_val_if_reached (FALSE);
    }

  if (!(egl_renderer->private_features & surfaceless_feature))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "EGL_KHR_surfaceless_%s extension not available",
                   surfaceless_feature_name);
      return FALSE;
    }

  resources = drmModeGetResources (kms_renderer->fd);
  if (!resources)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "drmModeGetResources failed");
      return FALSE;
    }

  for (i = 0; i < resources->count_connectors; i++)
    {
      connector = drmModeGetConnector (kms_renderer->fd,
                                       resources->connectors[i]);
      if (connector == NULL)
        continue;

      if (connector->connection == DRM_MODE_CONNECTED &&
          connector->count_modes > 0)
        break;

      drmModeFreeConnector(connector);
    }

  if (i == resources->count_connectors)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "No currently active connector found");
      return FALSE;
    }

  for (i = 0; i < resources->count_encoders; i++)
    {
      encoder = drmModeGetEncoder (kms_renderer->fd, resources->encoders[i]);

      if (encoder == NULL)
        continue;

      if (encoder->encoder_id == connector->encoder_id)
        break;

      drmModeFreeEncoder (encoder);
    }

  kms_display->saved_crtc = drmModeGetCrtc (kms_renderer->fd,
                                            encoder->crtc_id);

  kms_display->connector = connector;
  kms_display->encoder = encoder;
  kms_display->mode = connector->modes[0];
  kms_display->width = kms_display->mode.hdisplay;
  kms_display->height = kms_display->mode.vdisplay;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_slice_free (CoglDisplayKMS, egl_display->platform);
}

static gboolean
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  GError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;

  if (!eglMakeCurrent (egl_renderer->edpy,
                       EGL_NO_SURFACE,
                       EGL_NO_SURFACE,
                       egl_display->egl_context))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
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
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  /* Restore the saved CRTC - this failing should not propagate an error */
  if (kms_display->saved_crtc)
    {
      int ret = drmModeSetCrtc (kms_renderer->fd,
                                kms_display->saved_crtc->crtc_id,
                                kms_display->saved_crtc->buffer_id,
                                kms_display->saved_crtc->x,
                                kms_display->saved_crtc->y,
                                &kms_display->connector->connector_id, 1,
                                &kms_display->saved_crtc->mode);
      if (ret)
        g_critical (G_STRLOC ": Error restoring saved CRTC");

      drmModeFreeCrtc (kms_display->saved_crtc);
    }
}

static void
free_current_bo (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  if (kms_onscreen->current_fb_id)
    {
      drmModeRmFB (kms_renderer->fd,
                   kms_onscreen->current_fb_id);
      kms_onscreen->current_fb_id = 0;
    }
  if (kms_onscreen->current_bo)
    {
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->current_bo);
      kms_onscreen->current_bo = NULL;
    }
}

static void
page_flip_handler (int fd,
                   unsigned int frame,
                   unsigned int sec,
                   unsigned int usec,
                   void *data)
{
  CoglOnscreen *onscreen = data;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;

  free_current_bo (onscreen);

  kms_onscreen->current_fb_id = kms_onscreen->next_fb_id;
  kms_onscreen->next_fb_id = 0;

  kms_onscreen->current_bo = kms_onscreen->next_bo;
  kms_onscreen->next_bo = NULL;

  /* We only want to notify that the swap is complete when the
     application calls cogl_context_dispatch so instead of immediately
     notifying we'll set a flag to remember to notify later */
  kms_display->pending_swap_notify = TRUE;
  kms_onscreen->pending_swap_notify = TRUE;
}

static void
handle_drm_event (CoglRendererKMS *kms_renderer)
{
  drmEventContext evctx;

  memset (&evctx, 0, sizeof evctx);
  evctx.version = DRM_EVENT_CONTEXT_VERSION;
  evctx.page_flip_handler = page_flip_handler;
  drmHandleEvent (kms_renderer->fd, &evctx);
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  EGLint handle, pitch;

  /* If we already have a pending swap then block until it completes */
  while (kms_onscreen->next_fb_id != 0)
    handle_drm_event (kms_renderer);

  /* First chain-up. This will call eglSwapBuffers */
  parent_vtable->onscreen_swap_buffers (onscreen);

  /* Now we need to set the CRTC to whatever is the front buffer */
  kms_onscreen->next_bo = gbm_surface_lock_front_buffer (kms_onscreen->surface);

  pitch = gbm_bo_get_pitch (kms_onscreen->next_bo);
  handle = gbm_bo_get_handle (kms_onscreen->next_bo).u32;

  if (drmModeAddFB (kms_renderer->fd,
                    kms_display->width,
                    kms_display->height,
                    24, /* depth */
                    32, /* bpp */
                    pitch,
                    handle,
                    &kms_onscreen->next_fb_id) == 0)
    {
      drmModePageFlip (kms_renderer->fd,
                       kms_display->encoder->crtc_id,
                       kms_onscreen->next_fb_id,
                       DRM_MODE_PAGE_FLIP_EVENT,
                       onscreen);
    }
  else
    {
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->next_bo);
      kms_onscreen->next_bo = NULL;
      kms_onscreen->next_fb_id = 0;
    }
}

static gboolean
_cogl_winsys_egl_context_init (CoglContext *context,
                               GError **error)
{
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_SWAP_BUFFERS_EVENT, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT,
                  TRUE);

  return TRUE;
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenKMS *kms_onscreen;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

  kms_onscreen = g_slice_new0 (CoglOnscreenKMS);
  egl_onscreen->platform = kms_onscreen;

  kms_onscreen->surface =
    gbm_surface_create (kms_renderer->gbm,
                        kms_display->mode.hdisplay,
                        kms_display->mode.vdisplay,
                        GBM_BO_FORMAT_XRGB8888,
                        GBM_BO_USE_SCANOUT |
                        GBM_BO_USE_RENDERING);

  if (!kms_onscreen->surface)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) kms_onscreen->surface,
                            NULL);
  if (egl_onscreen->egl_surface == EGL_NO_SURFACE)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        kms_display->width,
                                        kms_display->height);

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;

  kms_onscreen = egl_onscreen->platform;

  /* If have a pending swap then block until it completes */
  while (kms_onscreen->next_fb_id != 0)
    handle_drm_event (kms_renderer);

  free_current_bo (onscreen);

  if (egl_onscreen->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface);
      egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

  if (kms_onscreen->surface)
    {
      gbm_surface_destroy (kms_onscreen->surface);
      kms_onscreen->surface = NULL;
    }

  g_slice_free (CoglOnscreenKMS, kms_onscreen);
  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
}

static void
_cogl_winsys_poll_get_info (CoglContext *context,
                            CoglPollFD **poll_fds,
                            int *n_poll_fds,
                            gint64 *timeout)
{
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  *poll_fds = &kms_renderer->poll_fd;
  *n_poll_fds = 1;

  /* If we've already got a pending swap notify then we'll dispatch
     immediately */
  *timeout = kms_display->pending_swap_notify ? 0 : -1;
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
      CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;

      if (kms_onscreen->pending_swap_notify)
        {
          _cogl_onscreen_notify_swap_buffers (onscreen);
          kms_onscreen->pending_swap_notify = FALSE;
        }
    }
}

static void
_cogl_winsys_poll_dispatch (CoglContext *context,
                            const CoglPollFD *poll_fds,
                            int n_poll_fds)
{
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  int i;

  for (i = 0; i < n_poll_fds; i++)
    if (poll_fds[i].fd == kms_renderer->fd)
      {
        if (poll_fds[i].revents)
          handle_drm_event (kms_renderer);

        break;
      }

  if (kms_display->pending_swap_notify)
    {
      g_list_foreach (context->framebuffers,
                      flush_pending_swap_notify_cb,
                      NULL);
      kms_display->pending_swap_notify = FALSE;
    }
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .cleanup_context = _cogl_winsys_egl_cleanup_context,
    .context_init = _cogl_winsys_egl_context_init
  };

const CoglWinsysVtable *
_cogl_winsys_egl_kms_get_vtable (void)
{
  static gboolean vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_KMS winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_EGL_KMS;
      vtable.name = "EGL_KMS";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;

      /* The KMS winsys doesn't support swap region */
      vtable.onscreen_swap_region = NULL;
      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;

      vtable.poll_get_info = _cogl_winsys_poll_get_info;
      vtable.poll_dispatch = _cogl_winsys_poll_dispatch;

      vtable_inited = TRUE;
    }

  return &vtable;
}

int
cogl_kms_renderer_get_kms_fd (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), -1);

  if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererKMS *kms_renderer = egl_renderer->platform;
      return kms_renderer->fd;
    }
  else
    return -1;
}
