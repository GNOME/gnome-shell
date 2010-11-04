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

#include "cogl.h"
#include "cogl-clip-stack.h"
#include "cogl-clip-state-private.h"
#include "cogl-context-private.h"
#include "cogl-internal.h"
#include "cogl-framebuffer-private.h"
#include "cogl-journal-private.h"
#include "cogl-util.h"
#include "cogl-matrix-private.h"

void
cogl_clip_push_window_rectangle (int x_offset,
                                 int y_offset,
                                 int width,
                                 int height)
{
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_draw_buffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  clip_state->stacks->data =
    _cogl_clip_stack_push_window_rectangle (clip_state->stacks->data,
                                            x_offset, y_offset,
                                            width, height);
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
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;
  CoglMatrix modelview_matrix;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_draw_buffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  cogl_get_modelview_matrix (&modelview_matrix);

  clip_state->stacks->data =
    _cogl_clip_stack_push_rectangle (clip_state->stacks->data,
                                     x_1, y_1, x_2, y_2,
                                     &modelview_matrix);
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
cogl_clip_push_from_path_preserve (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl2_clip_push_from_path (ctx->current_path);
}

#undef cogl_clip_push_from_path
void
cogl_clip_push_from_path (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_clip_push_from_path (ctx->current_path);

  cogl_object_unref (ctx->current_path);
  ctx->current_path = cogl2_path_new ();
}

static void
_cogl_clip_pop_real (CoglClipState *clip_state)
{
  clip_state->stacks->data = _cogl_clip_stack_pop (clip_state->stacks->data);
}

void
cogl_clip_pop (void)
{
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_draw_buffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_pop_real (clip_state);
}

void
_cogl_clip_state_flush (CoglClipState *clip_state)
{
  /* Flush the topmost stack. The clip stack code will bail out early
     if this is already flushed */
  _cogl_clip_stack_flush (clip_state->stacks->data);
}

/* XXX: This should never have been made public API! */
void
cogl_clip_ensure (void)
{
  CoglFramebuffer *framebuffer = _cogl_get_draw_buffer ();
  CoglClipState *clip_state;

  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);
  /* Flushing the clip state doesn't cause the journal to be
     flushed. This function may be being called by an external
     application however so it makes sense to flush the journal
     here */
  _cogl_framebuffer_flush_journal (framebuffer);
  _cogl_clip_state_flush (clip_state);
}

static void
_cogl_clip_stack_save_real (CoglClipState *clip_state)
{
  clip_state->stacks = g_slist_prepend (clip_state->stacks, NULL);
}

void
cogl_clip_stack_save (void)
{
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_draw_buffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_stack_save_real (clip_state);
}

static void
_cogl_clip_stack_restore_real (CoglClipState *clip_state)
{
  CoglHandle stack;

  g_return_if_fail (clip_state->stacks != NULL);

  stack = clip_state->stacks->data;

  _cogl_clip_stack_unref (stack);

  /* Revert to an old stack */
  clip_state->stacks = g_slist_delete_link (clip_state->stacks,
                                            clip_state->stacks);
}

void
cogl_clip_stack_restore (void)
{
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_draw_buffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_stack_restore_real (clip_state);
}

void
_cogl_clip_state_init (CoglClipState *clip_state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  clip_state->stacks = NULL;

  /* Add an intial stack */
  _cogl_clip_stack_save_real (clip_state);
}

void
_cogl_clip_state_destroy (CoglClipState *clip_state)
{
  /* Destroy all of the stacks */
  while (clip_state->stacks)
    _cogl_clip_stack_restore_real (clip_state);
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
