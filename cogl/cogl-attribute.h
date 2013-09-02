/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ATTRIBUTE_H__
#define __COGL_ATTRIBUTE_H__

/* We forward declare the CoglAttribute type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglAttribute CoglAttribute;

#include <cogl/cogl-attribute-buffer.h>
#include <cogl/cogl-indices.h>

#ifdef COGL_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-attribute
 * @short_description: Functions for declaring and drawing vertex
 *    attributes
 *
 * FIXME
 */

#ifdef COGL_HAS_GTYPE_SUPPORT
/**
 * cogl_attribute_get_gtype:
 *
 * Returns: a #GType that can be used with the GLib type system.
 */
GType cogl_attribute_get_gtype (void);
#endif

/**
 * cogl_attribute_new: (constructor)
 * @attribute_buffer: The #CoglAttributeBuffer containing the actual
 *                    attribute data
 * @name: The name of the attribute (used to reference it from GLSL)
 * @stride: The number of bytes to jump to get to the next attribute
 *          value for the next vertex. (Usually
 *          <literal>sizeof (MyVertex)</literal>)
 * @offset: The byte offset from the start of @attribute_buffer for
 *          the first attribute value. (Usually
 *          <literal>offsetof (MyVertex, component0)</literal>
 * @components: The number of components (e.g. 4 for an rgba color or
 *              3 for and (x,y,z) position)
 * @type: FIXME
 *
 * Describes the layout for a list of vertex attribute values (For
 * example, a list of texture coordinates or colors).
 *
 * The @name is used to access the attribute inside a GLSL vertex
 * shader and there are some special names you should use if they are
 * applicable:
 *  <itemizedlist>
 *    <listitem>"cogl_position_in" (used for vertex positions)</listitem>
 *    <listitem>"cogl_color_in" (used for vertex colors)</listitem>
 *    <listitem>"cogl_tex_coord0_in", "cogl_tex_coord1", ...
 * (used for vertex texture coordinates)</listitem>
 *    <listitem>"cogl_normal_in" (used for vertex normals)</listitem>
 *    <listitem>"cogl_point_size_in" (used to set the size of points
 *    per-vertex. Note this can only be used if
 *    %COGL_FEATURE_ID_POINT_SIZE_ATTRIBUTE is advertised and
 *    cogl_pipeline_set_per_vertex_point_size() is called on the pipeline.
 *    </listitem>
 *  </itemizedlist>
 *
 * The attribute values corresponding to different vertices can either
 * be tightly packed or interleaved with other attribute values. For
 * example it's common to define a structure for a single vertex like:
 * |[
 * typedef struct
 * {
 *   float x, y, z; /<!-- -->* position attribute *<!-- -->/
 *   float s, t; /<!-- -->* texture coordinate attribute *<!-- -->/
 * } MyVertex;
 * ]|
 *
 * And then create an array of vertex data something like:
 * |[
 * MyVertex vertices[100] = { .... }
 * ]|
 *
 * In this case, to describe either the position or texture coordinate
 * attribute you have to move <literal>sizeof (MyVertex)</literal> bytes to
 * move from one vertex to the next.  This is called the attribute
 * @stride. If you weren't interleving attributes and you instead had
 * a packed array of float x, y pairs then the attribute stride would
 * be <literal>(2 * sizeof (float))</literal>. So the @stride is the number of
 * bytes to move to find the attribute value of the next vertex.
 *
 * Normally a list of attributes starts at the beginning of an array.
 * So for the <literal>MyVertex</literal> example above the @offset is the
 * offset inside the <literal>MyVertex</literal> structure to the first
 * component of the attribute. For the texture coordinate attribute
 * the offset would be <literal>offsetof (MyVertex, s)</literal> or instead of
 * using the offsetof macro you could use <literal>sizeof (float) *
 * 3</literal>.  If you've divided your @array into blocks of non-interleved
 * attributes then you will need to calculate the @offset as the number of
 * bytes in blocks preceding the attribute you're describing.
 *
 * An attribute often has more than one component. For example a color
 * is often comprised of 4 red, green, blue and alpha @components, and a
 * position may be comprised of 2 x and y @components. You should aim
 * to keep the number of components to a minimum as more components
 * means more data needs to be mapped into the GPU which can be a
 * bottlneck when dealing with a large number of vertices.
 *
 * Finally you need to specify the component data type. Here you
 * should aim to use the smallest type that meets your precision
 * requirements. Again the larger the type then more data needs to be
 * mapped into the GPU which can be a bottlneck when dealing with
 * a large number of vertices.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          describing the layout for a list of attribute values
 *          stored in @array.
 *
 * Since: 1.4
 * Stability: Unstable
 */
