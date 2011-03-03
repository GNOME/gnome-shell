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
#include <cogl/cogl-attribute.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-primitive
 * @short_description: Functions for creating, manipulating and drawing
 *    primitives
 *
 * FIXME
 */

typedef struct _CoglPrimitive CoglPrimitive;

/**
 * CoglVertexP2:
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
} CoglVertexP2;

/**
 * CoglVertexP3:
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
} CoglVertexP3;

/**
 * CoglVertexP2C4:
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
} CoglVertexP2C4;

/**
 * CoglVertexP3C4:
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
} CoglVertexP3C4;

/**
 * CoglVertexP2T2:
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
} CoglVertexP2T2;

/**
 * CoglVertexP3T2:
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
} CoglVertexP3T2;


/**
 * CoglVertexP2T2C4:
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
} CoglVertexP2T2C4;

/**
 * CoglVertexP3T2C4:
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
} CoglVertexP3T2C4;

/**
 * cogl_primitive_new:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @Varargs: A %NULL terminated list of attributes
 *
 * Combines a set of #CoglAttribute<!-- -->s with a specific draw @mode
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

CoglPrimitive *
cogl_primitive_new_with_attributes (CoglVerticesMode mode,
                                    int n_vertices,
                                    CoglAttribute **attributes,
                                    int n_attributes);

/**
 * cogl_primitive_new_p2:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP2 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position
 * attribute with a #CoglAttribute and upload your data.
 *
 * For example to draw a convex polygon you can do:
 * |[
 * CoglVertexP2 triangle[] =
 * {
 *   { 0,   300 },
 *   { 150, 0,  },
 *   { 300, 300 }
 * };
 * prim = cogl_primitive_new_p2 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                               3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2 (CoglVerticesMode mode,
                       int n_vertices,
                       const CoglVertexP2 *data);

/**
 * cogl_primitive_new_p3:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP3 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position
 * attribute with a #CoglAttribute and upload your data.
 *
 * For example to draw a convex polygon you can do:
 * |[
 * CoglVertexP3 triangle[] =
 * {
 *   { 0,   300, 0 },
 *   { 150, 0,   0 },
 *   { 300, 300, 0 }
 * };
 * prim = cogl_primitive_new_p3 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                               3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3 (CoglVerticesMode mode,
                       int n_vertices,
                       const CoglVertexP3 *data);

/**
 * cogl_primitive_new_p2c4:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP2C4 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position
 * and color attributes with #CoglAttribute<!-- -->s and upload
 * your data.
 *
 * For example to draw a convex polygon with a linear gradient you
 * can do:
 * |[
 * CoglVertexP2C4 triangle[] =
 * {
 *   { 0,   300,  0xff, 0x00, 0x00, 0xff },
 *   { 150, 0,    0x00, 0xff, 0x00, 0xff },
 *   { 300, 300,  0xff, 0x00, 0x00, 0xff }
 * };
 * prim = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2c4 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP2C4 *data);

/**
 * cogl_primitive_new_p3c4:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP3C4 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position
 * and color attributes with #CoglAttribute<!-- -->s and upload
 * your data.
 *
 * For example to draw a convex polygon with a linear gradient you
 * can do:
 * |[
 * CoglVertexP3C4 triangle[] =
 * {
 *   { 0,   300, 0,  0xff, 0x00, 0x00, 0xff },
 *   { 150, 0,   0,  0x00, 0xff, 0x00, 0xff },
 *   { 300, 300, 0,  0xff, 0x00, 0x00, 0xff }
 * };
 * prim = cogl_primitive_new_p3c4 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3c4 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP3C4 *data);

/**
 * cogl_primitive_new_p2t2:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP2T2 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position and
 * texture coordinate attributes with #CoglAttribute<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping you can
 * do:
 * |[
 * CoglVertexP2T2 triangle[] =
 * {
 *   { 0,   300,  0.0, 1.0},
 *   { 150, 0,    0.5, 0.0},
 *   { 300, 300,  1.0, 1.0}
 * };
 * prim = cogl_primitive_new_p2t2 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2t2 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP2T2 *data);

/**
 * cogl_primitive_new_p3t2:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP3T2 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position and
 * texture coordinate attributes with #CoglAttribute<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping you can
 * do:
 * |[
 * CoglVertexP3T2 triangle[] =
 * {
 *   { 0,   300, 0,  0.0, 1.0},
 *   { 150, 0,   0,  0.5, 0.0},
 *   { 300, 300, 0,  1.0, 1.0}
 * };
 * prim = cogl_primitive_new_p3t2 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3t2 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP3T2 *data);

/**
 * cogl_primitive_new_p2t2c4:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP2T2C4 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position, texture
 * coordinate and color attributes with #CoglAttribute<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping and a
 * linear gradient you can do:
 * |[
 * CoglVertexP2T2C4 triangle[] =
 * {
 *   { 0,   300,  0.0, 1.0,  0xff, 0x00, 0x00, 0xff},
 *   { 150, 0,    0.5, 0.0,  0x00, 0xff, 0x00, 0xff},
 *   { 300, 300,  1.0, 1.0,  0xff, 0x00, 0x00, 0xff}
 * };
 * prim = cogl_primitive_new_p2t2c4 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                                   3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2t2c4 (CoglVerticesMode mode,
                           int n_vertices,
                           const CoglVertexP2T2C4 *data);

/**
 * cogl_primitive_new_p3t2c4:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @data: An array of #CoglVertexP3T2C4 vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #CoglAttributeBuffer storage, describe the position, texture
 * coordinate and color attributes with #CoglAttribute<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping and a
 * linear gradient you can do:
 * |[
 * CoglVertexP3T2C4 triangle[] =
 * {
 *   { 0,   300, 0,  0.0, 1.0,  0xff, 0x00, 0x00, 0xff},
 *   { 150, 0,   0,  0.5, 0.0,  0x00, 0xff, 0x00, 0xff},
 *   { 300, 300, 0,  1.0, 1.0,  0xff, 0x00, 0x00, 0xff}
 * };
 * prim = cogl_primitive_new_p3t2c4 (COGL_VERTICES_MODE_TRIANGLE_FAN,
 *                                   3, triangle);
 * cogl_primitive_draw (prim);
 * ]|
 *
 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: A newly allocated #CoglPrimitive with a reference of
 * 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3t2c4 (CoglVerticesMode mode,
                           int n_vertices,
                           const CoglVertexP3T2C4 *data);
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
 * @attributes: A %NULL terminated array of #CoglAttribute
 *              pointers
 *
 * Replaces all the attributes of the given #CoglPrimitive object.
 *
 * Since: 1.6
 * Stability: Unstable
 */
void
cogl_primitive_set_attributes (CoglPrimitive *primitive,
                               CoglAttribute **attributes,
                               int n_attributes);


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

