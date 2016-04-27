/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009, 2011 Intel Corporation.
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
 */

/* This is included multiple times with different definitions for
 * these macros. The macros are given the following arguments:
 *
 * COGL_EXT_BEGIN:
 *
 * @name: a unique symbol name for this feature
 *
 * @min_gl_major: the major part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 * @min_gl_minor: the minor part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 *
 * @gles_availability: flags to specify which versions of GLES the
 * functions are available in. Should be a combination of
 * COGL_EXT_IN_GLES and COGL_EXT_IN_GLES2.
 *
 * @extension_suffixes: A zero-separated list of suffixes in a
 * string. These are appended to the extension name to get a complete
 * extension name to try. The suffix is also appended to all of the
 * function names. The suffix can optionally include a ':' to specify
 * an alternate suffix for the function names.
 *
 * @extension_names: A list of extension names to try. If any of these
 * extensions match then it will be used.
 */

/* This lists functions that are unique to GL 2.0 or GLES 2.0 and are
 * not in the old GLSL extensions */
COGL_EXT_BEGIN (shaders_glsl_2_only, 2, 0,
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (GLuint, glCreateProgram,
                   (void))
COGL_EXT_FUNCTION (GLuint, glCreateShader,
                   (GLenum                shaderType))
COGL_EXT_FUNCTION (void, glDeleteShader,
                   (GLuint                shader))
COGL_EXT_FUNCTION (void, glAttachShader,
                   (GLuint                program,
                    GLuint                shader))
COGL_EXT_FUNCTION (void, glUseProgram,
                   (GLuint                program))
COGL_EXT_FUNCTION (void, glDeleteProgram,
                   (GLuint                program))
COGL_EXT_FUNCTION (void, glGetShaderInfoLog,
                   (GLuint                shader,
                    GLsizei               maxLength,
                    GLsizei              *length,
                    char                 *infoLog))
COGL_EXT_FUNCTION (void, glGetProgramInfoLog,
                   (GLuint                program,
                    GLsizei               bufSize,
                    GLsizei              *length,
                    char                 *infoLog))
COGL_EXT_FUNCTION (void, glGetShaderiv,
                   (GLuint                shader,
                    GLenum                pname,
                    GLint                *params))
COGL_EXT_FUNCTION (void, glGetProgramiv,
                   (GLuint                program,
                    GLenum                pname,
                    GLint                *params))
COGL_EXT_FUNCTION (void, glDetachShader,
                   (GLuint program, GLuint shader))
COGL_EXT_FUNCTION (void, glGetAttachedShaders,
                   (GLuint program,
                    GLsizei maxcount,
                    GLsizei* count,
                    GLuint* shaders))
COGL_EXT_FUNCTION (GLboolean, glIsShader,
                   (GLuint shader))
COGL_EXT_FUNCTION (GLboolean, glIsProgram,
                   (GLuint program))
COGL_EXT_END ()

/* These functions are provided by GL_ARB_shader_objects or are in GL
 * 2.0 core */
COGL_EXT_BEGIN (shader_objects_or_gl2, 2, 0,
                COGL_EXT_IN_GLES2,
                "ARB\0",
                "shader_objects\0")
COGL_EXT_FUNCTION (void, glShaderSource,
                   (GLuint                shader,
                    GLsizei               count,
                    const char   * const *string,
                    const GLint          *length))
COGL_EXT_FUNCTION (void, glCompileShader,
                   (GLuint                shader))
COGL_EXT_FUNCTION (void, glLinkProgram,
                   (GLuint                program))
COGL_EXT_FUNCTION (GLint, glGetUniformLocation,
                   (GLuint                program,
                    const char           *name))
COGL_EXT_FUNCTION (void, glUniform1f,
                   (GLint                 location,
                    GLfloat               v0))
COGL_EXT_FUNCTION (void, glUniform2f,
                   (GLint                 location,
                    GLfloat               v0,
                    GLfloat               v1))
COGL_EXT_FUNCTION (void, glUniform3f,
                   (GLint                 location,
                    GLfloat               v0,
                    GLfloat               v1,
                    GLfloat               v2))
COGL_EXT_FUNCTION (void, glUniform4f,
                   (GLint                 location,
                    GLfloat               v0,
                    GLfloat               v1,
                    GLfloat               v2,
                    GLfloat               v3))
COGL_EXT_FUNCTION (void, glUniform1fv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLfloat *       value))
COGL_EXT_FUNCTION (void, glUniform2fv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLfloat *       value))
COGL_EXT_FUNCTION (void, glUniform3fv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLfloat *       value))
COGL_EXT_FUNCTION (void, glUniform4fv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLfloat *       value))
COGL_EXT_FUNCTION (void, glUniform1i,
                   (GLint                 location,
                    GLint                 v0))
COGL_EXT_FUNCTION (void, glUniform2i,
                   (GLint                 location,
                    GLint                 v0,
                    GLint                 v1))
