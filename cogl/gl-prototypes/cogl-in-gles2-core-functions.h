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
                3, 0,
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
COGL_EXT_FUNCTION (GLboolean, glIsRenderbuffer,
                   (GLuint                renderbuffer))
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
COGL_EXT_FUNCTION (void, glGetRenderbufferParameteriv,
                   (GLenum                target,
                    GLenum                pname,
                    GLint                *params))
COGL_EXT_FUNCTION (GLboolean, glIsFramebuffer,
                   (GLuint                framebuffer))
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

COGL_EXT_BEGIN (gles2_only_api,
                4, 1,
                COGL_EXT_IN_GLES2,
                "ARB:\0",
                "ES2_compatibility\0")
COGL_EXT_FUNCTION (void, glReleaseShaderCompiler, (void))
COGL_EXT_FUNCTION (void, glGetShaderPrecisionFormat,
                   (GLenum shadertype,
                    GLenum precisiontype,
                    GLint* range,
                    GLint* precision))
COGL_EXT_FUNCTION (void, glShaderBinary,
                   (GLsizei n,
                    const GLuint* shaders,
                    GLenum binaryformat,
                    const GLvoid* binary,
                    GLsizei length))
COGL_EXT_END ()

/* GL and GLES 2.0 apis */
COGL_EXT_BEGIN (two_point_zero_api,
                2, 0,
                COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glStencilFuncSeparate,
                   (GLenum face, GLenum func, GLint ref, GLuint mask))
COGL_EXT_FUNCTION (void, glStencilMaskSeparate,
                   (GLenum face, GLuint mask))
COGL_EXT_FUNCTION (void, glStencilOpSeparate,
                   (GLenum face, GLenum fail, GLenum zfail, GLenum zpass))
COGL_EXT_END ()
