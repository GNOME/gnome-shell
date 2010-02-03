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
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "cogl-atlas.h"
#include "cogl-debug.h"

/* Implements a data structure which keeps track of unused
   sub-rectangles within a larger rectangle using a binary tree
   structure. The algorithm for this is based on the description here:

   http://www.blackpawn.com/texts/lightmaps/default.html
*/

#ifdef COGL_ENABLE_DEBUG

/* The cairo header is only used for debugging to generate an image of
   the atlas */
#include <cairo.h>

static void _cogl_atlas_dump_image (CoglAtlas *atlas);

#endif /* COGL_ENABLE_DEBUG */

typedef struct _CoglAtlasNode       CoglAtlasNode;
typedef struct _CoglAtlasStackEntry CoglAtlasStackEntry;

typedef void (* CoglAtlasInternalForeachCb) (CoglAtlasNode *node,
                                             gpointer data);

typedef enum
{
  COGL_ATLAS_BRANCH,
  COGL_ATLAS_FILLED_LEAF,
  COGL_ATLAS_EMPTY_LEAF
} CoglAtlasNodeType;

struct _CoglAtlas
{
  CoglAtlasNode *root;

  guint space_remaining;
  guint n_rectangles;

  GDestroyNotify value_destroy_func;
};

struct _CoglAtlasNode
{
  CoglAtlasNodeType type;

  CoglAtlasRectangle rectangle;

  CoglAtlasNode *parent;

  union
  {
    /* Fields used when this is a branch */
    struct
    {
      CoglAtlasNode *left;
      CoglAtlasNode *right;
    } branch;

    /* Field used when this is a filled leaf */
    gpointer data;
  } d;
};

struct _CoglAtlasStackEntry
{
  /* The node to search */
  CoglAtlasNode *node;
  /* Index of next branch of this node to explore. Basically either 0
     to go left or 1 to go right */
  gboolean next_index;
  /* Next entry in the stack */
  CoglAtlasStackEntry *next;
};

static CoglAtlasNode *
_cogl_atlas_node_new (void)
{
  return g_slice_new (CoglAtlasNode);
}

static void
_cogl_atlas_node_free (CoglAtlasNode *node)
{
  g_slice_free (CoglAtlasNode, node);
}

CoglAtlas *
_cogl_atlas_new (guint width, guint height,
                 GDestroyNotify value_destroy_func)
{
  CoglAtlas *atlas = g_new (CoglAtlas, 1);
  CoglAtlasNode *root = _cogl_atlas_node_new ();

  root->type = COGL_ATLAS_EMPTY_LEAF;
  root->parent = NULL;
  root->rectangle.x = 0;
  root->rectangle.y = 0;
  root->rectangle.width = width;
  root->rectangle.height = height;

  atlas->root = root;
  atlas->space_remaining = width * height;
  atlas->n_rectangles = 0;
  atlas->value_destroy_func = value_destroy_func;

  return atlas;
}

static CoglAtlasStackEntry *
_cogl_atlas_stack_push (CoglAtlasStackEntry *stack,
                        CoglAtlasNode *node,
                        gboolean next_index)
{
  CoglAtlasStackEntry *new_entry = g_slice_new (CoglAtlasStackEntry);

  new_entry->node = node;
  new_entry->next_index = next_index;
  new_entry->next = stack;

  return new_entry;
}

static CoglAtlasStackEntry *
_cogl_atlas_stack_pop (CoglAtlasStackEntry *stack)
{
  CoglAtlasStackEntry *next = stack->next;

  g_slice_free (CoglAtlasStackEntry, stack);

  return next;
}

