/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* This is included multiple times with different definitions for
   these macros */

COGL_FEATURE_BEGIN (offscreen, 255, 255,
                    /* for some reason the ARB version of this
                       extension doesn't have an ARB suffix for the
                       functions */
                    "ARB:\0EXT\0",
                    "framebuffer_object\0",
                    COGL_FEATURE_OFFSCREEN)
COGL_FEATURE_FUNCTION (void, glGenRenderbuffers,
                       (GLsizei               n,
                        GLuint               *renderbuffers))
COGL_FEATURE_FUNCTION (void, glDeleteRenderbuffers,
                       (GLsizei               n,
                        const GLuint         *renderbuffers))
COGL_FEATURE_FUNCTION (void, glBindRenderbuffer,
                       (GLenum                target,
                        GLuint                renderbuffer))
COGL_FEATURE_FUNCTION (void, glRenderbufferStorage,
                       (GLenum                target,
                        GLenum                internalformat,
                        GLsizei               width,
                        GLsizei               height))
COGL_FEATURE_FUNCTION (void, glGenFramebuffers,
                       (GLsizei               n,
                        GLuint               *framebuffers))
COGL_FEATURE_FUNCTION (void, glBindFramebuffer,
                       (GLenum                target,
                        GLuint                framebuffer))
COGL_FEATURE_FUNCTION (void, glFramebufferTexture2D,
                       (GLenum                target,
                        GLenum                attachment,
                        GLenum                textarget,
                        GLuint                texture,
                        GLint                 level))
COGL_FEATURE_FUNCTION (void, glFramebufferRenderbuffer,
                       (GLenum                target,
                        GLenum                attachment,
                        GLenum                renderbuffertarget,
                        GLuint                renderbuffer))
COGL_FEATURE_FUNCTION (GLenum, glCheckFramebufferStatus,
                       (GLenum                target))
COGL_FEATURE_FUNCTION (void, glDeleteFramebuffers,
                       (GLsizei               n,
                        const                 GLuint *framebuffers))
COGL_FEATURE_FUNCTION (void, glGenerateMipmap,
                       (GLenum                target))
COGL_FEATURE_END ()

COGL_FEATURE_BEGIN (offscreen_blit, 255, 255,
                    "EXT\0",
                    "framebuffer_blit\0",
                    COGL_FEATURE_OFFSCREEN_BLIT)
