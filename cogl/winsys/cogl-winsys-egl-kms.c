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
#include <xf86drmMode.h>
#include <gbm.h>
#include <glib.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "cogl-winsys-egl-kms-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-kms-renderer.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

typedef struct _CoglRendererKMS
{
  int fd;
  struct gbm_device *gbm;
} CoglRendererKMS;

typedef struct _CoglDisplayKMS
{
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  drmModeModeInfo mode;
  drmModeCrtcPtr saved_crtc;
  int width, height;
} CoglDisplayKMS;

typedef struct _CoglOnscreenKMS
{
  uint32_t fb_id[2];
  struct gbm_bo *bo[2];
  unsigned int fb, color_rb[2], depth_rb;
  EGLImageKHR image[2];
  int current_frame;
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
_cogl_winsys_egl_try_create_context (CoglDisplay *display,
                                     EGLint *attribs,
                                     GError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;

  egl_display->egl_context = eglCreateContext (egl_renderer->edpy,
                                               NULL,
                                               EGL_NO_CONTEXT,
                                               attribs);

  if (egl_display->egl_context == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Couldn't create EGL context");
      return FALSE;
    }

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

  if (drmModeSetCrtc (kms_renderer->fd,
                      kms_display->encoder->crtc_id,
                      kms_onscreen->fb_id[kms_onscreen->current_frame],
                      0, 0,
                      &kms_display->connector->connector_id,
                      1,
                      &kms_display->mode) != 0)
    {
      g_error (G_STRLOC ": Setting CRTC failed");
    }

  /* Update frame that we're drawing to be the new one */
  kms_onscreen->current_frame ^= 1;

  context->glBindFramebuffer (GL_FRAMEBUFFER_EXT, kms_onscreen->fb);
  context->glFramebufferRenderbuffer (GL_FRAMEBUFFER_EXT,
                                      GL_COLOR_ATTACHMENT0_EXT,
                                      GL_RENDERBUFFER_EXT,
                                      kms_onscreen->
                                      color_rb[kms_onscreen->current_frame]);

  if (context->glCheckFramebufferStatus (GL_FRAMEBUFFER_EXT) !=
      GL_FRAMEBUFFER_COMPLETE)
    {
      g_error (G_STRLOC ": FBO not complete");
    }
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
  int i;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

  kms_onscreen = g_slice_new0 (CoglOnscreenKMS);
  egl_onscreen->platform = kms_onscreen;

  context->glGenRenderbuffers (2, kms_onscreen->color_rb);

  for (i = 0; i < 2; i++)
    {
      uint32_t handle, stride;

      kms_onscreen->bo[i] =
        gbm_bo_create (kms_renderer->gbm,
                       kms_display->mode.hdisplay, kms_display->mode.vdisplay,
                       GBM_BO_FORMAT_XRGB8888,
                       GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
      if (!kms_onscreen->bo[i])
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "Failed to allocate buffer");
          return FALSE;
        }

      kms_onscreen->image[i] =
        _cogl_egl_create_image (context,
                                EGL_NATIVE_PIXMAP_KHR,
                                kms_onscreen->bo[i],
                                NULL);

      if (kms_onscreen->image[i] == EGL_NO_IMAGE_KHR)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "Failed to create EGL image");
          return FALSE;
        }

      context->glBindRenderbuffer (GL_RENDERBUFFER_EXT,
                                   kms_onscreen->color_rb[i]);
      context->glEGLImageTargetRenderbufferStorage (GL_RENDERBUFFER,
                                                    kms_onscreen->image[i]);
      context->glBindRenderbuffer (GL_RENDERBUFFER_EXT, 0);

      handle = gbm_bo_get_handle (kms_onscreen->bo[i]).u32;
      stride = gbm_bo_get_pitch (kms_onscreen->bo[i]);

      if (drmModeAddFB (kms_renderer->fd,
                        kms_display->mode.hdisplay,
                        kms_display->mode.vdisplay,
                        24, 32,
                        stride,
                        handle,
                        &kms_onscreen->fb_id[i]) != 0)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "Failed to create framebuffer from buffer");
          return FALSE;
        }
    }

  context->glGenFramebuffers (1, &kms_onscreen->fb);
  context->glBindFramebuffer (GL_FRAMEBUFFER_EXT, kms_onscreen->fb);

  context->glGenRenderbuffers (1, &kms_onscreen->depth_rb);
  context->glBindRenderbuffer (GL_RENDERBUFFER_EXT, kms_onscreen->depth_rb);
  context->glRenderbufferStorage (GL_RENDERBUFFER_EXT,
                                  GL_DEPTH_COMPONENT16,
                                  kms_display->mode.hdisplay,
                                  kms_display->mode.vdisplay);
  context->glBindRenderbuffer (GL_RENDERBUFFER_EXT, 0);

  context->glFramebufferRenderbuffer (GL_FRAMEBUFFER_EXT,
                                      GL_DEPTH_ATTACHMENT_EXT,
                                      GL_RENDERBUFFER_EXT,
                                      kms_onscreen->depth_rb);

  kms_onscreen->current_frame = 0;
  _cogl_winsys_onscreen_swap_buffers (onscreen);

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
  int i;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;

  kms_onscreen = egl_onscreen->platform;

  context->glBindFramebuffer (GL_FRAMEBUFFER_EXT, kms_onscreen->fb);
  context->glFramebufferRenderbuffer (GL_FRAMEBUFFER_EXT,
                                      GL_COLOR_ATTACHMENT0_EXT,
                                      GL_RENDERBUFFER_EXT,
                                      0);
  context->glDeleteRenderbuffers(2, kms_onscreen->color_rb);
  context->glFramebufferRenderbuffer (GL_FRAMEBUFFER_EXT,
                                      GL_DEPTH_ATTACHMENT_EXT,
                                      GL_RENDERBUFFER_EXT,
                                      0);
  context->glDeleteRenderbuffers(1, &kms_onscreen->depth_rb);

  for (i = 0; i < 2; i++)
    {
      drmModeRmFB (kms_renderer->fd, kms_onscreen->fb_id[i]);
      _cogl_egl_destroy_image (context, kms_onscreen->image[i]);
      gbm_bo_destroy (kms_onscreen->bo[i]);
    }

  g_slice_free (CoglOnscreenKMS, kms_onscreen);
  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  eglMakeCurrent (egl_renderer->edpy,
                  EGL_NO_SURFACE,
                  EGL_NO_SURFACE,
                  egl_display->egl_context);
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  _cogl_winsys_onscreen_bind (onscreen);
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .try_create_context = _cogl_winsys_egl_try_create_context,
    .cleanup_context = _cogl_winsys_egl_cleanup_context
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

      vtable = *_cogl_winsys_egl_get_vtable ();

      vtable.id = COGL_WINSYS_ID_EGL_KMS;
      vtable.name = "EGL_KMS";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;
      vtable.onscreen_bind = _cogl_winsys_onscreen_bind;

      /* The KMS winsys doesn't support swap region */
      vtable.onscreen_swap_region = NULL;
      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;

      vtable.onscreen_update_swap_throttled =
        _cogl_winsys_onscreen_update_swap_throttled;

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
