/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
