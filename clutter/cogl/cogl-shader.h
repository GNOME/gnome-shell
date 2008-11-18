/* cogl-shader.h: Shaders and programmable pipeline
 * This file is part of Clutter
 *
 * Copyright (C) 2008  Intel Corporation.
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

#ifndef __COGL_SHADER_H__
#define __COGL_SHADER_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-shaders
 * @short_description: Fuctions for accessing the programmable GL pipeline
 *
 * COGL allows accessing the GL programmable pipeline in order to create
 * vertex and fragment shaders.
 *
 * The only supported format is GLSL shaders.
 */

/**
 * cogl_create_shader:
 * @shader_type: CGL_VERTEX_SHADER or CGL_FRAGMENT_SHADER.
 *
 * Create a new shader handle, use #cogl_shader_source to set the source code
 * to be used on it.
 *
 * Returns: a new shader handle.
 */
CoglHandle      cogl_create_shader            (COGLenum            shader_type);

/**
 * cogl_shader_ref:
 * @handle: A #CoglHandle to a shader.
 *
 * Add an extra reference to a shader.
 *
 * Returns: @handle
 */
CoglHandle      cogl_shader_ref               (CoglHandle          handle);

/**
 * cogl_shader_unref:
 * @handle: A #CoglHandle to a shader.
 *
 * Removes a reference to a shader. If it was the last reference the
 * shader object will be destroyed.
 */
void            cogl_shader_unref             (CoglHandle          handle);

/**
 * cogl_is_shader:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing shader object.
 *
 * Returns: %TRUE if the handle references a shader,
 *   %FALSE otherwise
 */
gboolean        cogl_is_shader               (CoglHandle          handle);

/**
 * cogl_shader_source:
 * @shader: #CoglHandle for a shader.
 * @source: GLSL shader source.
 *
 * Replaces the current GLSL source associated with a shader with a new
 * one.
 */
void            cogl_shader_source            (CoglHandle          shader,
                                               const gchar        *source);
/**
 * cogl_shader_compile:
 * @handle: #CoglHandle for a shader.
 *
 * Compiles the shader, no return value, but the shader is now ready for
 * linking into a program.
 */
void            cogl_shader_compile           (CoglHandle        handle);

/**
 * cogl_shader_get_info_log:
 * @handle: #CoglHandle for a shader.
 * @size: maximum number of bytes to retrieve.
 * @buffer: location for info log.
 *
 * Retrieves the information log for a coglobject, can be used in conjunction
 * with #cogl_shader_get_parameteriv to retrieve the compiler warnings/error
 * messages that caused a shader to not compile correctly, mainly useful for
 * debugging purposes.
 */
void            cogl_shader_get_info_log      (CoglHandle          handle,
                                               guint               size,
                                               gchar              *buffer);

/**
 * cogl_shader_get_parameteriv:
 * @handle: #CoglHandle for a shader.
 * @pname: the named COGL parameter to retrieve.
 * @dest: storage location for COGLint return value.
 *
 * Retrieve a named parameter from a shader can be used to query to compile
 * satus of a shader by passing in CGL_OBJECT_COMPILE_STATUS for @pname.
 */
void            cogl_shader_get_parameteriv   (CoglHandle          handle,
                                               COGLenum            pname,
                                               COGLint            *dest);

/**
 * cogl_create_program:
 *
 * Create a new cogl program object that can be used to replace parts of the GL
 * rendering pipeline with custom code.
 *
 * Returns: a new cogl program.
 */
CoglHandle      cogl_create_program           (void);

/**
 * cogl_program_ref:
 * @handle: A #CoglHandle to a program.
 *
 * Add an extra reference to a program.
 *
 * Returns: @handle
 */
CoglHandle      cogl_program_ref              (CoglHandle        handle);

/**
 * cogl_program_unref:
 * @handle: A #CoglHandle to a program.
 *
 * Removes a reference to a program. If it was the last reference the
 * program object will be destroyed.
 */