static CoglAtlasNode *
_cogl_atlas_node_split_horizontally (CoglAtlasNode *node,
                                     guint left_width)
{
  /* Splits the node horizontally (according to emacs' definition, not
     vim) by converting it to a branch and adding two new leaf
     nodes. The leftmost branch will have the width left_width and
     will be returned. If the node is already just the right size it
     won't do anything */

  CoglAtlasNode *left_node, *right_node;

  if (node->rectangle.width == left_width)
    return node;

  left_node = _cogl_atlas_node_new ();
  left_node->type = COGL_ATLAS_EMPTY_LEAF;
  left_node->parent = node;
  left_node->rectangle.x = node->rectangle.x;
  left_node->rectangle.y = node->rectangle.y;
  left_node->rectangle.width = left_width;
  left_node->rectangle.height = node->rectangle.height;
  node->d.branch.left = left_node;

  right_node = _cogl_atlas_node_new ();
  right_node->type = COGL_ATLAS_EMPTY_LEAF;
  right_node->parent = node;
  right_node->rectangle.x = node->rectangle.x + left_width;
  right_node->rectangle.y = node->rectangle.y;
  right_node->rectangle.width = node->rectangle.width - left_width;
  right_node->rectangle.height = node->rectangle.height;
  node->d.branch.right = right_node;

  node->type = COGL_ATLAS_BRANCH;

  return left_node;
}

static CoglAtlasNode *
_cogl_atlas_node_split_vertically (CoglAtlasNode *node,
                                   guint top_height)
{
  /* Splits the node vertically (according to emacs' definition, not
     vim) by converting it to a branch and adding two new leaf
     nodes. The topmost branch will have the height top_height and
     will be returned. If the node is already just the right size it
     won't do anything */

  CoglAtlasNode *top_node, *bottom_node;

  if (node->rectangle.height == top_height)
    return node;

  top_node = _cogl_atlas_node_new ();
  top_node->type = COGL_ATLAS_EMPTY_LEAF;
  top_node->parent = node;
  top_node->rectangle.x = node->rectangle.x;
  top_node->rectangle.y = node->rectangle.y;
  top_node->rectangle.width = node->rectangle.width;
  top_node->rectangle.height = top_height;
  node->d.branch.left = top_node;

  bottom_node = _cogl_atlas_node_new ();
  bottom_node->type = COGL_ATLAS_EMPTY_LEAF;
  bottom_node->parent = node;
  bottom_node->rectangle.x = node->rectangle.x;
  bottom_node->rectangle.y = node->rectangle.y + top_height;
  bottom_node->rectangle.width = node->rectangle.width;
  bottom_node->rectangle.height = node->rectangle.height - top_height;
  node->d.branch.right = bottom_node;

  node->type = COGL_ATLAS_BRANCH;

  return top_node;
}

gboolean
_cogl_atlas_add_rectangle (CoglAtlas *atlas,
                           guint width, guint height,
                           gpointer data,
                           CoglAtlasRectangle *rectangle)
{
  /* Stack of nodes to search in */
  CoglAtlasStackEntry *node_stack;
  CoglAtlasNode *found_node = NULL;

  /* Zero-sized rectangles break the algorithm for removing rectangles
     so we'll disallow them */
  g_return_val_if_fail (width > 0 && height > 0, FALSE);

  /* Start with the root node */
  node_stack = _cogl_atlas_stack_push (NULL, atlas->root, FALSE);

  /* Depth-first search for an empty node that is big enough */
  while (node_stack)
    {
      /* Pop an entry off the stack */
      CoglAtlasNode *node = node_stack->node;
      int next_index = node_stack->next_index;
      node_stack = _cogl_atlas_stack_pop (node_stack);

      /* Regardless of the type of the node, there's no point
         descending any further if the new rectangle won't fit within
         it */
      if (node->rectangle.width >= width &&
          node->rectangle.height >= height)
        {
          if (node->type == COGL_ATLAS_EMPTY_LEAF)
            {
              /* We've found a node we can use */
              found_node = node;
              break;
            }
          else if (node->type == COGL_ATLAS_BRANCH)
            {
              if (next_index)
                /* Try the right branch */
                node_stack = _cogl_atlas_stack_push (node_stack,
                                                     node->d.branch.right,
                                                     0);
              else
                {
                  /* Make sure we remember to try the right branch once
                     we've finished descending the left branch */
                  node_stack = _cogl_atlas_stack_push (node_stack,
                                                       node,
                                                       1);
                  /* Try the left branch */
                  node_stack = _cogl_atlas_stack_push (node_stack,
                                                       node->d.branch.left,
                                                       0);
                }
            }
        }
    }

  /* Free the stack */
  while (node_stack)
    node_stack = _cogl_atlas_stack_pop (node_stack);

  if (found_node)
    {
      /* Split according to whichever axis will leave us with the
         largest space */
      if (found_node->rectangle.width - width >
          found_node->rectangle.height - height)
        {
          found_node = _cogl_atlas_node_split_horizontally (found_node, width);
          found_node = _cogl_atlas_node_split_vertically (found_node, height);
        }
      else
        {
          found_node = _cogl_atlas_node_split_vertically (found_node, height);
          found_node = _cogl_atlas_node_split_horizontally (found_node, width);
        }

      found_node->type = COGL_ATLAS_FILLED_LEAF;
      found_node->d.data = data;
      if (rectangle)
        *rectangle = found_node->rectangle;

      /* Record how much empty space is remaining after this rectangle
         is added */
      g_assert (width * height <= atlas->space_remaining);
      atlas->space_remaining -= width * height;
      atlas->n_rectangles++;

#ifdef COGL_ENABLE_DEBUG
      if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DUMP_ATLAS_IMAGE))
        _cogl_atlas_dump_image (atlas);
