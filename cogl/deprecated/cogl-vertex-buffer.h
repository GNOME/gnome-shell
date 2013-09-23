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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_VERTEX_BUFFER_H__
#define __COGL_VERTEX_BUFFER_H__

#include <glib.h>
#include <cogl/cogl-defines.h>
#include <cogl/cogl-types.h>
#include <cogl/cogl-macros.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-vertex-buffer
 * @short_description: An API for submitting extensible arrays of vertex
 *   attributes to be mapped into the GPU for fast drawing.
 *
 * For example to describe a textured triangle, you could create a new cogl
 * vertex buffer with 3 vertices, and then you might add 2 attributes for each
 * vertex:
 * <orderedlist>
 * <listitem>
 * a "gl_Position" describing the (x,y,z) position for each vertex.
 * </listitem>
 * <listitem>
 * a "gl_MultiTexCoord0" describing the (tx,ty) texture coordinates for each
 * vertex.
 * </listitem>
 * </orderedlist>
 *
 * The Vertex Buffer API is designed to be a fairly raw mechanism for
 * developers to be able to submit geometry to Cogl in a format that can be
 * directly consumed by an OpenGL driver and mapped into your GPU for fast
 * re-use. It is designed to avoid repeated validation of the attributes by the
 * driver; to minimize transport costs (e.g. considering indirect GLX
 * use-cases) and to potentially avoid repeated format conversions when
 * attributes are supplied in a format that is not natively supported by the
 * GPU.
 *
 * Although this API does allow you to modify attributes after they have been
 * submitted to the GPU you should be aware that modification is not that
 * cheap, since it implies validating the new data and potentially the
 * OpenGL driver will need to reformat it for the GPU.
 *
 * If at all possible think of tricks that let you re-use static attributes,
 * and if you do need to repeatedly update attributes (e.g. for some kind of
 * morphing geometry) then only update and re-submit the specific attributes
 * that have changed.
 */

