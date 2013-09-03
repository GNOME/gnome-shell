/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ATTRIBUTE_BUFFER_H__
#define __COGL_ATTRIBUTE_BUFFER_H__

/* We forward declare the CoglAttributeBuffer type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglAttributeBuffer CoglAttributeBuffer;

#include <cogl/cogl-context.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-attribute-buffer
 * @short_description: Functions for creating and manipulating attribute
 *   buffers
 *
 * FIXME
 */

#define COGL_ATTRIBUTE_BUFFER(buffer) ((CoglAttributeBuffer *)(buffer))

/**
 * cogl_attribute_buffer_new_with_size:
 * @context: A #CoglContext
 * @bytes: The number of bytes to allocate for vertex attribute data.
 *
 * Describes a new #CoglAttributeBuffer of @size bytes to contain
 * arrays of vertex attribute data. Afterwards data can be set using
 * cogl_buffer_set_data() or by mapping it into the application's
 * address space using cogl_buffer_map().
 *
 * The underlying storage of this buffer isn't allocated by this
 * function so that you have an opportunity to use the
 * cogl_buffer_set_update_hint() and cogl_buffer_set_usage_hint()
 * functions which may influence how the storage is allocated. The
 * storage will be allocated once you upload data to the buffer.
 *
 * Note: You can assume this function always succeeds and won't return
 * %NULL
 *
 * Return value: (transfer full): A newly allocated #CoglAttributeBuffer. Never %NULL.
 *
 * Stability: Unstable
 */
CoglAttributeBuffer *
cogl_attribute_buffer_new_with_size (CoglContext *context,
                                     size_t bytes);

/**
 * cogl_attribute_buffer_new:
 * @context: A #CoglContext
 * @bytes: The number of bytes to allocate for vertex attribute data.
 * @data: An optional pointer to vertex data to upload immediately.
 *
 * Describes a new #CoglAttributeBuffer of @size bytes to contain
 * arrays of vertex attribute data and also uploads @size bytes read
 * from @data to the new buffer.
 *
 * You should never pass a %NULL data pointer.
 *
 * <note>This function does not report out-of-memory errors back to
 * the caller by returning %NULL and so you can assume this function
 * always succeeds.</note>
 *
 * <note>In the unlikely case that there is an out of memory problem
 * then Cogl will abort the application with a message. If your
 * application needs to gracefully handle out-of-memory errors then
 * you can use cogl_attribute_buffer_new_with_size() and then
 * explicitly catch errors with cogl_buffer_set_data() or
 * cogl_buffer_map().</note>
 *
 * Return value: (transfer full): A newly allocated #CoglAttributeBuffer (never %NULL)
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglAttributeBuffer *
cogl_attribute_buffer_new (CoglContext *context,
                           size_t bytes,
                           const void *data);

/**
 * cogl_is_attribute_buffer:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglAttributeBuffer.
 *
 * Returns: %TRUE if @object references a #CoglAttributeBuffer,
 *   %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglBool
cogl_is_attribute_buffer (void *object);

COGL_END_DECLS

#endif /* __COGL_ATTRIBUTE_BUFFER_H__ */