COGL_FEATURE_FUNCTION (void, glBlitFramebuffer,
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
COGL_FEATURE_END ()

COGL_FEATURE_BEGIN (offscreen_multisample, 255, 255,
                    "EXT\0",
                    "framebuffer_multisample\0",
                    COGL_FEATURE_OFFSCREEN_MULTISAMPLE)
COGL_FEATURE_FUNCTION (void, glRenderbufferStorageMultisample,
                       (GLenum                target,
                        GLsizei               samples,
                        GLenum                internalformat,
                        GLsizei               width,
                        GLsizei               height))
COGL_FEATURE_END ()
COGL_FEATURE_BEGIN (read_pixels_async, 2, 1,
                    "EXT\0",
                    "pixel_buffer_object\0",
                    COGL_FEATURE_PBOS)
COGL_FEATURE_END ()

/* The function names in OpenGL 2.0 are different so we can't easily
   just check for GL 2.0 */
COGL_FEATURE_BEGIN (shaders_glsl, 255, 255,
                    "ARB\0",
                    "shader_objects\0"
                    "vertex_shader\0"
                    "fragment_shader\0",
                    COGL_FEATURE_SHADERS_GLSL)
COGL_FEATURE_FUNCTION (GLhandleARB, glCreateProgramObject,
                       (void))
COGL_FEATURE_FUNCTION (GLhandleARB, glCreateShaderObject,
                       (GLenum                shaderType))
COGL_FEATURE_FUNCTION (void, glShaderSource,
                       (GLhandleARB           shaderObj,
                        GLsizei               count,
                        const GLcharARB*     *string,
                        const GLint          *length))
COGL_FEATURE_FUNCTION (void, glCompileShader,
                       (GLhandleARB           shaderObj))
COGL_FEATURE_FUNCTION (void, glAttachObject,
                       (GLhandleARB           containerObj,
                        GLhandleARB           obj))
COGL_FEATURE_FUNCTION (void, glLinkProgram,
                       (GLhandleARB           programObj))
COGL_FEATURE_FUNCTION (void, glUseProgramObject,
                       (GLhandleARB           programObj))
COGL_FEATURE_FUNCTION (GLint, glGetUniformLocation,
                       (GLhandleARB           programObj,
                        const GLcharARB      *name))
COGL_FEATURE_FUNCTION (void, glDeleteObject,
                       (GLhandleARB           obj))
COGL_FEATURE_FUNCTION (void, glGetInfoLog,
                       (GLhandleARB           obj,
                        GLsizei               maxLength,
                        GLsizei              *length,
                        GLcharARB            *infoLog))
COGL_FEATURE_FUNCTION (void, glGetObjectParameteriv,
                       (GLhandleARB           obj,
                        GLenum                pname,
                        GLint                *params))

COGL_FEATURE_FUNCTION (void, glVertexAttribPointer,
                       (GLuint		 index,
                        GLint		 size,
                        GLenum		 type,
                        GLboolean		 normalized,
                        GLsizei		 stride,
                        const GLvoid		*pointer))
COGL_FEATURE_FUNCTION (void, glEnableVertexAttribArray,
                       (GLuint		 index))
COGL_FEATURE_FUNCTION (void, glDisableVertexAttribArray,
                       (GLuint		 index))

COGL_FEATURE_FUNCTION (void, glUniform1f,
                       (GLint                 location,
                        GLfloat               v0))
COGL_FEATURE_FUNCTION (void, glUniform2f,
                       (GLint                 location,
                        GLfloat               v0,
                        GLfloat               v1))
COGL_FEATURE_FUNCTION (void, glUniform3f,
                       (GLint                 location,
                        GLfloat               v0,
                        GLfloat               v1,
                        GLfloat               v2))
COGL_FEATURE_FUNCTION (void, glUniform4f,
                       (GLint                 location,
                        GLfloat               v0,
                        GLfloat               v1,
                        GLfloat               v2,
                        GLfloat               v3))
COGL_FEATURE_FUNCTION (void, glUniform1fv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLfloat *       value))
COGL_FEATURE_FUNCTION (void, glUniform2fv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLfloat *       value))
COGL_FEATURE_FUNCTION (void, glUniform3fv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLfloat *       value))
COGL_FEATURE_FUNCTION (void, glUniform4fv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLfloat *       value))
COGL_FEATURE_FUNCTION (void, glUniform1i,
                       (GLint                 location,
                        GLint                 v0))
COGL_FEATURE_FUNCTION (void, glUniform2i,
                       (GLint                 location,
                        GLint                 v0,
                        GLint                 v1))
COGL_FEATURE_FUNCTION (void, glUniform3i,
                       (GLint                 location,
                        GLint                 v0,
                        GLint                 v1,
                        GLint                 v2))
COGL_FEATURE_FUNCTION (void, glUniform4i,
                       (GLint                 location,
                        GLint                 v0,
                        GLint                 v1,
                        GLint                 v2,
                        GLint                 v3))
COGL_FEATURE_FUNCTION (void, glUniform1iv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLint *         value))
COGL_FEATURE_FUNCTION (void, glUniform2iv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLint *         value))
COGL_FEATURE_FUNCTION (void, glUniform3iv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLint *         value))
COGL_FEATURE_FUNCTION (void, glUniform4iv,
                       (GLint                 location,
                        GLsizei               count,
                        const GLint *         value))
COGL_FEATURE_FUNCTION (void, glUniformMatrix2fv,
                       (GLint                 location,
                        GLsizei               count,
                        GLboolean             transpose,
                        const GLfloat        *value))