/* XXX: look for a precedent to see if the stride/offset args should
 * have a different order. */
CoglAttribute *
cogl_attribute_new (CoglAttributeBuffer *attribute_buffer,
                    const char *name,
                    size_t stride,
                    size_t offset,
                    int components,
                    CoglAttributeType type);

/**
 * cogl_attribute_new_const_1f:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: The constant value for the attribute
 *
 * Creates a new, single component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constant @value is a single precision floating point scalar
 * which should have a corresponding declaration in GLSL code like:
 *
 * [|
 * attribute float name;
 * |]
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant @value.
 */
CoglAttribute *
cogl_attribute_new_const_1f (CoglContext *context,
                             const char *name,
                             float value);

/**
 * cogl_attribute_new_const_2f:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @component0: The first component of a 2 component vector
 * @component1: The second component of a 2 component vector
 *
 * Creates a new, 2 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (@component0, @component1) represent a 2 component
 * float vector which should have a corresponding declaration in GLSL
 * code like:
 *
 * [|
 * attribute vec2 name;
 * |]
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant vector.
 */
CoglAttribute *
cogl_attribute_new_const_2f (CoglContext *context,
                             const char *name,
                             float component0,
                             float component1);

/**
 * cogl_attribute_new_const_3f:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @component0: The first component of a 3 component vector
 * @component1: The second component of a 3 component vector
 * @component2: The third component of a 3 component vector
 *
 * Creates a new, 3 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (@component0, @component1, @component2) represent a 3
 * component float vector which should have a corresponding
 * declaration in GLSL code like:
 *
 * [|
 * attribute vec3 name;
 * |]
 *
 * unless the built in name "cogl_normal_in" is being used where no
 * explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant vector.
 */
CoglAttribute *
cogl_attribute_new_const_3f (CoglContext *context,
                             const char *name,
                             float component0,
                             float component1,
                             float component2);

/**
 * cogl_attribute_new_const_4f:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @component0: The first component of a 4 component vector
 * @component1: The second component of a 4 component vector
 * @component2: The third component of a 4 component vector
 * @component3: The fourth component of a 4 component vector
 *
 * Creates a new, 4 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (@component0, @component1, @component2, @constant3)
 * represent a 4 component float vector which should have a
 * corresponding declaration in GLSL code like:
 *
 * [|
 * attribute vec4 name;
 * |]
 *
 * unless one of the built in names "cogl_color_in",
 * "cogl_tex_coord0_in or "cogl_tex_coord1_in" etc is being used where
 * no explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant vector.
 */
CoglAttribute *
cogl_attribute_new_const_4f (CoglContext *context,
                             const char *name,
                             float component0,
                             float component1,
                             float component2,
                             float component3);

/**
 * cogl_attribute_new_const_2fv:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: A pointer to a 2 component float vector
 *
 * Creates a new, 2 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (value[0], value[1]) represent a 2 component float
 * vector which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * attribute vec2 name;
 * |]
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant vector.
 */
CoglAttribute *
cogl_attribute_new_const_2fv (CoglContext *context,
                              const char *name,
                              const float *value);

/**
 * cogl_attribute_new_const_3fv:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: A pointer to a 3 component float vector
 *
 * Creates a new, 3 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (value[0], value[1], value[2]) represent a 3
 * component float vector which should have a corresponding
 * declaration in GLSL code like:
 *
 * [|
 * attribute vec3 name;
 * |]
 *
 * unless the built in name "cogl_normal_in" is being used where no
 * explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant vector.
 */
CoglAttribute *
cogl_attribute_new_const_3fv (CoglContext *context,
                              const char *name,
                              const float *value);

/**
 * cogl_attribute_new_const_4fv:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: A pointer to a 4 component float vector
 *
 * Creates a new, 4 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (value[0], value[1], value[2], value[3]) represent a
 * 4 component float vector which should have a corresponding
 * declaration in GLSL code like:
 *
 * [|
 * attribute vec4 name;
 * |]
 *
 * unless one of the built in names "cogl_color_in",
 * "cogl_tex_coord0_in or "cogl_tex_coord1_in" etc is being used where
 * no explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant vector.
 */
CoglAttribute *
cogl_attribute_new_const_4fv (CoglContext *context,
                              const char *name,
                              const float *value);

/**
 * cogl_attribute_new_const_2x2fv:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @matrix2x2: A pointer to a 2 by 2 matrix
 * @transpose: Whether the matrix should be transposed on upload or
 *             not
 *
 * Creates a new matrix attribute whose value remains constant
 * across all the vertices of a primitive without needing to duplicate
 * the value for each vertex.
 *
 * @matrix2x2 represent a square 2 by 2 matrix specified in
 * column-major order (each pair of consecutive numbers represents a
 * column) which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * attribute mat2 name;
 * |]
 *
 * If @transpose is %TRUE then all matrix components are rotated
 * around the diagonal of the matrix such that the first column
 * becomes the first row and the second column becomes the second row.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant matrix.
 */
