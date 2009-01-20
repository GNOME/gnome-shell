/*
 * Cogl.
 *
 * An OpenGL/GLES Abstraction/Utility Layer
 *
 * Vertex Buffer API: Handle extensible arrays of vertex attributes
 *
 * Copyright (C) 2008  Intel Corporation.
 *
 * Authored by: Robert Bragg <robert@linux.intel.com>
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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_VERTEX_BUFFER_H__
#define __COGL_VERTEX_BUFFER_H__

#include <glib.h>
#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-vertex-buffer
 * @short_description: An API for submitting extensible arrays of vertex
 *		       attributes to OpenGL in a way that aims to minimise
 *		       copying or reformatting of the original data.
 *
 * The Attributes Buffer API is designed to be a fairly raw mechanism for
 * developers to be able to submit geometry to Cogl in a format that can be
 * directly consumed by an OpenGL driver and with awareness of the specific
 * hardware being used then costly format conversion can also be avoided.
 *
 * They are designed to work on top of buffer objects and developers should
 * understand that attribute buffers are not that cheap to create but once they
 * have been submitted they can be stored in GPU addressable memory and can
 * be quickly reused.
 *
 * Although this API does allow you to modify attribute buffers after they have
 * been submitted to the GPU you must note that modification is also not that
 * cheap, so if at all possible think of tricks that let you reuse a static
 * buffer. To help with this, it is possible to enable and disable individual
 * attributes cheaply.
 *
 * Take for example attributes representing an elipse. If you were to submit
 * color attributes, texture coordinates and normals, then you would be able
 * to draw an elipses in the following different ways without modifying
 * the vertex buffer, only by changing your source material.
 * <itemizedlist>
 * <listitem>Flat colored elipse</listitem>
 * <listitem>Textured elipse</listitem>
 * <listitem>Smoothly lit textured elipse blended with the color.</listitem>
 * </itemizedlist>
 *
 * Another trick that can be used is submitting highly detailed vertices and
 * then using cogl_vertex_buffer_draw_range_elements to sample sub-sets of
 * the geometry or lower resolution geometry out from a fixed buffer.
 *
 * The API doesn't currently give you any control over the actual OpenGL buffer
 * objects that are created, but you can expect that when you first submit
 * your attributes they start off in one or more GL_STATIC_DRAW buffers.
 * If you then update some of your attributes; then these attributes will
 * normally be moved into new GL_DYNAMIC_DRAW draw buffers.
 */

/**
 * cogl_vertex_buffer_new:
 * @n_vertices: The number of vertices that your attributes will correspond to.
 *
 * This creates a Cogl handle for a new vertex buffer that you can then start
 * to add attributes too.
 */
CoglHandle
cogl_vertex_buffer_new (guint n_vertices);

/**
 * cogl_vertex_buffer_add:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of your attribute. It should be a valid GLSL
 *		    variable name and standard attribute types must use one
 *		    of following built-in names: (Note: they correspond to the
 *		    built-in names in GLSL)
 *		    <itemizedlist>
 *		    <listitem>"gl_Color"</listitem>
 *		    <listitem>"gl_Normal"</listitem>
 *		    <listitem>"gl_MultiTexCoord0, gl_MultiTexCoord1, ..."</listitem>
 *		    <listitem>"gl_Vertex"</listitem>
 *		    </itemizedlist>
 *		    To support adding multiple variations of the same attribute
 *		    the name can have a detail component, E.g.
 *		    "gl_Color::active" or "gl_Color::inactive"
 * @n_components: The number of components per attribute and must be 1,2,3 or 4
 * @gl_type: Specifies the data type of each component (GL_BYTE, GL_UNSIGNED_BYTE,
 *	     GL_SHORT, GL_UNSIGNED_SHORT, GL_INT, GL_UNSIGNED_INT, GL_FLOAT or
 *	     GL_DOUBLE)
 * @normalized: If GL_TRUE, this specifies that values stored in an integer
 *		format should be mapped into the range [-1.0, 1.0] or [0.1, 1.0]
 *		for unsigned values. If GL_FALSE they are converted to floats
 *		directly.
 * @stride: This specifies the number of bytes from the start of one attribute
 *	    value to the start of the next value (for the same attribute). So
 *	    for example with a position interleved with color like this:
 *	    XYRGBAXYRGBAXYRGBA, then if each letter represents a byte, the
 *	    stride for both attributes is 6. The special value 0 means the
 *	    values are stored sequentially in memory.
 * @pointer: This addresses the first attribute in the vertex array. (This
 *	     must remain valid until you call cogl_vertex_buffer_submit)
 *
 * This function lets you add an attribute to a buffer. You either use one
 * of the built-in names to add standard attributes, like positions, colors
 * and normals or you can add custom attributes for use in shaders.
 *
 * Note: The number of vertices declared when first creating the vertex
 * buffer is used to determine how many attribute values will be read from the
 * supplied pointer.
 *
 * Note: the data supplied here isn't copied anywhere until you call
 * cogl_vertex_buffer_submit, so the supplied pointer must remain valid
 * until then.
 * (This is an important optimisation since we can't create the underlying
 * OpenGL buffer object until we know about all the attributes, and repeatedly
 * copying large buffers of vertex data may be very costly) If you add
 * attributes after submitting then you will need to re-call
 * cogl_vertex_buffer_submit to commit the changes to the GPU. (Be carefull
 * to minimize the number of calls to cogl_vertex_buffer_submit though)
 *
 * Note: If you are interleving attributes it is assumed that that each
 * interleaved attribute starts no farther than +- stride bytes from the other
 * attributes it is interleved with. I.e. this is ok:
 * |-0-0-0-0-0-0-0-0-0-0|
 * This is not ok:
 * |- - - - -0-0-0-0-0-0 0 0 0 0|
 * (Though you can have multiple groups of interleved attributes)
 */
