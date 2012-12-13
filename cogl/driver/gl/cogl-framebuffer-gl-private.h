/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010,2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_FRAMEBUFFER_GL_PRIVATE_H__
#define __COGL_FRAMEBUFFER_GL_PRIVATE_H__

CoglBool
_cogl_offscreen_gl_allocate (CoglOffscreen *offscreen,
                             CoglError **error);

void
_cogl_offscreen_gl_free (CoglOffscreen *offscreen);

void
_cogl_framebuffer_gl_flush_state (CoglFramebuffer *draw_buffer,
                                  CoglFramebuffer *read_buffer,
                                  CoglFramebufferState state);

void
_cogl_framebuffer_gl_clear (CoglFramebuffer *framebuffer,
                            unsigned long buffers,
                            float red,
                            float green,
                            float blue,
                            float alpha);

void
_cogl_framebuffer_gl_query_bits (CoglFramebuffer *framebuffer,
                                 CoglFramebufferBits *bits);

void
_cogl_framebuffer_gl_finish (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_gl_discard_buffers (CoglFramebuffer *framebuffer,
                                      unsigned long buffers);

void
_cogl_framebuffer_gl_bind (CoglFramebuffer *framebuffer, GLenum target);

void
_cogl_framebuffer_gl_draw_attributes (CoglFramebuffer *framebuffer,
                                      CoglPipeline *pipeline,
                                      CoglVerticesMode mode,
                                      int first_vertex,
                                      int n_vertices,
                                      CoglAttribute **attributes,
                                      int n_attributes,
                                      CoglDrawFlags flags);

void
_cogl_framebuffer_gl_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                              CoglPipeline *pipeline,
                                              CoglVerticesMode mode,
                                              int first_vertex,
                                              int n_vertices,
                                              CoglIndices *indices,
                                              CoglAttribute **attributes,
                                              int n_attributes,
                                              CoglDrawFlags flags);

CoglBool
_cogl_framebuffer_gl_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                              int x,
                                              int y,
                                              CoglReadPixelsFlags source,
                                              CoglBitmap *bitmap,
                                              CoglError **error);

#endif /* __COGL_FRAMEBUFFER_GL_PRIVATE_H__ */


