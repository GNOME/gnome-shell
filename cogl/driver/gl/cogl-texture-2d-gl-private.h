/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef _COGL_TEXTURE_2D_GL_PRIVATE_H_
#define _COGL_TEXTURE_2D_GL_PRIVATE_H_

#include "cogl-types.h"
#include "cogl-context-private.h"
#include "cogl-texture.h"

void
_cogl_texture_2d_gl_free (CoglTexture2D *tex_2d);

CoglBool
_cogl_texture_2d_gl_can_create (CoglContext *ctx,
                                int width,
                                int height,
                                CoglPixelFormat internal_format);

void
_cogl_texture_2d_gl_init (CoglTexture2D *tex_2d);

CoglBool
_cogl_texture_2d_gl_allocate (CoglTexture *tex,
                              CoglError **error);

CoglTexture2D *
_cogl_texture_2d_gl_new_from_bitmap (CoglBitmap *bmp,
                                     CoglPixelFormat internal_format,
                                     CoglBool can_convert_in_place,
                                     CoglError **error);

#if defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base)
CoglTexture2D *
_cogl_egl_texture_2d_gl_new_from_image (CoglContext *ctx,
                                        int width,
                                        int height,
                                        CoglPixelFormat format,
                                        EGLImageKHR image,
                                        CoglError **error);
#endif

void
_cogl_texture_2d_gl_flush_legacy_texobj_filters (CoglTexture *tex,
                                                 GLenum min_filter,
                                                 GLenum mag_filter);

void
_cogl_texture_2d_gl_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                    GLenum wrap_mode_s,
                                                    GLenum wrap_mode_t,
                                                    GLenum wrap_mode_p);

void
_cogl_texture_2d_gl_copy_from_framebuffer (CoglTexture2D *tex_2d,
                                           int src_x,
                                           int src_y,
                                           int width,
                                           int height,
                                           CoglFramebuffer *src_fb,
                                           int dst_x,
                                           int dst_y,
                                           int level);

unsigned int
_cogl_texture_2d_gl_get_gl_handle (CoglTexture2D *tex_2d);

void
_cogl_texture_2d_gl_generate_mipmap (CoglTexture2D *tex_2d);

CoglBool
_cogl_texture_2d_gl_copy_from_bitmap (CoglTexture2D *tex_2d,
                                      int src_x,
                                      int src_y,
                                      int width,
                                      int height,
                                      CoglBitmap *bitmap,
                                      int dst_x,
                                      int dst_y,
                                      int level,
                                      CoglError **error);

void
_cogl_texture_2d_gl_get_data (CoglTexture2D *tex_2d,
                              CoglPixelFormat format,
                              int rowstride,
                              uint8_t *data);

#endif /* _COGL_TEXTURE_2D_GL_PRIVATE_H_ */
