/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __COGL_DRIVER_H
#define __COGL_DRIVER_H

#include "cogl-context.h"
#include "cogl-offscreen.h"
#include "cogl-framebuffer-private.h"

typedef struct _CoglDriverVtable CoglDriverVtable;

struct _CoglDriverVtable
{
  CoglBool
  (* pixel_format_from_gl_internal) (CoglContext *context,
                                     GLenum gl_int_format,
                                     CoglPixelFormat *out_format);

  CoglPixelFormat
  (* pixel_format_to_gl) (CoglContext *context,
                          CoglPixelFormat format,
                          GLenum *out_glintformat,
                          GLenum *out_glformat,
                          GLenum *out_gltype);

  CoglBool
  (* update_features) (CoglContext *context,
                       GError **error);

  CoglBool
  (* offscreen_allocate) (CoglOffscreen *offscreen,
                          GError **error);

  void
  (* offscreen_free) (CoglOffscreen *offscreen);

  void
  (* framebuffer_flush_state) (CoglFramebuffer *draw_buffer,
                               CoglFramebuffer *read_buffer,
                               CoglFramebufferState state);

  void
  (* framebuffer_clear) (CoglFramebuffer *framebuffer,
                         unsigned long buffers,
                         float red,
                         float green,
                         float blue,
                         float alpha);

  void
  (* framebuffer_query_bits) (CoglFramebuffer *framebuffer,
                              int *red,
                              int *green,
                              int *blue,
                              int *alpha);

  void
  (* framebuffer_finish) (CoglFramebuffer *framebuffer);

  void
  (* framebuffer_discard_buffers) (CoglFramebuffer *framebuffer,
                                   unsigned long buffers);

  void
  (* framebuffer_draw_attributes) (CoglFramebuffer *framebuffer,
                                   CoglPipeline *pipeline,
                                   CoglVerticesMode mode,
                                   int first_vertex,
                                   int n_vertices,
                                   CoglAttribute **attributes,
                                   int n_attributes,
                                   CoglDrawFlags flags);

  void
  (* framebuffer_draw_indexed_attributes) (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           CoglVerticesMode mode,
                                           int first_vertex,
                                           int n_vertices,
                                           CoglIndices *indices,
                                           CoglAttribute **attributes,
                                           int n_attributes,
                                           CoglDrawFlags flags);
};

#endif /* __COGL_DRIVER_H */

