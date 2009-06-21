/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>

#include "cogl.h"
#include "cogl-clip-stack.h"
#include "cogl-primitives.h"
#include "cogl-context.h"
#include "cogl-internal.h"

/* These are defined in the particular backend (float in GL vs fixed
   in GL ES) */
void _cogl_set_clip_planes (float x,
			    float y,
			    float width,
			    float height);
void _cogl_add_stencil_clip (float x,
			     float y,
			     float width,
			     float height,
			     gboolean     first);
void _cogl_add_path_to_stencil_buffer (floatVec2 nodes_min,
                                       floatVec2 nodes_max,
                                       guint         path_size,
                                       CoglPathNode *path,
                                       gboolean      merge);
void _cogl_enable_clip_planes (void);
void _cogl_disable_clip_planes (void);
void _cogl_disable_stencil_buffer (void);
void _cogl_set_matrix (const CoglMatrix *matrix);

typedef struct _CoglClipStack CoglClipStack;

typedef struct _CoglClipStackEntryRect CoglClipStackEntryRect;
typedef struct _CoglClipStackEntryWindowRect CoglClipStackEntryWindowRect;
typedef struct _CoglClipStackEntryPath CoglClipStackEntryPath;

typedef enum
  {
    COGL_CLIP_STACK_RECT,
    COGL_CLIP_STACK_WINDOW_RECT,
    COGL_CLIP_STACK_PATH
  } CoglClipStackEntryType;

struct _CoglClipStack
{
  GList *stack_top;
};

struct _CoglClipStackEntryRect
{
  CoglClipStackEntryType type;

  /* The rectangle for this clip */
  float                  x_offset;
  float                  y_offset;
  float                  width;
  float                  height;

  /* The matrix that was current when the clip was set */
  CoglMatrix             matrix;
};

struct _CoglClipStackEntryWindowRect
{
  CoglClipStackEntryType type;

  /* The window space rectangle for this clip */
  float                  x0;
  float                  y0;
  float                  x1;
  float                  y1;
};

struct _CoglClipStackEntryPath
{
  CoglClipStackEntryType type;

  /* The matrix that was current when the clip was set */
  CoglMatrix             matrix;

  floatVec2              path_nodes_min;
  floatVec2              path_nodes_max;

  guint                  path_size;
  CoglPathNode           path[1];
};

void
cogl_clip_push_window_rect (float x_offset,
	                    float y_offset,
	                    float width,
	                    float height)
{
  CoglClipStackEntryWindowRect *entry;
  CoglClipStack *stack;
  float v[4];

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  cogl_get_viewport (v);

  entry = g_slice_new (CoglClipStackEntryWindowRect);

  /* We convert from coords with (0,0) at top left to coords
   * with (0,0) at bottom left. */
  entry->type = COGL_CLIP_STACK_WINDOW_RECT;
  entry->x0 = x_offset;
  entry->y0 = v[3] - y_offset - height;
  entry->x1 = x_offset + width;
  entry->y1 = v[3] - y_offset;

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  ctx->clip.stack_dirty = TRUE;
}

/* Scale from OpenGL <-1,1> coordinates system to window coordinates
 * <0,window-size> with (0,0) being top left. */
#define VIEWPORT_SCALE_X(x, w, width, origin) \
    ((((((x) / (w)) + 1.0) / 2) * (width)) + (origin))
#define VIEWPORT_SCALE_Y(y, w, height, origin) \
    ((height) - (((((y) / (w)) + 1.0) / 2) * (height)) + (origin))

static void
transform_point (CoglMatrix *matrix_mv,
                 CoglMatrix *matrix_p,
                 float *viewport,
                 float *x,
                 float *y)
{
  float z = 0;
  float w = 1;

  /* Apply the model view matrix */
  cogl_matrix_transform_point (matrix_mv, x, y, &z, &w);

  /* Apply the projection matrix */
  cogl_matrix_transform_point (matrix_p, x, y, &z, &w);
  /* Apply viewport transform */
  *x = VIEWPORT_SCALE_X (*x, w, viewport[2], viewport[0]);
  *y = VIEWPORT_SCALE_Y (*y, w, viewport[3], viewport[1]);
}

#undef VIEWPORT_SCALE_X
#undef VIEWPORT_SCALE_Y

/* Try to push a rectangle given in object coordinates as a rectangle in window
 * coordinates instead of object coordinates */
