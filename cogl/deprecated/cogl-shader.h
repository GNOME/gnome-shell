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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_SHADER_H__
#define __COGL_SHADER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-defines.h>
#include <cogl/cogl-macros.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-shaders
 * @short_description: Fuctions for accessing the programmable GL pipeline
 *
 * Cogl allows accessing the GL programmable pipeline in order to create
 * vertex and fragment shaders.
 *
 * The shader source code can either be GLSL or ARBfp. If the source
 * code is ARBfp, it must begin with the string “!!ARBfp1.0”. The
 * application should check for the %COGL_FEATURE_SHADERS_GLSL or
 * %COGL_FEATURE_SHADERS_ARBFP features before using shaders.
 *
 * When using GLSL Cogl provides replacement names for most of the
 * builtin varyings and uniforms. It is recommended to use these names
 * wherever possible to increase portability between OpenGL 2.0 and
 * GLES 2.0. GLES 2.0 does not have most of the builtins under their
 * original names so they will only work with the Cogl names.
 *
 * For use in all GLSL shaders, the Cogl builtins are as follows:
 *
 * <tip>
 * <glosslist>
 *  <glossentry>
 *   <glossterm>uniform mat4
 *         <emphasis>cogl_modelview_matrix</emphasis></glossterm>
 *   <glossdef><para>
 *    The current modelview matrix. This is equivalent to
 *    #gl_ModelViewMatrix.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>uniform mat4
 *         <emphasis>cogl_projection_matrix</emphasis></glossterm>
 *   <glossdef><para>
 *    The current projection matrix. This is equivalent to
 *    #gl_ProjectionMatrix.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>uniform mat4
 *         <emphasis>cogl_modelview_projection_matrix</emphasis></glossterm>
 *   <glossdef><para>
 *    The combined modelview and projection matrix. A vertex shader
 *    would typically use this to transform the incoming vertex
 *    position. The separate modelview and projection matrices are
 *    usually only needed for lighting calculations. This is
 *    equivalent to #gl_ModelViewProjectionMatrix.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>uniform mat4
 *         <emphasis>cogl_texture_matrix</emphasis>[]</glossterm>
 *   <glossdef><para>
 *    An array of matrices for transforming the texture
 *    coordinates. This is equivalent to #gl_TextureMatrix.
 *   </para></glossdef>
 *  </glossentry>
 * </glosslist>
 * </tip>
 *
 * In a vertex shader, the following are also available:
 *
 * <tip>
 * <glosslist>
 *  <glossentry>
 *   <glossterm>attribute vec4
 *         <emphasis>cogl_position_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The incoming vertex position. This is equivalent to #gl_Vertex.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>attribute vec4
 *         <emphasis>cogl_color_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The incoming vertex color. This is equivalent to #gl_Color.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>attribute vec4
 *         <emphasis>cogl_tex_coord_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The texture coordinate for the first texture unit. This is
 *    equivalent to #gl_MultiTexCoord0.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>attribute vec4
 *         <emphasis>cogl_tex_coord0_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The texture coordinate for the first texture unit. This is
 *    equivalent to #gl_MultiTexCoord0. There is also
 *    #cogl_tex_coord1_in and so on.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>attribute vec3
 *         <emphasis>cogl_normal_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The normal of the vertex. This is equivalent to #gl_Normal.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>vec4
 *         <emphasis>cogl_position_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated position of the vertex. This must be written to
 *    in all vertex shaders. This is equivalent to #gl_Position.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>float
 *         <emphasis>cogl_point_size_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated size of a point. This is equivalent to #gl_PointSize.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>varying vec4
 *         <emphasis>cogl_color_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated color of a vertex. This is equivalent to #gl_FrontColor.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>varying vec4
 *         <emphasis>cogl_tex_coord_out</emphasis>[]</glossterm>
 *   <glossdef><para>
 *    An array of calculated texture coordinates for a vertex. This is
 *    equivalent to #gl_TexCoord.
 *   </para></glossdef>
 *  </glossentry>
 * </glosslist>
 * </tip>
 *
 * In a fragment shader, the following are also available:
 *
 * <tip>
 * <glosslist>
 *  <glossentry>
 *   <glossterm>varying vec4 <emphasis>cogl_color_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated color of a vertex. This is equivalent to #gl_FrontColor.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>varying vec4
 *              <emphasis>cogl_tex_coord_in</emphasis>[]</glossterm>
 *   <glossdef><para>
 *    An array of calculated texture coordinates for a vertex. This is
 *    equivalent to #gl_TexCoord.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>vec4 <emphasis>cogl_color_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The final calculated color of the fragment. All fragment shaders
 *    must write to this variable. This is equivalent to
 *    #gl_FrontColor.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>float <emphasis>cogl_depth_out</emphasis></glossterm>
 *   <glossdef><para>
 *    An optional output variable specifying the depth value to use
 *    for this fragment. This is equivalent to #gl_FragDepth.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>bool <emphasis>cogl_front_facing</emphasis></glossterm>
 *   <glossdef><para>
 *    A readonly variable that will be true if the current primitive
 *    is front facing. This can be used to implement two-sided
 *    coloring algorithms. This is equivalent to #gl_FrontFacing.
 *   </para></glossdef>
 *  </glossentry>
 * </glosslist>
 * </tip>
 *
 * It's worth nothing that this API isn't what Cogl would like to have
 * in the long term and it may be removed in Cogl 2.0. The
 * experimental #CoglShader API is the proposed replacement.
 */

