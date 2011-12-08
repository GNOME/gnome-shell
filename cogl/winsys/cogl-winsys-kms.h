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
 * Authors:
 *   Rob Bradford <rob@linux.intel.com>
 */

#ifndef _COGL_WINSYS_KMS_PRIVATE_H_
#define _COGL_WINSYS_KMS_PRIVATE_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <glib.h>

#include "cogl-winsys-private.h"

typedef struct _CoglRendererKMS
{
  int fd;
  struct gbm_device *gbm;
  EGLDisplay *dpy;
} CoglRendererKMS;

typedef struct _CoglDisplayKMS
{
  EGLContext egl_context;
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  drmModeModeInfo mode;
  drmModeCrtcPtr saved_crtc;
  int width, height;
} CoglDisplayKMS;

typedef struct _CoglOnscreenKMS
{
  CoglContext *cogl_context;

  uint32_t fb_id[2];
  struct gbm_bo *bo[2];
  unsigned int fb, color_rb[2], depth_rb;
  EGLImageKHR image[2];
  int current_frame;
} CoglOnscreenKMS;

gboolean
_cogl_winsys_kms_connect (CoglRendererKMS  *kms_renderer,
                          GError          **error);

gboolean
_cogl_winsys_kms_onscreen_init (CoglContext      *context,
                                CoglRendererKMS  *kms_renderer,
                                CoglDisplayKMS   *kms_display,
                                CoglOnscreenKMS  *kms_onscreen,
                                GError          **error);
void
_cogl_winsys_kms_onscreen_deinit (CoglRendererKMS *kms_renderer,
                                  CoglOnscreenKMS *kms_onscreen);

gboolean
_cogl_winsys_kms_display_setup (CoglDisplay  *display,
                                GError      **error);

void
_cogl_winsys_kms_swap_buffers (CoglRendererKMS *kms_renderer,
                               CoglDisplayKMS  *kms_display,
                               CoglOnscreenKMS *kms_onscreen);

void
_cogl_winsys_kms_bind (CoglRendererKMS *kms_renderer,
                       CoglDisplayKMS  *kms_display);

gboolean
_cogl_winsys_kms_create_context (CoglRenderer *renderer,
                                 CoglDisplay *display,
                                 GError **error);

gboolean
_cogl_winsys_kms_destroy_context (CoglRendererKMS  *kms_renderer,
                                  CoglDisplayKMS   *kms_display,
                                  GError          **error);

#endif /* _COGL_WINSYS_KMS_PRIVATE_H_ */
