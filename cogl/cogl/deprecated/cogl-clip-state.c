/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#include <string.h>
#include <math.h>

#include <glib.h>

#include "cogl-clip-state.h"
#include "cogl-clip-stack.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-journal-private.h"
#include "cogl-util.h"
#include "cogl-matrix-private.h"
#include "cogl1-context.h"

void
cogl_clip_push_window_rectangle (int x_offset,
                                 int y_offset,
                                 int width,
                                 int height)
{
  cogl_framebuffer_push_scissor_clip (cogl_get_draw_framebuffer (),
                                      x_offset, y_offset, width, height);
}

/* XXX: This is deprecated API */
void
cogl_clip_push_window_rect (float x_offset,
                            float y_offset,
                            float width,
                            float height)
{
  cogl_clip_push_window_rectangle (x_offset, y_offset, width, height);
}

void
cogl_clip_push_rectangle (float x_1,
                          float y_1,
                          float x_2,
                          float y_2)
{
  cogl_framebuffer_push_rectangle_clip (cogl_get_draw_framebuffer (),
                                        x_1, y_1, x_2, y_2);
}

/* XXX: Deprecated API */
void
cogl_clip_push (float x_offset,
                float y_offset,
                float width,
                float height)
{
  cogl_clip_push_rectangle (x_offset,
                            y_offset,
                            x_offset + width,
                            y_offset + height);
}

void
cogl_clip_push_primitive (CoglPrimitive *primitive,
                          float bounds_x1,
                          float bounds_y1,
                          float bounds_x2,
                          float bounds_y2)
{
  cogl_framebuffer_push_primitive_clip (cogl_get_draw_framebuffer (),
                                        primitive,
                                        bounds_x1,
                                        bounds_y1,
                                        bounds_x2,
                                        bounds_y2);
}

void
cogl_clip_pop (void)
{
  cogl_framebuffer_pop_clip (cogl_get_draw_framebuffer ());
}

void
cogl_clip_stack_save (void)
{
  /* This function was just used to temporarily switch the clip stack
   * when using an offscreen buffer. This is no longer needed because
   * each framebuffer maintains its own clip stack. The function is
   * documented to do nothing since version 1.2 */
}

void
cogl_clip_stack_restore (void)
{
  /* Do nothing. See cogl_clip_stack_save() */
}

/* XXX: This should never have been made public API! */
void
cogl_clip_ensure (void)
{
  /* Do nothing.
   *
   * This API shouldn't be used by anyone and the documented semantics
   * are basically vague enough that we can get away with doing
   * nothing here.
   */
}
