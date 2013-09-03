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

#ifndef __COGL_INDEX_BUFFER_H__
#define __COGL_INDEX_BUFFER_H__

#include <cogl/cogl-context.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-index-buffer
 * @short_description: Functions for creating and manipulating vertex
 * indices.
 *
 * FIXME
 */

#define COGL_INDEX_BUFFER(buffer) ((CoglIndexBuffer*) buffer)

typedef struct _CoglIndexBuffer	      CoglIndexBuffer;

/**
 * cogl_index_buffer_new:
 * @context: A #CoglContext
 * @bytes: The number of bytes to allocate for vertex attribute data.
 *
 * Declares a new #CoglIndexBuffer of @size bytes to contain vertex
 * indices. Once declared, data can be set using
 * cogl_buffer_set_data() or by mapping it into the application's
 * address space using cogl_buffer_map().
 *
 * Return value: (transfer full): A newly allocated #CoglIndexBuffer
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglIndexBuffer *
cogl_index_buffer_new (CoglContext *context,
                       size_t bytes);

/**
 * cogl_is_index_buffer:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglIndexBuffer.
 *
 * Returns: %TRUE if the @object references a #CoglIndexBuffer,
 *   %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglBool
cogl_is_index_buffer (void *object);

COGL_END_DECLS

#endif /* __COGL_INDEX_BUFFER_H__ */