CoglAttribute *
cogl_attribute_new_const_2x2fv (CoglContext *context,
                                const char *name,
                                const float *matrix2x2,
                                CoglBool transpose);

/**
 * cogl_attribute_new_const_3x3fv:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @matrix3x3: A pointer to a 3 by 3 matrix
 * @transpose: Whether the matrix should be transposed on upload or
 *             not
 *
 * Creates a new matrix attribute whose value remains constant
 * across all the vertices of a primitive without needing to duplicate
 * the value for each vertex.
 *
 * @matrix3x3 represent a square 3 by 3 matrix specified in
 * column-major order (each triple of consecutive numbers represents a
 * column) which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * attribute mat3 name;
 * |]
 *
 * If @transpose is %TRUE then all matrix components are rotated
 * around the diagonal of the matrix such that the first column
 * becomes the first row and the second column becomes the second row
 * etc.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant matrix.
 */
CoglAttribute *
cogl_attribute_new_const_3x3fv (CoglContext *context,
                                const char *name,
                                const float *matrix3x3,
                                CoglBool transpose);

/**
 * cogl_attribute_new_const_4x4fv:
 * @context: A #CoglContext
 * @name: The name of the attribute (used to reference it from GLSL)
 * @matrix4x4: A pointer to a 4 by 4 matrix
 * @transpose: Whether the matrix should be transposed on upload or
 *             not
 *
 * Creates a new matrix attribute whose value remains constant
 * across all the vertices of a primitive without needing to duplicate
 * the value for each vertex.
 *
 * @matrix4x4 represent a square 4 by 4 matrix specified in
 * column-major order (each 4-tuple of consecutive numbers represents a
 * column) which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * attribute mat4 name;
 * |]
 *
 * If @transpose is %TRUE then all matrix components are rotated
 * around the diagonal of the matrix such that the first column
 * becomes the first row and the second column becomes the second row
 * etc.
 *
 * Return value: (transfer full): A newly allocated #CoglAttribute
 *          representing the given constant matrix.
 */
CoglAttribute *
cogl_attribute_new_const_4x4fv (CoglContext *context,
                                const char *name,
                                const float *matrix4x4,
                                CoglBool transpose);

/**
 * cogl_attribute_set_normalized:
 * @attribute: A #CoglAttribute
 * @normalized: The new value for the normalized property.
 *
 * Sets whether fixed point attribute types are mapped to the range
 * 0→1. For example when this property is TRUE and a
 * %COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE type is used then the value 255
 * will be mapped to 1.0.
 *
 * The default value of this property depends on the name of the
 * attribute. For the builtin properties cogl_color_in and
 * cogl_normal_in it will default to TRUE and for all other names it
 * will default to FALSE.
 *
 * Stability: unstable
 * Since: 1.10
 */
void
cogl_attribute_set_normalized (CoglAttribute *attribute,
                               CoglBool normalized);

/**
 * cogl_attribute_get_normalized:
 * @attribute: A #CoglAttribute
 *
 * Return value: the value of the normalized property set with
 * cogl_attribute_set_normalized().
 *
 * Stability: unstable
 * Since: 1.10
 */
CoglBool
cogl_attribute_get_normalized (CoglAttribute *attribute);

/**
 * cogl_attribute_get_buffer:
 * @attribute: A #CoglAttribute
 *
 * Return value: (transfer none): the #CoglAttributeBuffer that was
 *        set with cogl_attribute_set_buffer() or cogl_attribute_new().
 *
 * Stability: unstable
 * Since: 1.10
 */
CoglAttributeBuffer *
cogl_attribute_get_buffer (CoglAttribute *attribute);

/**
 * cogl_attribute_set_buffer:
 * @attribute: A #CoglAttribute
 * @attribute_buffer: A #CoglAttributeBuffer
 *
 * Sets a new #CoglAttributeBuffer for the attribute.
 *
 * Stability: unstable
 * Since: 1.10
 */
void
cogl_attribute_set_buffer (CoglAttribute *attribute,
                           CoglAttributeBuffer *attribute_buffer);

/**
 * cogl_is_attribute:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglAttribute.
 *
 * Return value: %TRUE if the @object references a #CoglAttribute,
 *   %FALSE otherwise
 */
CoglBool
cogl_is_attribute (void *object);

COGL_END_DECLS

#endif /* __COGL_ATTRIBUTE_H__ */

