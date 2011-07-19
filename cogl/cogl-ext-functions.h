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

/* These are the core GL functions which we assume will always be
   available */
COGL_EXT_BEGIN (core,
                0, 0,
                COGL_EXT_IN_GLES | COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glBindTexture,
                   (GLenum target, GLuint texture))
COGL_EXT_FUNCTION (void, glBlendFunc,
                   (GLenum sfactor, GLenum dfactor))
COGL_EXT_FUNCTION (void, glClear,
                   (GLbitfield mask))
COGL_EXT_FUNCTION (void, glClearColor,
                   (GLclampf red,
                    GLclampf green,
                    GLclampf blue,
                    GLclampf alpha))
COGL_EXT_FUNCTION (void, glClearStencil,
                   (GLint s))
COGL_EXT_FUNCTION (void, glColorMask,
                   (GLboolean red,
                    GLboolean green,
                    GLboolean blue,
                    GLboolean alpha))
COGL_EXT_FUNCTION (void, glCopyTexSubImage2D,
                   (GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height))
COGL_EXT_FUNCTION (void, glDeleteTextures,
                   (GLsizei n, const GLuint* textures))
COGL_EXT_FUNCTION (void, glDepthFunc,
                   (GLenum func))
COGL_EXT_FUNCTION (void, glDepthMask,
                   (GLboolean flag))
COGL_EXT_FUNCTION (void, glDisable,
                   (GLenum cap))
COGL_EXT_FUNCTION (void, glDrawArrays,
                   (GLenum mode, GLint first, GLsizei count))
COGL_EXT_FUNCTION (void, glDrawElements,
                   (GLenum mode,
                    GLsizei count,
                    GLenum type,
                    const GLvoid* indices))
COGL_EXT_FUNCTION (void, glEnable,
                   (GLenum cap))
COGL_EXT_FUNCTION (void, glFinish,
                   (void))
COGL_EXT_FUNCTION (void, glFlush,
                   (void))
COGL_EXT_FUNCTION (void, glFrontFace,
                   (GLenum mode))
COGL_EXT_FUNCTION (void, glGenTextures,
                   (GLsizei n, GLuint* textures))
COGL_EXT_FUNCTION (GLenum, glGetError,
                   (void))
COGL_EXT_FUNCTION (void, glGetIntegerv,
                   (GLenum pname, GLint* params))
COGL_EXT_FUNCTION (const GLubyte*, glGetString,
                   (GLenum name))
COGL_EXT_FUNCTION (void, glHint,
                   (GLenum target, GLenum mode))
COGL_EXT_FUNCTION (GLboolean, glIsTexture,
                   (GLuint texture))
COGL_EXT_FUNCTION (void, glPixelStorei,
                   (GLenum pname, GLint param))
COGL_EXT_FUNCTION (void, glReadPixels,
                   (GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    GLvoid* pixels))
COGL_EXT_FUNCTION (void, glScissor,
                   (GLint x, GLint y, GLsizei width, GLsizei height))
COGL_EXT_FUNCTION (void, glStencilFunc,
                   (GLenum func, GLint ref, GLuint mask))
COGL_EXT_FUNCTION (void, glStencilMask,
                   (GLuint mask))
COGL_EXT_FUNCTION (void, glStencilOp,
                   (GLenum fail, GLenum zfail, GLenum zpass))
COGL_EXT_FUNCTION (void, glTexImage2D,
                   (GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const GLvoid* pixels))
COGL_EXT_FUNCTION (void, glTexParameterfv,
                   (GLenum target, GLenum pname, const GLfloat* params))
COGL_EXT_FUNCTION (void, glTexParameteri,
                   (GLenum target, GLenum pname, GLint param))
COGL_EXT_FUNCTION (void, glTexSubImage2D,
                   (GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    const GLvoid* pixels))
COGL_EXT_FUNCTION (void, glViewport,
                   (GLint x, GLint y, GLsizei width, GLsizei height))
COGL_EXT_END ()

/* These are the core GL functions which are available when the API
   supports fixed-function (ie, GL and GLES1.1) */
