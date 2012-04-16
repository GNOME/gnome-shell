/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010 Intel Corporation.
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
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-internal.h"
#include "cogl-matrix-stack.h"
#include "cogl-framebuffer-private.h"
#include "cogl-object-private.h"
#include "cogl-offscreen.h"

typedef struct {
  CoglMatrix matrix;
  CoglBool is_identity;
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
  CoglObject _parent;

  GArray *stack;

  unsigned int age;
};

static void _cogl_matrix_stack_free (CoglMatrixStack *stack);

COGL_OBJECT_INTERNAL_DEFINE (MatrixStack, matrix_stack);

/* XXX: this doesn't initialize the matrix! */
static void
_cogl_matrix_state_init (CoglMatrixState *state)
{
  state->push_count = 0;
  state->is_identity = FALSE;
}

static CoglMatrixState *
_cogl_matrix_stack_top (CoglMatrixStack *stack)
{
  return &g_array_index (stack->stack, CoglMatrixState, stack->stack->len - 1);
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
                                CoglBool initialize)
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

  g_array_set_size (stack->stack, stack->stack->len + 1);
  /* if g_array_set_size reallocs we need to get state
   * pointer again */
  state = &g_array_index (stack->stack, CoglMatrixState,
                            stack->stack->len - 2);
  new_top = _cogl_matrix_stack_top(stack);
  _cogl_matrix_state_init (new_top);

  if (initialize)
    {
      if (state->is_identity)
        cogl_matrix_init_identity (&new_top->matrix);
      else
        new_top->matrix = state->matrix;
    }

  return new_top;
}

CoglMatrixStack*
_cogl_matrix_stack_new (void)
{
  CoglMatrixStack *stack;
  CoglMatrixState *state;

  stack = g_slice_new0 (CoglMatrixStack);

  stack->stack = g_array_sized_new (FALSE, FALSE,
                                    sizeof (CoglMatrixState), 10);
  g_array_set_size (stack->stack, 1);
  state = &g_array_index (stack->stack, CoglMatrixState, 0);
  _cogl_matrix_state_init (state);
  state->is_identity = TRUE;

  stack->age = 0;

  return _cogl_matrix_stack_object_new (stack);
}

static void
_cogl_matrix_stack_free (CoglMatrixStack *stack)
{
  g_array_free (stack->stack, TRUE);
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
      if (stack->stack->len == 1)
        {
          g_warning ("Too many matrix pops");
          return;
        }

      stack->age++;
      g_array_set_size (stack->stack, stack->stack->len - 1);
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
      stack->age++;
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
  state->is_identity = FALSE;
  stack->age++;
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
  state->is_identity = FALSE;
  stack->age++;
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
  state->is_identity = FALSE;
  stack->age++;
}

void
_cogl_matrix_stack_multiply (CoglMatrixStack  *stack,
                             const CoglMatrix *matrix)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);
  cogl_matrix_multiply (&state->matrix, &state->matrix, matrix);
  state->is_identity = FALSE;
  stack->age++;
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
  state->is_identity = FALSE;
  stack->age++;
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
  state->is_identity = FALSE;
  stack->age++;
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
  state->is_identity = FALSE;
  stack->age++;
}

CoglBool
_cogl_matrix_stack_get_inverse (CoglMatrixStack *stack,
                                CoglMatrix      *inverse)
{
  CoglMatrixState *state;

  state = _cogl_matrix_stack_top_mutable (stack, TRUE);

  return cogl_matrix_get_inverse (&state->matrix, inverse);
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
  state->is_identity = FALSE;
  stack->age++;
}

static void
_cogl_matrix_stack_flush_matrix_to_gl_builtin (CoglContext *ctx,
                                               CoglBool is_identity,
                                               CoglMatrix *matrix,
                                               CoglMatrixMode mode)
{
  g_assert (ctx->driver == COGL_DRIVER_GL ||
            ctx->driver == COGL_DRIVER_GLES1);

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
  if (ctx->flushed_matrix_mode != mode)
    {
      GLenum gl_mode = 0;

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

      GE (ctx, glMatrixMode (gl_mode));
      ctx->flushed_matrix_mode = mode;
    }

  if (is_identity)
    GE (ctx, glLoadIdentity ());
  else
    GE (ctx, glLoadMatrixf (cogl_matrix_get_array (matrix)));
#endif
}

