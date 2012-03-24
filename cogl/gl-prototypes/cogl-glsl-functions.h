/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009, 2011 Intel Corporation.
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

/* The function names in OpenGL 2.0 are different so we can't easily
   just check for GL 2.0 */
COGL_EXT_BEGIN (shaders_glsl, 2, 0,
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (GLuint, glCreateProgram,
                   (void))
COGL_EXT_FUNCTION (GLuint, glCreateShader,
                   (GLenum                shaderType))
COGL_EXT_FUNCTION (void, glShaderSource,
                   (GLuint                shader,
                    GLsizei               count,
                    const char   * const *string,
                    const GLint          *length))
COGL_EXT_FUNCTION (void, glCompileShader,
                   (GLuint                shader))
COGL_EXT_FUNCTION (void, glDeleteShader,
                   (GLuint                shader))
COGL_EXT_FUNCTION (void, glAttachShader,
                   (GLuint                program,
                    GLuint                shader))
COGL_EXT_FUNCTION (void, glLinkProgram,
                   (GLuint                program))
COGL_EXT_FUNCTION (void, glUseProgram,
                   (GLuint                program))
COGL_EXT_FUNCTION (GLint, glGetUniformLocation,
                   (GLuint                program,
                    const char           *name))
COGL_EXT_FUNCTION (void, glDeleteProgram,
                   (GLuint                program))
COGL_EXT_FUNCTION (void, glGetShaderInfoLog,
                   (GLuint                shader,
                    GLsizei               maxLength,
                    GLsizei              *length,
                    char                 *infoLog))
COGL_EXT_FUNCTION (void, glGetShaderiv,
                   (GLuint                shader,
                    GLenum                pname,
                    GLint                *params))

COGL_EXT_FUNCTION (void, glVertexAttribPointer,
                   (GLuint		 index,
                    GLint		 size,
                    GLenum		 type,
                    GLboolean		 normalized,
                    GLsizei		 stride,
                    const GLvoid		*pointer))
COGL_EXT_FUNCTION (void, glEnableVertexAttribArray,
                   (GLuint		 index))
COGL_EXT_FUNCTION (void, glDisableVertexAttribArray,
                   (GLuint		 index))

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

COGL_EXT_FUNCTION (void, glGetProgramiv,
                   (GLuint                program,
                    GLenum                pname,
                    GLint                *params))

COGL_EXT_FUNCTION (void, glGetProgramInfoLog,
                   (GLuint                program,
                    GLsizei               bufSize,
                    GLsizei              *length,
                    char                 *infoLog))

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
COGL_EXT_FUNCTION (void, glGetActiveUniform,
                   (GLuint program,
                    GLuint index,
                    GLsizei bufsize,
                    GLsizei* length,
                    GLint* size,
                    GLenum* type,
                    GLchar* name))
COGL_EXT_FUNCTION (void, glDetachShader,
                   (GLuint program, GLuint shader))
COGL_EXT_FUNCTION (void, glGetAttachedShaders,
                   (GLuint program,
                    GLsizei maxcount,
                    GLsizei* count,
                    GLuint* shaders))
COGL_EXT_FUNCTION (void, glGetShaderSource,
                   (GLuint shader,
                    GLsizei bufsize,
                    GLsizei* length,
                    GLchar* source))

COGL_EXT_FUNCTION (GLboolean, glIsShader,
                   (GLuint shader))
COGL_EXT_FUNCTION (GLboolean, glIsProgram,
                   (GLuint program))

COGL_EXT_FUNCTION (void, glValidateProgram, (GLuint program))

COGL_EXT_END ()
