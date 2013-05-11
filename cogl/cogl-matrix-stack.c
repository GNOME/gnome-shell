/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010,2012 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-matrix-stack.h"
#include "cogl-framebuffer-private.h"
#include "cogl-object-private.h"
#include "cogl-offscreen.h"
#include "cogl-matrix-private.h"
#include "cogl-magazine-private.h"

static void _cogl_matrix_stack_free (CoglMatrixStack *stack);

COGL_OBJECT_DEFINE (MatrixStack, matrix_stack);

static CoglMagazine *cogl_matrix_stack_magazine;
static CoglMagazine *cogl_matrix_stack_matrices_magazine;

/* XXX: Note: this leaves entry->parent uninitialized! */
static CoglMatrixEntry *
_cogl_matrix_entry_new (CoglMatrixOp operation)
{
  CoglMatrixEntry *entry =
    _cogl_magazine_chunk_alloc (cogl_matrix_stack_magazine);

  entry->ref_count = 1;
  entry->op = operation;

#ifdef COGL_DEBUG_ENABLED
  entry->composite_gets = 0;
#endif

  return entry;
}

static void *
_cogl_matrix_stack_push_entry (CoglMatrixStack *stack,
                               CoglMatrixEntry *entry)
{
  /* NB: The initial reference of the entry is transferred to the
   * stack here.
   *
   * The stack only maintains a reference to the top of the stack (the
   * last entry pushed) and each entry in-turn maintains a reference
   * to its parent.
   *
   * We don't need to take a reference to the parent from the entry
   * here because the we are stealing the reference that was held by
   * the stack while that parent was previously the top of the stack.
   */
  entry->parent = stack->last_entry;
  stack->last_entry = entry;

  return entry;
}

static void *
_cogl_matrix_stack_push_operation (CoglMatrixStack *stack,
                                   CoglMatrixOp operation)
{
  CoglMatrixEntry *entry = _cogl_matrix_entry_new (operation);

  _cogl_matrix_stack_push_entry (stack, entry);

  return entry;
}

static void *
_cogl_matrix_stack_push_replacement_entry (CoglMatrixStack *stack,
                                           CoglMatrixOp operation)
{
  CoglMatrixEntry *old_top = stack->last_entry;
  CoglMatrixEntry *new_top;

  /* This would only be called for operations that completely replace
   * the matrix. In that case we don't need to keep a reference to
   * anything up to the last save entry. This optimisation could be
   * important for applications that aren't using the stack but
   * instead just perform their own matrix manipulations and load a
   * new stack every frame. If this optimisation isn't done then the
   * stack would just grow endlessly. See the comments
   * cogl_matrix_stack_pop for a description of how popping works. */
  for (new_top = old_top;
       new_top->op != COGL_MATRIX_OP_SAVE && new_top->parent;
       new_top = new_top->parent)
    ;

  cogl_matrix_entry_ref (new_top);
  cogl_matrix_entry_unref (old_top);
  stack->last_entry = new_top;

  return _cogl_matrix_stack_push_operation (stack, operation);
}

void
_cogl_matrix_entry_identity_init (CoglMatrixEntry *entry)
{
  entry->ref_count = 1;
  entry->op = COGL_MATRIX_OP_LOAD_IDENTITY;
  entry->parent = NULL;
#ifdef COGL_DEBUG_ENABLED
  entry->composite_gets = 0;
#endif
}

void
cogl_matrix_stack_load_identity (CoglMatrixStack *stack)
{
  _cogl_matrix_stack_push_replacement_entry (stack,
                                             COGL_MATRIX_OP_LOAD_IDENTITY);
}

void
cogl_matrix_stack_translate (CoglMatrixStack *stack,
                              float x,
                              float y,
                              float z)
{
  CoglMatrixEntryTranslate *entry;

  entry = _cogl_matrix_stack_push_operation (stack, COGL_MATRIX_OP_TRANSLATE);

  entry->x = x;
  entry->y = y;
  entry->z = z;
}

void
cogl_matrix_stack_rotate (CoglMatrixStack *stack,
                           float angle,
                           float x,
                           float y,
                           float z)
{
  CoglMatrixEntryRotate *entry;

  entry = _cogl_matrix_stack_push_operation (stack, COGL_MATRIX_OP_ROTATE);

  entry->angle = angle;
  entry->x = x;
  entry->y = y;
  entry->z = z;
}