void
_cogl_matrix_stack_flush_to_gl_builtins (CoglContext *ctx,
                                         CoglMatrixStack *stack,
                                         CoglMatrixMode mode,
                                         CoglBool disable_flip)
{
  g_assert (ctx->driver == COGL_DRIVER_GL ||
            ctx->driver == COGL_DRIVER_GLES1);

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
  {
    CoglBool needs_flip;
    CoglMatrixState *state;
    CoglMatrixStackCache *cache;

    state = _cogl_matrix_stack_top (stack);

    if (mode == COGL_MATRIX_PROJECTION)
      {
        /* Because Cogl defines texture coordinates to have a top left
         * origin and because offscreen framebuffers may be used for
         * rendering to textures we always render upside down to
         * offscreen buffers. Also for some backends we need to render
         * onscreen buffers upside-down too.
         */
        if (disable_flip)
          needs_flip = FALSE;
        else
          needs_flip = cogl_is_offscreen (ctx->current_draw_buffer);

        cache = &ctx->builtin_flushed_projection;
      }
    else
      {
        needs_flip = FALSE;

        if (mode == COGL_MATRIX_MODELVIEW)
          cache = &ctx->builtin_flushed_modelview;
        else
          cache = NULL;
      }

    /* We don't need to do anything if the state is the same */
    if (!cache ||
        _cogl_matrix_stack_check_and_update_cache (stack, cache, needs_flip))
      {
        CoglBool is_identity = state->is_identity && !needs_flip;

        if (needs_flip)
          {
            CoglMatrix flipped_matrix;

            cogl_matrix_multiply (&flipped_matrix,
                                  &ctx->y_flip_matrix,
                                  state->is_identity ?
                                  &ctx->identity_matrix :
                                  &state->matrix);

            _cogl_matrix_stack_flush_matrix_to_gl_builtin (ctx,
                                                           /* not identity */
                                                           FALSE,
                                                           &flipped_matrix,
                                                           mode);
          }
        else
          _cogl_matrix_stack_flush_matrix_to_gl_builtin (ctx,
                                                         is_identity,
                                                         &state->matrix,
                                                         mode);
      }
  }
#endif
}

unsigned int
_cogl_matrix_stack_get_age (CoglMatrixStack *stack)
{
  return stack->age;
}

CoglBool
_cogl_matrix_stack_has_identity_flag (CoglMatrixStack *stack)
{
  return _cogl_matrix_stack_top (stack)->is_identity;
}

CoglBool
_cogl_matrix_stack_equal (CoglMatrixStack *stack0,
                          CoglMatrixStack *stack1)
{
  CoglMatrixState *state0 = _cogl_matrix_stack_top (stack0);
  CoglMatrixState *state1 = _cogl_matrix_stack_top (stack1);

  if (state0->is_identity != state1->is_identity)
    return FALSE;

  if (state0->is_identity)
    return TRUE;
  else
    return cogl_matrix_equal (&state0->matrix, &state1->matrix);
}

CoglBool
_cogl_matrix_stack_check_and_update_cache (CoglMatrixStack *stack,
                                           CoglMatrixStackCache *cache,
                                           CoglBool flip)
{
  CoglBool is_identity =
    _cogl_matrix_stack_has_identity_flag (stack) && !flip;
  CoglBool is_dirty;

  if (is_identity && cache->flushed_identity)
    is_dirty = FALSE;
  else if (cache->stack == NULL ||
           cache->stack->age != cache->age ||
           flip != cache->flipped)
    is_dirty = TRUE;
  else
    is_dirty = (cache->stack != stack &&
                !_cogl_matrix_stack_equal (cache->stack, stack));

  /* We'll update the cache values even if the stack isn't dirty in
     case the reason it wasn't dirty is because we compared the
     matrices and found them to be the same. In that case updating the
     cache values will avoid the comparison next time */
  cache->age = stack->age;
  cogl_object_ref (stack);
  if (cache->stack)
    cogl_object_unref (cache->stack);
  cache->stack = stack;
  cache->flushed_identity = is_identity;
  cache->flipped = flip;

  return is_dirty;
}

void
_cogl_matrix_stack_init_cache (CoglMatrixStackCache *cache)
{
  cache->stack = NULL;
  cache->flushed_identity = FALSE;
}

void
_cogl_matrix_stack_destroy_cache (CoglMatrixStackCache *cache)
{
  if (cache->stack)
    cogl_object_unref (cache->stack);
}