gboolean
try_pushing_rect_as_window_rect (float x_offset,
	                         float y_offset,
                                 float width,
                                 float height)
{
  CoglMatrix matrix;
  CoglMatrix matrix_p;
  float v[4];
  float _x0 = x_offset;
  float _y0 = y_offset;
  float _x1 = x_offset + width;
  float _y1 = y_offset + height;

  cogl_get_modelview_matrix (&matrix);

  if (matrix.xy != 0 || matrix.xz != 0 ||
      matrix.yx != 0 || matrix.yz != 0 ||
      matrix.zx != 0 || matrix.zy != 0)
    return FALSE;

  cogl_get_projection_matrix (&matrix_p);
  cogl_get_viewport (v);

  transform_point (&matrix, &matrix_p, v, &_x0, &_y0);
  transform_point (&matrix, &matrix_p, v, &_x1, &_y1);

  cogl_clip_push_window_rect (_x0, _y0, _x1 - _x0, _y1 - _y0);
  return TRUE;
}

void
cogl_clip_push (float x_offset,
	        float y_offset,
	        float width,
	        float height)
{
  CoglClipStackEntryRect *entry;
  CoglClipStack *stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Try and catch window space rectangles so we can redirect to
   * cogl_clip_push_window_rect which will use scissoring. */
  if (try_pushing_rect_as_window_rect (x_offset, y_offset, width, height))
    return;

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  entry = g_slice_new (CoglClipStackEntryRect);

  /* Make a new entry */
  entry->type = COGL_CLIP_STACK_RECT;
  entry->x_offset = x_offset;
  entry->y_offset = y_offset;
  entry->width = width;
  entry->height = height;

  cogl_get_modelview_matrix (&entry->matrix);

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  ctx->clip.stack_dirty = TRUE;
}

void
cogl_clip_push_from_path_preserve (void)
{
  CoglClipStackEntryPath *entry;
  CoglClipStack *stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  entry = g_malloc (sizeof (CoglClipStackEntryPath)
                    + sizeof (CoglPathNode) * (ctx->path_nodes->len - 1));

  entry->type = COGL_CLIP_STACK_PATH;
  entry->path_nodes_min = ctx->path_nodes_min;
  entry->path_nodes_max = ctx->path_nodes_max;
  entry->path_size = ctx->path_nodes->len;
  memcpy (entry->path, ctx->path_nodes->data,
          sizeof (CoglPathNode) * ctx->path_nodes->len);

  cogl_get_modelview_matrix (&entry->matrix);

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  ctx->clip.stack_dirty = TRUE;
}

void
cogl_clip_push_from_path (void)
{
  cogl_clip_push_from_path_preserve ();

  cogl_path_new ();
}

void
cogl_clip_pop (void)
{
  gpointer entry;
  CoglClipStack *stack;
  CoglClipStackEntryType type;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  g_return_if_fail (stack->stack_top != NULL);

  entry = stack->stack_top->data;
  type = *(CoglClipStackEntryType *) entry;

  /* Remove the top entry from the stack */
  if (type == COGL_CLIP_STACK_RECT)
    g_slice_free (CoglClipStackEntryRect, entry);
  else if (type == COGL_CLIP_STACK_WINDOW_RECT)
    g_slice_free (CoglClipStackEntryWindowRect, entry);
  else
    g_free (entry);

  stack->stack_top = g_list_delete_link (stack->stack_top,
                                         stack->stack_top);

  ctx->clip.stack_dirty = TRUE;
}

