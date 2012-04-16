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

#ifndef __COGL_ATTRIBUTE_H__
#define __COGL_ATTRIBUTE_H__

/* We forward declare the CoglAttribute type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglAttribute CoglAttribute;

#include <cogl/cogl-attribute-buffer.h>
#include <cogl/cogl-indices.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-attribute
 * @short_description: Functions for declaring and drawing vertex
 *    attributes
 *
 * FIXME
 */

/**
 * cogl_attribute_new:
 * @attribute_buffer: The #CoglAttributeBuffer containing the actual
 *                    attribute data
 * @name: The name of the attribute (used to reference it from GLSL)
 * @stride: The number of bytes to jump to get to the next attribute
 *          value for the next vertex. (Usually
 *          <pre>sizeof (MyVertex)</pre>)
 * @offset: The byte offset from the start of @attribute_buffer for
 *          the first attribute value. (Usually
 *          <pre>offsetof (MyVertex, component0)</pre>
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
 * attribute you have to move <pre>sizeof (MyVertex)</pre> bytes to
 * move from one vertex to the next.  This is called the attribute
 * @stride. If you weren't interleving attributes and you instead had
 * a packed array of float x, y pairs then the attribute stride would
 * be <pre>(2 * sizeof (float))</pre>. So the @stride is the number of
 * bytes to move to find the attribute value of the next vertex.
 *
 * Normally a list of attributes starts at the beginning of an array.
 * So for the <pre>MyVertex</pre> example above the @offset is the
 * offset inside the <pre>MyVertex</pre> structure to the first
 * component of the attribute. For the texture coordinate attribute
 * the offset would be <pre>offsetof (MyVertex, s)</pre> or instead of
 * using the offsetof macro you could use <pre>sizeof (float) * 3</pre>.
 * If you've divided your @array into blocks of non-interleved
 * attributes then you will need to calculate the @offset as the
 * number of bytes in blocks preceding the attribute you're
 * describing.
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
 * Returns: A newly allocated #CoglAttribute describing the
 *          layout for a list of attribute values stored in @array.
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
 * Return value: the #CoglAttributeBuffer that was set with
 * cogl_attribute_set_buffer() or cogl_attribute_new().
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

G_END_DECLS

#endif /* __COGL_ATTRIBUTE_H__ */