/**
 * cogl_vertex_buffer_new:
 * @n_vertices: The number of vertices that your attributes will correspond to.
 *
 * Creates a new vertex buffer that you can use to add attributes.
 *
 * Return value: a new #CoglHandle
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
CoglHandle
cogl_vertex_buffer_new (unsigned int n_vertices);

/**
 * cogl_vertex_buffer_get_n_vertices:
 * @handle: A vertex buffer handle
 *
 * Retrieves the number of vertices that @handle represents
 *
 * Return value: the number of vertices
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
unsigned int
cogl_vertex_buffer_get_n_vertices (CoglHandle handle);

/**
 * cogl_vertex_buffer_add:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of your attribute. It should be a valid GLSL
 *   variable name and standard attribute types must use one of following
 *   built-in names: (Note: they correspond to the built-in names of GLSL)
 *   <itemizedlist>
 *     <listitem>"gl_Color"</listitem>
 *     <listitem>"gl_Normal"</listitem>
 *     <listitem>"gl_MultiTexCoord0, gl_MultiTexCoord1, ..."</listitem>
 *     <listitem>"gl_Vertex"</listitem>
 *   </itemizedlist>
 *   To support adding multiple variations of the same attribute the name
 *   can have a detail component, E.g. "gl_Color::active" or
 *   "gl_Color::inactive"
 * @n_components: The number of components per attribute and must be 1, 2,
 *   3 or 4
 * @type: a #CoglAttributeType specifying the data type of each component.
 * @normalized: If %TRUE, this specifies that values stored in an integer
 *   format should be mapped into the range [-1.0, 1.0] or [0.0, 1.0]
 *   for unsigned values. If %FALSE they are converted to floats
 *   directly.
 * @stride: This specifies the number of bytes from the start of one attribute
 *   value to the start of the next value (for the same attribute). So, for
 *   example, with a position interleved with color like this:
 *   XYRGBAXYRGBAXYRGBA, then if each letter represents a byte, the
 *   stride for both attributes is 6. The special value 0 means the
 *   values are stored sequentially in memory.
 * @pointer: This addresses the first attribute in the vertex array. This
 *   must remain valid until you either call cogl_vertex_buffer_submit() or
 *   issue a draw call.
 *
 * Adds an attribute to a buffer, or replaces a previously added
 * attribute with the same name.
 *
 * You either can use one of the built-in names such as "gl_Vertex", or
 * "gl_MultiTexCoord0" to add standard attributes, like positions, colors
 * and normals, or you can add custom attributes for use in shaders.
 *
 * The number of vertices declared when calling cogl_vertex_buffer_new()
 * determines how many attribute values will be read from the supplied
 * @pointer.
 *
 * The data for your attribute isn't copied anywhere until you call
 * cogl_vertex_buffer_submit(), or issue a draw call which automatically
 * submits pending attribute changes. so the supplied pointer must remain
 * valid until then. If you are updating an existing attribute (done by
 * re-adding it) then you still need to re-call cogl_vertex_buffer_submit()
 * to commit the changes to the GPU. Be carefull to minimize the number
 * of calls to cogl_vertex_buffer_submit(), though.
 *
 * <note>If you are interleving attributes it is assumed that each interleaved
 * attribute starts no farther than +- stride bytes from the other attributes
 * it is interleved with. I.e. this is ok:
 * <programlisting>
 * |-0-0-0-0-0-0-0-0-0-0|
 * </programlisting>
 * This is not ok:
 * <programlisting>
 * |- - - - -0-0-0-0-0-0 0 0 0 0|
 * </programlisting>
 * (Though you can have multiple groups of interleved attributes)</note>
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
void
cogl_vertex_buffer_add (CoglHandle         handle,
		        const char        *attribute_name,
			uint8_t            n_components,
			CoglAttributeType  type,
			CoglBool           normalized,
			uint16_t           stride,
			const void        *pointer);

/**
 * cogl_vertex_buffer_delete:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of a previously added attribute
 *
 * Deletes an attribute from a buffer. You will need to call
 * cogl_vertex_buffer_submit() or issue a draw call to commit this
 * change to the GPU.
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
void
cogl_vertex_buffer_delete (CoglHandle   handle,
			   const char  *attribute_name);

/**
 * cogl_vertex_buffer_submit:
 * @handle: A vertex buffer handle
 *
 * Submits all the user added attributes to the GPU; once submitted, the
 * attributes can be used for drawing.
 *
 * You should aim to minimize calls to this function since it implies
 * validating your data; it potentially incurs a transport cost (especially if
 * you are using GLX indirect rendering) and potentially a format conversion
 * cost if the GPU doesn't natively support any of the given attribute formats.
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
void
cogl_vertex_buffer_submit (CoglHandle handle);

/**
 * cogl_vertex_buffer_disable:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of the attribute you want to disable
 *
 * Disables a previosuly added attribute.
 *
 * Since it can be costly to add and remove new attributes to buffers; to make
 * individual buffers more reuseable it is possible to enable and disable
 * attributes before using a buffer for drawing.
 *
 * You don't need to call cogl_vertex_buffer_submit() after using this
 * function.
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
void
cogl_vertex_buffer_disable (CoglHandle  handle,
			    const char *attribute_name);

/**
 * cogl_vertex_buffer_enable:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of the attribute you want to enable
 *
 * Enables a previosuly disabled attribute.
 *
 * Since it can be costly to add and remove new attributes to buffers; to make
 * individual buffers more reuseable it is possible to enable and disable
 * attributes before using a buffer for drawing.
 *
 * You don't need to call cogl_vertex_buffer_submit() after using this function
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
void
cogl_vertex_buffer_enable (CoglHandle  handle,
			   const char *attribute_name);

/**
 * cogl_vertex_buffer_draw:
 * @handle: A vertex buffer handle
 * @mode: A #CoglVerticesMode specifying how the vertices should be
 *   interpreted.
 * @first: Specifies the index of the first vertex you want to draw with
 * @count: Specifies the number of vertices you want to draw.
 *
 * Allows you to draw geometry using all or a subset of the
 * vertices in a vertex buffer.
 *
 * Any un-submitted attribute changes are automatically submitted before
 * drawing.
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
void
cogl_vertex_buffer_draw (CoglHandle       handle,
		         CoglVerticesMode mode,
		         int              first,
		         int              count);

/**
 * cogl_vertex_buffer_indices_new:
 * @indices_type: a #CoglIndicesType specifying the data type used for
 *    the indices.
 * @indices_array: (array length=indices_len): Specifies the address of
 *   your array of indices
 * @indices_len: The number of indices in indices_array
 *
 * Depending on how much geometry you are submitting it can be worthwhile
 * optimizing the number of redundant vertices you submit. Using an index
 * array allows you to reference vertices multiple times, for example
 * during triangle strips.
 *
 * Return value: A CoglHandle for the indices which you can pass to
 *   cogl_vertex_buffer_draw_elements().
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
CoglHandle
cogl_vertex_buffer_indices_new (CoglIndicesType  indices_type,
                                const void      *indices_array,
                                int              indices_len);

/**
 * cogl_vertex_buffer_indices_get_type:
 * @indices: An indices handle
 *
 * Queries back the data type used for the given indices
 *
 * Returns: The CoglIndicesType used
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
CoglIndicesType
cogl_vertex_buffer_indices_get_type (CoglHandle indices);

/**
 * cogl_vertex_buffer_draw_elements:
 * @handle: A vertex buffer handle
 * @mode: A #CoglVerticesMode specifying how the vertices should be
 *    interpreted.
 * @indices: A CoglHandle for a set of indices allocated via
 *    cogl_vertex_buffer_indices_new ()
 * @min_index: Specifies the minimum vertex index contained in indices
 * @max_index: Specifies the maximum vertex index contained in indices
 * @indices_offset: An offset into named indices. The offset marks the first
 *    index to use for drawing.
 * @count: Specifies the number of vertices you want to draw.
 *
 * This function lets you use an array of indices to specify the vertices
 * within your vertex buffer that you want to draw. The indices themselves
 * are created by calling cogl_vertex_buffer_indices_new ()
 *
 * Any un-submitted attribute changes are automatically submitted before
 * drawing.
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
void
cogl_vertex_buffer_draw_elements (CoglHandle       handle,
			          CoglVerticesMode mode,
                                  CoglHandle       indices,
                                  int              min_index,
                                  int              max_index,
                                  int              indices_offset,
                                  int              count);

/**
 * cogl_vertex_buffer_ref:
 * @handle: a @CoglHandle.
 *
 * Increment the reference count for a vertex buffer
 *
 * Return value: the @handle.
 *
 * Deprecated: 1.2: Use cogl_object_ref() instead
 */
