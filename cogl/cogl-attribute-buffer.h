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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ATTRIBUTE_BUFFER_H__
#define __COGL_ATTRIBUTE_BUFFER_H__

/* We forward declare the CoglAttributeBuffer type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglAttributeBuffer CoglAttributeBuffer;

#include <cogl/cogl-context.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-attribute-buffer
 * @short_description: Functions for creating and manipulating attribute
 *   buffers
 *
 * FIXME
 */

#define COGL_ATTRIBUTE_BUFFER(buffer) ((CoglAttributeBuffer *)(buffer))

/**
 * cogl_attribute_buffer_new:
 * @context: A #CoglContext
 * @bytes: The number of bytes to allocate for vertex attribute data.
 * @data: An optional pointer to vertex data to upload immediately.
 *
 * Declares a new #CoglAttributeBuffer of @size bytes to contain arrays of vertex
 * attribute data. Once declared, data can be set using cogl_buffer_set_data()
 * or by mapping it into the application's address space using cogl_buffer_map().
 *
 * If @data isn't %NULL then @size bytes will be read from @data and
 * immediately copied into the new buffer.
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglAttributeBuffer *
cogl_attribute_buffer_new (CoglContext *context,
                           gsize bytes,
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
gboolean
cogl_is_attribute_buffer (void *object);

G_END_DECLS

#endif /* __COGL_ATTRIBUTE_BUFFER_H__ */

