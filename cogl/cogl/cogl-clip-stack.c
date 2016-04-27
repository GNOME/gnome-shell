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

#include "cogl-clip-stack.h"
#include "cogl-primitives.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-journal-private.h"
#include "cogl-util.h"
#include "cogl-matrix-private.h"
#include "cogl-primitives-private.h"
#include "cogl-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-attribute-private.h"
#include "cogl-primitive-private.h"
#include "cogl1-context.h"
#include "cogl-offscreen.h"
#include "cogl-matrix-stack.h"



static void *
_cogl_clip_stack_push_entry (CoglClipStack *clip_stack,
                             size_t size,
                             CoglClipStackType type)
{
  CoglClipStack *entry = g_slice_alloc (size);

  /* The new entry starts with a ref count of 1 because the stack
     holds a reference to it as it is the top entry */
  entry->ref_count = 1;
  entry->type = type;
  entry->parent = clip_stack;

  /* We don't need to take a reference to the parent from the entry
     because the we are stealing the ref in the new stack top */

  return entry;
}

static void
get_transformed_corners (float x_1,
                         float y_1,
                         float x_2,
                         float y_2,
                         CoglMatrix *modelview,
                         CoglMatrix *projection,
                         const float *viewport,
                         float *transformed_corners)
{
  int i;

  transformed_corners[0] = x_1;
  transformed_corners[1] = y_1;
  transformed_corners[2] = x_2;
  transformed_corners[3] = y_1;
  transformed_corners[4] = x_2;
  transformed_corners[5] = y_2;
  transformed_corners[6] = x_1;
  transformed_corners[7] = y_2;


  /* Project the coordinates to window space coordinates */
  for (i = 0; i < 4; i++)
    {
      float *v = transformed_corners + i * 2;
      _cogl_transform_point (modelview, projection, viewport, v, v + 1);
    }
}

/* Sets the window-space bounds of the entry based on the projected
   coordinates of the given rectangle */
static void
_cogl_clip_stack_entry_set_bounds (CoglClipStack *entry,
                                   float *transformed_corners)
{
  float min_x = G_MAXFLOAT, min_y = G_MAXFLOAT;
  float max_x = -G_MAXFLOAT, max_y = -G_MAXFLOAT;
  int i;

  for (i = 0; i < 4; i++)
    {
      float *v = transformed_corners + i * 2;

      if (v[0] > max_x)
        max_x = v[0];
      if (v[0] < min_x)
        min_x = v[0];
      if (v[1] > max_y)
        max_y = v[1];
      if (v[1] < min_y)
        min_y = v[1];
    }

  entry->bounds_x0 = floorf (min_x);
  entry->bounds_x1 = ceilf (max_x);
  entry->bounds_y0 = floorf (min_y);
  entry->bounds_y1 = ceilf (max_y);
}

CoglClipStack *
_cogl_clip_stack_push_window_rectangle (CoglClipStack *stack,
                                        int x_offset,
                                        int y_offset,
                                        int width,
                                        int height)
{
  CoglClipStack *entry;

  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackWindowRect),
                                       COGL_CLIP_STACK_WINDOW_RECT);

  entry->bounds_x0 = x_offset;
  entry->bounds_x1 = x_offset + width;
  entry->bounds_y0 = y_offset;
  entry->bounds_y1 = y_offset + height;

  return entry;
}

CoglClipStack *
_cogl_clip_stack_push_rectangle (CoglClipStack *stack,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2,
                                 CoglMatrixEntry *modelview_entry,
                                 CoglMatrixEntry *projection_entry,
                                 const float *viewport)
{
  CoglClipStackRect *entry;
  CoglMatrix modelview;
  CoglMatrix projection;
  CoglMatrix modelview_projection;

  /* Corners of the given rectangle in an clockwise order:
   *  (0, 1)     (2, 3)
   *
   *
   *
   *  (6, 7)     (4, 5)
   */
  float rect[] = {
    x_1, y_1,
    x_2, y_1,
    x_2, y_2,
    x_1, y_2
  };

  /* Make a new entry */
  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackRect),
                                       COGL_CLIP_STACK_RECT);

  entry->x0 = x_1;
  entry->y0 = y_1;
  entry->x1 = x_2;
  entry->y1 = y_2;

  entry->matrix_entry = cogl_matrix_entry_ref (modelview_entry);

  cogl_matrix_entry_get (modelview_entry, &modelview);
  cogl_matrix_entry_get (projection_entry, &projection);

  cogl_matrix_multiply (&modelview_projection,
                        &projection,
                        &modelview);

  /* Technically we could avoid the viewport transform at this point
   * if we want to make this a bit faster. */
  _cogl_transform_point (&modelview, &projection, viewport, &rect[0], &rect[1]);
  _cogl_transform_point (&modelview, &projection, viewport, &rect[2], &rect[3]);
  _cogl_transform_point (&modelview, &projection, viewport, &rect[4], &rect[5]);
  _cogl_transform_point (&modelview, &projection, viewport, &rect[6], &rect[7]);

  /* If the fully transformed rectangle isn't still axis aligned we
   * can't handle it using a scissor.
   *
   * We don't use an epsilon here since we only really aim to catch
   * simple cases where the transform doesn't leave the rectangle screen
   * aligned and don't mind some false positives.
   */
  if (rect[0] != rect[6] ||
      rect[1] != rect[3] ||
      rect[2] != rect[4] ||
      rect[7] != rect[5])
    {
      entry->can_be_scissor = FALSE;

      _cogl_clip_stack_entry_set_bounds ((CoglClipStack *) entry,
                                         rect);
    }
  else
    {
      CoglClipStack *base_entry = (CoglClipStack *) entry;
      x_1 = rect[0];
      y_1 = rect[1];
      x_2 = rect[4];
      y_2 = rect[5];

      /* Consider that the modelview matrix may flip the rectangle
       * along the x or y axis... */
#define SWAP(A,B) do { float tmp = B; B = A; A = tmp; } while (0)
      if (x_1 > x_2)
        SWAP (x_1, x_2);
      if (y_1 > y_2)
        SWAP (y_1, y_2);
#undef SWAP

      base_entry->bounds_x0 = COGL_UTIL_NEARBYINT (x_1);
      base_entry->bounds_y0 = COGL_UTIL_NEARBYINT (y_1);
      base_entry->bounds_x1 = COGL_UTIL_NEARBYINT (x_2);
      base_entry->bounds_y1 = COGL_UTIL_NEARBYINT (y_2);
      entry->can_be_scissor = TRUE;
    }

  return (CoglClipStack *) entry;
}

