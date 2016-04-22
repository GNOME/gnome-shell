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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef _COGL_TEXTURE_2D_GL_H_
#define _COGL_TEXTURE_2D_GL_H_

#include "cogl-context.h"
#include "cogl-texture-2d.h"

COGL_BEGIN_DECLS

/**
 * cogl_texture_2d_gl_new_from_foreign:
 * @ctx: A #CoglContext
 * @gl_handle: A GL handle for a GL_TEXTURE_2D texture object
 * @width: Width of the foreign GL texture
 * @height: Height of the foreign GL texture
 * @format: The format of the texture
 *
 * Wraps an existing GL_TEXTURE_2D texture object as a #CoglTexture2D.
 * This can be used for integrating Cogl with software using OpenGL
 * directly.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can declare whether the texture is premultiplied
 * with cogl_texture_set_premultiplied().
 *
 * <note>The results are undefined for passing an invalid @gl_handle
 * or if @width or @height don't have the correct texture
 * geometry.</note>
 *
 * Returns: (transfer full): A newly allocated #CoglTexture2D
 *
 * Since: 2.0
 */
CoglTexture2D *
cogl_texture_2d_gl_new_from_foreign (CoglContext *ctx,
                                     unsigned int gl_handle,
                                     int width,
                                     int height,
                                     CoglPixelFormat format);

COGL_END_DECLS

#endif /* _COGL_TEXTURE_2D_GL_H_ */