void
cogl_matrix_stack_rotate_quaternion (CoglMatrixStack *stack,
                                     const CoglQuaternion *quaternion)
{
  CoglMatrixEntryRotateQuaternion *entry;

  entry = _cogl_matrix_stack_push_operation (stack,
                                             COGL_MATRIX_OP_ROTATE_QUATERNION);

  entry->values[0] = quaternion->w;
  entry->values[1] = quaternion->x;
  entry->values[2] = quaternion->y;
  entry->values[3] = quaternion->z;
}

void
cogl_matrix_stack_rotate_euler (CoglMatrixStack *stack,
                                 const CoglEuler *euler)
{
  CoglMatrixEntryRotateEuler *entry;

  entry = _cogl_matrix_stack_push_operation (stack,
                                             COGL_MATRIX_OP_ROTATE_EULER);

  entry->heading = euler->heading;
  entry->pitch = euler->pitch;
  entry->roll = euler->roll;
}

void
cogl_matrix_stack_scale (CoglMatrixStack *stack,
                          float x,
                          float y,
                          float z)
{
  CoglMatrixEntryScale *entry;

  entry = _cogl_matrix_stack_push_operation (stack, COGL_MATRIX_OP_SCALE);

  entry->x = x;
  entry->y = y;
  entry->z = z;
}

void
cogl_matrix_stack_multiply (CoglMatrixStack *stack,
                            const CoglMatrix *matrix)
{
  CoglMatrixEntryMultiply *entry;

  entry = _cogl_matrix_stack_push_operation (stack, COGL_MATRIX_OP_MULTIPLY);

  entry->matrix =
    _cogl_magazine_chunk_alloc (cogl_matrix_stack_matrices_magazine);

  cogl_matrix_init_from_array (entry->matrix, (float *)matrix);
}

void
cogl_matrix_stack_set (CoglMatrixStack *stack,
                       const CoglMatrix *matrix)
{
  CoglMatrixEntryLoad *entry;

  entry =
    _cogl_matrix_stack_push_replacement_entry (stack,
                                               COGL_MATRIX_OP_LOAD);

  entry->matrix =
    _cogl_magazine_chunk_alloc (cogl_matrix_stack_matrices_magazine);

  cogl_matrix_init_from_array (entry->matrix, (float *)matrix);
}

void
cogl_matrix_stack_frustum (CoglMatrixStack *stack,
                            float left,
                            float right,
                            float bottom,
                            float top,
                            float z_near,
                            float z_far)
{
  CoglMatrixEntryLoad *entry;

  entry =
    _cogl_matrix_stack_push_replacement_entry (stack,
                                               COGL_MATRIX_OP_LOAD);

  entry->matrix =
    _cogl_magazine_chunk_alloc (cogl_matrix_stack_matrices_magazine);

  cogl_matrix_init_identity (entry->matrix);
  cogl_matrix_frustum (entry->matrix,
                       left, right, bottom, top,
                       z_near, z_far);
}

void
cogl_matrix_stack_perspective (CoglMatrixStack *stack,
                                float fov_y,
                                float aspect,
                                float z_near,
                                float z_far)
{
  CoglMatrixEntryLoad *entry;

  entry =
    _cogl_matrix_stack_push_replacement_entry (stack,
                                               COGL_MATRIX_OP_LOAD);

  entry->matrix =
    _cogl_magazine_chunk_alloc (cogl_matrix_stack_matrices_magazine);

  cogl_matrix_init_identity (entry->matrix);
  cogl_matrix_perspective (entry->matrix,
                           fov_y, aspect, z_near, z_far);
}

void
cogl_matrix_stack_orthographic (CoglMatrixStack *stack,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2,
                                 float near,
                                 float far)
{
  CoglMatrixEntryLoad *entry;

  entry =
    _cogl_matrix_stack_push_replacement_entry (stack,
                                               COGL_MATRIX_OP_LOAD);

  entry->matrix =
    _cogl_magazine_chunk_alloc (cogl_matrix_stack_matrices_magazine);

  cogl_matrix_init_identity (entry->matrix);
  cogl_matrix_orthographic (entry->matrix,
                            x_1, y_1, x_2, y_2, near, far);
}