COGL_EXT_FUNCTION (void, glUniform3i,
                   (GLint                 location,
                    GLint                 v0,
                    GLint                 v1,
                    GLint                 v2))
COGL_EXT_FUNCTION (void, glUniform4i,
                   (GLint                 location,
                    GLint                 v0,
                    GLint                 v1,
                    GLint                 v2,
                    GLint                 v3))
COGL_EXT_FUNCTION (void, glUniform1iv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLint *         value))
COGL_EXT_FUNCTION (void, glUniform2iv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLint *         value))
COGL_EXT_FUNCTION (void, glUniform3iv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLint *         value))
COGL_EXT_FUNCTION (void, glUniform4iv,
                   (GLint                 location,
                    GLsizei               count,
                    const GLint *         value))
COGL_EXT_FUNCTION (void, glUniformMatrix2fv,
                   (GLint                 location,
                    GLsizei               count,
                    GLboolean             transpose,
                    const GLfloat        *value))
COGL_EXT_FUNCTION (void, glUniformMatrix3fv,
                   (GLint                 location,
                    GLsizei               count,
                    GLboolean             transpose,
                    const GLfloat        *value))
COGL_EXT_FUNCTION (void, glUniformMatrix4fv,
                   (GLint                 location,
                    GLsizei               count,
                    GLboolean             transpose,
                    const GLfloat        *value))

COGL_EXT_FUNCTION (void, glGetUniformfv,
                   (GLuint                program,
                    GLint                 location,
                    GLfloat              *params))
COGL_EXT_FUNCTION (void, glGetUniformiv,
                   (GLuint                program,
                    GLint                 location,
                    GLint                *params))
COGL_EXT_FUNCTION (void, glGetActiveUniform,
                   (GLuint program,
                    GLuint index,
                    GLsizei bufsize,
                    GLsizei* length,
                    GLint* size,
                    GLenum* type,
                    GLchar* name))
COGL_EXT_FUNCTION (void, glGetShaderSource,
                   (GLuint shader,
                    GLsizei bufsize,
                    GLsizei* length,
                    GLchar* source))
COGL_EXT_FUNCTION (void, glValidateProgram, (GLuint program))
COGL_EXT_END ()

/* These functions are provided by GL_ARB_vertex_shader or are in GL
 * 2.0 core */
COGL_EXT_BEGIN (vertex_shaders, 2, 0,
                COGL_EXT_IN_GLES2,
                "ARB\0",
                "vertex_shader\0")
COGL_EXT_FUNCTION (void, glVertexAttribPointer,
                   (GLuint		 index,
                    GLint		 size,
                    GLenum		 type,
                    GLboolean		 normalized,
                    GLsizei		 stride,
                    const GLvoid        *pointer))
COGL_EXT_FUNCTION (void, glEnableVertexAttribArray,
                   (GLuint		 index))
COGL_EXT_FUNCTION (void, glDisableVertexAttribArray,
                   (GLuint		 index))
COGL_EXT_FUNCTION (void, glVertexAttrib1f, (GLuint indx, GLfloat x))
COGL_EXT_FUNCTION (void, glVertexAttrib1fv,
                   (GLuint indx, const GLfloat* values))
COGL_EXT_FUNCTION (void, glVertexAttrib2f, (GLuint indx, GLfloat x, GLfloat y))
COGL_EXT_FUNCTION (void, glVertexAttrib2fv,
                   (GLuint indx, const GLfloat* values))
COGL_EXT_FUNCTION (void, glVertexAttrib3f,
                   (GLuint indx, GLfloat x, GLfloat y, GLfloat z))
COGL_EXT_FUNCTION (void, glVertexAttrib3fv,
                   (GLuint indx, const GLfloat* values))
COGL_EXT_FUNCTION (void, glVertexAttrib4f,
                   (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w))
COGL_EXT_FUNCTION (void, glVertexAttrib4fv,
                   (GLuint indx, const GLfloat* values))
COGL_EXT_FUNCTION (void, glGetVertexAttribfv,
                   (GLuint index, GLenum pname, GLfloat* params))
COGL_EXT_FUNCTION (void, glGetVertexAttribiv,
                   (GLuint index, GLenum pname, GLint* params))
COGL_EXT_FUNCTION (void, glGetVertexAttribPointerv,
                   (GLuint index, GLenum pname, GLvoid** pointer))
COGL_EXT_FUNCTION (GLint, glGetAttribLocation,
                   (GLuint program, const char *name))
COGL_EXT_FUNCTION (void, glBindAttribLocation,
                   (GLuint program,
                    GLuint index,
                    const GLchar* name))
COGL_EXT_FUNCTION (void, glGetActiveAttrib,
                   (GLuint program,
                    GLuint index,
                    GLsizei bufsize,
                    GLsizei* length,
                    GLint* size,
                    GLenum* type,
                    GLchar* name))
COGL_EXT_END ()
