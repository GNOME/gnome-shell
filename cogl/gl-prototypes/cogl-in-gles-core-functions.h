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

COGL_EXT_BEGIN (only_in_both_gles,
                255, 255,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glDepthRangef,
                   (GLfloat near_val, GLfloat far_val))
COGL_EXT_FUNCTION (void, glClearDepthf,
                   (GLclampf depth))
COGL_EXT_END ()

COGL_EXT_BEGIN (only_in_both_gles_and_gl_1_3,
                1, 3,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glCompressedTexImage2D,
                   (GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLsizei imageSize,
                    const GLvoid* data))
COGL_EXT_FUNCTION (void, glCompressedTexSubImage2D,
                   (GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLsizei imageSize,
                    const GLvoid* data))
COGL_EXT_FUNCTION (void, glSampleCoverage,
                   (GLclampf value, GLboolean invert))
COGL_EXT_END ()

COGL_EXT_BEGIN (only_in_both_gles_and_gl_1_5,
                1, 5,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glGetBufferParameteriv,
                   (GLenum target, GLenum pname, GLint* params))
COGL_EXT_END ()

COGL_EXT_BEGIN (vbos, 1, 5,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "ARB\0",
                "vertex_buffer_object\0")
COGL_EXT_FUNCTION (void, glGenBuffers,
                   (GLsizei		 n,
                    GLuint		*buffers))
COGL_EXT_FUNCTION (void, glBindBuffer,
                   (GLenum		 target,
                    GLuint		 buffer))
COGL_EXT_FUNCTION (void, glBufferData,
                   (GLenum		 target,
                    GLsizeiptr		 size,
                    const GLvoid		*data,
                    GLenum		 usage))
COGL_EXT_FUNCTION (void, glBufferSubData,
                   (GLenum		 target,
                    GLintptr		 offset,
                    GLsizeiptr		 size,
                    const GLvoid		*data))
COGL_EXT_FUNCTION (void, glDeleteBuffers,
                   (GLsizei		 n,
                    const GLuint		*buffers))
COGL_EXT_FUNCTION (GLboolean, glIsBuffer,
                   (GLuint               buffer))
COGL_EXT_END ()

/* Available in GL 1.3, the multitexture extension or GLES. These are
   required */
COGL_EXT_BEGIN (multitexture_part0, 1, 3,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "ARB\0",
                "multitexture\0")
COGL_EXT_FUNCTION (void, glActiveTexture,
                   (GLenum                texture))
COGL_EXT_END ()

