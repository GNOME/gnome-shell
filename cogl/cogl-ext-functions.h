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
COGL_EXT_BEGIN (offscreen,
                255, 255,
                COGL_EXT_IN_GLES2,
                /* for some reason the ARB version of this
                   extension doesn't have an ARB suffix for the
                   functions */
                "ARB:\0EXT\0OES\0",
                "framebuffer_object\0")
COGL_EXT_FUNCTION (void, glGenRenderbuffers,
                   (GLsizei               n,
                    GLuint               *renderbuffers))
COGL_EXT_FUNCTION (void, glDeleteRenderbuffers,
                   (GLsizei               n,
                    const GLuint         *renderbuffers))
COGL_EXT_FUNCTION (void, glBindRenderbuffer,
                   (GLenum                target,
                    GLuint                renderbuffer))
COGL_EXT_FUNCTION (void, glRenderbufferStorage,
                   (GLenum                target,
                    GLenum                internalformat,
                    GLsizei               width,
                    GLsizei               height))
COGL_EXT_FUNCTION (void, glGenFramebuffers,
                   (GLsizei               n,
                    GLuint               *framebuffers))
COGL_EXT_FUNCTION (void, glBindFramebuffer,
                   (GLenum                target,
                    GLuint                framebuffer))
COGL_EXT_FUNCTION (void, glFramebufferTexture2D,
                   (GLenum                target,
                    GLenum                attachment,
                    GLenum                textarget,
                    GLuint                texture,
                    GLint                 level))
COGL_EXT_FUNCTION (void, glFramebufferRenderbuffer,
                   (GLenum                target,
                    GLenum                attachment,
                    GLenum                renderbuffertarget,
                    GLuint                renderbuffer))
COGL_EXT_FUNCTION (GLenum, glCheckFramebufferStatus,
                   (GLenum                target))
COGL_EXT_FUNCTION (void, glDeleteFramebuffers,
                   (GLsizei               n,
                    const                 GLuint *framebuffers))
COGL_EXT_FUNCTION (void, glGenerateMipmap,
                   (GLenum                target))
COGL_EXT_FUNCTION (void, glGetFramebufferAttachmentParameteriv,
                   (GLenum                target,
                    GLenum                attachment,
                    GLenum                pname,
                    GLint                *params))
COGL_EXT_END ()

COGL_EXT_BEGIN (offscreen_blit, 255, 255,
                0, /* not in either GLES */
                "EXT\0ANGLE\0",
                "framebuffer_blit\0")
COGL_EXT_FUNCTION (void, glBlitFramebuffer,
                   (GLint                 srcX0,
                    GLint                 srcY0,
                    GLint                 srcX1,
                    GLint                 srcY1,
                    GLint                 dstX0,
                    GLint                 dstY0,
                    GLint                 dstX1,
                    GLint                 dstY1,
                    GLbitfield            mask,
                    GLenum                filter))
COGL_EXT_END ()

COGL_EXT_BEGIN (offscreen_multisample, 255, 255,
                0, /* not in either GLES */
                "EXT\0",
                "framebuffer_multisample\0")
COGL_EXT_FUNCTION (void, glRenderbufferStorageMultisample,
                   (GLenum                target,
                    GLsizei               samples,
                    GLenum                internalformat,
                    GLsizei               width,
                    GLsizei               height))
COGL_EXT_END ()

/* ARB_fragment_program */
COGL_EXT_BEGIN (arbfp, 255, 255,
                0, /* not in either GLES */
                "ARB\0",
                "fragment_program\0")
COGL_EXT_FUNCTION (void, glGenPrograms,
                   (GLsizei               n,
                    GLuint               *programs))
COGL_EXT_FUNCTION (void, glDeletePrograms,
                   (GLsizei               n,
                    GLuint               *programs))
COGL_EXT_FUNCTION (void, glBindProgram,
                   (GLenum                target,
                    GLuint                program))
COGL_EXT_FUNCTION (void, glProgramString,
                   (GLenum                target,
                    GLenum                format,
                    GLsizei               len,
                    const void           *program))
COGL_EXT_FUNCTION (void, glProgramLocalParameter4fv,
                   (GLenum                target,
                    GLuint                index,
                    GLfloat              *params))
COGL_EXT_END ()

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
                    const GLchar        **string,
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
                    const GLchar         *name))
COGL_EXT_FUNCTION (void, glDeleteProgram,
                   (GLuint                program))