void
cogl_matrix_stack_push (CoglMatrixStack *stack)
{
  CoglMatrixEntrySave *entry;

  entry = _cogl_matrix_stack_push_operation (stack, COGL_MATRIX_OP_SAVE);

  entry->cache_valid = FALSE;
}

CoglMatrixEntry *
cogl_matrix_entry_ref (CoglMatrixEntry *entry)
{
  /* A NULL pointer is considered a valid stack so we should accept
     that as an argument */
  if (entry)
    entry->ref_count++;

  return entry;
}

void
cogl_matrix_entry_unref (CoglMatrixEntry *entry)
{
  CoglMatrixEntry *parent;

  for (; entry && --entry->ref_count <= 0; entry = parent)
    {
      parent = entry->parent;

      switch (entry->op)
        {
        case COGL_MATRIX_OP_LOAD_IDENTITY:
        case COGL_MATRIX_OP_TRANSLATE:
        case COGL_MATRIX_OP_ROTATE:
        case COGL_MATRIX_OP_ROTATE_QUATERNION:
        case COGL_MATRIX_OP_ROTATE_EULER:
        case COGL_MATRIX_OP_SCALE:
          break;
        case COGL_MATRIX_OP_MULTIPLY:
          {
            CoglMatrixEntryMultiply *multiply =
              (CoglMatrixEntryMultiply *)entry;
            _cogl_magazine_chunk_free (cogl_matrix_stack_matrices_magazine,
                                       multiply->matrix);
            break;
          }
        case COGL_MATRIX_OP_LOAD:
          {
            CoglMatrixEntryLoad *load = (CoglMatrixEntryLoad *)entry;
            _cogl_magazine_chunk_free (cogl_matrix_stack_matrices_magazine,
                                       load->matrix);
            break;
          }
        case COGL_MATRIX_OP_SAVE:
          {
            CoglMatrixEntrySave *save = (CoglMatrixEntrySave *)entry;
            if (save->cache_valid)
              _cogl_magazine_chunk_free (cogl_matrix_stack_matrices_magazine,
                                         save->cache);
            break;
          }
        }

      _cogl_magazine_chunk_free (cogl_matrix_stack_magazine, entry);
    }
}

void
cogl_matrix_stack_pop (CoglMatrixStack *stack)
{
  CoglMatrixEntry *old_top;
  CoglMatrixEntry *new_top;

  _COGL_RETURN_IF_FAIL (stack != NULL);

  old_top = stack->last_entry;
  _COGL_RETURN_IF_FAIL (old_top != NULL);

  /* To pop we are moving the top of the stack to the old top's parent
   * node. The stack always needs to have a reference to the top entry
   * so we must take a reference to the new top. The stack would have
   * previously had a reference to the old top so we need to decrease
   * the ref count on that. We need to ref the new head first in case
   * this stack was the only thing referencing the old top. In that
   * case the call to cogl_matrix_entry_unref will unref the parent.
   */

  /* Find the last save operation and remove it */

  /* XXX: it would be an error to pop to the very beginning of the
   * stack so we don't need to check for NULL pointer dereferencing. */
  for (new_top = old_top;
       new_top->op != COGL_MATRIX_OP_SAVE;
       new_top = new_top->parent)
    ;

  new_top = new_top->parent;
  cogl_matrix_entry_ref (new_top);

  cogl_matrix_entry_unref (old_top);

  stack->last_entry = new_top;
}

CoglBool
cogl_matrix_stack_get_inverse (CoglMatrixStack *stack,
                                CoglMatrix *inverse)
{
  CoglMatrix matrix;
  CoglMatrix *internal = cogl_matrix_stack_get (stack, &matrix);

  if (internal)
    return cogl_matrix_get_inverse (internal, inverse);
  else
    return cogl_matrix_get_inverse (&matrix, inverse);
}

/* In addition to writing the stack matrix into the give @matrix
 * argument this function *may* sometimes also return a pointer
 * to a matrix too so if we are querying the inverse matrix we
 * should query from the return matrix so that the result can
 * be cached within the stack. */