#endif

      return TRUE;
    }
  else
    return FALSE;
}

void
_cogl_atlas_remove_rectangle (CoglAtlas *atlas,
                              const CoglAtlasRectangle *rectangle)
{
  CoglAtlasNode *node = atlas->root;

  /* We can do a binary-chop down the search tree to find the rectangle */
  while (node->type == COGL_ATLAS_BRANCH)
    {
      CoglAtlasNode *left_node = node->d.branch.left;

      /* If and only if the rectangle is in the left node then the x,y
         position of the rectangle will be within the node's
         rectangle */
      if (rectangle->x < left_node->rectangle.x + left_node->rectangle.width &&
          rectangle->y < left_node->rectangle.y + left_node->rectangle.height)
        /* Go left */
        node = left_node;
      else
        /* Go right */
        node = node->d.branch.right;
    }

  /* Make sure we found the right node */
  if (node->type != COGL_ATLAS_FILLED_LEAF ||
      node->rectangle.x != rectangle->x ||
      node->rectangle.y != rectangle->y ||
      node->rectangle.width != rectangle->width ||
      node->rectangle.height != rectangle->height)
    /* This should only happen if someone tried to remove a rectangle
       that was not in the atlas so something has gone wrong */
    g_return_if_reached ();
  else
    {
      /* Convert the node back to an empty node */
      if (atlas->value_destroy_func)
        atlas->value_destroy_func (node->d.data);
      node->type = COGL_ATLAS_EMPTY_LEAF;

      /* Walk back up the tree combining branch nodes that have two
         empty leaves back into a single empty leaf */
      for (node = node->parent; node; node = node->parent)
        {
          /* This node is a parent so it should always be a branch */
          g_assert (node->type == COGL_ATLAS_BRANCH);

          if (node->d.branch.left->type == COGL_ATLAS_EMPTY_LEAF &&
              node->d.branch.right->type == COGL_ATLAS_EMPTY_LEAF)
            {
              _cogl_atlas_node_free (node->d.branch.left);
              _cogl_atlas_node_free (node->d.branch.right);
              node->type = COGL_ATLAS_EMPTY_LEAF;
            }
          else
            break;
        }

      /* There is now more free space and one less rectangle */
      atlas->space_remaining += rectangle->width * rectangle->height;
      g_assert (atlas->n_rectangles > 0);
      atlas->n_rectangles--;
    }

#ifdef COGL_ENABLE_DEBUG
  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DUMP_ATLAS_IMAGE))
    _cogl_atlas_dump_image (atlas);
#endif
}

guint
_cogl_atlas_get_width (CoglAtlas *atlas)
{
  return atlas->root->rectangle.width;
}

guint
_cogl_atlas_get_height (CoglAtlas *atlas)
{
  return atlas->root->rectangle.height;
}

guint
_cogl_atlas_get_remaining_space (CoglAtlas *atlas)
{
  return atlas->space_remaining;
}

guint
_cogl_atlas_get_n_rectangles (CoglAtlas *atlas)
{
  return atlas->n_rectangles;
}

