/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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
 */

#ifndef __COGL_TEXTURE_2D_PRIVATE_H
#define __COGL_TEXTURE_2D_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d.h"

#ifdef COGL_HAS_EGL_SUPPORT
#include "cogl-egl-defines.h"
#endif

struct _CoglTexture2D
{
  CoglTexture _parent;

  /* The internal format of the GL texture represented as a
     CoglPixelFormat */
  CoglPixelFormat internal_format;

  CoglBool auto_mipmap;
  CoglBool mipmaps_dirty;
  CoglBool is_foreign;

  /* TODO: factor out these OpenGL specific members into some form
   * of driver private state. */

  /* The internal format of the GL texture represented as a GL enum */
  GLenum gl_internal_format;
  /* The texture object number */
  GLuint gl_texture;
  GLenum gl_legacy_texobj_min_filter;
  GLenum gl_legacy_texobj_mag_filter;
  GLint gl_legacy_texobj_wrap_mode_s;
  GLint gl_legacy_texobj_wrap_mode_t;
  CoglTexturePixel first_pixel;
};

CoglTexture2D *
_cogl_texture_2d_new_from_bitmap (CoglBitmap *bmp,
                                  CoglBool can_convert_in_place);

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
                                     CoglError **error);
#endif

CoglTexture2D *
_cogl_texture_2d_create_base (CoglContext *ctx,
                              int width,
                              int height,
                              CoglPixelFormat internal_format,
                              CoglTextureLoader *loader);

void
_cogl_texture_2d_set_auto_mipmap (CoglTexture *tex,
                                  CoglBool value);

/*
 * _cogl_texture_2d_externally_modified:
 * @texture: A #CoglTexture2D object
 *
 * This should be called whenever the texture is modified other than
 * by using cogl_texture_set_region. It will cause the mipmaps to be
 * invalidated
 */
void
_cogl_texture_2d_externally_modified (CoglTexture *texture);

/*
 * _cogl_texture_2d_copy_from_framebuffer:
 * @texture: A #CoglTexture2D pointer
 * @src_x: X-position to within the framebuffer to read from
 * @src_y: Y-position to within the framebuffer to read from
 * @width: width of the rectangle to copy
 * @height: height of the rectangle to copy
 * @src_fb: A source #CoglFramebuffer to copy from
 * @dst_x: X-position to store the image within the texture
 * @dst_y: Y-position to store the image within the texture
 * @level: The mipmap level of @texture to copy too
 *
 * This copies a portion of the given @src_fb into the
 * texture.
 */
void
_cogl_texture_2d_copy_from_framebuffer (CoglTexture2D *texture,
                                        int src_x,
                                        int src_y,
                                        int width,
                                        int height,
                                        CoglFramebuffer *src_fb,
                                        int dst_x,
                                        int dst_y,
                                        int level);

#endif /* __COGL_TEXTURE_2D_PRIVATE_H */
