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

#ifndef __COGL_PRIMITIVE_H__
#define __COGL_PRIMITIVE_H__

/* We forward declare the CoglPrimitive type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglPrimitive CoglPrimitive;

#include <cogl/cogl-vertex-buffer.h> /* for CoglVerticesMode */
#include <cogl/cogl-attribute.h>
#include <cogl/cogl-framebuffer.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-primitive
 * @short_description: Functions for creating, manipulating and drawing
 *    primitives
 *
 * FIXME
 */

/**
 * CoglVertexP2:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_p2().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
   float x, y;
} CoglVertexP2;

/**
 * CoglVertexP3:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_p3().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
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
 * cogl_primitive_new_p2c4().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
   float x, y;
   uint8_t r, g, b, a;
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
 * cogl_primitive_new_p3c4().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
   float x, y, z;
   uint8_t r, g, b, a;
} CoglVertexP3C4;

/**
 * CoglVertexP2T2:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 *
 * A convenience vertex definition that can be used with
 * cogl_primitive_new_p2t2().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
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
 * cogl_primitive_new_p3t2().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
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
 * cogl_primitive_new_p3t2c4().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
   float x, y;
   float s, t;
   uint8_t r, g, b, a;
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
 * cogl_primitive_new_p3t2c4().
 *
 * Since: 1.6
 * Stability: Unstable
 */
typedef struct {
   float x, y, z;
   float s, t;
   uint8_t r, g, b, a;
} CoglVertexP3T2C4;

/**
 * cogl_primitive_new:
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @...: A %NULL terminated list of attributes
 *
 * Combines a set of #CoglAttribute<!-- -->s with a specific draw @mode
 * and defines a vertex count so a #CoglPrimitive object can be retained and
 * drawn later with no addition information required.
 *
 * The value passed as @n_vertices will simply update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive object
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
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.

 * @data: (array length=n_vertices): (type Cogl.VertexP2): An array
 *        of #CoglVertexP2 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2 (CoglContext *context,
                       CoglVerticesMode mode,
                       int n_vertices,
                       const CoglVertexP2 *data);

/**
 * cogl_primitive_new_p3:
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type Cogl.VertexP3): An array of
 *        #CoglVertexP3 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3 (CoglContext *context,
                       CoglVerticesMode mode,
                       int n_vertices,
                       const CoglVertexP3 *data);

/**
 * cogl_primitive_new_p2c4:
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type Cogl.VertexP2C4): An array
 *        of #CoglVertexP2C4 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2c4 (CoglContext *context,
                         CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP2C4 *data);

/**
 * cogl_primitive_new_p3c4:
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type Cogl.VertexP3C4): An array
 *        of #CoglVertexP3C4 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3c4 (CoglContext *context,
                         CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP3C4 *data);

/**
 * cogl_primitive_new_p2t2:
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type Cogl.VertexP2T2): An array
 *        of #CoglVertexP2T2 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2t2 (CoglContext *context,
                         CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP2T2 *data);

/**
 * cogl_primitive_new_p3t2:
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type Cogl.VertexP3T2): An array
 *        of #CoglVertexP3T2 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3t2 (CoglContext *context,
                         CoglVerticesMode mode,
                         int n_vertices,
                         const CoglVertexP3T2 *data);

/**
 * cogl_primitive_new_p2t2c4:
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type Cogl.VertexP2T2C4): An
 *        array of #CoglVertexP2T2C4 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p2t2c4 (CoglContext *context,
                           CoglVerticesMode mode,
                           int n_vertices,
                           const CoglVertexP2T2C4 *data);

/**
 * cogl_primitive_new_p3t2c4:
 * @context: A #CoglContext
 * @mode: A #CoglVerticesMode defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type Cogl.VertexP3T2C4): An
 *        array of #CoglVertexP3T2C4 vertices
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
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.

 * <note>The primitive API doesn't support drawing with sliced
 * textures (since switching between slices implies changing state and
 * so that implies multiple primitives need to be submitted). You
 * should pass the %COGL_TEXTURE_NO_SLICING flag to all textures that
 * might be used while drawing with this API. If your hardware doesn't
 * support non-power of two textures (For example you are using GLES
 * 1.1) then you will need to make sure your assets are resized to a
 * power-of-two size (though they don't have to be square)</note>
 *
 * Return value: (transfer full): A newly allocated #CoglPrimitive
 * with a reference of 1. This can be freed using cogl_object_unref().
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglPrimitive *
cogl_primitive_new_p3t2c4 (CoglContext *context,
                           CoglVerticesMode mode,
                           int n_vertices,
                           const CoglVertexP3T2C4 *data);
int
cogl_primitive_get_first_vertex (CoglPrimitive *primitive);

void
cogl_primitive_set_first_vertex (CoglPrimitive *primitive,
                                 int first_vertex);

/**
 * cogl_primitive_get_n_vertices:
 * @primitive: A #CoglPrimitive object
 *
 * Queries the number of vertices to read when drawing the given
 * @primitive. Usually this value is implicitly set when associating
 * vertex data or indices with a #CoglPrimitive.
 *
 * If cogl_primitive_set_indices() has been used to associate a
 * sequence of #CoglIndices with the given @primitive then the
 * number of vertices to read can also be phrased as the number
 * of indices to read.
 *
 * <note>To be clear; it doesn't refer to the number of vertices - in
 * terms of data - associated with the primitive it's just the number
 * of vertices to read and draw.</note>
 *
 * Returns: The number of vertices to read when drawing.
 *
 * Since: 1.8
 * Stability: unstable
 */
