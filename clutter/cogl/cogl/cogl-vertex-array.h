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

#ifndef __COGL_VERTEX_ARRAY_H__
#define __COGL_VERTEX_ARRAY_H__

G_BEGIN_DECLS

/**
 * SECTION:cogl-vertex-array
 * @short_description: Fuctions for creating and manipulating vertex arrays
 *
 * FIXME
 */

typedef struct _CoglVertexArray	      CoglVertexArray;

/**
 * cogl_vertex_array_new:
 * @size: The number of bytes to allocate for vertex attribute data.
 *
 * Declares a new #CoglVertexArray of @size bytes to contain arrays of vertex
 * attribute data. Once declared, data can be set using cogl_buffer_set_data()
 * or by mapping it into the application's address space using cogl_buffer_map().
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglVertexArray *
cogl_vertex_array_new (gsize bytes);

/**
 * cogl_is_vertex_array:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglVertexArray.
 *
 * Returns: %TRUE if the handle references a #CoglVertexArray,
 *   %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
gboolean
cogl_is_vertex_array (void *object);

G_END_DECLS

#endif /* __COGL_VERTEX_ARRAY_H__ */

