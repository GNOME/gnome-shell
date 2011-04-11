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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

/* This is included multiple times with different definitions for
   these macros */

COGL_FEATURE_BEGIN (offscreen, 255, 255,
                    "OES\0",
                    "framebuffer_object\0",
                    COGL_FEATURE_OFFSCREEN,
                    0)
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
COGL_FEATURE_FUNCTION (void, glGetFramebufferAttachmentParameteriv,
                       (GLenum                target,
                        GLenum                attachment,
                        GLenum                pname,
                        GLint                *params))
COGL_FEATURE_END ()

COGL_FEATURE_BEGIN (offscreen_blit, 255, 255,
                    "ANGLE\0",
                    "framebuffer_blit\0",
                    COGL_FEATURE_OFFSCREEN_BLIT,
                    0)
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

COGL_FEATURE_BEGIN (element_index_uint, 255, 255,
                    "OES\0",
                    "element_index_uint\0",
                    COGL_FEATURE_UNSIGNED_INT_INDICES,
                    0)
COGL_FEATURE_END ()

COGL_FEATURE_BEGIN (texture_npot, 255, 255,
                    "OES\0IMG\0",
                    "texture_npot\0",
                    COGL_FEATURE_TEXTURE_NPOT |
                    COGL_FEATURE_TEXTURE_NPOT_BASIC |
                    COGL_FEATURE_TEXTURE_NPOT_MIPMAP |
                    COGL_FEATURE_TEXTURE_NPOT_REPEAT,
                    0)
COGL_FEATURE_END ()

COGL_FEATURE_BEGIN (texture_3d, 1, 2,
                    "OES\0",
                    "texture_3D\0",
                    COGL_FEATURE_TEXTURE_3D,
                    0)
COGL_FEATURE_FUNCTION (void, glTexImage3D,
                       (GLenum target, GLint level,
                        GLint internalFormat,
                        GLsizei width, GLsizei height,
                        GLsizei depth, GLint border,
                        GLenum format, GLenum type,
                        const GLvoid *pixels))
COGL_FEATURE_FUNCTION (void, glTexSubImage3D,
                       (GLenum target, GLint level,
                        GLint xoffset, GLint yoffset, GLint zoffset,
                        GLsizei width, GLsizei height, GLsizei depth,
                        GLenum format, GLenum type, const GLvoid* pixels))
COGL_FEATURE_END ()

COGL_FEATURE_BEGIN (map_buffer, 255, 255,
                    "OES\0",
                    "mapbuffer\0",
                    COGL_FEATURE_MAP_BUFFER_FOR_WRITE,
                    0)
COGL_FEATURE_FUNCTION (void *, glMapBuffer,
                       (GLenum           target,
                        GLenum           access))
COGL_FEATURE_FUNCTION (GLboolean, glUnmapBuffer,
                       (GLenum           target))
COGL_FEATURE_END ()