CoglClipStack *
_cogl_clip_stack_push_primitive (CoglClipStack *stack,
                                 CoglPrimitive *primitive,
                                 float bounds_x1,
                                 float bounds_y1,
                                 float bounds_x2,
                                 float bounds_y2,
                                 CoglMatrixEntry *modelview_entry,
                                 CoglMatrixEntry *projection_entry,
                                 const float *viewport)
{
  CoglClipStackPrimitive *entry;
  CoglMatrix modelview;
  CoglMatrix projection;
  float transformed_corners[8];

  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackPrimitive),
                                       COGL_CLIP_STACK_PRIMITIVE);

  entry->primitive = cogl_object_ref (primitive);

  entry->matrix_entry = cogl_matrix_entry_ref (modelview_entry);

  entry->bounds_x1 = bounds_x1;
  entry->bounds_y1 = bounds_y1;
  entry->bounds_x2 = bounds_x2;
  entry->bounds_y2 = bounds_y2;

  cogl_matrix_entry_get (modelview_entry, &modelview);
  cogl_matrix_entry_get (projection_entry, &projection);

  get_transformed_corners (bounds_x1, bounds_y1, bounds_x2, bounds_y2,
                           &modelview,
                           &projection,
                           viewport,
                           transformed_corners);

  /* NB: this is referring to the bounds in window coordinates as opposed
   * to the bounds above in primitive local coordinates. */
  _cogl_clip_stack_entry_set_bounds ((CoglClipStack *) entry,
                                     transformed_corners);

  return (CoglClipStack *) entry;
}

CoglClipStack *
_cogl_clip_stack_ref (CoglClipStack *entry)
{
  /* A NULL pointer is considered a valid stack so we should accept
     that as an argument */
  if (entry)
    entry->ref_count++;

  return entry;
}

void
_cogl_clip_stack_unref (CoglClipStack *entry)
{
  /* Unref all of the entries until we hit the root of the list or the
     entry still has a remaining reference */
  while (entry && --entry->ref_count <= 0)
    {
      CoglClipStack *parent = entry->parent;

      switch (entry->type)
        {
        case COGL_CLIP_STACK_RECT:
          {
            CoglClipStackRect *rect = (CoglClipStackRect *) entry;
            cogl_matrix_entry_unref (rect->matrix_entry);
            g_slice_free1 (sizeof (CoglClipStackRect), entry);
            break;
          }
        case COGL_CLIP_STACK_WINDOW_RECT:
          g_slice_free1 (sizeof (CoglClipStackWindowRect), entry);
          break;
        case COGL_CLIP_STACK_PRIMITIVE:
          {
            CoglClipStackPrimitive *primitive_entry =
              (CoglClipStackPrimitive *) entry;
            cogl_matrix_entry_unref (primitive_entry->matrix_entry);
            cogl_object_unref (primitive_entry->primitive);
            g_slice_free1 (sizeof (CoglClipStackPrimitive), entry);
            break;
          }
        default:
          g_assert_not_reached ();
        }

      entry = parent;
    }
}

CoglClipStack *
_cogl_clip_stack_pop (CoglClipStack *stack)
{
  CoglClipStack *new_top;

  _COGL_RETURN_VAL_IF_FAIL (stack != NULL, NULL);

  /* To pop we are moving the top of the stack to the old top's parent
     node. The stack always needs to have a reference to the top entry
     so we must take a reference to the new top. The stack would have
     previously had a reference to the old top so we need to decrease
     the ref count on that. We need to ref the new head first in case
     this stack was the only thing referencing the old top. In that
     case the call to _cogl_clip_stack_entry_unref will unref the
     parent. */
  new_top = stack->parent;

  _cogl_clip_stack_ref (new_top);

  _cogl_clip_stack_unref (stack);

  return new_top;
}

void
_cogl_clip_stack_get_bounds (CoglClipStack *stack,
                             int *scissor_x0,
                             int *scissor_y0,
                             int *scissor_x1,
                             int *scissor_y1)
{
  CoglClipStack *entry;

  *scissor_x0 = 0;
  *scissor_y0 = 0;
  *scissor_x1 = G_MAXINT;
  *scissor_y1 = G_MAXINT;

  for (entry = stack; entry; entry = entry->parent)
    {
      /* Get the intersection of the current scissor and the bounding
         box of this clip */
      _cogl_util_scissor_intersect (entry->bounds_x0,
                                    entry->bounds_y0,
                                    entry->bounds_x1,
                                    entry->bounds_y1,
                                    scissor_x0,
                                    scissor_y0,
                                    scissor_x1,
                                    scissor_y1);
    }
}

void
_cogl_clip_stack_flush (CoglClipStack *stack,
                        CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  ctx->driver_vtable->clip_stack_flush (stack, framebuffer);
}
