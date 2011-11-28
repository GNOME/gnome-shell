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
 * Some code inspired by mesa demos eglkms.c.
 *
 * Authors:
 *   Rob Bradford <rob@linux.intel.com>
 *   Kristian Høgsberg (from eglkms.c)
 *   Benjamin Franzke (from eglkms.c)
 */

#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#include "cogl-util.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-winsys-private.h"
#include "cogl-feature-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-onscreen-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-private.h"

#include "cogl-winsys-kms.h"

static const char device_name[] = "/dev/dri/card0";

gboolean
_cogl_winsys_kms_connect (CoglRendererKMS  *kms_renderer,
                          GError          **error)
{
  EGLint major, minor;

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

  kms_renderer->dpy = eglGetDisplay ((EGLNativeDisplayType)kms_renderer->gbm);
  if (kms_renderer->dpy == EGL_NO_DISPLAY)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't get eglDisplay");
      goto destroy_gbm_device;
    }

  if (!eglInitialize (kms_renderer->dpy, &major, &minor))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't initialize EGL");
      goto egl_terminate;
    }

  return TRUE;
egl_terminate:
   eglTerminate (kms_renderer->dpy);
destroy_gbm_device:
   gbm_device_destroy (kms_renderer->gbm);
close_fd:
   close (kms_renderer->fd);

   return FALSE;
}

gboolean
_cogl_winsys_kms_display_setup (CoglDisplay *display, GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = &egl_display->kms_display;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererKMS *kms_renderer = &egl_renderer->kms_renderer;
  drmModeRes *resources;
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  int i;

  if (!(egl_renderer->private_features &
        COGL_EGL_WINSYS_FEATURE_SURFACELESS_OPENGL))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "EGL_KHR_surfaceless_opengl extension not available");
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
      connector = drmModeGetConnector (kms_renderer->fd, resources->connectors[i]);
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
                                            kms_display->encoder->crtc_id);

  kms_display->connector = connector;
  kms_display->encoder = encoder;
  kms_display->mode = connector->modes[0];
  kms_display->width = kms_display->mode.hdisplay;
  kms_display->height = kms_display->mode.vdisplay;

  return TRUE;
}

gboolean
_cogl_winsys_kms_create_context (CoglRendererKMS *kms_renderer,
                                 CoglDisplayKMS  *kms_display,
                                 GError          **error)
{
  kms_display->egl_context = eglCreateContext (kms_renderer->dpy, NULL, EGL_NO_CONTEXT, NULL);

  if (kms_display->egl_context == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Couldn't create EGL context");
      return FALSE;
    }

  if (!eglMakeCurrent (kms_renderer->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, kms_display->egl_context))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to make context current");
      return FALSE;
    }

  return TRUE;
}

gboolean
_cogl_winsys_kms_onscreen_init (CoglContext      *context,
                                CoglRendererKMS  *kms_renderer,
                                CoglDisplayKMS   *kms_display,
                                CoglOnscreenKMS  *kms_onscreen,
                                GError          **error)
{
  int i;

  kms_onscreen->cogl_context = context;

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

      kms_onscreen->image[i] = _cogl_egl_create_image (context,
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

      context->glBindRenderbuffer (GL_RENDERBUFFER_EXT, kms_onscreen->color_rb[i]);
      context->glEGLImageTargetRenderbufferStorage (GL_RENDERBUFFER, kms_onscreen->image[i]);
      context->glBindRenderbuffer (GL_RENDERBUFFER_EXT, 0);

      handle = gbm_bo_get_handle (kms_onscreen->bo[i]).u32;
      stride = gbm_bo_get_pitch (kms_onscreen->bo[i]);

      if (drmModeAddFB (kms_renderer->fd,
                         kms_display->mode.hdisplay, kms_display->mode.vdisplay,
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

  context->glGenRenderbuffers(1, &kms_onscreen->depth_rb);
  context->glBindRenderbuffer(GL_RENDERBUFFER_EXT, kms_onscreen->depth_rb);
  context->glRenderbufferStorage(GL_RENDERBUFFER_EXT,
                        GL_DEPTH_COMPONENT,
                        kms_display->mode.hdisplay, kms_display->mode.vdisplay);
  context->glBindRenderbuffer (GL_RENDERBUFFER_EXT, 0);

  context->glFramebufferRenderbuffer (GL_FRAMEBUFFER_EXT,
                                      GL_DEPTH_ATTACHMENT_EXT,
                                      GL_RENDERBUFFER_EXT,
                                      kms_onscreen->depth_rb);

  kms_onscreen->current_frame = 0;
  _cogl_winsys_kms_swap_buffers (kms_renderer, kms_display, kms_onscreen);

  return TRUE;
}

void
_cogl_winsys_kms_onscreen_deinit (CoglRendererKMS *kms_renderer,
                                  CoglOnscreenKMS *kms_onscreen)
{
  int i;

  CoglContext *context = kms_onscreen->cogl_context;

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
}

gboolean
_cogl_winsys_kms_destroy_context (CoglRendererKMS  *kms_renderer,
                                  CoglDisplayKMS   *kms_display,
                                  GError          **error)
{
  int ret;

  /* Restore the saved CRTC - this failing should not propagate an error */
  ret = drmModeSetCrtc (kms_renderer->fd,
                        kms_display->saved_crtc->crtc_id,
                        kms_display->saved_crtc->buffer_id,
                        kms_display->saved_crtc->x, kms_display->saved_crtc->y,
                        &kms_display->connector->connector_id, 1,
                        &kms_display->saved_crtc->mode);
  if (ret)
    {
      g_critical (G_STRLOC ": Error restoring saved CRTC");
    }

  drmModeFreeCrtc (kms_display->saved_crtc);

  return TRUE;
}

void
_cogl_winsys_kms_swap_buffers (CoglRendererKMS *kms_renderer,
                               CoglDisplayKMS  *kms_display,
                               CoglOnscreenKMS *kms_onscreen)
{
  CoglContext *context = kms_onscreen->cogl_context;

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
                                      kms_onscreen->color_rb[kms_onscreen->current_frame]);

  if (context->glCheckFramebufferStatus (GL_FRAMEBUFFER_EXT) !=
      GL_FRAMEBUFFER_COMPLETE)
    {
      g_error (G_STRLOC ": FBO not complete");
    }
}

void
_cogl_winsys_kms_bind (CoglRendererKMS *kms_renderer,
                       CoglDisplayKMS  *kms_display)
{
  eglMakeCurrent (kms_renderer->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, kms_display->egl_context);
}
