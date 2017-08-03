/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

#include "config.h"

#define GL_GLEXT_PROTOTYPES

#include "backends/native/meta-renderer-native-gles3.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <gio/gio.h>
#include <GLES3/gl3.h>
#include <string.h>

#include "backends/meta-egl-ext.h"
#include "backends/meta-gles3.h"
#include "backends/meta-gles3-table.h"

/*
 * GL/gl.h being included may conflit with gl3.h on some architectures.
 * Make sure that hasn't happened on any architecture.
 */
#ifdef GL_VERSION_1_1
#error "Somehow included OpenGL headers when we shouldn't have"
#endif

static EGLImageKHR
create_egl_image (MetaEgl       *egl,
                  EGLDisplay     egl_display,
                  EGLContext     egl_context,
                  unsigned int   width,
                  unsigned int   height,
                  uint32_t       n_planes,
                  uint32_t      *strides,
                  uint32_t      *offsets,
                  uint64_t      *modifiers,
                  uint32_t       format,
                  int            fd,
                  GError       **error)
{
  EGLint attribs[37];
  int atti = 0;
  gboolean has_modifier;

  /* This requires the Mesa commit in
   * Mesa 10.3 (08264e5dad4df448e7718e782ad9077902089a07) or
   * Mesa 10.2.7 (55d28925e6109a4afd61f109e845a8a51bd17652).
   * Otherwise Mesa closes the fd behind our back and re-importing
   * will fail.
   * https://bugs.freedesktop.org/show_bug.cgi?id=76188
   */

  attribs[atti++] = EGL_WIDTH;
  attribs[atti++] = width;
  attribs[atti++] = EGL_HEIGHT;
  attribs[atti++] = height;
  attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
  attribs[atti++] = format;

  has_modifier = (modifiers[0] != DRM_FORMAT_MOD_INVALID);

  if (n_planes > 0)
    {
      attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
      attribs[atti++] = fd;
      attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
      attribs[atti++] = offsets[0];
      attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
      attribs[atti++] = strides[0];
      if (has_modifier)
        {
          attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
          attribs[atti++] = modifiers[0] & 0xFFFFFFFF;
          attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
          attribs[atti++] = modifiers[0] >> 32;
        }
    }

  if (n_planes > 1)
    {
      attribs[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
      attribs[atti++] = fd;
      attribs[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
      attribs[atti++] = offsets[1];
      attribs[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
      attribs[atti++] = strides[1];
      if (has_modifier)
        {
          attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
          attribs[atti++] = modifiers[1] & 0xFFFFFFFF;
          attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
          attribs[atti++] = modifiers[1] >> 32;
        }
    }

  if (n_planes > 2)
    {
      attribs[atti++] = EGL_DMA_BUF_PLANE2_FD_EXT;
      attribs[atti++] = fd;
      attribs[atti++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
      attribs[atti++] = offsets[2];
      attribs[atti++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
      attribs[atti++] = strides[2];
      if (has_modifier)
        {
          attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
          attribs[atti++] = modifiers[2] & 0xFFFFFFFF;
          attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
          attribs[atti++] = modifiers[2] >> 32;
        }
    }

  attribs[atti++] = EGL_NONE;

  return meta_egl_create_image (egl, egl_display, EGL_NO_CONTEXT,
                                EGL_LINUX_DMA_BUF_EXT, NULL,
                                attribs,
                                error);
}

static void
paint_egl_image (MetaGles3   *gles3,
                 EGLImageKHR  egl_image,
                 int          width,
                 int          height)
{
  GLuint texture;
  GLuint framebuffer;

  meta_gles3_clear_error (gles3);

  GLBAS (gles3, glGenFramebuffers, (1, &framebuffer));
  GLBAS (gles3, glBindFramebuffer, (GL_READ_FRAMEBUFFER, framebuffer));

  GLBAS (gles3, glActiveTexture, (GL_TEXTURE0));
  GLBAS (gles3, glGenTextures, (1, &texture));
  GLBAS (gles3, glBindTexture, (GL_TEXTURE_2D, texture));
  GLEXT (gles3, glEGLImageTargetTexture2DOES, (GL_TEXTURE_2D, egl_image));
  GLBAS (gles3, glTexParameteri, (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                  GL_NEAREST));
  GLBAS (gles3, glTexParameteri, (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                  GL_NEAREST));
  GLBAS (gles3, glTexParameteri, (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                  GL_CLAMP_TO_EDGE));
  GLBAS (gles3, glTexParameteri, (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                  GL_CLAMP_TO_EDGE));
  GLBAS (gles3, glTexParameteri, (GL_TEXTURE_2D, GL_TEXTURE_WRAP_R_OES,
                                  GL_CLAMP_TO_EDGE));

  GLBAS (gles3, glFramebufferTexture2D, (GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                         GL_TEXTURE_2D, texture, 0));

  GLBAS (gles3, glBindFramebuffer, (GL_READ_FRAMEBUFFER, framebuffer));
  GLBAS (gles3, glBlitFramebuffer, (0, height, width, 0,
                                    0, 0, width, height,
                                    GL_COLOR_BUFFER_BIT,
                                    GL_NEAREST));
}

gboolean
meta_renderer_native_gles3_blit_shared_bo (MetaEgl        *egl,
                                           MetaGles3      *gles3,
                                           EGLDisplay      egl_display,
                                           EGLContext      egl_context,
                                           EGLSurface      egl_surface,
                                           struct gbm_bo  *shared_bo,
                                           GError        **error)
{
  int shared_bo_fd;
  unsigned int width;
  unsigned int height;
  uint32_t i, n_planes;
  uint32_t strides[4] = { 0, };
  uint32_t offsets[4] = { 0, };
  uint64_t modifiers[4] = { 0, };
  uint32_t format;
  EGLImageKHR egl_image;

  shared_bo_fd = gbm_bo_get_fd (shared_bo);
  if (shared_bo_fd < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to export gbm_bo: %s", strerror (errno));
      return FALSE;
    }

  width = gbm_bo_get_width (shared_bo);
  height = gbm_bo_get_height (shared_bo);
  format = gbm_bo_get_format (shared_bo);

  n_planes = gbm_bo_get_plane_count (shared_bo);
  for (i = 0; i < n_planes; i++)
    {
      strides[i] = gbm_bo_get_stride_for_plane (shared_bo, i);
      offsets[i] = gbm_bo_get_offset (shared_bo, i);
      modifiers[i] = gbm_bo_get_modifier (shared_bo);
    }

  egl_image = create_egl_image (egl,
                                egl_display,
                                egl_context,
                                width, height,
                                n_planes,
                                strides, offsets,
                                modifiers, format,
                                shared_bo_fd,
                                error);
  close (shared_bo_fd);

  if (!egl_image)
    return FALSE;

  paint_egl_image (gles3, egl_image, width, height);

  meta_egl_destroy_image (egl, egl_display, egl_image, NULL);

  return TRUE;
}

void
meta_renderer_native_gles3_read_pixels (MetaEgl   *egl,
                                        MetaGles3 *gles3,
                                        int        width,
                                        int        height,
                                        uint8_t   *target_data)
{
  int y;

  GLBAS (gles3, glFinish, ());

  for (y = 0; y < height; y++)
    {
      GLBAS (gles3, glReadPixels, (0, height - y, width, 1,
                                   GL_RGBA, GL_UNSIGNED_BYTE,
                                   target_data + width * y * 4));
    }
}