COGL_DEPRECATED_FOR (cogl_object_ref)
CoglHandle
cogl_vertex_buffer_ref (CoglHandle handle);

/**
 * cogl_vertex_buffer_unref:
 * @handle: a @CoglHandle.
 *
 * Decrement the reference count for a vertex buffer
 *
 * Deprecated: 1.2: Use cogl_object_unref() instead
 */
COGL_DEPRECATED_FOR (cogl_object_unref)
void
cogl_vertex_buffer_unref (CoglHandle handle);

/**
 * cogl_vertex_buffer_indices_get_for_quads:
 * @n_indices: the number of indices in the vertex buffer.
 *
 * Creates a vertex buffer containing the indices needed to draw pairs
 * of triangles from a list of vertices grouped as quads. There will
 * be at least @n_indices entries in the buffer (but there may be
 * more).
 *
 * The indices will follow this pattern:
 *
 * 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 ... etc
 *
 * For example, if you submit vertices for a quad like like that shown
 * in <xref linkend="quad-indices-order"/> then you can request 6
 * indices to render two triangles like those shown in <xref
 * linkend="quad-indices-triangles"/>.
 *
 * <figure id="quad-indices-order">
 *   <title>Example of vertices submitted to form a quad</title>
 *   <graphic fileref="quad-indices-order.png" format="PNG"/>
 * </figure>
 *
 * <figure id="quad-indices-triangles">
 *   <title>Illustration of the triangle indices that will be generated</title>
 *   <graphic fileref="quad-indices-triangles.png" format="PNG"/>
 * </figure>
 *
 * Returns: A %CoglHandle containing the indices. The handled is
 * owned by Cogl and should not be modified or unref'd.
 *
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
CoglHandle
cogl_vertex_buffer_indices_get_for_quads (unsigned int n_indices);

/**
 * cogl_is_vertex_buffer:
 * @handle: a #CoglHandle for a vertex buffer object
 *
 * Checks whether @handle is a Vertex Buffer Object
 *
 * Return value: %TRUE if the handle is a VBO, and %FALSE
 *   otherwise
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
CoglBool
cogl_is_vertex_buffer (CoglHandle handle);

/**
 * cogl_is_vertex_buffer_indices:
 * @handle: a #CoglHandle
 *
 * Checks whether @handle is a handle to the indices for a vertex
 * buffer object
 *
 * Return value: %TRUE if the handle is indices, and %FALSE
 *   otherwise
 *
 * Since: 1.4
 * Deprecated: 1.16: Use the #CoglPrimitive api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_primitive_API)
CoglBool
cogl_is_vertex_buffer_indices (CoglHandle handle);
COGL_END_DECLS

#endif /* __COGL_VERTEX_BUFFER_H__ */
