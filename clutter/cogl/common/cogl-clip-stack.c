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

#include "cogl.h"
#include "cogl-clip-stack.h"

/* These are defined in the particular backend (float in GL vs fixed
   in GL ES) */
void _cogl_set_clip_planes (CoglFixed x,
			    CoglFixed y,
			    CoglFixed width,
			    CoglFixed height);
void _cogl_init_stencil_buffer (void);
void _cogl_add_stencil_clip (CoglFixed x,
			     CoglFixed y,
			     CoglFixed width,
			     CoglFixed height,
			     gboolean     first);
void _cogl_disable_clip_planes (void);
void _cogl_disable_stencil_buffer (void);
void _cogl_set_matrix (const CoglFixed *matrix);

typedef struct _CoglClipStackEntry CoglClipStackEntry;

struct _CoglClipStackEntry
{
  /* If this is set then this entry clears the clip stack. This is
     used to clear the stack when drawing an FBO put to keep the
     entries so they can be restored when the FBO drawing is
     completed */
  gboolean            clear;

  /* The rectangle for this clip */
  CoglFixed        x_offset;
  CoglFixed        y_offset;
  CoglFixed        width;
  CoglFixed        height;

  /* The matrix that was current when the clip was set */
  CoglFixed        matrix[16];
};

static GList *cogl_clip_stack_top = NULL;
static int    cogl_clip_stack_depth = 0;

static void
_cogl_clip_stack_add (const CoglClipStackEntry *entry, int depth)
{
  int has_clip_planes = cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES);

  /* If this is the first entry and we support clip planes then use
     that instead */
  if (depth == 1 && has_clip_planes)
    _cogl_set_clip_planes (entry->x_offset,
			   entry->y_offset,
			   entry->width,
			   entry->height);
  else
    _cogl_add_stencil_clip (entry->x_offset,
			    entry->y_offset,
			    entry->width,
			    entry->height,
			    depth == (has_clip_planes ? 2 : 1));
}

void
cogl_clip_set (CoglFixed x_offset,
	       CoglFixed y_offset,
	       CoglFixed width,
	       CoglFixed height)
{
  CoglClipStackEntry *entry = g_slice_new (CoglClipStackEntry);

  /* Make a new entry */
  entry->clear = FALSE;
  entry->x_offset = x_offset;
  entry->y_offset = y_offset;
  entry->width = width;
  entry->height = height;

  cogl_get_modelview_matrix (entry->matrix);

  /* Add the entry to the current clip */
  _cogl_clip_stack_add (entry, ++cogl_clip_stack_depth);

  /* Store it in the stack */
  cogl_clip_stack_top = g_list_prepend (cogl_clip_stack_top, entry);
}

void
cogl_clip_unset (void)
{
  g_return_if_fail (cogl_clip_stack_top != NULL);

  /* Remove the top entry from the stack */
  g_slice_free (CoglClipStackEntry, cogl_clip_stack_top->data);
  cogl_clip_stack_top = g_list_delete_link (cogl_clip_stack_top,
					    cogl_clip_stack_top);
  cogl_clip_stack_depth--;

  /* Rebuild the clip */
  _cogl_clip_stack_rebuild (FALSE);
}

void
_cogl_clip_stack_rebuild (gboolean just_stencil)
{
  int has_clip_planes = cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES);
  GList *node;
  int depth = 0;

  /* Disable clip planes if the stack is empty */
  if (has_clip_planes && cogl_clip_stack_depth < 1)
    _cogl_disable_clip_planes ();

  /* Disable the stencil buffer if there isn't enough entries */
  if (cogl_clip_stack_depth < (has_clip_planes ? 2 : 1))
    _cogl_disable_stencil_buffer ();

  /* Find the bottom of the stack */
  for (node = cogl_clip_stack_top; depth < cogl_clip_stack_depth - 1;
       node = node->next)
    depth++;

  /* Re-add every entry from the bottom of the stack up */
  depth = 1;
  for (; depth <= cogl_clip_stack_depth; node = node->prev, depth++)
    if (!just_stencil || !has_clip_planes || depth > 1)
      {
	const CoglClipStackEntry *entry = (CoglClipStackEntry *) node->data;
	cogl_push_matrix ();
	_cogl_set_matrix (entry->matrix);
	_cogl_clip_stack_add (entry, depth);
	cogl_pop_matrix ();
      }
}

void
_cogl_clip_stack_merge (void)
{
  GList *node = cogl_clip_stack_top;
  int i;

  /* Merge the current clip stack on top of whatever is in the stencil
     buffer */
  if (cogl_clip_stack_depth)
    {
      for (i = 0; i < cogl_clip_stack_depth - 1; i++)
	node = node->next;

      /* Skip the first entry if we have clipping planes */
      if (cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES))
	node = node->prev;

      while (node)
	{
	  const CoglClipStackEntry *entry = (CoglClipStackEntry *) node->data;
	  cogl_push_matrix ();
	  _cogl_set_matrix (entry->matrix);
	  _cogl_clip_stack_add (entry, 3);
	  cogl_pop_matrix ();

	  node = node->prev;
	}
    }
}

void
cogl_clip_stack_save (void)
{
  CoglClipStackEntry *entry = g_slice_new (CoglClipStackEntry);

  /* Push an entry into the stack to mark that it should be cleared */
  entry->clear = TRUE;
  cogl_clip_stack_top = g_list_prepend (cogl_clip_stack_top, entry);

  /* Reset the depth to zero */
  cogl_clip_stack_depth = 0;

  /* Rebuilding the stack will now disabling all clipping */
  _cogl_clip_stack_rebuild (FALSE);
}

void
cogl_clip_stack_restore (void)
{
  GList *node;

  /* The top of the stack should be a clear marker */
  g_assert (cogl_clip_stack_top);
  g_assert (((CoglClipStackEntry *) cogl_clip_stack_top->data)->clear);

  /* Remove the top entry */
  g_slice_free (CoglClipStackEntry, cogl_clip_stack_top->data);
  cogl_clip_stack_top = g_list_delete_link (cogl_clip_stack_top,
					    cogl_clip_stack_top);

  /* Recalculate the depth of the stack */
  cogl_clip_stack_depth = 0;
  for (node = cogl_clip_stack_top;
       node && !((CoglClipStackEntry *) node->data)->clear;
       node = node->next)
    cogl_clip_stack_depth++;

  _cogl_clip_stack_rebuild (FALSE);
}
