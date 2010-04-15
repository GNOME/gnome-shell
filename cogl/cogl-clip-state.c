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

void
cogl_clip_push_window_rectangle (int x_offset,
                                 int y_offset,
                                 int width,
                                 int height)
{
  CoglHandle framebuffer;
  CoglClipState *clip_state;
  CoglHandle stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  stack = clip_state->stacks->data;

  _cogl_clip_stack_push_window_rectangle (stack, x_offset, y_offset,
                                          width, height);

  clip_state->stack_dirty = TRUE;
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

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

/* Transform a homogeneous vertex position from model space to Cogl
 * window coordinates (with 0,0 being top left) */
static void
transform_point (CoglMatrix *matrix_mv,
                 CoglMatrix *matrix_p,
                 float *viewport,
                 float *x,
                 float *y)
{
  float z = 0;
  float w = 1;

  /* Apply the modelview matrix transform */
  cogl_matrix_transform_point (matrix_mv, x, y, &z, &w);

  /* Apply the projection matrix transform */
  cogl_matrix_transform_point (matrix_p, x, y, &z, &w);

  /* Perform perspective division */
  *x /= w;
  *y /= w;

  /* Apply viewport transform */
  *x = VIEWPORT_TRANSFORM_X (*x, viewport[0], viewport[2]);
  *y = VIEWPORT_TRANSFORM_Y (*y, viewport[1], viewport[3]);
}

#undef VIEWPORT_SCALE_X
#undef VIEWPORT_SCALE_Y

/* Try to push a rectangle given in object coordinates as a rectangle in window
 * coordinates instead of object coordinates */
gboolean
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

  transform_point (&matrix, &matrix_p, v, &x_1, &y_1);
  transform_point (&matrix, &matrix_p, v, &x_2, &y_2);

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
  CoglHandle framebuffer;
  CoglClipState *clip_state;
  CoglHandle stack;
  CoglMatrix modelview_matrix;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  /* Try and catch window space rectangles so we can redirect to
   * cogl_clip_push_window_rect which will use scissoring. */
  if (try_pushing_rect_as_window_rect (x_1, y_1, x_2, y_2))
    return;

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  stack = clip_state->stacks->data;

  cogl_get_modelview_matrix (&modelview_matrix);

  _cogl_clip_stack_push_rectangle (stack, x_1, y_1, x_2, y_2,
                                   &modelview_matrix);

  clip_state->stack_dirty = TRUE;
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
  CoglHandle framebuffer;
  CoglClipState *clip_state;
  CoglHandle stack;
  CoglMatrix modelview_matrix;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  stack = clip_state->stacks->data;

  cogl_get_modelview_matrix (&modelview_matrix);

  _cogl_clip_stack_push_from_path (stack, cogl_path_get (),
                                   &modelview_matrix);

  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_push_from_path (void)
{
  cogl_clip_push_from_path_preserve ();

  cogl_path_new ();
}

static void
_cogl_clip_pop_real (CoglClipState *clip_state)
{
  CoglHandle stack;

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  stack = clip_state->stacks->data;

  _cogl_clip_stack_pop (stack);

  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_pop (void)
{
  CoglHandle framebuffer;
  CoglClipState *clip_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = _cogl_get_framebuffer ();
  clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

  _cogl_clip_pop_real (clip_state);
}

void
_cogl_clip_state_flush (CoglClipState *clip_state)
{
  CoglHandle stack;

  if (!clip_state->stack_dirty)
    return;

  /* The current primitive journal does not support tracking changes to the
   * clip stack...  */
  _cogl_journal_flush ();

  /* XXX: the handling of clipping is quite complex. It may involve use of
   * the Cogl Journal or other Cogl APIs which may end up recursively
   * wanting to ensure the clip state is flushed. We need to ensure we
   * don't recurse infinitely...
   */
  clip_state->stack_dirty = FALSE;

  stack = clip_state->stacks->data;

  _cogl_clip_stack_flush (stack, &clip_state->stencil_used);
}

/* XXX: This should never have been made public API! */
void
cogl_clip_ensure (void)
{
  CoglClipState *clip_state;

  clip_state = _cogl_framebuffer_get_clip_state (_cogl_get_framebuffer ());
  _cogl_clip_state_flush (clip_state);
}

static void
_cogl_clip_stack_save_real (CoglClipState *clip_state)
{
  CoglHandle stack;

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  stack = _cogl_clip_stack_new ();

  clip_state->stacks = g_slist_prepend (clip_state->stacks, stack);
  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_stack_save (void)
{
  CoglHandle framebuffer;
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

  /* We don't log clip stack changes in the journal so we must flush
   * it before making modifications */
  _cogl_journal_flush ();

  stack = clip_state->stacks->data;

  cogl_handle_unref (stack);

  /* Revert to an old stack */
  clip_state->stacks = g_slist_delete_link (clip_state->stacks,
                                            clip_state->stacks);

  clip_state->stack_dirty = TRUE;
}

void
cogl_clip_stack_restore (void)
{
  CoglHandle framebuffer;
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
  clip_state->stack_dirty = TRUE;

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

void
_cogl_clip_state_dirty (CoglClipState *clip_state)
{
  clip_state->stack_dirty = TRUE;
}