void
cogl_vertex_buffer_add (CoglHandle  handle,
		        const char *attribute_name,
			guint8      n_components,
			GLenum      gl_type,
			gboolean    normalized,
			guint16     stride,
			const void *pointer);

/**
 * cogl_vertex_buffer_delete:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of a previously added attribute
 *
 * This function deletes an attribute from a buffer. You will need to
 * call cogl_vertex_buffer_submit to commit this change to the GPU.
 */
void
cogl_vertex_buffer_delete (CoglHandle   handle,
			   const char  *attribute_name);

/**
 * cogl_vertex_buffer_enable:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of the attribute you want to enable
 *
 * This function enables a previosuly added attribute
 *
 * Since it can be costly to add and remove new attributes to buffers; to make
 * individual buffers more reuseable it is possible to enable and disable
 * attributes before using a buffer for drawing.
 *
 * Note: You don't need to call cogl_vertex_buffer_submit after using this
 * function
 */
void
cogl_vertex_buffer_enable (CoglHandle  handle,
			   const char *attribute_name);

/**
 * cogl_vertex_buffer_submit:
 * @handle: A vertex buffer handle
 *
 * This function copies all the user added attributes into buffer objects
 * managed by the OpenGL driver.
 *
 * You should aim to minimize calls to this function.
 */
void
cogl_vertex_buffer_submit (CoglHandle handle);

/**
 * cogl_vertex_buffer_disable:
 * @handle: A vertex buffer handle
 * @attribute_name: The name of the attribute you want to disable
 *
 * This function disables a previosuly added attribute
 *
 * Since it can be costly to add and remove new attributes to buffers; to make
 * individual buffers more reuseable it is possible to enable and disable
 * attributes before using a buffer for drawing.
 *
 * Note: You don't need to call cogl_vertex_buffer_submit after using this
 * function
 */
void
cogl_vertex_buffer_disable (CoglHandle  handle,
			    const char *attribute_name);

/**
 * cogl_vertex_buffer_draw:
 * @handle: A vertex buffer handle
 * @mode: Specifies how the vertices should be interpreted, and should be
 *        a valid GL primitive type:
 *	  <itemizedlist>
 *	  <listitem>GL_POINTS</listitem>
 *	  <listitem>GL_LINE_STRIP</listitem>
 *	  <listitem>GL_LINE_LOOP</listitem>
 *	  <listitem>GL_LINES</listitem>
 *	  <listitem>GL_TRIANGLE_STRIP</listitem>
 *	  <listitem>GL_TRIANGLE_FAN</listitem>
 *	  <listitem>GL_TRIANGLES</listitem>
 *	  </itemizedlist>
 *	  (Note: only types available in GLES are listed)
 * @first: Specifies the index of the first vertex you want to draw with
 * @count: Specifies the number of vertices you want to draw.
 *
 * This function lets you draw geometry using all or a subset of the
 * vertices in a vertex buffer.
 */
void
cogl_vertex_buffer_draw (CoglHandle handle,
		         GLenum     mode,
		         GLint      first,
		         GLsizei    count);

/**
 * cogl_vertex_buffer_draw_range_elements:
 * @handle: A vertex buffer handle
 * @mode: Specifies how the vertices should be interpreted, and should be
 *        a valid GL primitive type:
 *	  <itemizedlist>
 *	  <listitem>GL_POINTS</listitem>
 *	  <listitem>GL_LINE_STRIP</listitem>
 *	  <listitem>GL_LINE_LOOP</listitem>
 *	  <listitem>GL_LINES</listitem>
 *	  <listitem>GL_TRIANGLE_STRIP</listitem>
 *	  <listitem>GL_TRIANGLE_FAN</listitem>
 *	  <listitem>GL_TRIANGLES</listitem>
 *	  </itemizedlist>
 *	  (Note: only types available in GLES are listed)
 * @min_index: Specifies the minimum vertex index contained in indices
 * @max_index: Specifies the maximum vertex index contained in indices
 * @count: Specifies the number of vertices you want to draw.
 * @indices_typetype: Specifies the data type used for the indices, and must be
 *	              one of:
 *	  <itemizedlist>
 *	  <listitem>GL_UNSIGNED_BYTE</listitem>
 *	  <listitem>GL_UNSIGNED_SHORT</listitem>
 *        <listitem>GL_UNSIGNED_INT</listitem>
 *        </itemizedlist>
 * @indices: Specifies the address of your array of indices
 *
 * This function lets you use an array of indices to specify the vertices
 * within your vertex buffer that you want to draw.
 */
void
cogl_vertex_buffer_draw_range_elements (CoglHandle     handle,
			                GLenum         mode,
			                GLuint         min_index,
			                GLuint         max_index,
			                GLsizei        count,
			                GLenum         indices_type,
			                const GLvoid  *indices);

/**
 * cogl_vertex_buffer_ref:
 * @handle: a @CoglHandle.
 *
 * Increment the reference count for a vertex buffer
 *
 * Returns: the @handle.
 */
CoglHandle
cogl_vertex_buffer_ref (CoglHandle handle);

/**
 * cogl_vertex_buffer_unref:
 * @handle: a @CoglHandle.
 *
 * Decrement the reference count for a vertex buffer
 */
void
cogl_vertex_buffer_unref (CoglHandle handle);


G_END_DECLS

#endif /* __COGL_VERTEX_BUFFER_H__ */