COGL_FEATURE_FUNCTION (void, glUniformMatrix3fv,
                       (GLint                 location,
                        GLsizei               count,
                        GLboolean             transpose,
                        const GLfloat        *value))
COGL_FEATURE_FUNCTION (void, glUniformMatrix4fv,
                       (GLint                 location,
                        GLsizei               count,
                        GLboolean             transpose,
                        const GLfloat        *value))

COGL_FEATURE_END ()

COGL_FEATURE_BEGIN (vbos, 1, 5,
                    "ARB\0",
                    "vertex_buffer_object\0",
                    COGL_FEATURE_VBOS)
COGL_FEATURE_FUNCTION (void, glGenBuffers,
                       (GLuint		 n,
                        GLuint		*buffers))
COGL_FEATURE_FUNCTION (void, glBindBuffer,
                       (GLenum		 target,
                        GLuint		 buffer))
COGL_FEATURE_FUNCTION (void, glBufferData,
                       (GLenum		 target,
                        GLsizeiptr		 size,
                        const GLvoid		*data,
                        GLenum		 usage))
COGL_FEATURE_FUNCTION (void, glBufferSubData,
                       (GLenum		 target,
                        GLintptr		 offset,
                        GLsizeiptr		 size,
                        const GLvoid		*data))
COGL_FEATURE_FUNCTION (void *, glMapBuffer,
                       (GLenum		 target,
                        GLenum		 access))
COGL_FEATURE_FUNCTION (GLboolean, glUnmapBuffer,
                       (GLenum		 target))
COGL_FEATURE_FUNCTION (void, glDeleteBuffers,
                       (GLsizei		 n,
                        const GLuint		*buffers))
COGL_FEATURE_END ()

/* Cogl requires OpenGL 1.2 so we assume these functions are always
   available and don't bother setting any feature flags. We still have
   to fetch the function pointers though because under Windows you can
   not call any function defined after GL 1.1 directly */
COGL_FEATURE_BEGIN (in_1_2, 1, 2,
                    "\0",
                    "\0",
                    0)
COGL_FEATURE_FUNCTION (void, glDrawRangeElements,
                       (GLenum                mode,
                        GLuint                start,
                        GLuint                end,
                        GLsizei               count,
                        GLenum                type,
                        const GLvoid         *indices))
COGL_FEATURE_FUNCTION (void, glBlendEquation,
                       (GLenum                mode))
COGL_FEATURE_FUNCTION (void, glBlendColor,
                       (GLclampf              red,
                        GLclampf              green,
                        GLclampf              blue,
                        GLclampf              alpha))
COGL_FEATURE_END ()

/* Available in GL 1.3 or the multitexture extension. These are
   required */
COGL_FEATURE_BEGIN (multitexture, 1, 3,
                    "ARB\0",
                    "multitexture\0",
                    0)
COGL_FEATURE_FUNCTION (void, glActiveTexture,
                       (GLenum                texture))
COGL_FEATURE_FUNCTION (void, glClientActiveTexture,
                       (GLenum                texture))
COGL_FEATURE_END ()

/* Optional, declared in 1.4 */
COGL_FEATURE_BEGIN (blend_func_separate, 1, 4,
                    "EXT\0",
                    "blend_func_separate\0",
                    0)
COGL_FEATURE_FUNCTION (void, glBlendFuncSeparate,
                       (GLenum                srcRGB,
                        GLenum                dstRGB,
                        GLenum                srcAlpha,
                        GLenum                dstAlpha))
COGL_FEATURE_END ()

/* Optional, declared in 2.0 */
COGL_FEATURE_BEGIN (blend_equation_separate, 2, 0,
                    "EXT\0",
                    "blend_equation_separate\0",
                    0)
COGL_FEATURE_FUNCTION (void, glBlendEquationSeparate,
                       (GLenum                modeRGB,
                        GLenum                modeAlpha))
COGL_FEATURE_END ()
