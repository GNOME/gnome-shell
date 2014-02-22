/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __COGL_PRIMITIVES_PRIVATE_H
#define __COGL_PRIMITIVES_PRIVATE_H

#include <glib.h>

COGL_BEGIN_DECLS

/* Draws a rectangle without going through the journal so that it will
   be flushed immediately. This should only be used in situations
   where the code may be called while the journal is already being
   flushed. In that case using the journal would go wrong */
void
_cogl_rectangle_immediate (CoglFramebuffer *framebuffer,
                           CoglPipeline *pipeline,
                           float x_1,
                           float y_1,
                           float x_2,
                           float y_2);

typedef struct _CoglMultiTexturedRect
{
  const float *position; /* x0,y0,x1,y1 */
  const float *tex_coords; /* (tx0,ty0,tx1,ty1)(tx0,ty0,tx1,ty1)(... */
  int tex_coords_len; /* number of floats in tex_coords? */
} CoglMultiTexturedRect;

void
_cogl_framebuffer_draw_multitextured_rectangles (
                                        CoglFramebuffer *framebuffer,
                                        CoglPipeline *pipeline,
                                        CoglMultiTexturedRect *rects,
                                        int n_rects,
                                        CoglBool disable_legacy_state);

COGL_END_DECLS

#endif /* __COGL_PRIMITIVES_PRIVATE_H */
