/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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

#ifndef __COGL_TEXURE_RECTANGLE_H
#define __COGL_TEXURE_RECTANGLE_H

#include "cogl-context.h"

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-texture-rectangle
 * @short_description: Functions for creating and manipulating rectangle
 *                     textures for use with non-normalized coordinates.
 *
 * These functions allow low-level "rectangle" textures to be allocated.
 * These textures are never constrained to power-of-two sizes but they
 * also don't support having a mipmap and can only be wrapped with
 * %COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE.
 *
 * The most notable difference between rectangle textures and 2D
 * textures is that rectangle textures are sampled using un-normalized
 * texture coordinates, so instead of using coordinates (0,0) and
 * (1,1) to map to the top-left and bottom right corners of the
 * texture you would instead use (0,0) and (width,height).
 *
 * The use of non-normalized coordinates can be particularly
 * convenient when writing glsl shaders that use a texture as a lookup
 * table since you don't need to upload separate uniforms to map
 * normalized coordinates to texels.
 *
 * If you want to sample from a rectangle texture from GLSL you should
 * use the sampler2DRect sampler type.
 *
 * Applications wanting to use #CoglTextureRectangle should first check
 * for the %COGL_FEATURE_ID_TEXTURE_RECTANGLE feature using
 * cogl_has_feature().
 */

typedef struct _CoglTextureRectangle CoglTextureRectangle;
#define COGL_TEXTURE_RECTANGLE(X) ((CoglTextureRectangle *)X)

/**
 * cogl_is_texture_rectangle:
 * @object: A #CoglObject
 *
 * Gets whether the given object references an existing
 * #CoglTextureRectangle object.
 *
 * Return value: %TRUE if the object references a
 *               #CoglTextureRectangle, %FALSE otherwise.
 */
CoglBool
cogl_is_texture_rectangle (void *object);

/**
 * cogl_texture_rectangle_new_with_size:
 * @ctx: A #CoglContext pointer
 * @width: The texture width to allocate
 * @height: The texture height to allocate
 *
 * Creates a new #CoglTextureRectangle texture with a given @width,
 * and @height. This texture is a low-level texture that the GPU can
 * sample from directly unlike high-level textures such as
 * #CoglTexture2DSliced and #CoglAtlasTexture.
 *
 * <note>Unlike for #CoglTexture2D textures, coordinates for
 * #CoglTextureRectangle textures should not be normalized. So instead
 * of using the coordinate (1, 1) to sample the bottom right corner of
 * a rectangle texture you would use (@width, @height) where @width
 * and @height are the width and height of the texture.</note>
 *
 * <note>If you want to sample from a rectangle texture from GLSL you
 * should use the sampler2DRect sampler type.</note>
 *
 * <note>Applications wanting to use #CoglTextureRectangle should
 * first check for the %COGL_FEATURE_ID_TEXTURE_RECTANGLE feature
 * using cogl_has_feature().</note>
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cogl_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let Cogl
 * automatically allocate storage lazily when it may know more about
 * how the texture is going to be used and can optimize how it is
 * allocated.
 *
 * Returns value: (transfer full): A pointer to a new #CoglTextureRectangle
 *          object with no storage allocated yet.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglTextureRectangle *
cogl_texture_rectangle_new_with_size (CoglContext *ctx,
                                      int width,
                                      int height);

/**
 * cogl_texture_rectangle_new_from_bitmap:
 * @bitmap: A #CoglBitmap
 *
 * Allocates a new #CoglTextureRectangle texture which will be
 * initialized with the pixel data from @bitmap. This texture is a
 * low-level texture that the GPU can sample from directly unlike
 * high-level textures such as #CoglTexture2DSliced and
 * #CoglAtlasTexture.
 *
 * <note>Unlike for #CoglTexture2D textures, coordinates for
 * #CoglTextureRectangle textures should not be normalized. So instead
 * of using the coordinate (1, 1) to sample the bottom right corner of
 * a rectangle texture you would use (@width, @height) where @width
 * and @height are the width and height of the texture.</note>
 *
 * <note>If you want to sample from a rectangle texture from GLSL you
 * should use the sampler2DRect sampler type.</note>
 *
 * <note>Applications wanting to use #CoglTextureRectangle should
 * first check for the %COGL_FEATURE_ID_TEXTURE_RECTANGLE feature
 * using cogl_has_feature().</note>
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cogl_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let Cogl
 * automatically allocate storage lazily when it may know more about
 * how the texture is going to be used and can optimize how it is
 * allocated.
 *
 * Return value: (transfer full): A pointer to a new
 *               #CoglTextureRectangle texture.
 * Since: 2.0
 * Stability: unstable
 */
CoglTextureRectangle *
cogl_texture_rectangle_new_from_bitmap (CoglBitmap *bitmap);

/**
 * cogl_texture_rectangle_new_from_foreign:
 * @ctx: A #CoglContext
 * @gl_handle: A GL handle for a GL_TEXTURE_RECTANGLE texture object
 * @width: Width of the foreign GL texture
 * @height: Height of the foreign GL texture
 * @format: The format of the texture
 *
 * Wraps an existing GL_TEXTURE_RECTANGLE texture object as a
 * #CoglTextureRectangle.  This can be used for integrating Cogl with
 * software using OpenGL directly.
 *
 * <note>Unlike for #CoglTexture2D textures, coordinates for
 * #CoglTextureRectangle textures should not be normalized. So instead
 * of using the coordinate (1, 1) to sample the bottom right corner of
 * a rectangle texture you would use (@width, @height) where @width
 * and @height are the width and height of the texture.</note>
 *
 * <note>The results are undefined for passing an invalid @gl_handle
 * or if @width or @height don't have the correct texture
 * geometry.</note>
 *
 * <note>If you want to sample from a rectangle texture from GLSL you
 * should use the sampler2DRect sampler type.</note>
 *
 * <note>Applications wanting to use #CoglTextureRectangle should
 * first check for the %COGL_FEATURE_ID_TEXTURE_RECTANGLE feature
 * using cogl_has_feature().</note>
 *
 * The texture is still configurable until it has been allocated so
 * for example you can declare whether the texture is premultiplied
 * with cogl_texture_set_premultiplied().
 *
 * Return value: (transfer full): A new #CoglTextureRectangle texture
 */
CoglTextureRectangle *
cogl_texture_rectangle_new_from_foreign (CoglContext *ctx,
                                         unsigned int gl_handle,
                                         int width,
                                         int height,
                                         CoglPixelFormat format);

COGL_END_DECLS

#endif /* __COGL_TEXURE_RECTANGLE_H */
