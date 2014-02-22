/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
