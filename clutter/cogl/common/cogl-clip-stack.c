/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
#include "cogl.h"
#include "cogl-clip-stack.h"
#include "cogl-primitives.h"
#include "cogl-context.h"

/* These are defined in the particular backend (float in GL vs fixed
   in GL ES) */
void _cogl_set_clip_planes (CoglFixed x,
			    CoglFixed y,
			    CoglFixed width,
			    CoglFixed height);
void _cogl_add_stencil_clip (CoglFixed x,
			     CoglFixed y,
			     CoglFixed width,
			     CoglFixed height,
			     gboolean     first);
void _cogl_add_path_to_stencil_buffer (CoglFixedVec2 nodes_min,
                                       CoglFixedVec2 nodes_max,
                                       guint         path_size,
                                       CoglPathNode *path,
                                       gboolean      merge);
void _cogl_enable_clip_planes (void);
void _cogl_disable_clip_planes (void);
void _cogl_disable_stencil_buffer (void);
void _cogl_set_matrix (const CoglFixed *matrix);

typedef struct _CoglClipStack CoglClipStack;

typedef struct _CoglClipStackEntryRect CoglClipStackEntryRect;
typedef struct _CoglClipStackEntryPath CoglClipStackEntryPath;

typedef enum
  {
    COGL_CLIP_STACK_RECT,
    COGL_CLIP_STACK_PATH
  } CoglClipStackEntryType;

struct _CoglClipStack
{
  GList *stack_top;
};

struct _CoglClipStackEntryRect
{
  CoglClipStackEntryType     type;

  /* The rectangle for this clip */
  CoglFixed                  x_offset;
  CoglFixed                  y_offset;
  CoglFixed                  width;
  CoglFixed                  height;

  /* The matrix that was current when the clip was set */
  CoglFixed                  matrix[16];
};

struct _CoglClipStackEntryPath
{
  CoglClipStackEntryType     type;

  /* The matrix that was current when the clip was set */
  CoglFixed                  matrix[16];

  CoglFixedVec2              path_nodes_min;
  CoglFixedVec2              path_nodes_max;

  guint                      path_size;
  CoglPathNode               path[1];
};

void
cogl_clip_set (CoglFixed x_offset,
	       CoglFixed y_offset,
	       CoglFixed width,
	       CoglFixed height)
{
  CoglClipStackEntryRect *entry;
  CoglClipStack *stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  entry = g_slice_new (CoglClipStackEntryRect);

  /* Make a new entry */
  entry->type = COGL_CLIP_STACK_RECT;
  entry->x_offset = x_offset;
  entry->y_offset = y_offset;
  entry->width = width;
  entry->height = height;

  cogl_get_modelview_matrix (entry->matrix);

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  ctx->clip.stack_dirty = TRUE;
}

void
cogl_clip_set_from_path_preserve (void)
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

  cogl_get_modelview_matrix (entry->matrix);

  /* Store it in the stack */
  stack->stack_top = g_list_prepend (stack->stack_top, entry);

  ctx->clip.stack_dirty = TRUE;
}

void
cogl_clip_set_from_path (void)
{
  cogl_clip_set_from_path_preserve ();

  cogl_path_new ();
}

void
cogl_clip_unset (void)
{
  gpointer entry;
  CoglClipStack *stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  g_return_if_fail (stack->stack_top != NULL);

  entry = stack->stack_top->data;

  /* Remove the top entry from the stack */
  if (*(CoglClipStackEntryType *) entry == COGL_CLIP_STACK_RECT)
    g_slice_free (CoglClipStackEntryRect, entry);
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

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  stack = (CoglClipStack *) ctx->clip.stacks->data;

  ctx->clip.stack_dirty = FALSE;
  ctx->clip.stencil_used = FALSE;

  _cogl_disable_clip_planes ();
  _cogl_disable_stencil_buffer ();
  
  /* If the stack is empty then there's nothing else to do */
  if (stack->stack_top == NULL)
    return;

  /* Find the bottom of the stack */
  for (node = stack->stack_top; node->next; node = node->next);

  /* Re-add every entry from the bottom of the stack up */
  for (; node; node = node->prev)
    {
      gpointer entry = node->data;

      if (*(CoglClipStackEntryType *) entry == COGL_CLIP_STACK_PATH)
        {
          CoglClipStackEntryPath *path = (CoglClipStackEntryPath *) entry;

          cogl_push_matrix ();
          _cogl_set_matrix (path->matrix);

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
      else
        {
          CoglClipStackEntryRect *rect = (CoglClipStackEntryRect *) entry;

          cogl_push_matrix ();
          _cogl_set_matrix (rect->matrix);

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
    }

  /* Enabling clip planes is delayed to now so that they won't affect
     setting up the stencil buffer */
  if (using_clip_planes)
    _cogl_enable_clip_planes ();

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
    cogl_clip_unset ();

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
