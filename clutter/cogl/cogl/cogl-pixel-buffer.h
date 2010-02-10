/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PIXEL_BUFFER_H__
#define __COGL_PIXEL_BUFFER_H__

#include <glib.h>
#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * cogl_pixel_buffer_new:
 * @size: size of the buffer in bytes
 *
 * Creates a new buffer to store pixel data. You can create a new texture from
 * this buffer using cogl_texture_new_from_buffer().
 *
 * Return value: a #CoglHandle representing the newly created buffer or
 *               %COGL_INVALID_HANDLE on failure
 *
 * Since: 1.2
 * Stability: Unstable
 */
CoglHandle
cogl_pixel_buffer_new (unsigned int size);

/**
 * cogl_pixel_buffer_new_for_size:
 * @width: width of the pixel buffer in pixels
 * @height: height of the pixel buffer in pixels
 * @format: the format of the pixels the buffer will store
 * @stride: if not %NULL the function will return the stride of the buffer
 *          in bytes
 *
 * Creates a new buffer to store pixel data.
 *
 * <note>COGL will try its best to provide a hardware buffer you can map,
 * write into and effectively do a zero copy upload when creating a texture
 * from it with cogl_texture_new_from_buffer(). For various reasons, such
 * buffers are likely to have a stride larger than width * bytes_per_pixel. The
 * user must take the stride into account when writing into it.</note>
 *
 * Return value: a #CoglHandle representing the newly created buffer or
 *               %COGL_INVALID_HANDLE on failure
 *
 * Since: 1.2
 * Stability: Unstable
 */
CoglHandle
cogl_pixel_buffer_new_for_size (unsigned int     width,
                                unsigned int     height,
                                CoglPixelFormat  format,
                                unsigned int    *stride);

/**
 * cogl_is_pixel_buffer:
 * @handle: a #CoglHandle to test
 *
 * Checks whether @handle is a pixel buffer.
 *
 * Return value: %TRUE if the @handle is a pixel buffer, and %FALSE
 *   otherwise
 *
 * Since: 1.2
 * Stability: Unstable
 */
gboolean
cogl_is_pixel_buffer (CoglHandle handle);

/*
 * cogl_pixel_buffer_set_region:
 * @buffer: the #CoglHandle of a pixel buffer
 * @data: pixel data to upload to @buffer
 * @src_width: width in pixels of the region to update
 * @src_height: height in pixels of the region to update
 * @src_rowstride: row stride in bytes of the source buffer
 * @dst_x: upper left destination horizontal coordinate
 * @dst_y: upper left destination vertical coordinate
 *
 * Uploads new data into a pixel buffer. The source data pointed by @data can
 * have a different stride than @buffer in which case the function will do the
 * right thing for you. For performance reasons, it is recommended for the
 * source data to have the same stride than @buffer.
 *
 * Return value: %TRUE if the upload succeeded, %FALSE otherwise
 *
 * Since: 1.2
 * Stability: Unstable
 */
#if 0
gboolean
cogl_pixel_buffer_set_region (CoglHandle    buffer,
                              guint8       *data,
                              unsigned int  src_width,
                              unsigned int  src_height,
                              unsigned int  src_rowstride,
                              unsigned int  dst_x,
                              unsigned int  dst_y);
#endif

/* the functions above are experimental, the actual symbols are suffixed by
 * _EXP so we can ensure ABI compatibility and leave the cogl_buffer namespace
 * free for future use. A bunch of defines translates the symbols documented
 * above into the real symbols */

CoglHandle
cogl_pixel_buffer_new_EXP (unsigned int size);

CoglHandle
cogl_pixel_buffer_new_for_size_EXP (unsigned int    width,
                                    unsigned int    height,
                                    CoglPixelFormat format,
                                    unsigned int   *stride);
gboolean
cogl_is_pixel_buffer_EXP (CoglHandle handle);

#if 0
gboolean
cogl_pixel_buffer_set_region_EXP (CoglHandle   buffer,
                                  guint8      *data,
                                  unsigned int src_width,
                                  unsigned int src_height,
                                  unsigned int src_rowstride,
                                  unsigned int dst_x,
                                  unsigned int dst_y);
#endif

#define cogl_pixel_buffer_new cogl_pixel_buffer_new_EXP
#define cogl_pixel_buffer_new_for_size cogl_pixel_buffer_new_for_size_EXP
#define cogl_is_pixel_buffer cogl_is_pixel_buffer_EXP
#if 0
#define cogl_pixel_buffer_set_region cogl_pixel_buffer_set_region_EXP
#endif

G_END_DECLS

#endif /* __COGL_PIXEL_BUFFER_H__ */