void
_cogl_clip_stack_rebuild (void)
{
  int has_clip_planes = cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES);
  gboolean using_clip_planes = FALSE;
  gboolean using_stencil_buffer = FALSE;
  GList *node;
  CoglClipStack *stack;
  gint scissor_x0 = 0;
  gint scissor_y0 = 0;
  gint scissor_x1 = G_MAXINT;
  gint scissor_y1 = G_MAXINT;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  ctx->clip.stack_dirty = FALSE;
  ctx->clip.stencil_used = FALSE;

  _cogl_disable_clip_planes ();
  _cogl_disable_stencil_buffer ();
  GE (glDisable (GL_SCISSOR_TEST));

  /* If the stack is empty then there's nothing else to do */
  if (stack->stack_top == NULL)
    return;

  /* Find the bottom of the stack */
  for (node = stack->stack_top; node->next; node = node->next);

  /* Re-add every entry from the bottom of the stack up */
  for (; node; node = node->prev)
    {
      gpointer entry = node->data;
      CoglClipStackEntryType type = *(CoglClipStackEntryType *) entry;

      if (type == COGL_CLIP_STACK_PATH)
        {
          CoglClipStackEntryPath *path = (CoglClipStackEntryPath *) entry;

          cogl_push_matrix ();
          _cogl_set_matrix (&path->matrix);

          _cogl_add_path_to_stencil_buffer (path->path_nodes_min,
                                            path->path_nodes_max,
                                            path->path_size,
                                            path->path,
                                            using_stencil_buffer);

          cogl_pop_matrix ();

          using_stencil_buffer = TRUE;

          /* We can't use clip planes any more */
          has_clip_planes = FALSE;
        }
      else if (type == COGL_CLIP_STACK_RECT)
        {
          CoglClipStackEntryRect *rect = (CoglClipStackEntryRect *) entry;

          cogl_push_matrix ();
          _cogl_set_matrix (&rect->matrix);

          /* If this is the first entry and we support clip planes then use
             that instead */
          if (has_clip_planes)
            {
              _cogl_set_clip_planes (rect->x_offset,
                                     rect->y_offset,
                                     rect->width,
                                     rect->height);
              using_clip_planes = TRUE;
              /* We can't use clip planes a second time */
              has_clip_planes = FALSE;
            }
          else
            {
              _cogl_add_stencil_clip (rect->x_offset,
                                      rect->y_offset,
                                      rect->width,
                                      rect->height,
                                      !using_stencil_buffer);
              using_stencil_buffer = TRUE;
            }

          cogl_pop_matrix ();
        }
      else
        {
          /* Get the intersection of all window space rectangles in the clip
           * stack */
          CoglClipStackEntryWindowRect *window_rect = entry;
          scissor_x0 = MAX (scissor_x0, window_rect->x0);
          scissor_y0 = MAX (scissor_y0, window_rect->y0);
          scissor_x1 = MIN (scissor_x1, window_rect->x1);
          scissor_y1 = MIN (scissor_y1, window_rect->y1);
        }
    }

  /* Enabling clip planes is delayed to now so that they won't affect
     setting up the stencil buffer */
  if (using_clip_planes)
    _cogl_enable_clip_planes ();

  if (scissor_x0 >= scissor_x1 || scissor_y0 >= scissor_y1)
    scissor_x0 = scissor_y0 = scissor_x1 = scissor_y1 = 0;

  if (!(scissor_x0 == 0 && scissor_y0 == 0 &&
        scissor_x1 == G_MAXINT && scissor_y1 == G_MAXINT))
    {
      GE (glEnable (GL_SCISSOR_TEST));
      GE (glScissor (scissor_x0, scissor_y0,
                     scissor_x1 - scissor_x0,
                     scissor_y1 - scissor_y0));
    }

  ctx->clip.stencil_used = using_stencil_buffer;
}

void
cogl_clip_ensure (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->clip.stack_dirty)
    _cogl_clip_stack_rebuild ();
}

void
cogl_clip_stack_save (void)
{
  CoglClipStack *stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = g_slice_new (CoglClipStack);
  stack->stack_top = NULL;

  ctx->clip.stacks = g_slist_prepend (ctx->clip.stacks, stack);

  ctx->clip.stack_dirty = TRUE;
}

void
cogl_clip_stack_restore (void)
{
  CoglClipStack *stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (ctx->clip.stacks != NULL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  /* Empty the current stack */
  while (stack->stack_top)
    cogl_clip_pop ();

  /* Revert to an old stack */
  g_slice_free (CoglClipStack, stack);
  ctx->clip.stacks = g_slist_delete_link (ctx->clip.stacks,
                                          ctx->clip.stacks);

  ctx->clip.stack_dirty = TRUE;
}

void
_cogl_clip_stack_state_init (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  ctx->clip.stacks = NULL;
  ctx->clip.stack_dirty = TRUE;

  /* Add an intial stack */
  cogl_clip_stack_save ();
}

void
_cogl_clip_stack_state_destroy (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Destroy all of the stacks */
  while (ctx->clip.stacks)
    cogl_clip_stack_restore ();
}