static void
_cogl_atlas_internal_foreach (CoglAtlas *atlas,
                              CoglAtlasInternalForeachCb callback,
                              gpointer data)
{
  /* Stack of nodes to search in */
  CoglAtlasStackEntry *node_stack;

  /* Start with the root node */
  node_stack = _cogl_atlas_stack_push (NULL, atlas->root, 0);

  /* Iterate all nodes depth-first */
  while (node_stack)
    {
      CoglAtlasNode *node = node_stack->node;

      switch (node->type)
        {
        case COGL_ATLAS_BRANCH:
          if (node_stack->next_index == 0)
            {
              /* Next time we come back to this node, go to the right */
              node_stack->next_index = 1;

              /* Explore the left branch next */
              node_stack = _cogl_atlas_stack_push (node_stack,
                                                  node->d.branch.left,
                                                  0);
            }
          else if (node_stack->next_index == 1)
            {
              /* Next time we come back to this node, stop processing it */
              node_stack->next_index = 2;

              /* Explore the right branch next */
              node_stack = _cogl_atlas_stack_push (node_stack,
                                                  node->d.branch.right,
                                                  0);
            }
          else
            {
              /* We're finished with this node so we can call the callback */
              callback (node, data);
              node_stack = _cogl_atlas_stack_pop (node_stack);
            }
          break;

        default:
          /* Some sort of leaf node, just call the callback */
          callback (node, data);
          node_stack = _cogl_atlas_stack_pop (node_stack);
          break;
        }
    }

  /* The stack should now be empty */
  g_assert (node_stack == NULL);
}

typedef struct _CoglAtlasForeachClosure
{
  CoglAtlasCallback callback;
  gpointer data;
} CoglAtlasForeachClosure;

static void
_cogl_atlas_foreach_cb (CoglAtlasNode *node, gpointer data)
{
  CoglAtlasForeachClosure *closure = data;

  if (node->type == COGL_ATLAS_FILLED_LEAF)
    closure->callback (&node->rectangle, node->d.data, closure->data);
}

void
_cogl_atlas_foreach (CoglAtlas *atlas,
                     CoglAtlasCallback callback,
                     gpointer data)
{
  CoglAtlasForeachClosure closure;

  closure.callback = callback;
  closure.data = data;

  _cogl_atlas_internal_foreach (atlas, _cogl_atlas_foreach_cb, &closure);
}

static void
_cogl_atlas_free_cb (CoglAtlasNode *node, gpointer data)
{
  CoglAtlas *atlas = data;

  if (node->type == COGL_ATLAS_FILLED_LEAF && atlas->value_destroy_func)
    atlas->value_destroy_func (node->d.data);

  _cogl_atlas_node_free (node);
}

void
_cogl_atlas_free (CoglAtlas *atlas)
{
  _cogl_atlas_internal_foreach (atlas, _cogl_atlas_free_cb, atlas);
  g_free (atlas);
}

#ifdef COGL_ENABLE_DEBUG

static void
_cogl_atlas_dump_image_cb (CoglAtlasNode *node, gpointer data)
{
  cairo_t *cr = data;

  if (node->type == COGL_ATLAS_FILLED_LEAF ||
      node->type == COGL_ATLAS_EMPTY_LEAF)
    {
      /* Fill the rectangle using a different colour depending on
         whether the rectangle is used */
      if (node->type == COGL_ATLAS_FILLED_LEAF)
        cairo_set_source_rgb (cr, 0.0, 0.0, 1.0);
      else
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

      cairo_rectangle (cr,
                       node->rectangle.x,
                       node->rectangle.y,
                       node->rectangle.width,
                       node->rectangle.height);

      cairo_fill_preserve (cr);

      /* Draw a white outline around the rectangle */
      cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
      cairo_stroke (cr);
    }
}

static void
_cogl_atlas_dump_image (CoglAtlas *atlas)
{
  /* This dumps a png to help visualize the atlas. Each leaf rectangle
     is drawn with a white outline. Unused leaves are filled in black
     and used leaves are blue */

  cairo_surface_t *surface =
    cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                _cogl_atlas_get_width (atlas),
                                _cogl_atlas_get_height (atlas));
  cairo_t *cr = cairo_create (surface);

  _cogl_atlas_internal_foreach (atlas, _cogl_atlas_dump_image_cb, cr);

  cairo_destroy (cr);

  cairo_surface_write_to_png (surface, "cogl-atlas-dump.png");

  cairo_surface_destroy (surface);
}

#endif /* COGL_ENABLE_DEBUG */