COGL_EXT_FUNCTION (void, glGetShaderInfoLog,
                   (GLuint                shader,
                    GLsizei               maxLength,
                    GLsizei              *length,
                    GLchar               *infoLog))
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

COGL_EXT_FUNCTION (void, glGetProgramiv,
                   (GLuint                program,
                    GLenum                pname,
                    GLint                *params))

COGL_EXT_FUNCTION (void, glGetProgramInfoLog,
                   (GLuint                program,
                    GLsizei               bufSize,
                    GLsizei              *length,
                    GLchar               *infoLog))

COGL_EXT_END ()

COGL_EXT_BEGIN (vbos, 1, 5,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "ARB\0",
                "vertex_buffer_object\0")
COGL_EXT_FUNCTION (void, glGenBuffers,
                   (GLuint		 n,
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
COGL_EXT_END ()

/* GLES doesn't support mapping buffers in core so this has to be a
   separate check */
COGL_EXT_BEGIN (map_vbos, 1, 5,
                0, /* not in GLES core */
                "ARB\0OES\0",
                "vertex_buffer_object\0mapbuffer\0")
COGL_EXT_FUNCTION (void *, glMapBuffer,
                   (GLenum		 target,
                    GLenum		 access))
COGL_EXT_FUNCTION (GLboolean, glUnmapBuffer,
                   (GLenum		 target))
COGL_EXT_END ()

COGL_EXT_BEGIN (draw_range_elements, 1, 2,
                0, /* not in either GLES */
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glDrawRangeElements,
                   (GLenum                mode,
                    GLuint                start,
                    GLuint                end,
                    GLsizei               count,
                    GLenum                type,
                    const GLvoid         *indices))
COGL_EXT_END ()

COGL_EXT_BEGIN (blending, 1, 2,
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glBlendEquation,
                   (GLenum                mode))
COGL_EXT_FUNCTION (void, glBlendColor,
                   (GLclampf              red,
                    GLclampf              green,
                    GLclampf              blue,
                    GLclampf              alpha))
COGL_EXT_END ()

COGL_EXT_BEGIN (texture_3d, 1, 2,
                0, /* not in either GLES */
                "OES\0",
                "texture_3D\0")
COGL_EXT_FUNCTION (void, glTexImage3D,
                   (GLenum target, GLint level,
                    GLint internalFormat,
                    GLsizei width, GLsizei height,
                    GLsizei depth, GLint border,
                    GLenum format, GLenum type,
                    const GLvoid *pixels))
COGL_EXT_FUNCTION (void, glTexSubImage3D,
                   (GLenum target, GLint level,
                    GLint xoffset, GLint yoffset,
                    GLint zoffset, GLsizei width,
                    GLsizei height, GLsizei depth,
                    GLenum format,
                    GLenum type, const GLvoid *pixels))
COGL_EXT_END ()

/* Available in GL 1.3, the multitexture extension or GLES. These are
   required */
COGL_EXT_BEGIN (multitexture, 1, 3,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "ARB\0",
                "multitexture\0")
COGL_EXT_FUNCTION (void, glActiveTexture,
                   (GLenum                texture))
COGL_EXT_FUNCTION (void, glClientActiveTexture,
                   (GLenum                texture))
COGL_EXT_END ()

/* Optional, declared in 1.4 or GLES 1.2 */
COGL_EXT_BEGIN (blend_func_separate, 1, 4,
                COGL_EXT_IN_GLES2,
                "EXT\0",
                "blend_func_separate\0")
COGL_EXT_FUNCTION (void, glBlendFuncSeparate,
                   (GLenum                srcRGB,
                    GLenum                dstRGB,
                    GLenum                srcAlpha,
                    GLenum                dstAlpha))
COGL_EXT_END ()

/* Optional, declared in 2.0 */
COGL_EXT_BEGIN (blend_equation_separate, 2, 0,
                COGL_EXT_IN_GLES2,
                "EXT\0",
                "blend_equation_separate\0")
COGL_EXT_FUNCTION (void, glBlendEquationSeparate,
                   (GLenum                modeRGB,
                    GLenum                modeAlpha))
COGL_EXT_END ()

COGL_EXT_BEGIN (EGL_image, 255, 255,
                0, /* not in either GLES */
                "OES\0",
                "EGL_image\0")
COGL_EXT_FUNCTION (void, glEGLImageTargetTexture2D,
                   (GLenum           target,
                    GLeglImageOES    image))
COGL_EXT_FUNCTION (void, glEGLImageTargetRenderbufferStorage,
                   (GLenum           target,
                    GLeglImageOES    image))
COGL_EXT_END ()