CoglMatrix *
cogl_matrix_entry_get (CoglMatrixEntry *entry,
                        CoglMatrix *matrix)
{
  int depth;
  CoglMatrixEntry *current;
  CoglMatrixEntry **children;
  int i;

  for (depth = 0, current = entry;
       current;
       current = current->parent, depth++)
    {
      switch (current->op)
        {
        case COGL_MATRIX_OP_LOAD_IDENTITY:
          cogl_matrix_init_identity (matrix);
          goto initialized;
        case COGL_MATRIX_OP_LOAD:
          {
            CoglMatrixEntryLoad *load = (CoglMatrixEntryLoad *)current;
            _cogl_matrix_init_from_matrix_without_inverse (matrix,
                                                           load->matrix);
            goto initialized;
          }
        case COGL_MATRIX_OP_SAVE:
          {
            CoglMatrixEntrySave *save = (CoglMatrixEntrySave *)current;
            if (!save->cache_valid)
              {
                CoglMagazine *matrices_magazine =
                  cogl_matrix_stack_matrices_magazine;
                save->cache = _cogl_magazine_chunk_alloc (matrices_magazine);
                cogl_matrix_entry_get (current->parent, save->cache);
                save->cache_valid = TRUE;
              }
            _cogl_matrix_init_from_matrix_without_inverse (matrix, save->cache);
            goto initialized;
          }
        default:
          continue;
        }
    }

initialized:

  if (depth == 0)
    {
      switch (entry->op)
        {
        case COGL_MATRIX_OP_LOAD_IDENTITY:
        case COGL_MATRIX_OP_TRANSLATE:
        case COGL_MATRIX_OP_ROTATE:
        case COGL_MATRIX_OP_ROTATE_QUATERNION:
        case COGL_MATRIX_OP_ROTATE_EULER:
        case COGL_MATRIX_OP_SCALE:
        case COGL_MATRIX_OP_MULTIPLY:
          return NULL;

        case COGL_MATRIX_OP_LOAD:
          {
            CoglMatrixEntryLoad *load = (CoglMatrixEntryLoad *)entry;
            return load->matrix;
          }
        case COGL_MATRIX_OP_SAVE:
          {
            CoglMatrixEntrySave *save = (CoglMatrixEntrySave *)entry;
            return save->cache;
          }
        }
      g_warn_if_reached ();
      return NULL;
    }

#ifdef COGL_ENABLE_DEBUG
  if (!current)
    {
      g_warning ("Inconsistent matrix stack");
      return NULL;
    }

  entry->composite_gets++;
#endif

  children = g_alloca (sizeof (CoglMatrixEntry) * depth);

  /* We need walk the list of entries from the init/load/save entry
   * back towards the leaf node but the nodes don't link to their
   * children so we need to re-walk them here to add to a separate
   * array. */
  for (i = depth - 1, current = entry;
       i >= 0 && current;
       i--, current = current->parent)
    {
      children[i] = current;
    }

#ifdef COGL_ENABLE_DEBUG
  if (COGL_DEBUG_ENABLED (COGL_DEBUG_PERFORMANCE) &&
      entry->composite_gets >= 2)
    {
      COGL_NOTE (PERFORMANCE,
                 "Re-composing a matrix stack entry multiple times");
    }
#endif

  for (i = 0; i < depth; i++)
    {
      switch (children[i]->op)
        {
        case COGL_MATRIX_OP_TRANSLATE:
          {
            CoglMatrixEntryTranslate *translate =
              (CoglMatrixEntryTranslate *)children[i];
            cogl_matrix_translate (matrix,
                                   translate->x,
                                   translate->y,
                                   translate->z);
            continue;
          }
        case COGL_MATRIX_OP_ROTATE:
          {
            CoglMatrixEntryRotate *rotate=
              (CoglMatrixEntryRotate *)children[i];
            cogl_matrix_rotate (matrix,
                                rotate->angle,
                                rotate->x,
                                rotate->y,
                                rotate->z);
            continue;
          }
        case COGL_MATRIX_OP_ROTATE_EULER:
          {
            CoglMatrixEntryRotateEuler *rotate =
              (CoglMatrixEntryRotateEuler *)children[i];
            CoglEuler euler;
            cogl_euler_init (&euler,
                             rotate->heading,
                             rotate->pitch,
                             rotate->roll);
            cogl_matrix_rotate_euler (matrix,
                                      &euler);
            continue;
          }
        case COGL_MATRIX_OP_ROTATE_QUATERNION:
          {
            CoglMatrixEntryRotateQuaternion *rotate =
              (CoglMatrixEntryRotateQuaternion *)children[i];
            CoglQuaternion quaternion;
            cogl_quaternion_init_from_array (&quaternion, rotate->values);
            cogl_matrix_rotate_quaternion (matrix, &quaternion);
            continue;
          }
        case COGL_MATRIX_OP_SCALE:
          {
            CoglMatrixEntryScale *scale =
              (CoglMatrixEntryScale *)children[i];
            cogl_matrix_scale (matrix,
                               scale->x,
                               scale->y,
                               scale->z);
            continue;
          }
        case COGL_MATRIX_OP_MULTIPLY:
          {
            CoglMatrixEntryMultiply *multiply =
              (CoglMatrixEntryMultiply *)children[i];
            cogl_matrix_multiply (matrix, matrix, multiply->matrix);
            continue;
          }

        case COGL_MATRIX_OP_LOAD_IDENTITY:
        case COGL_MATRIX_OP_LOAD:
        case COGL_MATRIX_OP_SAVE:
          g_warn_if_reached ();
          continue;
        }
    }

  return NULL;
}