int
cogl_primitive_get_n_vertices (CoglPrimitive *primitive);

/**
 * cogl_primitive_set_n_vertices:
 * @primitive: A #CoglPrimitive object
 * @n_vertices: The number of vertices to read when drawing.
 *
 * Specifies how many vertices should be read when drawing the given
 * @primitive.
 *
 * Usually this value is set implicitly when associating vertex data
 * or indices with a #CoglPrimitive.
 *
 * <note>To be clear; it doesn't refer to the number of vertices - in
 * terms of data - associated with the primitive it's just the number
 * of vertices to read and draw.</note>
 *
 * Since: 1.8
 * Stability: unstable
 */
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
 * @attributes: an array of #CoglAttribute pointers
 * @n_attributes: the number of elements in @attributes
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

/**
 * cogl_primitive_set_indices:
 * @primitive: A #CoglPrimitive
 * @indices: A #CoglIndices array
 * @n_indices: The number of indices to reference when drawing
 *
 * Associates a sequence of #CoglIndices with the given @primitive.
 *
 * #CoglIndices provide a way to virtualize your real vertex data by
 * providing a sequence of indices that index into your real vertex
 * data. The GPU will walk though the index values to indirectly
 * lookup the data for each vertex instead of sequentially walking
 * through the data directly. This lets you save memory by indexing
 * shared data multiple times instead of duplicating the data.
 *
 * The value passed as @n_indices will simply update the
 * #CoglPrimitive <structfield>n_vertices</structfield> property as if
 * cogl_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to draw or, put another way, how many
 * indices should be read from @indices when drawing.
 *
 * <note>The #CoglPrimitive <structfield>first_vertex</structfield> property
 * also affects drawing with indices by defining the first entry of the
 * indices to start drawing from.</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_primitive_set_indices (CoglPrimitive *primitive,
                            CoglIndices *indices,
                            int n_indices);

/**
 * cogl_primitive_get_indices:
 * @primitive: A #CoglPrimitive
 *
 * Return value: (transfer none): the indices that were set with
 * cogl_primitive_set_indices() or %NULL if no indices were set.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglIndices *
cogl_primitive_get_indices (CoglPrimitive *primitive);

/**
 * cogl_primitive_copy:
 * @primitive: A primitive copy
 *
 * Makes a copy of an existing #CoglPrimitive. Note that the primitive
 * is a shallow copy which means it will use the same attributes and
 * attribute buffers as the original primitive.
 *
 * Return value: (transfer full): the new primitive
 * Since: 1.10
 * Stability: unstable
 */
CoglPrimitive *
cogl_primitive_copy (CoglPrimitive *primitive);

/**
 * cogl_is_primitive:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglPrimitive.
 *
 * Returns: %TRUE if the @object references a #CoglPrimitive,
 *   %FALSE otherwise
 *
 * Since: 1.6
 * Stability: Unstable
 */
CoglBool
cogl_is_primitive (void *object);

/**
 * CoglPrimitiveAttributeCallback:
 * @primitive: The #CoglPrimitive whose attributes are being iterated
 * @attribute: The #CoglAttribute
 * @user_data: The private data passed to cogl_primitive_foreach_attribute()
 *
 * The callback prototype used with cogl_primitive_foreach_attribute()
 * for iterating all the attributes of a #CoglPrimitive.
 *
 * The function should return TRUE to continue iteration or FALSE to
 * stop.
 *
 * Since: 1.10
 * Stability: Unstable
 */
typedef CoglBool (* CoglPrimitiveAttributeCallback) (CoglPrimitive *primitive,
                                                     CoglAttribute *attribute,
                                                     void *user_data);

/**
 * cogl_primitive_foreach_attribute:
 * @primitive: A #CoglPrimitive object
 * @callback: A #CoglPrimitiveAttributeCallback to be called for each attribute
 * @user_data: Private data that will be passed to the callback
 *
 * Iterates all the attributes of the given #CoglPrimitive.
 *
 * Since: 1.10
 * Stability: Unstable
 */
void
cogl_primitive_foreach_attribute (CoglPrimitive *primitive,
                                  CoglPrimitiveAttributeCallback callback,
                                  void *user_data);

/**
 * cogl_primitive_draw:
 * @primitive: A #CoglPrimitive geometry object
 * @framebuffer: A destination #CoglFramebuffer
 * @pipeline: A #CoglPipeline state object
 *
 * Draws the given @primitive geometry to the specified destination
 * @framebuffer using the graphics processing state described by @pipeline.
 *
 * This drawing api doesn't support high-level meta texture types such
 * as #CoglTexture2DSliced so it is the user's responsibility to
 * ensure that only low-level textures that can be directly sampled by
 * a GPU such as #CoglTexture2D, #CoglTextureRectangle or #CoglTexture3D
 * are associated with layers of the given @pipeline.
 *
 * Stability: unstable
 * Since: 1.16
 */
void
cogl_primitive_draw (CoglPrimitive *primitive,
                     CoglFramebuffer *framebuffer,
                     CoglPipeline *pipeline);


COGL_END_DECLS

#endif /* __COGL_PRIMITIVE_H__ */