/**
 * CoglShaderType:
 * @COGL_SHADER_TYPE_VERTEX: A program for proccessing vertices
 * @COGL_SHADER_TYPE_FRAGMENT: A program for processing fragments
 *
 * Types of shaders
 *
 * Since: 1.0
 */
typedef enum {
  COGL_SHADER_TYPE_VERTEX,
  COGL_SHADER_TYPE_FRAGMENT
} CoglShaderType;

/**
 * cogl_create_shader:
 * @shader_type: COGL_SHADER_TYPE_VERTEX or COGL_SHADER_TYPE_FRAGMENT.
 *
 * Create a new shader handle, use cogl_shader_source() to set the
 * source code to be used on it.
 *
 * Returns: a new shader handle.
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglHandle
cogl_create_shader (CoglShaderType shader_type);

/**
 * cogl_shader_ref:
 * @handle: A #CoglHandle to a shader.
 *
 * Add an extra reference to a shader.
 *
 * Returns: @handle
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglHandle
cogl_shader_ref (CoglHandle handle);

/**
 * cogl_shader_unref:
 * @handle: A #CoglHandle to a shader.
 *
 * Removes a reference to a shader. If it was the last reference the
 * shader object will be destroyed.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_shader_unref (CoglHandle handle);

/**
 * cogl_is_shader:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing shader object.
 *
 * Returns: %TRUE if the handle references a shader,
 *   %FALSE otherwise
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglBool
cogl_is_shader (CoglHandle handle);

/**
 * cogl_shader_source:
 * @shader: #CoglHandle for a shader.
 * @source: Shader source.
 *
 * Replaces the current source associated with a shader with a new
 * one.
 *
 * Please see <link
 * linkend="cogl-Shaders-and-Programmable-Pipeline.description">above</link>
 * for a description of the recommended format for the shader code.
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_shader_source (CoglHandle  shader,
                    const char *source);

/**
 * cogl_shader_compile:
 * @handle: #CoglHandle for a shader.
 *
 * Compiles the shader, no return value, but the shader is now ready
 * for linking into a program. Note that calling this function is
 * optional. If it is not called then the shader will be automatically
 * compiled when it is linked.
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_shader_compile (CoglHandle handle);

/**
 * cogl_shader_get_info_log:
 * @handle: #CoglHandle for a shader.
 *
 * Retrieves the information log for a coglobject, can be used in conjunction
 * with cogl_shader_get_parameteriv() to retrieve the compiler warnings/error
 * messages that caused a shader to not compile correctly, mainly useful for
 * debugging purposes.
 *
 * Return value: a newly allocated string containing the info log. Use
 *   g_free() to free it
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
char *
cogl_shader_get_info_log (CoglHandle handle);

/**
 * cogl_shader_get_type:
 * @handle: #CoglHandle for a shader.
 *
 * Retrieves the type of a shader #CoglHandle
 *
 * Return value: %COGL_SHADER_TYPE_VERTEX if the shader is a vertex processor
 *          or %COGL_SHADER_TYPE_FRAGMENT if the shader is a frament processor
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglShaderType
cogl_shader_get_type (CoglHandle handle);

/**
 * cogl_shader_is_compiled:
 * @handle: #CoglHandle for a shader.
 *
 * Retrieves whether a shader #CoglHandle has been compiled
 *
 * Return value: %TRUE if the shader object has sucessfully be compiled
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglBool
cogl_shader_is_compiled (CoglHandle handle);

/**
 * cogl_create_program:
 *
 * Create a new cogl program object that can be used to replace parts of the GL
 * rendering pipeline with custom code.
 *
 * Returns: a new cogl program.
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglHandle
cogl_create_program (void);

/**
 * cogl_program_ref:
 * @handle: A #CoglHandle to a program.
 *
 * Add an extra reference to a program.
 *
 * Deprecated: 1.0: Please use cogl_object_ref() instead.
 *
 * Returns: @handle
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglHandle
cogl_program_ref (CoglHandle handle);

/**
 * cogl_program_unref:
 * @handle: A #CoglHandle to a program.
 *
 * Removes a reference to a program. If it was the last reference the
 * program object will be destroyed.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_unref (CoglHandle handle);

/**
 * cogl_is_program:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing program object.
 *
 * Returns: %TRUE if the handle references a program,
 *   %FALSE otherwise
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglBool
cogl_is_program (CoglHandle handle);

/**
 * cogl_program_attach_shader:
 * @program_handle: a #CoglHandle for a shdaer program.
 * @shader_handle: a #CoglHandle for a vertex of fragment shader.
 *
 * Attaches a shader to a program object. A program can have multiple
 * vertex or fragment shaders but only one of them may provide a
 * main() function. It is allowed to use a program with only a vertex
 * shader or only a fragment shader.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_attach_shader (CoglHandle program_handle,
                            CoglHandle shader_handle);

/**
 * cogl_program_link:
 * @handle: a #CoglHandle for a shader program.
 *
 * Links a program making it ready for use. Note that calling this
 * function is optional. If it is not called the program will
 * automatically be linked the first time it is used.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_link (CoglHandle handle);

/**
 * cogl_program_use:
 * @handle: a #CoglHandle for a shader program or %COGL_INVALID_HANDLE.
 *
 * Activate a specific shader program replacing that part of the GL
 * rendering pipeline, if passed in %COGL_INVALID_HANDLE the default
 * behavior of GL is reinstated.
 *
 * This function affects the global state of the current Cogl
 * context. It is much more efficient to attach the shader to a
 * specific material used for rendering instead by calling
 * cogl_material_set_user_program().
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_use (CoglHandle handle);

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
 * Deprecated: 1.16: Use #CoglSnippet api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
int
cogl_program_get_uniform_location (CoglHandle  handle,
                                   const char *uniform_name);

/**
 * cogl_program_set_uniform_1f:
 * @program: A #CoglHandle for a linked program
 * @uniform_location: the uniform location retrieved from
 *    cogl_program_get_uniform_location().
 * @value: the new value of the uniform.
 *
 * Changes the value of a floating point uniform for the given linked
 * @program.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use #CoglSnippet api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_set_uniform_1f (CoglHandle program,
                             int uniform_location,
                             float value);

/**
 * cogl_program_set_uniform_1i:
 * @program: A #CoglHandle for a linked program
 * @uniform_location: the uniform location retrieved from
 *    cogl_program_get_uniform_location().
 * @value: the new value of the uniform.
 *
 * Changes the value of an integer uniform for the given linked
 * @program.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use #CoglSnippet api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_set_uniform_1i (CoglHandle program,
                             int uniform_location,
                             int value);

/**
 * cogl_program_set_uniform_float:
 * @program: A #CoglHandle for a linked program
 * @uniform_location: the uniform location retrieved from
 *    cogl_program_get_uniform_location().
 * @n_components: The number of components for the uniform. For
 * example with glsl you'd use 3 for a vec3 or 4 for a vec4.
 * @count: For uniform arrays this is the array length otherwise just
 * pass 1
 * @value: (array length=count): the new value of the uniform[s].
 *
 * Changes the value of a float vector uniform, or uniform array for
 * the given linked @program.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use #CoglSnippet api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_set_uniform_float (CoglHandle program,
                                int uniform_location,
                                int n_components,
                                int count,
                                const float *value);

/**
 * cogl_program_set_uniform_int:
 * @program: A #CoglHandle for a linked program
 * @uniform_location: the uniform location retrieved from
 *    cogl_program_get_uniform_location().
 * @n_components: The number of components for the uniform. For
 * example with glsl you'd use 3 for a vec3 or 4 for a vec4.
 * @count: For uniform arrays this is the array length otherwise just
 * pass 1
 * @value: (array length=count): the new value of the uniform[s].
 *
 * Changes the value of a int vector uniform, or uniform array for
 * the given linked @program.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use #CoglSnippet api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_set_uniform_int (CoglHandle program,
                              int uniform_location,
                              int n_components,
                              int count,
                              const int *value);

/**
 * cogl_program_set_uniform_matrix:
 * @program: A #CoglHandle for a linked program
 * @uniform_location: the uniform location retrieved from
 *    cogl_program_get_uniform_location().
 * @dimensions: The dimensions of the matrix. So for for example pass
 *    2 for a 2x2 matrix or 3 for 3x3.
 * @count: For uniform arrays this is the array length otherwise just
 * pass 1
 * @transpose: Whether to transpose the matrix when setting the uniform.
 * @value: (array length=count): the new value of the uniform.
 *
 * Changes the value of a matrix uniform, or uniform array in the
 * given linked @program.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use #CoglSnippet api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_set_uniform_matrix (CoglHandle program,
                                 int uniform_location,
                                 int dimensions,
                                 int count,
                                 CoglBool transpose,
                                 const float *value);

/**
 * cogl_program_uniform_1f:
 * @uniform_no: the uniform to set.
 * @value: the new value of the uniform.
 *
 * Changes the value of a floating point uniform in the currently
 * used (see cogl_program_use()) shader program.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_uniform_1f (int   uniform_no,
                         float value);

/**
 * cogl_program_uniform_1i:
 * @uniform_no: the uniform to set.
 * @value: the new value of the uniform.
 *
 * Changes the value of an integer uniform in the currently
 * used (see cogl_program_use()) shader program.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_uniform_1i (int uniform_no,
                         int value);

/**
 * cogl_program_uniform_float:
 * @uniform_no: the uniform to set.
 * @size: Size of float vector.
 * @count: Size of array of uniforms.
 * @value: (array length=count): the new value of the uniform.
 *
 * Changes the value of a float vector uniform, or uniform array in the
 * currently used (see cogl_program_use()) shader program.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_uniform_float (int            uniform_no,
                            int            size,
                            int            count,
                            const float   *value);

/**
 * cogl_program_uniform_int:
 * @uniform_no: the uniform to set.
 * @size: Size of int vector.
 * @count: Size of array of uniforms.
 * @value: (array length=count): the new value of the uniform.
 *
 * Changes the value of a int vector uniform, or uniform array in the
 * currently used (see cogl_program_use()) shader program.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_uniform_int (int        uniform_no,
                          int        size,
                          int        count,
                          const int *value);

/**
 * cogl_program_uniform_matrix:
 * @uniform_no: the uniform to set.
 * @size: Size of matrix.
 * @count: Size of array of uniforms.
 * @transpose: Whether to transpose the matrix when setting the uniform.
 * @value: (array length=count): the new value of the uniform.
 *
 * Changes the value of a matrix uniform, or uniform array in the
 * currently used (see cogl_program_use()) shader program. The @size
 * parameter is used to determine the square size of the matrix.
 *
 * Deprecated: 1.16: Use #CoglSnippet api
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_program_uniform_matrix (int          uniform_no,
                             int          size,
                             int          count,
                             CoglBool     transpose,
                             const float *value);

COGL_END_DECLS

#endif /* __COGL_SHADER_H__ */