void            cogl_program_unref            (CoglHandle        handle);

/**
 * cogl_is_program:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing program object.
 *
 * Returns: %TRUE if the handle references a program,
 *   %FALSE otherwise
 */
gboolean        cogl_is_program               (CoglHandle          handle);

/**
 * cogl_program_attach_shader:
 * @program_handle: a #CoglHandle for a shdaer program.
 * @shader_handle: a #CoglHandle for a vertex of fragment shader.
 *
 * Attaches a shader to a program object, a program can have one vertex shader
 * and one fragment shader attached.
 */
void            cogl_program_attach_shader    (CoglHandle        program_handle,
                                               CoglHandle        shader_handle);


/**
 * cogl_program_link:
 * @handle: a #CoglHandle for a shader program.
 *
 * Links a program making it ready for use.
 */
void            cogl_program_link             (CoglHandle        handle);

/**
 * cogl_program_use:
 * @handle: a #CoglHandle for a shader program or %COGL_INVALID_HANDLE.
 *
 * Activate a specific shader program replacing that part of the GL
 * rendering pipeline, if passed in %COGL_INVALID_HANDLE the default
 * behavior of GL is reinstated.
 */
void            cogl_program_use              (CoglHandle        handle);

/**
 * cogl_program_get_uniform_location:
 * @handle: a #CoglHandle for a shader program.
 * @uniform_name: the name of a uniform.
 *
 * Retrieve the location (offset) of a uniform variable in a shader program,
 * a uniform is a variable that is constant for all vertices/fragments for a
 * shader object and is possible to modify as an external parameter.
 *
 * Return value: the offset of a uniform in a specified program.
 *   This uniform can be set using cogl_program_uniform_1f() when the
 *   program is in use.
 */
COGLint         cogl_program_get_uniform_location
                                              (CoglHandle        handle,
                                               const gchar      *uniform_name);

/**
 * cogl_program_uniform_1f:
 * @uniform_no: the unform to set.
 * @value: the new value of the uniform.
 *
 * Changes the value of a uniform in the currently used (see
 * cogl_program_use()) shader program.
 */
void            cogl_program_uniform_1f       (COGLint           uniform_no,
                                               gfloat            value);

 /**
 * cogl_program_uniform_float:
 * @uniform_no: the uniform to set.
 * @size: Size of float vector.
 * @count: Size of array of uniforms.
 * @value: the new value of the uniform.
 *
 * Changes the value of a float vector uniform, or uniform array in the
 * currently used (see #cogl_program_use) shader program.
 */
void            cogl_program_uniform_float    (COGLint           uniform_no,
                                               gint              size,
                                               gint              count,
                                               const GLfloat    *value);

/**
 * cogl_program_uniform_int:
 * @uniform_no: the uniform to set.
 * @size: Size of int vector.
 * @count: Size of array of uniforms.
 * @value: the new value of the uniform.
 *
 * Changes the value of a int vector uniform, or uniform array in the
 * currently used (see cogl_program_use()) shader program.
 */
void            cogl_program_uniform_int      (COGLint           uniform_no,
                                               gint              size,
                                               gint              count,
                                               const COGLint    *value);

/**
 * cogl_program_uniform_matrix:
 * @uniform_no: the uniform to set.
 * @size: Size of matrix.
 * @count: Size of array of uniforms.
 * @transpose: Whether to transpose the matrix when setting the uniform.
 * @value: the new value of the uniform.
 *
 * Changes the value of a matrix uniform, or uniform array in the
 * currently used (see cogl_program_use()) shader program. The @size
 * parameter is used to determine the square size of the matrix.
 */
void            cogl_program_uniform_matrix   (COGLint           uniform_no,
                                               gint              size,
                                               gint              count,
                                               gboolean          transpose,
                                               const GLfloat    *value);

G_END_DECLS

#endif /* __COGL_SHADER_H__ */
