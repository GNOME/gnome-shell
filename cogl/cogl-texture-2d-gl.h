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
 * cogl_texture_2d_new_from_foreign:
 * @ctx: A #CoglContext
 * @gl_handle: A GL handle for a GL_TEXTURE_2D texture object
 * @width: Width of the foreign GL texture
 * @height: Height of the foreign GL texture
 * @format: The format of the texture
 * @error: A #CoglError for exceptions
 *
 * Wraps an existing GL_TEXTURE_2D texture object as a #CoglTexture2D.
 * This can be used for integrating Cogl with software using OpenGL
 * directly.
 *
 * <note>The results are undefined for passing an invalid @gl_handle
 * or if @width or @height don't have the correct texture
 * geometry.</note>
 *
 * Returns: A newly allocated #CoglTexture2D, or if Cogl could not
 *          validate the @gl_handle in some way (perhaps because of
 *          an unsupported format) it will return %NULL and set
 *          @error.
 *
 * Since: 2.0
 */
CoglTexture2D *
cogl_texture_2d_new_from_foreign (CoglContext *ctx,
                                  unsigned int gl_handle,
                                  int width,
                                  int height,
                                  CoglPixelFormat format,
                                  CoglError **error);

COGL_END_DECLS

#endif /* _COGL_TEXTURE_2D_GL_H_ */
