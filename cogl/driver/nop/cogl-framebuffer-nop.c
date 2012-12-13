/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2012 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-framebuffer-nop-private.h"

#include <glib.h>
#include <string.h>

void
_cogl_framebuffer_nop_flush_state (CoglFramebuffer *draw_buffer,
                                   CoglFramebuffer *read_buffer,
                                   CoglFramebufferState state)
{
}

CoglBool
_cogl_offscreen_nop_allocate (CoglOffscreen *offscreen,
                              CoglError **error)
{
  return TRUE;
}

void
_cogl_offscreen_nop_free (CoglOffscreen *offscreen)
{
}

void
_cogl_framebuffer_nop_clear (CoglFramebuffer *framebuffer,
                             unsigned long buffers,
                             float red,
                             float green,
                             float blue,
                             float alpha)
{
}

void
_cogl_framebuffer_nop_query_bits (CoglFramebuffer *framebuffer,
                                  CoglFramebufferBits *bits)
{
  memset (bits, 0, sizeof (CoglFramebufferBits));
}

void
_cogl_framebuffer_nop_finish (CoglFramebuffer *framebuffer)
{
}

void
_cogl_framebuffer_nop_discard_buffers (CoglFramebuffer *framebuffer,
                                       unsigned long buffers)
{
}

void
_cogl_framebuffer_nop_draw_attributes (CoglFramebuffer *framebuffer,
                                       CoglPipeline *pipeline,
                                       CoglVerticesMode mode,
                                       int first_vertex,
                                       int n_vertices,
                                       CoglAttribute **attributes,
                                       int n_attributes,
                                       CoglDrawFlags flags)
{
}

void
_cogl_framebuffer_nop_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                               CoglPipeline *pipeline,
                                               CoglVerticesMode mode,
                                               int first_vertex,
                                               int n_vertices,
                                               CoglIndices *indices,
                                               CoglAttribute **attributes,
                                               int n_attributes,
                                               CoglDrawFlags flags)
{
}

CoglBool
_cogl_framebuffer_nop_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                               int x,
                                               int y,
                                               CoglReadPixelsFlags source,
                                               CoglBitmap *bitmap,
                                               CoglError **error)
{
  return TRUE;
}
