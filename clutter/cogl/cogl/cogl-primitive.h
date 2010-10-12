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

#ifndef __COGL_PRIMITIVE_H__
#define __COGL_PRIMITIVE_H__

#include <cogl/cogl-vertex-buffer.h> /* for CoglVerticesMode */
#include <cogl/cogl-vertex-attribute.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-primitive
 * @short_description: Functions for creating, manipulating and drawing
 *    primitive
 *
 * FIXME
 */

typedef struct _CoglPrimitive CoglPrimitive;

/**
 * CoglV2Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v2_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y;
} CoglV2Vertex;

/**
 * CoglV3Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v3_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y, z;
} CoglV3Vertex;

/**
 * CoglV2C4Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v2c4_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y;
   guint8 r, g, b, a;
} CoglV2C4Vertex;

/**
 * CoglV3C4Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v3c4_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y, z;
   guint8 r, g, b, a;
} CoglV3C4Vertex;

/**
 * CoglV2T2Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v2t2_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y;
   float s, t;
} CoglV2T2Vertex;

/**
 * CoglV3T2Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v3t2_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y, z;
   float s, t;
} CoglV3T2Vertex;


/**
 * CoglV2T2C4Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v3t2c4_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y;
   float s, t;
   guint8 r, g, b, a;
} CoglV2T2C4Vertex;

/**
 * CoglV3T2C4Vertex:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_with_v3t2c4_attributes().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct
{
   float x, y, z;
   float s, t;
   guint8 r, g, b, a;
} CoglV3T2C4Vertex;

/**
 * cogl_primitive_new:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @Varargs: A %NULL terminated list of attributes
 *
 * Combines a set of #CoglVertexAttribute<!-- -->s with a specific draw @mode
 * and defines a vertex count so a #CoglPrimitive object can be retained and
 * drawn later with no addition information required.
 *
 * Returns: A newly allocated #CoglPrimitive object
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new (CoglVerticesMode mode,
                    int n_vertices,
                    ...);

/* XXX: how about just: cogl_primitive_new_with_attributes () ? */
CoglPrimitive *
cogl_primitive_new_with_attributes_array (CoglVerticesMode mode,
                                          int n_vertices,
                                          CoglVertexAttribute **attributes);

int
cogl_primitive_get_first_vertex (CoglPrimitive *primitive);

void
cogl_primitive_set_first_vertex (CoglPrimitive *primitive,
                                 int first_vertex);

int
cogl_primitive_get_n_vertices (CoglPrimitive *primitive);

void
cogl_primitive_set_n_vertices (CoglPrimitive *primitive,
                               int n_vertices);

CoglVerticesMode
cogl_primitive_get_mode (CoglPrimitive *primitive);

void
cogl_primitive_set_mode (CoglPrimitive *primitive,
                         CoglVerticesMode mode);

/**
 * cogl_primitive_set_attributes:
 * @primitive: A #CoglPrimitive object
 * @attributes: A %NULL terminated array of #CoglVertexAttribute
 *              pointers
 *
 * Replaces all the attributes of the given #CoglPrimitive object.
 *
 * Since: 1.6
 * Stability: Unstable
 */
void
cogl_primitive_set_attributes (CoglPrimitive *primitive,
                               CoglVertexAttribute **attributes);


void
cogl_primitive_set_indices (CoglPrimitive *primitive,
                            CoglIndices *indices);

/**
 * cogl_primitive_draw:
 * @primitive: A #CoglPrimitive object
 *
 * Draw the given @primitive with the current source material.
 *
 * Since: 1.6
 * Stability: Unstable
 */
void
cogl_primitive_draw (CoglPrimitive *primitive);

/**
 * cogl_is_primitive:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglPrimitive.
 *
 * Returns: %TRUE if the handle references a #CoglPrimitive,
 *   %FALSE otherwise
 *
 * Since: 1.6
 * Stability: Unstable
 */
gboolean
cogl_is_primitive (void *object);

G_END_DECLS

#endif /* __COGL_PRIMITIVE_H__ */

