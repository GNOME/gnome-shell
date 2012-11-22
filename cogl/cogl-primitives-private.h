/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