COGL_EXT_BEGIN (fixed_function_core,
                0, 0,
                COGL_EXT_IN_GLES,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glAlphaFunc,
                   (GLenum func, GLclampf ref))
COGL_EXT_FUNCTION (void, glFogf,
                   (GLenum pname, GLfloat param))
COGL_EXT_FUNCTION (void, glFogfv,
                   (GLenum pname, const GLfloat *params))
COGL_EXT_FUNCTION (void, glLoadMatrixf,
                   (const GLfloat *m))
COGL_EXT_FUNCTION (void, glMaterialfv,
                   (GLenum face, GLenum pname, const GLfloat *params))
COGL_EXT_FUNCTION (void, glPointSize,
                   (GLfloat size))
COGL_EXT_FUNCTION (void, glTexEnvfv,
                   (GLenum target, GLenum pname, const GLfloat *params))
COGL_EXT_FUNCTION (void, glColor4ub,
                   (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha))
COGL_EXT_FUNCTION (void, glColorPointer,
                   (GLint size,
                    GLenum type,
                    GLsizei stride,
                    const GLvoid *pointer))
COGL_EXT_FUNCTION (void, glDisableClientState,
                   (GLenum array))
COGL_EXT_FUNCTION (void, glEnableClientState,
                   (GLenum array))
COGL_EXT_FUNCTION (void, glLoadIdentity,
                   (void))
COGL_EXT_FUNCTION (void, glMatrixMode,
                   (GLenum mode))
COGL_EXT_FUNCTION (void, glNormalPointer,
                   (GLenum type, GLsizei stride, const GLvoid *pointer))
COGL_EXT_FUNCTION (void, glTexCoordPointer,
                   (GLint size,
                    GLenum type,
                    GLsizei stride,
                    const GLvoid *pointer))
COGL_EXT_FUNCTION (void, glTexEnvi,
                   (GLenum target,
                    GLenum pname,
                    GLint param))
COGL_EXT_FUNCTION (void, glVertexPointer,
                   (GLint size,
                    GLenum type,
                    GLsizei stride,
                    const GLvoid *pointer))
COGL_EXT_END ()

/* These are the core GL functions which are only available in big
   GL */
COGL_EXT_BEGIN (only_in_big_gl,
                0, 0,
                0, /* not in GLES */
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glGetTexLevelParameteriv,
                   (GLenum target, GLint level,
                    GLenum pname, GLint *params))
COGL_EXT_FUNCTION (void, glGetTexImage,
                   (GLenum target, GLint level,
                    GLenum format, GLenum type,
                    GLvoid *pixels))
COGL_EXT_FUNCTION (void, glClipPlane,
                   (GLenum plane, const double *equation))
COGL_EXT_FUNCTION (void, glDepthRange,
                   (double near_val, double far_val))
COGL_EXT_FUNCTION (void, glDrawBuffer,
                   (GLenum mode))
COGL_EXT_END ()

/* These functions are only available in GLES and are used as
   replacements for some GL equivalents that only accept double
   arguments */
COGL_EXT_BEGIN (only_in_gles1,
                255, 255,
                COGL_EXT_IN_GLES,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glClipPlanef,
                   (GLenum plane, const GLfloat *equation))
COGL_EXT_END ()
COGL_EXT_BEGIN (only_in_both_gles,
                255, 255,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glDepthRangef,
                   (GLfloat near_val, GLfloat far_val))
COGL_EXT_END ()

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

COGL_EXT_FUNCTION (void, glVertexAttrib4f,
                   (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w))

COGL_EXT_FUNCTION (GLint, glGetAttribLocation,
                   (GLuint program, const GLchar *name))

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
COGL_EXT_BEGIN (multitexture_part0, 1, 3,
                COGL_EXT_IN_GLES |
                COGL_EXT_IN_GLES2,
                "ARB\0",
                "multitexture\0")
COGL_EXT_FUNCTION (void, glActiveTexture,
                   (GLenum                texture))
COGL_EXT_END ()
COGL_EXT_BEGIN (multitexture_part1, 1, 3,
                COGL_EXT_IN_GLES,
                "ARB\0",
                "multitexture\0")
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
