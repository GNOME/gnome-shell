/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_TEXTURE_2D_H
#define __COGL_TEXTURE_2D_H

#include "cogl-handle.h"
#include "cogl-pipeline-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d.h"

struct _CoglTexture2D
{
  CoglTexture     _parent;

  /* The internal format of the GL texture represented as a
     CoglPixelFormat */
  CoglPixelFormat format;
  /* The internal format of the GL texture represented as a GL enum */
  GLenum          gl_format;
  /* The texture object number */
  GLuint          gl_texture;
  int             width;
  int             height;
  GLenum          min_filter;
  GLenum          mag_filter;
  GLint           wrap_mode_s;
  GLint           wrap_mode_t;
  gboolean        auto_mipmap;
  gboolean        mipmaps_dirty;
  gboolean        is_foreign;

  CoglTexturePixel first_pixel;
};

#if defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base)
/* NB: The reason we require the width, height and format to be passed
 * even though they may seem redundant is because GLES 1/2 don't
 * provide a way to query these properties. */
CoglTexture2D *
_cogl_egl_texture_2d_new_from_image (CoglContext *ctx,
                                     int width,
                                     int height,
                                     CoglPixelFormat format,
                                     EGLImageKHR image,
                                     GError **error);
#endif

/*
 * _cogl_texture_2d_externally_modified:
 * @handle: A handle to a 2D texture
 *
 * This should be called whenever the texture is modified other than
 * by using cogl_texture_set_region. It will cause the mipmaps to be
 * invalidated
 */
void
_cogl_texture_2d_externally_modified (CoglHandle handle);

/*
 * _cogl_texture_2d_copy_from_framebuffer:
 * @handle: A handle to a 2D texture
 * @dst_x: X-position to store the image within the texture
 * @dst_y: Y-position to store the image within the texture
 * @src_x: X-position to within the framebuffer to read from
 * @src_y: Y-position to within the framebuffer to read from
 * @width: width of the rectangle to copy
 * @height: height of the rectangle to copy
 *
 * This copies a portion of the current read framebuffer into the
 * texture.
 */
void
_cogl_texture_2d_copy_from_framebuffer (CoglHandle handle,
                                        int dst_x,
                                        int dst_y,
                                        int src_x,
                                        int src_y,
                                        int width,
                                        int height);

#endif /* __COGL_TEXTURE_2D_H */
