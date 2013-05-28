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

/* The functions in this file are part of the core GL,GLES1 and GLES2 apis */
#include "cogl-core-functions.h"

/* The functions in this file are core to GLES1 only but may also be
 * extensions available for GLES2 and GL */
#include "cogl-in-gles1-core-functions.h"

/* The functions in this file are core to GLES2 only but
 * may be extensions for GLES1 and GL */
#include "cogl-in-gles2-core-functions.h"

/* The functions in this file are core to GLES1 and GLES2 but not core
 * to GL but they may be extensions available for GL */
#include "cogl-in-gles-core-functions.h"

/* These are fixed-function APIs core to GL and GLES1 */
#include "cogl-fixed-functions.h"

/* These are GLSL shader APIs core to GL 2.0 and GLES2 */
#include "cogl-glsl-functions.h"

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



COGL_EXT_BEGIN (offscreen_blit, 3, 0,
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

COGL_EXT_BEGIN (framebuffer_discard, 255, 255,
                0, /* not in either GLES */
                "EXT\0",
                "framebuffer_discard\0")
COGL_EXT_FUNCTION (void, glDiscardFramebuffer,
                   (GLenum           target,
                    GLsizei          numAttachments,
                    const GLenum    *attachments))
COGL_EXT_END ()

COGL_EXT_BEGIN (IMG_multisampled_render_to_texture, 255, 255,
                0, /* not in either GLES */
                "\0",
                "IMG_multisampled_render_to_texture\0")
COGL_EXT_FUNCTION (void, glRenderbufferStorageMultisampleIMG,
                   (GLenum           target,
                    GLsizei          samples,
                    GLenum           internal_format,
                    GLsizei          width,
                    GLsizei          height))
COGL_EXT_FUNCTION (void, glFramebufferTexture2DMultisampleIMG,
                   (GLenum           target,
                    GLenum           attachment,
                    GLenum           textarget,
                    GLuint           texture,
                    GLint            level,
                    GLsizei          samples))
COGL_EXT_END ()

COGL_EXT_BEGIN (ARB_sampler_objects, 3, 3,
                0, /* not in either GLES */
                "ARB:\0",
                "sampler_objects\0")
COGL_EXT_FUNCTION (void, glGenSamplers,
                   (GLsizei count,
                    GLuint *samplers))
COGL_EXT_FUNCTION (void, glDeleteSamplers,
                   (GLsizei count,
                    const GLuint *samplers))
COGL_EXT_FUNCTION (void, glBindSampler,
                   (GLuint unit,
                    GLuint sampler))
COGL_EXT_FUNCTION (void, glSamplerParameteri,
                   (GLuint sampler,
                    GLenum pname,
                    GLint param))
COGL_EXT_END ()

/* These only list functions that come from the old GLSL extensions.
 * Functions that are common to the extensions and GLSL 2.0 should
 * instead be listed in cogl-glsl-functions.h */
COGL_EXT_BEGIN (shader_objects, 255, 255,
                0, /* not in either GLES */
                "ARB\0",
                "shader_objects\0")
COGL_EXT_FUNCTION (GLuint, glCreateProgramObject,
                   (void))
COGL_EXT_FUNCTION (GLuint, glCreateShaderObject,
                   (GLenum shaderType))
COGL_EXT_FUNCTION (void, glDeleteObject,
                   (GLuint obj))
COGL_EXT_FUNCTION (void, glAttachObject,
                   (GLuint container, GLuint obj))
COGL_EXT_FUNCTION (void, glUseProgramObject,
                   (GLuint programObj))
COGL_EXT_FUNCTION (void, glGetInfoLog,
                   (GLuint                obj,
                    GLsizei               maxLength,
                    GLsizei              *length,
                    char                 *infoLog))
COGL_EXT_FUNCTION (void, glGetObjectParameteriv,
                   (GLuint                obj,
                    GLenum                pname,
                    GLint                *params))
COGL_EXT_FUNCTION (void, glDetachObject,
                   (GLuint container, GLuint obj))
COGL_EXT_FUNCTION (void, glGetAttachedObjects,
                   (GLuint program,
                    GLsizei maxcount,
                    GLsizei* count,
                    GLuint* shaders))
COGL_EXT_END ()

COGL_EXT_BEGIN (only_gl3, 3, 0,
                0, /* not in either GLES */
                "\0",
                "\0")
COGL_EXT_FUNCTION (const GLubyte *, glGetStringi,
                   (GLenum name, GLuint index))
COGL_EXT_END ()

COGL_EXT_BEGIN (vertex_array_object, 3, 0,
                0, /* not in either GLES */
                "ARB\0OES\0",
                "vertex_array_object\0")
COGL_EXT_FUNCTION (void, glBindVertexArray,
                   (GLuint array))
COGL_EXT_FUNCTION (void, glDeleteVertexArrays,
                   (GLsizei n,
                    const GLuint *arrays))
COGL_EXT_FUNCTION (void, glGenVertexArrays,
                   (GLsizei n,
                    GLuint *arrays))
COGL_EXT_END ()

COGL_EXT_BEGIN (map_region, 3, 0,
                0, /* not in either GLES */
                "ARB:\0",
                "map_buffer_range\0")
COGL_EXT_FUNCTION (GLvoid *, glMapBufferRange,
                   (GLenum target,
                    GLintptr offset,
                    GLsizeiptr length,
                    GLbitfield access))
COGL_EXT_END ()

#ifdef GL_ARB_sync
COGL_EXT_BEGIN (sync, 3, 2,
                0, /* not in either GLES */
                "ARB:\0",
                "sync\0")
COGL_EXT_FUNCTION (GLsync, glFenceSync,
                   (GLenum condition, GLbitfield flags))
COGL_EXT_FUNCTION (GLenum, glClientWaitSync,
                   (GLsync sync, GLbitfield flags, GLuint64 timeout))
COGL_EXT_FUNCTION (void, glDeleteSync,
                   (GLsync sync))
COGL_EXT_END ()
#endif