CoglMatrixEntry *
cogl_matrix_stack_get_entry (CoglMatrixStack *stack)
{
  return stack->last_entry;
}

/* In addition to writing the stack matrix into the give @matrix
 * argument this function *may* sometimes also return a pointer
 * to a matrix too so if we are querying the inverse matrix we
 * should query from the return matrix so that the result can
 * be cached within the stack. */
CoglMatrix *
cogl_matrix_stack_get (CoglMatrixStack *stack,
                       CoglMatrix *matrix)
{
  return cogl_matrix_entry_get (stack->last_entry, matrix);
}

static void
_cogl_matrix_stack_free (CoglMatrixStack *stack)
{
  cogl_matrix_entry_unref (stack->last_entry);
  g_slice_free (CoglMatrixStack, stack);
}

CoglMatrixStack *
cogl_matrix_stack_new (CoglContext *ctx)
{
  CoglMatrixStack *stack = g_slice_new (CoglMatrixStack);

  if (G_UNLIKELY (cogl_matrix_stack_magazine == NULL))
    {
      cogl_matrix_stack_magazine =
        _cogl_magazine_new (sizeof (CoglMatrixEntryFull), 20);
      cogl_matrix_stack_matrices_magazine =
        _cogl_magazine_new (sizeof (CoglMatrix), 20);
    }

  stack->context = ctx;
  stack->last_entry = NULL;

  cogl_matrix_entry_ref (&ctx->identity_entry);
  _cogl_matrix_stack_push_entry (stack, &ctx->identity_entry);

  return _cogl_matrix_stack_object_new (stack);
}

static CoglMatrixEntry *
_cogl_matrix_entry_skip_saves (CoglMatrixEntry *entry)
{
  /* We currently assume that every stack starts with an
   * _OP_LOAD_IDENTITY so we don't need to worry about
   * NULL pointer dereferencing here. */
  while (entry->op == COGL_MATRIX_OP_SAVE)
    entry = entry->parent;

  return entry;
}

