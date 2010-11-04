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
#include "cogl-clip-state.h"
#include "cogl-context.h"
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

  framebuffer = _cogl_get_framebuffer ();
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

/* Try to push a rectangle given in object coordinates as a rectangle in window
 * coordinates instead of object coordinates */
static gboolean
try_pushing_rect_as_window_rect (float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2)
{
  CoglMatrix matrix;
  CoglMatrix matrix_p;
  float v[4];

  cogl_get_modelview_matrix (&matrix);

  /* If the modelview meets these constraints then a transformed rectangle
   * should still be a rectangle when it reaches screen coordinates.
   *
   * FIXME: we are are making certain assumptions about the projection
   * matrix a.t.m and should really be looking at the combined modelview
   * and projection matrix.
   * FIXME: we don't consider rotations that are a multiple of 90 degrees
   * which could be quite common.
   */
  if (matrix.xy != 0 || matrix.xz != 0 ||
      matrix.yx != 0 || matrix.yz != 0 ||
      matrix.zx != 0 || matrix.zy != 0)
    return FALSE;

  cogl_get_projection_matrix (&matrix_p);
  cogl_get_viewport (v);

  _cogl_transform_point (&matrix, &matrix_p, v, &x_1, &y_1);
  _cogl_transform_point (&matrix, &matrix_p, v, &x_2, &y_2);

  /* Consider that the modelview matrix may flip the rectangle
   * along the x or y axis... */
#define SWAP(A,B) do { float tmp = B; B = A; A = tmp; } while (0)
  if (x_1 > x_2)
    SWAP (x_1, x_2);
  if (y_1 > y_2)
    SWAP (y_1, y_2);
#undef SWAP

  cogl_clip_push_window_rectangle (COGL_UTIL_NEARBYINT (x_1),
                                   COGL_UTIL_NEARBYINT (y_1),
                                   COGL_UTIL_NEARBYINT (x_2 - x_1),
                                   COGL_UTIL_NEARBYINT (y_2 - y_1));
  return TRUE;
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

  /* Try and catch window space rectangles so we can redirect to
   * cogl_clip_push_window_rect which will use scissoring. */
  if (try_pushing_rect_as_window_rect (x_1, y_1, x_2, y_2))
    return;

  framebuffer = _cogl_get_framebuffer ();
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
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;
  CoglMatrix modelview_matrix;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  cogl_get_modelview_matrix (&modelview_matrix);

  clip_state->stacks->data =
    _cogl_clip_stack_push_from_path (clip_state->stacks->data,
                                     ctx->current_path,
                                     &modelview_matrix);
}

void
cogl_clip_push_from_path (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_clip_push_from_path_preserve ();

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

  framebuffer = _cogl_get_framebuffer ();
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
  CoglClipState *clip_state;

  clip_state = _cogl_framebuffer_get_clip_state (_cogl_get_framebuffer ());
  /* Flushing the clip state doesn't cause the journal to be
     flushed. This function may be being called by an external
     application however so it makes sense to flush the journal
     here */
  _cogl_journal_flush ();
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

  framebuffer = _cogl_get_framebuffer ();
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

  framebuffer = _cogl_get_framebuffer ();
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
_cogl_get_clip_stack (void)
{
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  return clip_state->stacks->data;
}

void
_cogl_set_clip_stack (CoglClipStack *stack)
{
  CoglFramebuffer *framebuffer;
  CoglClipState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  /* Replace the top of the stack of stacks */
  _cogl_clip_stack_ref (stack);
  _cogl_clip_stack_unref (clip_state->stacks->data);
  clip_state->stacks->data = stack;
}
