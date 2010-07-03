/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *
 * Authors:
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PIXEL_ARRAY_H__
#define __COGL_PIXEL_ARRAY_H__

#include <glib.h>
#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/* All of the cogl-pixel-array API is currently experimental so we
 * suffix the actual symbols with _EXP so if somone is monitoring for
 * ABI changes it will hopefully be clearer to them what's going on if
 * any of the symbols dissapear at a later date.
 */

#define cogl_pixel_array_new cogl_pixel_array_new_EXP
#define cogl_pixel_array_new_with_size cogl_pixel_array_new_with_size_EXP
#define cogl_is_pixel_array cogl_is_pixel_array_EXP
#if 0
#define cogl_pixel_array_set_region cogl_pixel_array_set_region_EXP
#endif

typedef struct _CoglPixelArray CoglPixelArray;

/**
 * cogl_pixel_array_new_with_size:
 * @width: width of the pixel array in pixels
 * @height: height of the pixel array in pixels
 * @format: the format of the pixels the array will store
 * @stride: if not %NULL the function will return the stride of the array
 *          in bytes
 *
 * Creates a new array to store pixel data.
 *
 * <note>COGL will try its best to provide a hardware array you can map,
 * write into and effectively do a zero copy upload when creating a texture
 * from it with cogl_texture_new_from_buffer(). For various reasons, such
 * arrays are likely to have a stride larger than width * bytes_per_pixel. The
 * user must take the stride into account when writing into it.</note>
 *
 * Return value: a #CoglPixelArray representing the newly created array or
 *               %NULL on failure
 *
 * Since: 1.2
 * Stability: Unstable
 */
CoglPixelArray *
cogl_pixel_array_new_with_size (unsigned int     width,
                                unsigned int     height,
                                CoglPixelFormat  format,
                                unsigned int    *stride);

/**
 * cogl_is_pixel_array:
 * @object: a #CoglObject to test
 *
 * Checks whether @handle is a pixel array.
 *
 * Return value: %TRUE if the @handle is a pixel array, and %FALSE
 *   otherwise
 *
 * Since: 1.2
 * Stability: Unstable
 */
gboolean
cogl_is_pixel_array (void *object);

#if 0
/*
 * cogl_pixel_array_set_region:
 * @array: the #CoglHandle of a pixel array
 * @data: pixel data to upload to @array
 * @src_width: width in pixels of the region to update
 * @src_height: height in pixels of the region to update
 * @src_rowstride: row stride in bytes of the source array
 * @dst_x: upper left destination horizontal coordinate
 * @dst_y: upper left destination vertical coordinate
 *
 * Uploads new data into a pixel array. The source data pointed by @data can
 * have a different stride than @array in which case the function will do the
 * right thing for you. For performance reasons, it is recommended for the
 * source data to have the same stride than @array.
 *
 * Return value: %TRUE if the upload succeeded, %FALSE otherwise
 *
 * Since: 1.2
 * Stability: Unstable
 */
gboolean
cogl_pixel_array_set_region (CoglHandle    array,
                             guint8       *data,
                             unsigned int  src_width,
                             unsigned int  src_height,
                             unsigned int  src_rowstride,
                             unsigned int  dst_x,
                             unsigned int  dst_y);
#endif

G_END_DECLS

#endif /* __COGL_PIXEL_ARRAY_H__ */