CoglBool
cogl_matrix_entry_calculate_translation (CoglMatrixEntry *entry0,
                                         CoglMatrixEntry *entry1,
                                         float *x,
                                         float *y,
                                         float *z)
{
  GSList *head0 = NULL;
  GSList *head1 = NULL;
  CoglMatrixEntry *node0;
  CoglMatrixEntry *node1;
  int len0 = 0;
  int len1 = 0;
  int count;
  GSList *common_ancestor0;
  GSList *common_ancestor1;

  /* Algorithm:
   *
   * 1) Ignoring _OP_SAVE entries walk the ancestors of each entry to
   *    the root node or any non-translation node, adding a pointer to
   *    each ancestor node to two linked lists.
   *
   * 2) Compare the lists to find the nodes where they start to
   *    differ marking the common_ancestor node for each list.
   *
   * 3) For the list corresponding to entry0, start iterating after
   *    the common ancestor applying the negative of all translations
   *    to x, y and z.
   *
   * 4) For the list corresponding to entry1, start iterating after
   *    the common ancestor applying the positive of all translations
   *    to x, y and z.
   *
   * If we come across any non-translation operations during 3) or 4)
   * then bail out returning FALSE.
   */

  for (node0 = entry0; node0; node0 = node0->parent)
    {
      GSList *link;

      if (node0->op == COGL_MATRIX_OP_SAVE)
        continue;

      link = alloca (sizeof (GSList));
      link->next = head0;
      link->data = node0;
      head0 = link;
      len0++;

      if (node0->op != COGL_MATRIX_OP_TRANSLATE)
        break;
    }
  for (node1 = entry1; node1; node1 = node1->parent)
    {
      GSList *link;

      if (node1->op == COGL_MATRIX_OP_SAVE)
        continue;

      link = alloca (sizeof (GSList));
      link->next = head1;
      link->data = node1;
      head1 = link;
      len1++;

      if (node1->op != COGL_MATRIX_OP_TRANSLATE)
        break;
    }

  if (head0->data != head1->data)
    return FALSE;

  common_ancestor0 = head0;
  common_ancestor1 = head1;
  head0 = head0->next;
  head1 = head1->next;
  count = MIN (len0, len1) - 1;
  while (count--)
    {
      if (head0->data != head1->data)
        break;
      common_ancestor0 = head0;
      common_ancestor1 = head1;
      head0 = head0->next;
      head1 = head1->next;
    }

  *x = 0;
  *y = 0;
  *z = 0;

  for (head0 = common_ancestor0->next; head0; head0 = head0->next)
    {
      CoglMatrixEntryTranslate *translate;

      node0 = head0->data;

      if (node0->op != COGL_MATRIX_OP_TRANSLATE)
        return FALSE;

      translate = (CoglMatrixEntryTranslate *)node0;

      *x = *x - translate->x;
      *y = *y - translate->y;
      *z = *z - translate->z;
    }
  for (head1 = common_ancestor1->next; head1; head1 = head1->next)
    {
      CoglMatrixEntryTranslate *translate;

      node1 = head1->data;

      if (node1->op != COGL_MATRIX_OP_TRANSLATE)
        return FALSE;

      translate = (CoglMatrixEntryTranslate *)node1;

      *x = *x + translate->x;
      *y = *y + translate->y;
      *z = *z + translate->z;
    }

  return TRUE;
}

CoglBool
cogl_matrix_entry_is_identity (CoglMatrixEntry *entry)
{
  return entry ? entry->op == COGL_MATRIX_OP_LOAD_IDENTITY : FALSE;
}

static void
_cogl_matrix_flush_to_gl_builtin (CoglContext *ctx,
                                  CoglBool is_identity,
                                  CoglMatrix *matrix,
                                  CoglMatrixMode mode)
{
  g_assert ((ctx->private_feature_flags & COGL_PRIVATE_FEATURE_GL_FIXED));

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
_cogl_matrix_entry_flush_to_gl_builtins (CoglContext *ctx,
                                         CoglMatrixEntry *entry,
                                         CoglMatrixMode mode,
                                         CoglFramebuffer *framebuffer,
                                         CoglBool disable_flip)
{
  g_assert ((ctx->private_feature_flags & COGL_PRIVATE_FEATURE_GL_FIXED));

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
  {
    CoglBool needs_flip;
    CoglMatrixEntryCache *cache;

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
          needs_flip = cogl_is_offscreen (framebuffer);

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
        _cogl_matrix_entry_cache_maybe_update (cache, entry, needs_flip))
      {
        CoglBool is_identity;
        CoglMatrix matrix;

        if (entry->op == COGL_MATRIX_OP_LOAD_IDENTITY)
          is_identity = TRUE;
        else
          {
            is_identity = FALSE;
            cogl_matrix_entry_get (entry, &matrix);
          }

        if (needs_flip)
          {
            CoglMatrix flipped_matrix;

            cogl_matrix_multiply (&flipped_matrix,
                                  &ctx->y_flip_matrix,
                                  is_identity ?
                                  &ctx->identity_matrix :
                                  &matrix);

            _cogl_matrix_flush_to_gl_builtin (ctx,
                                              /* not identity */
                                              FALSE,
                                              &flipped_matrix,
                                              mode);
          }
        else
          {
            _cogl_matrix_flush_to_gl_builtin (ctx,
                                              is_identity,
                                              &matrix,
                                              mode);
          }
      }
  }
#endif
}

