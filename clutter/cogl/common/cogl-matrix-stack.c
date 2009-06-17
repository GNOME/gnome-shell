/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-context.h"
#include "cogl-internal.h"
#include "cogl-matrix-stack.h"

typedef struct {
  CoglMatrix matrix;
  /* count of pushes with no changes; when a change is
   * requested, we create a new state and decrement this
   */
  int push_count;
} CoglMatrixState;

/**
 * CoglMatrixStack:
 *
 * Stores a cogl-side matrix stack, which we use as a cache
 * so we can get the matrix efficiently when using indirect
 * rendering.
 */
struct _CoglMatrixStack
{
  GSList *stack;

  /* which state does GL have, NULL if unknown */
  CoglMatrixState *flushed_state;
};


static CoglMatrixState*
_cogl_matrix_state_new (void)
{
  CoglMatrixState *state;

  state = g_slice_new0 (CoglMatrixState);

  /* load identity */
  cogl_matrix_init_identity (&state->matrix);

  /* state->push_count defaults to 0 */

  return state;
}

static void
_cogl_matrix_state_destroy (CoglMatrixState *state)
{

  g_slice_free (CoglMatrixState, state);
}

static CoglMatrixState*
_cogl_matrix_stack_top (CoglMatrixStack *stack)
{
  return stack->stack->data;
}

static CoglMatrixState*
_cogl_matrix_stack_top_mutable (CoglMatrixStack *stack)
{
  CoglMatrixState *state;
  CoglMatrixState *new_top;

  state = _cogl_matrix_stack_top (stack);

  if (state->push_count == 0)
    return state;

  state->push_count -= 1;

  new_top = _cogl_matrix_state_new ();

  new_top->matrix = state->matrix;

  if (stack->flushed_state == state)
    {
      stack->flushed_state = new_top;
    }

  stack->stack =
    g_slist_prepend (stack->stack,
                     new_top);

  return new_top;
}

CoglMatrixStack*
_cogl_matrix_stack_new (void)
{
  CoglMatrixStack *stack;
  CoglMatrixState *state;

  stack = g_slice_new0 (CoglMatrixStack);
  state = _cogl_matrix_state_new ();
  stack->stack =
    g_slist_prepend (stack->stack,
                     state);

  return stack;
}

void
_cogl_matrix_stack_destroy (CoglMatrixStack *stack)
{
  while (stack->stack)
    {
      CoglMatrixState *state;

      state = stack->stack->data;
      _cogl_matrix_state_destroy (state);
      stack->stack =
        g_slist_delete_link (stack->stack,
                             stack->stack);
    }

  g_slice_free (CoglMatrixStack, stack);
}

void
_cogl_matrix_stack_push (CoglMatrixStack *stack)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top (stack);

  /* we lazily create a new stack top if someone changes the matrix
   * while push_count > 0
   */
  state->push_count += 1;
}

void
_cogl_matrix_stack_pop (CoglMatrixStack *stack)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top (stack);

  if (state->push_count > 0)
    {
      state->push_count -= 1;
    }
  else
    {
      if (stack->stack->next == NULL)
        {
          g_warning ("Too many matrix pops");
          return;
        }

      if (stack->flushed_state == state)
        {
          stack->flushed_state = NULL;
        }

      stack->stack =
        g_slist_delete_link (stack->stack,
                             stack->stack);
      _cogl_matrix_state_destroy (state);
    }
}

void
_cogl_matrix_stack_load_identity (CoglMatrixStack *stack)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_init_identity (&state->matrix);

  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_scale (CoglMatrixStack *stack,
                          float            x,
                          float            y,
                          float            z)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_scale (&state->matrix, x, y, z);
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_translate (CoglMatrixStack *stack,
                              float            x,
                              float            y,
                              float            z)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_translate (&state->matrix, x, y, z);
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_rotate (CoglMatrixStack *stack,
                           float            angle,
                           float            x,
                           float            y,
                           float            z)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_rotate (&state->matrix, angle, x, y, z);
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_multiply (CoglMatrixStack  *stack,
                             const CoglMatrix *matrix)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_multiply (&state->matrix, &state->matrix, matrix);
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_frustum (CoglMatrixStack *stack,
                            float            left,
                            float            right,
                            float            bottom,
                            float            top,
                            float            z_near,
                            float            z_far)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_frustum (&state->matrix,
                       left, right, bottom, top,
                       z_near, z_far);
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_perspective (CoglMatrixStack *stack,
                                float            fov_y,
                                float            aspect,
                                float            z_near,
                                float            z_far)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_perspective (&state->matrix,
                           fov_y, aspect, z_near, z_far);
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_ortho (CoglMatrixStack *stack,
                          float            left,
                          float            right,
                          float            bottom,
                          float            top,
                          float            z_near,
                          float            z_far)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  cogl_matrix_ortho (&state->matrix,
                     left, right, bottom, top, z_near, z_far);
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_get (CoglMatrixStack *stack,
                        CoglMatrix      *matrix)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top (stack);

  *matrix = state->matrix;
}

void
_cogl_matrix_stack_set (CoglMatrixStack  *stack,
                        const CoglMatrix *matrix)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack);
  state->matrix = *matrix;
  /* mark dirty */
  stack->flushed_state = NULL;
}

void
_cogl_matrix_stack_flush_to_gl (CoglMatrixStack *stack,
                                GLenum           gl_mode)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top (stack);

  if (stack->flushed_state == state)
    return;

  /* NOTE we assume caller was in MODELVIEW mode */

  if (gl_mode != GL_MODELVIEW)
    GE (glMatrixMode (gl_mode));

  /* In theory it might help the GL implementation if we used our
   * local analysis of the matrix and called Translate/Scale rather
   * than LoadMatrix to send a 2D matrix
   */

  GE (glLoadMatrixf (cogl_matrix_get_array (&state->matrix)));
  stack->flushed_state = state;

  if (gl_mode != GL_MODELVIEW)
    GE (glMatrixMode (GL_MODELVIEW));
}

void
_cogl_matrix_stack_dirty (CoglMatrixStack *stack)
{
  stack->flushed_state = NULL;
}

