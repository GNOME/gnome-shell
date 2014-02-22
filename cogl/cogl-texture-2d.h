/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011,2013 Intel Corporation.
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

#ifndef __COGL_TEXTURE_2D_H
#define __COGL_TEXTURE_2D_H

#include "cogl-context.h"
#include "cogl-bitmap.h"

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-texture-2d
 * @short_description: Functions for creating and manipulating 2D textures
 *
 * These functions allow low-level 2D textures to be allocated. These
 * differ from sliced textures for example which may internally be
 * made up of multiple 2D textures, or atlas textures where Cogl must
 * internally modify user texture coordinates before they can be used
 * by the GPU.
 *
 * You should be aware that many GPUs only support power of two sizes
 * for #CoglTexture2D textures. You can check support for non power of
 * two textures by checking for the %COGL_FEATURE_ID_TEXTURE_NPOT feature
 * via cogl_has_feature().
 */

typedef struct _CoglTexture2D CoglTexture2D;
#define COGL_TEXTURE_2D(X) ((CoglTexture2D *)X)

/**
 * cogl_is_texture_2d:
 * @object: A #CoglObject
 *
 * Gets whether the given object references an existing #CoglTexture2D
 * object.
 *
 * Return value: %TRUE if the object references a #CoglTexture2D,
 *   %FALSE otherwise
 */
CoglBool
cogl_is_texture_2d (void *object);

/**
 * cogl_texture_2d_new_with_size:
 * @ctx: A #CoglContext
 * @width: Width of the texture to allocate
 * @height: Height of the texture to allocate
 *
 * Creates a low-level #CoglTexture2D texture with a given @width and
 * @height that your GPU can texture from directly.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cogl_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let Cogl
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cogl_texture_set_components() and
 * cogl_texture_set_premultiplied().
 *
 * <note>Many GPUs only support power of two sizes for #CoglTexture2D
 * textures. You can check support for non power of two textures by
 * checking for the %COGL_FEATURE_ID_TEXTURE_NPOT feature via
 * cogl_has_feature().</note>
 *
 * Returns: (transfer full): A new #CoglTexture2D object with no storage yet allocated.
 *
 * Since: 2.0
 */
CoglTexture2D *
cogl_texture_2d_new_with_size (CoglContext *ctx,
                               int width,
                               int height);

/**
 * cogl_texture_2d_new_from_file:
 * @ctx: A #CoglContext
 * @filename: the file to load
 * @error: A #CoglError to catch exceptional errors or %NULL
 *
 * Creates a low-level #CoglTexture2D texture from an image file.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cogl_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let Cogl
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cogl_texture_set_components() and
 * cogl_texture_set_premultiplied().
 *
 * <note>Many GPUs only support power of two sizes for #CoglTexture2D
 * textures. You can check support for non power of two textures by
 * checking for the %COGL_FEATURE_ID_TEXTURE_NPOT feature via
 * cogl_has_feature().</note>
 *
 * Return value: (transfer full): A newly created #CoglTexture2D or %NULL on failure
 *               and @error will be updated.
 *
 * Since: 1.16
 */
CoglTexture2D *
cogl_texture_2d_new_from_file (CoglContext *ctx,
                               const char *filename,
                               CoglError **error);

/**
 * cogl_texture_2d_new_from_data:
 * @ctx: A #CoglContext
 * @width: width of texture in pixels
 * @height: height of texture in pixels
 * @format: the #CoglPixelFormat the buffer is stored in in RAM
 * @rowstride: the memory offset in bytes between the starts of
 *    scanlines in @data. A value of 0 will make Cogl automatically
 *    calculate @rowstride from @width and @format.
 * @data: pointer the memory region where the source buffer resides
 * @error: A #CoglError for exceptions
 *
 * Creates a low-level #CoglTexture2D texture based on data residing
 * in memory.
 *
 * <note>This api will always immediately allocate GPU memory for the
 * texture and upload the given data so that the @data pointer does
 * not need to remain valid once this function returns. This means it
 * is not possible to configure the texture before it is allocated. If
 * you do need to configure the texture before allocation (to specify
 * constraints on the internal format for example) then you can
 * instead create a #CoglBitmap for your data and use
 * cogl_texture_2d_new_from_bitmap() or use
 * cogl_texture_2d_new_with_size() and then upload data using
 * cogl_texture_set_data()</note>
 *
 * <note>Many GPUs only support power of two sizes for #CoglTexture2D
 * textures. You can check support for non power of two textures by
 * checking for the %COGL_FEATURE_ID_TEXTURE_NPOT feature via
 * cogl_has_feature().</note>
 *
 * Returns: (transfer full): A newly allocated #CoglTexture2D, or if
 *          the size is not supported (because it is too large or a
 *          non-power-of-two size that the hardware doesn't support)
 *          it will return %NULL and set @error.
 *
 * Since: 2.0
 */
CoglTexture2D *
cogl_texture_2d_new_from_data (CoglContext *ctx,
                               int width,
                               int height,
                               CoglPixelFormat format,
                               int rowstride,
                               const uint8_t *data,
                               CoglError **error);

/**
 * cogl_texture_2d_new_from_bitmap:
 * @bitmap: A #CoglBitmap
 *
 * Creates a low-level #CoglTexture2D texture based on data residing
 * in a #CoglBitmap.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cogl_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let Cogl
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cogl_texture_set_components() and
 * cogl_texture_set_premultiplied().
 *
 * <note>Many GPUs only support power of two sizes for #CoglTexture2D
 * textures. You can check support for non power of two textures by
 * checking for the %COGL_FEATURE_ID_TEXTURE_NPOT feature via
 * cogl_has_feature().</note>
 *
 * Returns: (transfer full): A newly allocated #CoglTexture2D
 *
 * Since: 2.0
 * Stability: unstable
 */
CoglTexture2D *
cogl_texture_2d_new_from_bitmap (CoglBitmap *bitmap);

COGL_END_DECLS

#endif /* __COGL_TEXTURE_2D_H */