CoglBool
cogl_matrix_entry_equal (CoglMatrixEntry *entry0,
                         CoglMatrixEntry *entry1)
{
  for (;
       entry0 && entry1;
       entry0 = entry0->parent, entry1 = entry1->parent)
    {
      entry0 = _cogl_matrix_entry_skip_saves (entry0);
      entry1 = _cogl_matrix_entry_skip_saves (entry1);

      if (entry0 == entry1)
        return TRUE;

      if (entry0->op != entry1->op)
        return FALSE;

      switch (entry0->op)
        {
        case COGL_MATRIX_OP_LOAD_IDENTITY:
          return TRUE;
        case COGL_MATRIX_OP_TRANSLATE:
          {
            CoglMatrixEntryTranslate *translate0 =
              (CoglMatrixEntryTranslate *)entry0;
            CoglMatrixEntryTranslate *translate1 =
              (CoglMatrixEntryTranslate *)entry1;
            /* We could perhaps use an epsilon to compare here?
             * I expect the false negatives are probaly never going to
             * be a problem and this is a bit cheaper. */
            if (translate0->x != translate1->x ||
                translate0->y != translate1->y ||
                translate0->z != translate1->z)
              return FALSE;
          }
          break;
        case COGL_MATRIX_OP_ROTATE:
          {
            CoglMatrixEntryRotate *rotate0 =
              (CoglMatrixEntryRotate *)entry0;
            CoglMatrixEntryRotate *rotate1 =
              (CoglMatrixEntryRotate *)entry1;
            if (rotate0->angle != rotate1->angle ||
                rotate0->x != rotate1->x ||
                rotate0->y != rotate1->y ||
                rotate0->z != rotate1->z)
              return FALSE;
          }
          break;
        case COGL_MATRIX_OP_ROTATE_QUATERNION:
          {
            CoglMatrixEntryRotateQuaternion *rotate0 =
              (CoglMatrixEntryRotateQuaternion *)entry0;
            CoglMatrixEntryRotateQuaternion *rotate1 =
              (CoglMatrixEntryRotateQuaternion *)entry1;
            int i;
            for (i = 0; i < 4; i++)
              if (rotate0->values[i] != rotate1->values[i])
                return FALSE;
          }
          break;
        case COGL_MATRIX_OP_ROTATE_EULER:
          {
            CoglMatrixEntryRotateEuler *rotate0 =
              (CoglMatrixEntryRotateEuler *)entry0;
            CoglMatrixEntryRotateEuler *rotate1 =
              (CoglMatrixEntryRotateEuler *)entry1;

            if (rotate0->heading != rotate1->heading ||
                rotate0->pitch != rotate1->pitch ||
                rotate0->roll != rotate1->roll)
              return FALSE;
          }
          break;
        case COGL_MATRIX_OP_SCALE:
          {
            CoglMatrixEntryScale *scale0 = (CoglMatrixEntryScale *)entry0;
            CoglMatrixEntryScale *scale1 = (CoglMatrixEntryScale *)entry1;
            if (scale0->x != scale1->x ||
                scale0->y != scale1->y ||
                scale0->z != scale1->z)
              return FALSE;
          }
          break;
        case COGL_MATRIX_OP_MULTIPLY:
          {
            CoglMatrixEntryMultiply *mult0 = (CoglMatrixEntryMultiply *)entry0;
            CoglMatrixEntryMultiply *mult1 = (CoglMatrixEntryMultiply *)entry1;
            if (!cogl_matrix_equal (mult0->matrix, mult1->matrix))
              return FALSE;
          }
          break;
        case COGL_MATRIX_OP_LOAD:
          {
            CoglMatrixEntryLoad *load0 = (CoglMatrixEntryLoad *)entry0;
            CoglMatrixEntryLoad *load1 = (CoglMatrixEntryLoad *)entry1;
            /* There's no need to check any further since an
             * _OP_LOAD makes all the ancestors redundant as far as
             * the final matrix value is concerned. */
            return cogl_matrix_equal (load0->matrix, load1->matrix);
          }
        case COGL_MATRIX_OP_SAVE:
          /* We skip over saves above so we shouldn't see save entries */
          g_warn_if_reached ();
        }
    }

  return FALSE;
}

