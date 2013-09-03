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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PIXEL_BUFFER_H__
#define __COGL_PIXEL_BUFFER_H__

/* XXX: We forward declare CoglPixelBuffer here to allow for circular
 * dependencies between some headers */
typedef struct _CoglPixelBuffer CoglPixelBuffer;

#include <cogl/cogl-types.h>
#include <cogl/cogl-context.h>

COGL_BEGIN_DECLS

#define COGL_PIXEL_BUFFER(buffer) ((CoglPixelBuffer *)(buffer))

/**
 * cogl_pixel_buffer_new:
 * @context: A #CoglContext
 * @size: The number of bytes to allocate for the pixel data.
 * @data: An optional pointer to vertex data to upload immediately
 *
 * Declares a new #CoglPixelBuffer of @size bytes to contain arrays of
 * pixels. Once declared, data can be set using cogl_buffer_set_data()
 * or by mapping it into the application's address space using
 * cogl_buffer_map().
 *
 * If @data isn't %NULL then @size bytes will be read from @data and
 * immediately copied into the new buffer.
 *
 * Return value: (transfer full): a newly allocated #CoglPixelBuffer
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglPixelBuffer *
cogl_pixel_buffer_new (CoglContext *context,
                       size_t size,
                       const void *data);

/**
 * cogl_is_pixel_buffer:
 * @object: a #CoglObject to test
 *
 * Checks whether @object is a pixel buffer.
 *
 * Return value: %TRUE if the @object is a pixel buffer, and %FALSE
 *   otherwise
 *
 * Since: 1.2
 * Stability: Unstable
 */
CoglBool
cogl_is_pixel_buffer (void *object);

#if 0
/*
 * cogl_pixel_buffer_set_region:
 * @buffer: A #CoglPixelBuffer object
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
CoglBool
cogl_pixel_buffer_set_region (CoglPixelBuffer *buffer,
                              uint8_t *data,
                              unsigned int src_width,
                              unsigned int src_height,
                              unsigned int src_rowstride,
                              unsigned int dst_x,
                              unsigned int dst_y);
#endif

COGL_END_DECLS

#endif /* __COGL_PIXEL_BUFFER_H__ */
