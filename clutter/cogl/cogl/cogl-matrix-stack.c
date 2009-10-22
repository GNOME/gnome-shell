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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-context.h"
#include "cogl-internal.h"
#include "cogl-matrix-stack.h"
#include "cogl-draw-buffer-private.h"

typedef struct {
  CoglMatrix matrix;
  gboolean is_identity;
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
  gboolean flushed_identity;
};

/* XXX: this doesn't initialize the matrix! */
static CoglMatrixState*
_cogl_matrix_state_new (void)
{
  CoglMatrixState *state;

  state = g_slice_new (CoglMatrixState);
  state->push_count = 0;
  state->is_identity = FALSE;

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

/* XXX:
 * Operations like scale, translate, rotate etc need to have an
 * initialized state->matrix to work with, so they will pass
 * initialize = TRUE.
 *
 * _cogl_matrix_stack_load_identity and _cogl_matrix_stack_set on the
 * other hand don't so they will pass initialize = FALSE
 *
 * NB: Identity matrices are represented by setting
 * state->is_identity=TRUE in which case state->matrix will be
 * uninitialized.
 */
static CoglMatrixState *
_cogl_matrix_stack_top_mutable (CoglMatrixStack *stack,
                                gboolean initialize)
{
  CoglMatrixState *state;
  CoglMatrixState *new_top;

  state = _cogl_matrix_stack_top (stack);

  if (state->push_count == 0)
    {
      if (state->is_identity && initialize)
        cogl_matrix_init_identity (&state->matrix);
      return state;
    }

  state->push_count -= 1;

  new_top = _cogl_matrix_state_new ();

  if (initialize)
    {
      if (state->is_identity)
        cogl_matrix_init_identity (&new_top->matrix);
      else
        new_top->matrix = state->matrix;

      if (stack->flushed_state == state)
        stack->flushed_state = new_top;
    }

  stack->stack = g_slist_prepend (stack->stack, new_top);

  return new_top;
}

CoglMatrixStack*
_cogl_matrix_stack_new (void)
{
  CoglMatrixStack *stack;
  CoglMatrixState *state;

  stack = g_slice_new0 (CoglMatrixStack);

  state = _cogl_matrix_state_new ();
  state->is_identity = TRUE;

  stack->stack = g_slist_prepend (stack->stack, state);

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

  state = _cogl_matrix_stack_top_mutable (stack, FALSE);

  /* NB: Identity matrices are represented by setting
   * state->is_identity = TRUE and leaving state->matrix
   * uninitialized.
   *
   * This is done to optimize the heavy usage of
   * _cogl_matrix_stack_load_identity by the Cogl Journal.
   */
  if (!state->is_identity)
    {
      state->is_identity = TRUE;

      /* mark dirty */
      stack->flushed_state = NULL;
    }
}

void
_cogl_matrix_stack_scale (CoglMatrixStack *stack,
                          float            x,
                          float            y,
                          float            z)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_scale (&state->matrix, x, y, z);
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
}

void
_cogl_matrix_stack_translate (CoglMatrixStack *stack,
                              float            x,
                              float            y,
                              float            z)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_translate (&state->matrix, x, y, z);
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
}

void
_cogl_matrix_stack_rotate (CoglMatrixStack *stack,
                           float            angle,
                           float            x,
                           float            y,
                           float            z)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_rotate (&state->matrix, angle, x, y, z);
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
}

void
_cogl_matrix_stack_multiply (CoglMatrixStack  *stack,
                             const CoglMatrix *matrix)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_multiply (&state->matrix, &state->matrix, matrix);
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
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

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_frustum (&state->matrix,
                       left, right, bottom, top,
                       z_near, z_far);
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
}

void
_cogl_matrix_stack_perspective (CoglMatrixStack *stack,
                                float            fov_y,
                                float            aspect,
                                float            z_near,
                                float            z_far)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_perspective (&state->matrix,
                           fov_y, aspect, z_near, z_far);
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
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

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_ortho (&state->matrix,
                     left, right, bottom, top, z_near, z_far);
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
}

void
_cogl_matrix_stack_get (CoglMatrixStack *stack,
                        CoglMatrix      *matrix)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top (stack);

  /* NB: identity matrices are lazily initialized because we can often avoid
   * initializing them at all if nothing is pushed on top of them since we
   * load them using glLoadIdentity()
   *
   * The Cogl journal typically loads an identiy matrix because it performs
   * software transformations, which is why we have optimized this case.
   */
  if (state->is_identity)
    cogl_matrix_init_identity (matrix);
  else
    *matrix = state->matrix;
}

void
_cogl_matrix_stack_set (CoglMatrixStack  *stack,
                        const CoglMatrix *matrix)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, FALSE);
  state->matrix = *matrix;
  /* mark dirty */
  stack->flushed_state = NULL;
  state->is_identity = FALSE;
}

void
_cogl_matrix_stack_flush_to_gl (CoglMatrixStack *stack,
                                CoglMatrixMode   mode)
{
  CoglMatrixState *state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  state = _cogl_matrix_stack_top (stack);

  if (stack->flushed_state == state)
    return;

  if (ctx->flushed_matrix_mode != mode)
    {
      GLenum gl_mode;
      switch (mode)
        {
        case COGL_MATRIX_MODELVIEW:
          gl_mode = GL_MODELVIEW;
          break;
        case COGL_MATRIX_PROJECTION:
          gl_mode = GL_PROJECTION;
          break;
        case COGL_MATRIX_TEXTURE:
          gl_mode = GL_TEXTURE;
          break;
        }
      GE (glMatrixMode (gl_mode));
      ctx->flushed_matrix_mode = mode;
    }

  /* Because Cogl defines texture coordinates to have a top left origin and
   * because offscreen draw buffers may be used for rendering to textures we
   * always render upside down to offscreen buffers.
   */
  if (mode == COGL_MATRIX_PROJECTION &&
      cogl_is_offscreen (_cogl_get_draw_buffer ()))
    {
      CoglMatrix flipped_projection;
      CoglMatrix *projection =
        state->is_identity ? &ctx->identity_matrix : &state->matrix;

      cogl_matrix_multiply (&flipped_projection,
                            &ctx->y_flip_matrix, projection);
      GE (glLoadMatrixf (cogl_matrix_get_array (&flipped_projection)));
      stack->flushed_identity = FALSE;
    }
  else
    {
      if (state->is_identity)
        {
          if (!stack->flushed_identity)
            GE (glLoadIdentity ());
          stack->flushed_identity = TRUE;
        }
      else
        {
            GE (glLoadMatrixf (cogl_matrix_get_array (&state->matrix)));
          stack->flushed_identity = FALSE;
        }
    }
  stack->flushed_state = state;
}

void
_cogl_matrix_stack_dirty (CoglMatrixStack *stack)
{
  stack->flushed_state = NULL;
  stack->flushed_identity = FALSE;
}