void
cogl_debug_matrix_entry_print (CoglMatrixEntry *entry)
{
  int depth;
  CoglMatrixEntry *e;
  CoglMatrixEntry **children;
  int i;

  for (depth = 0, e = entry; e; e = e->parent)
    depth++;

  children = g_alloca (sizeof (CoglMatrixEntry) * depth);

  for (i = depth - 1, e = entry;
       i >= 0 && e;
       i--, e = e->parent)
    {
      children[i] = e;
    }

  g_print ("MatrixEntry %p =\n", entry);

  for (i = 0; i < depth; i++)
    {
      entry = children[i];

      switch (entry->op)
        {
        case COGL_MATRIX_OP_LOAD_IDENTITY:
          g_print ("  LOAD IDENTITY\n");
          continue;
        case COGL_MATRIX_OP_TRANSLATE:
          {
            CoglMatrixEntryTranslate *translate =
              (CoglMatrixEntryTranslate *)entry;
            g_print ("  TRANSLATE X=%f Y=%f Z=%f\n",
                     translate->x,
                     translate->y,
                     translate->z);
            continue;
          }
        case COGL_MATRIX_OP_ROTATE:
          {
            CoglMatrixEntryRotate *rotate =
              (CoglMatrixEntryRotate *)entry;
            g_print ("  ROTATE ANGLE=%f X=%f Y=%f Z=%f\n",
                     rotate->angle,
                     rotate->x,
                     rotate->y,
                     rotate->z);
            continue;
          }
        case COGL_MATRIX_OP_ROTATE_QUATERNION:
          {
            CoglMatrixEntryRotateQuaternion *rotate =
              (CoglMatrixEntryRotateQuaternion *)entry;
            g_print ("  ROTATE QUATERNION w=%f x=%f y=%f z=%f\n",
                     rotate->values[0],
                     rotate->values[1],
                     rotate->values[2],
                     rotate->values[3]);
            continue;
          }
        case COGL_MATRIX_OP_ROTATE_EULER:
          {
            CoglMatrixEntryRotateEuler *rotate =
              (CoglMatrixEntryRotateEuler *)entry;
            g_print ("  ROTATE EULER heading=%f pitch=%f roll=%f\n",
                     rotate->heading,
                     rotate->pitch,
                     rotate->roll);
            continue;
          }
        case COGL_MATRIX_OP_SCALE:
          {
            CoglMatrixEntryScale *scale = (CoglMatrixEntryScale *)entry;
            g_print ("  SCALE X=%f Y=%f Z=%f\n",
                     scale->x,
                     scale->y,
                     scale->z);
            continue;
          }
        case COGL_MATRIX_OP_MULTIPLY:
          {
            CoglMatrixEntryMultiply *mult = (CoglMatrixEntryMultiply *)entry;
            g_print ("  MULT:\n");
            _cogl_matrix_prefix_print ("    ", mult->matrix);
            continue;
          }
        case COGL_MATRIX_OP_LOAD:
          {
            CoglMatrixEntryLoad *load = (CoglMatrixEntryLoad *)entry;
            g_print ("  LOAD:\n");
            _cogl_matrix_prefix_print ("    ", load->matrix);
            continue;
          }
        case COGL_MATRIX_OP_SAVE:
          g_print ("  SAVE\n");
        }
    }
}

void
_cogl_matrix_entry_cache_init (CoglMatrixEntryCache *cache)
{
  cache->entry = NULL;
  cache->flushed_identity = FALSE;
  cache->flipped = FALSE;
}

/* NB: This function can report false negatives since it never does a
 * deep comparison of the stack matrices. */
CoglBool
_cogl_matrix_entry_cache_maybe_update (CoglMatrixEntryCache *cache,
                                       CoglMatrixEntry *entry,
                                       CoglBool flip)
{
  CoglBool is_identity;
  CoglBool updated = FALSE;

  if (cache->flipped != flip)
    {
      cache->flipped = flip;
      updated = TRUE;
    }

  is_identity = (entry->op == COGL_MATRIX_OP_LOAD_IDENTITY);
  if (cache->flushed_identity != is_identity)
    {
      cache->flushed_identity = is_identity;
      updated = TRUE;
    }

  if (cache->entry != entry)
    {
      cogl_matrix_entry_ref (entry);
      if (cache->entry)
        cogl_matrix_entry_unref (cache->entry);
      cache->entry = entry;

      /* We want to make sure here that if the cache->entry and the
       * given @entry are both identity matrices then even though they
       * are different entries we don't want to consider this an
       * update...
       */
      updated |= !is_identity;
    }

  return updated;
}

void
_cogl_matrix_entry_cache_destroy (CoglMatrixEntryCache *cache)
{
  if (cache->entry)
    cogl_matrix_entry_unref (cache->entry);
}
