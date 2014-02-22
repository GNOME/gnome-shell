/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
