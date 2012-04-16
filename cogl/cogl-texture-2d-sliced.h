/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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

#ifndef __COGL_TEXURE_2D_SLICED_H
#define __COGL_TEXURE_2D_SLICED_H

#include "cogl-context.h"
#include "cogl-types.h"

#include <glib.h>

/**
 * SECTION:cogl-texture-2d-sliced
 * @short_description: Functions for creating and manipulating 2D meta
 *                     textures that may internally be comprised of
 *                     multiple 2D textures with power-of-two sizes.
 *
 * These functions allow high-level meta textures (See the
 * #CoglMetaTexture interface) to be allocated that may internally be
 * comprised of multiple 2D texture "slices" with power-of-two sizes.
 *
 * This API can be useful when working with GPUs that don't have
 * native support for non-power-of-two textures or if you want to load
 * a texture that is larger than the GPUs maximum texture size limits.
 *
 * The algorithm for slicing works by first trying to map a virtual
 * size to the next larger power-of-two size and then seeing how many
 * wasted pixels that would result in. For example if you have a
 * virtual texture that's 259 texels wide, the next pot size = 512 and
 * the amount of waste would be 253 texels. If the amount of waste is
 * above a max-waste threshold then we would next slice that texture
 * into one that's 256 texels and then looking at how many more texels
 * remain unallocated after that we choose the next power-of-two size.
 * For the example of a 259 texel image that would mean having a 256
 * texel wide texture, leaving 3 texels unallocated so we'd then
 * create a 4 texel wide texture - now there is only one texel of
 * waste. The algorithm continues to slice the right most textures
 * until the amount of waste is less than or equal to a specfied
 * max-waste threshold. The same logic for slicing from left to right
 * is also applied from top to bottom.
 */

typedef struct _CoglTexture2DSliced CoglTexture2DSliced;
#define COGL_TEXTURE_2D_SLICED(X) ((CoglTexture2DSliced *)X)

/**
 * cogl_texture_2d_sliced_new_with_size:
 * @ctx: A #CoglContext
 * @width: The virtual width of your sliced texture.
 * @height: The virtual height of your sliced texture.
 * @max_waste: The threshold of how wide a strip of wasted texels
 *             are allowed in the non-power-of-two textures before
 *             they must be sliced to reduce the amount of waste.
 * @internal_format: The format of the texture
 * @error: A #GError for exceptions.
 *
 * Creates a #CoglTexture2DSliced that may internally be comprised of
 * 1 or more #CoglTexture2D textures with power-of-two sizes.
 * @max_waste is used as a threshold for recursively slicing the
 * right-most or bottom-most slices into smaller power-of-two sizes
 * until the wasted padding at the bottom and right of the
 * power-of-two textures is less than specified.
 *
 * Returns: A newly allocated #CoglTexture2DSliced or if there was
 *          an error allocating any of the internal slices %NULL is
 *          returned and @error is updated.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglTexture2DSliced *
cogl_texture_2d_sliced_new_with_size (CoglContext *ctx,
                                      unsigned int width,
                                      unsigned int height,
                                      int max_waste,
                                      CoglPixelFormat internal_format,
                                      GError **error);

/**
 * cogl_is_texture_2d_sliced:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglTexture2dSliced.
 *
 * Return value: %TRUE if the object references a #CoglTexture2dSliced
 *   and %FALSE otherwise.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_texture_2d_sliced (void *object);

#endif /* __COGL_TEXURE_2D_SLICED_H */
