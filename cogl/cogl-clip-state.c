/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

#include <glib.h>

#include "cogl-clip-stack.h"
#include "cogl-clip-state-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-journal-private.h"
#include "cogl-util.h"
#include "cogl-matrix-private.h"
#include "cogl-clip-state.h"
#include "cogl1-context.h"
#include "cogl-path/cogl-path.h"

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
  _cogl_framebuffer_save_clip_stack (cogl_get_draw_framebuffer ());
}

void
cogl_clip_stack_restore (void)
{
  _cogl_framebuffer_restore_clip_stack (cogl_get_draw_framebuffer ());
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

void
_cogl_clip_state_init (CoglClipState *clip_state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  clip_state->stacks = NULL;

  /* Add an intial stack */
  _cogl_clip_state_save_clip_stack (clip_state);
}

void
_cogl_clip_state_destroy (CoglClipState *clip_state)
{
  /* Destroy all of the stacks */
  while (clip_state->stacks)
    _cogl_clip_state_restore_clip_stack (clip_state);
}

CoglClipStack *
_cogl_clip_state_get_stack (CoglClipState *clip_state)
{
  return clip_state->stacks->data;
}

void
_cogl_clip_state_set_stack (CoglClipState *clip_state,
                            CoglClipStack *stack)
{
  /* Replace the top of the stack of stacks */
  _cogl_clip_stack_ref (stack);
  _cogl_clip_stack_unref (clip_state->stacks->data);
  clip_state->stacks->data = stack;
}

void
_cogl_clip_state_save_clip_stack (CoglClipState *clip_state)
{
  clip_state->stacks = g_slist_prepend (clip_state->stacks, NULL);
}

void
_cogl_clip_state_restore_clip_stack (CoglClipState *clip_state)
{
  CoglClipStack *stack;

  _COGL_RETURN_IF_FAIL (clip_state->stacks != NULL);

  stack = clip_state->stacks->data;

  _cogl_clip_stack_unref (stack);

  /* Revert to an old stack */
  clip_state->stacks = g_slist_delete_link (clip_state->stacks,
                                            clip_state->stacks);
}
