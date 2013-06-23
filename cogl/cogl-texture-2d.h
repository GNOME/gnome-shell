/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011,2013 Intel Corporation.
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
 * @internal_format: The format of the texture
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
                               int height,
                               CoglPixelFormat internal_format);

/**
 * cogl_texture_2d_new_from_file:
 * @ctx: A #CoglContext
 * @filename: the file to load
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 *    texture. If %COGL_PIXEL_FORMAT_ANY is given then a premultiplied
 *    format similar to the format of the source data will be used. The
 *    default blending equations of Cogl expect premultiplied color data;
 *    the main use of passing a non-premultiplied format here is if you
 *    have non-premultiplied source data and are going to adjust the blend
 *    mode (see cogl_material_set_blend()) or use the data for something
 *    other than straight blending.
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
                               CoglPixelFormat internal_format,
                               CoglError **error);

/**
 * cogl_texture_2d_new_from_data:
 * @ctx: A #CoglContext
 * @width: width of texture in pixels
 * @height: height of texture in pixels
 * @format: the #CoglPixelFormat the buffer is stored in in RAM
 * @internal_format: the #CoglPixelFormat that will be used for storing
 *    the buffer on the GPU. If %COGL_PIXEL_FORMAT_ANY is given then a
 *    premultiplied format similar to the format of the source data will
 *    be used. The default blending equations of Cogl expect premultiplied
 *    color data; the main use of passing a non-premultiplied format here
 *    is if you have non-premultiplied source data and are going to adjust
 *    the blend mode (see cogl_pipeline_set_blend()) or use the data for
 *    something other than straight blending.
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
                               CoglPixelFormat internal_format,
                               int rowstride,
                               const uint8_t *data,
                               CoglError **error);

/**
 * cogl_texture_2d_new_from_bitmap:
 * @bitmap: A #CoglBitmap
 * @internal_format: the #CoglPixelFormat that will be used for storing
 *    the buffer on the GPU. If %COGL_PIXEL_FORMAT_ANY is given then a
 *    premultiplied format similar to the format of the source data will
 *    be used. The default blending equations of Cogl expect premultiplied
 *    color data; the main use of passing a non-premultiplied format here
 *    is if you have non-premultiplied source data and are going to adjust
 *    the blend mode (see cogl_pipeline_set_blend()) or use the data for
 *    something other than straight blending.
 * @error: A #CoglError for exceptions
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
 * Returns: (transfer full): A newly allocated #CoglTexture2D, or if
 *          the size is not supported (because it is too large or a
 *          non-power-of-two size that the hardware doesn't support)
 *          it will return %NULL and set @error.
 *
 * Since: 2.0
 * Stability: unstable
 */
CoglTexture2D *
cogl_texture_2d_new_from_bitmap (CoglBitmap *bitmap,
                                 CoglPixelFormat internal_format,
                                 CoglError **error);

COGL_END_DECLS

#endif /* __COGL_TEXTURE_2D_H */
