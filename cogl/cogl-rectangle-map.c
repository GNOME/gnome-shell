/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "cogl-util.h"
#include "cogl-rectangle-map.h"
#include "cogl-debug.h"

/* Implements a data structure which keeps track of unused
   sub-rectangles within a larger rectangle using a binary tree
   structure. The algorithm for this is based on the description here:

   http://www.blackpawn.com/texts/lightmaps/default.html
*/

#if defined (COGL_ENABLE_DEBUG) && defined (HAVE_CAIRO)

/* The cairo header is only used for debugging to generate an image of
   the atlas */
#include <cairo.h>

static void _cogl_rectangle_map_dump_image (CoglRectangleMap *map);

#endif /* COGL_ENABLE_DEBUG && HAVE_CAIRO */

typedef struct _CoglRectangleMapNode       CoglRectangleMapNode;
typedef struct _CoglRectangleMapStackEntry CoglRectangleMapStackEntry;

typedef void (* CoglRectangleMapInternalForeachCb) (CoglRectangleMapNode *node,
                                                    void *data);

typedef enum
{
  COGL_RECTANGLE_MAP_BRANCH,
  COGL_RECTANGLE_MAP_FILLED_LEAF,
  COGL_RECTANGLE_MAP_EMPTY_LEAF
} CoglRectangleMapNodeType;

struct _CoglRectangleMap
{
  CoglRectangleMapNode *root;

  unsigned int n_rectangles;

  unsigned int space_remaining;

  GDestroyNotify value_destroy_func;

  /* Stack used for walking the structure. This is only used during
     the lifetime of a single function call but it is kept here as an
     optimisation to avoid reallocating it every time it is needed */
  GArray *stack;
};

struct _CoglRectangleMapNode
{
  CoglRectangleMapNodeType type;

  CoglRectangleMapEntry rectangle;

  unsigned int largest_gap;

  CoglRectangleMapNode *parent;

  union
  {
    /* Fields used when this is a branch */
    struct
    {
      CoglRectangleMapNode *left;
      CoglRectangleMapNode *right;
    } branch;

    /* Field used when this is a filled leaf */
    void *data;
  } d;
};

struct _CoglRectangleMapStackEntry
{
  /* The node to search */
  CoglRectangleMapNode *node;
  /* Index of next branch of this node to explore. Basically either 0
     to go left or 1 to go right */
  CoglBool next_index;
};

static CoglRectangleMapNode *
_cogl_rectangle_map_node_new (void)
{
  return g_slice_new (CoglRectangleMapNode);
}

static void
_cogl_rectangle_map_node_free (CoglRectangleMapNode *node)
{
  g_slice_free (CoglRectangleMapNode, node);
}

CoglRectangleMap *
_cogl_rectangle_map_new (unsigned int width,
                         unsigned int height,
                         GDestroyNotify value_destroy_func)
{
  CoglRectangleMap *map = g_new (CoglRectangleMap, 1);
  CoglRectangleMapNode *root = _cogl_rectangle_map_node_new ();

  root->type = COGL_RECTANGLE_MAP_EMPTY_LEAF;
  root->parent = NULL;
  root->rectangle.x = 0;
  root->rectangle.y = 0;
  root->rectangle.width = width;
  root->rectangle.height = height;
  root->largest_gap = width * height;

  map->root = root;
  map->n_rectangles = 0;
  map->value_destroy_func = value_destroy_func;
  map->space_remaining = width * height;

  map->stack = g_array_new (FALSE, FALSE, sizeof (CoglRectangleMapStackEntry));

  return map;
}

static void
_cogl_rectangle_map_stack_push (GArray *stack,
                                CoglRectangleMapNode *node,
                                CoglBool next_index)
{
  CoglRectangleMapStackEntry *new_entry;

  g_array_set_size (stack, stack->len + 1);

  new_entry = &g_array_index (stack, CoglRectangleMapStackEntry,
                              stack->len - 1);

  new_entry->node = node;
  new_entry->next_index = next_index;
}

static void
_cogl_rectangle_map_stack_pop (GArray *stack)
{
  g_array_set_size (stack, stack->len - 1);
}

static CoglRectangleMapStackEntry *
_cogl_rectangle_map_stack_get_top (GArray *stack)
{
  return &g_array_index (stack, CoglRectangleMapStackEntry,
                         stack->len - 1);
}

static CoglRectangleMapNode *
_cogl_rectangle_map_node_split_horizontally (CoglRectangleMapNode *node,
                                             unsigned int left_width)
{
  /* Splits the node horizontally (according to emacs' definition, not
     vim) by converting it to a branch and adding two new leaf
     nodes. The leftmost branch will have the width left_width and
     will be returned. If the node is already just the right size it
     won't do anything */

  CoglRectangleMapNode *left_node, *right_node;

  if (node->rectangle.width == left_width)
    return node;

  left_node = _cogl_rectangle_map_node_new ();
  left_node->type = COGL_RECTANGLE_MAP_EMPTY_LEAF;
  left_node->parent = node;
  left_node->rectangle.x = node->rectangle.x;
  left_node->rectangle.y = node->rectangle.y;
  left_node->rectangle.width = left_width;
  left_node->rectangle.height = node->rectangle.height;
  left_node->largest_gap = (left_node->rectangle.width *
                            left_node->rectangle.height);
  node->d.branch.left = left_node;

  right_node = _cogl_rectangle_map_node_new ();
  right_node->type = COGL_RECTANGLE_MAP_EMPTY_LEAF;
  right_node->parent = node;
  right_node->rectangle.x = node->rectangle.x + left_width;
  right_node->rectangle.y = node->rectangle.y;
  right_node->rectangle.width = node->rectangle.width - left_width;
  right_node->rectangle.height = node->rectangle.height;
  right_node->largest_gap = (right_node->rectangle.width *
                             right_node->rectangle.height);
  node->d.branch.right = right_node;

  node->type = COGL_RECTANGLE_MAP_BRANCH;

  return left_node;
}

static CoglRectangleMapNode *
_cogl_rectangle_map_node_split_vertically (CoglRectangleMapNode *node,
                                           unsigned int top_height)
{
  /* Splits the node vertically (according to emacs' definition, not
     vim) by converting it to a branch and adding two new leaf
     nodes. The topmost branch will have the height top_height and
     will be returned. If the node is already just the right size it
     won't do anything */

  CoglRectangleMapNode *top_node, *bottom_node;

  if (node->rectangle.height == top_height)
    return node;

  top_node = _cogl_rectangle_map_node_new ();
  top_node->type = COGL_RECTANGLE_MAP_EMPTY_LEAF;
  top_node->parent = node;
  top_node->rectangle.x = node->rectangle.x;
  top_node->rectangle.y = node->rectangle.y;
  top_node->rectangle.width = node->rectangle.width;
  top_node->rectangle.height = top_height;
  top_node->largest_gap = (top_node->rectangle.width *
                           top_node->rectangle.height);
  node->d.branch.left = top_node;

  bottom_node = _cogl_rectangle_map_node_new ();
  bottom_node->type = COGL_RECTANGLE_MAP_EMPTY_LEAF;
  bottom_node->parent = node;
  bottom_node->rectangle.x = node->rectangle.x;
  bottom_node->rectangle.y = node->rectangle.y + top_height;
  bottom_node->rectangle.width = node->rectangle.width;
  bottom_node->rectangle.height = node->rectangle.height - top_height;
  bottom_node->largest_gap = (bottom_node->rectangle.width *
                              bottom_node->rectangle.height);
  node->d.branch.right = bottom_node;

  node->type = COGL_RECTANGLE_MAP_BRANCH;

  return top_node;
}

#ifdef COGL_ENABLE_DEBUG

static unsigned int
_cogl_rectangle_map_verify_recursive (CoglRectangleMapNode *node)
{
  /* This is just used for debugging the data structure. It
     recursively walks the tree to verify that the largest gap values
     all add up */

  switch (node->type)
    {
    case COGL_RECTANGLE_MAP_BRANCH:
      {
        int sum =
          _cogl_rectangle_map_verify_recursive (node->d.branch.left) +
          _cogl_rectangle_map_verify_recursive (node->d.branch.right);
        g_assert (node->largest_gap ==
                  MAX (node->d.branch.left->largest_gap,
                       node->d.branch.right->largest_gap));
        return sum;
      }

    case COGL_RECTANGLE_MAP_EMPTY_LEAF:
      g_assert (node->largest_gap ==
                node->rectangle.width * node->rectangle.height);
      return 0;

    case COGL_RECTANGLE_MAP_FILLED_LEAF:
      g_assert (node->largest_gap == 0);
      return 1;
    }

  return 0;
}

static unsigned int
_cogl_rectangle_map_get_space_remaining_recursive (CoglRectangleMapNode *node)
{
  /* This is just used for debugging the data structure. It
     recursively walks the tree to verify that the remaining space
     value adds up */

  switch (node->type)
    {
    case COGL_RECTANGLE_MAP_BRANCH:
      {
        CoglRectangleMapNode *l = node->d.branch.left;
        CoglRectangleMapNode *r = node->d.branch.right;

        return (_cogl_rectangle_map_get_space_remaining_recursive (l) +
                _cogl_rectangle_map_get_space_remaining_recursive (r));
      }

    case COGL_RECTANGLE_MAP_EMPTY_LEAF:
      return node->rectangle.width * node->rectangle.height;

    case COGL_RECTANGLE_MAP_FILLED_LEAF:
      return 0;
    }

  return 0;
}

static void
_cogl_rectangle_map_verify (CoglRectangleMap *map)
{
  unsigned int actual_n_rectangles =
    _cogl_rectangle_map_verify_recursive (map->root);
  unsigned int actual_space_remaining =
    _cogl_rectangle_map_get_space_remaining_recursive (map->root);

  g_assert_cmpuint (actual_n_rectangles, ==, map->n_rectangles);
  g_assert_cmpuint (actual_space_remaining, ==, map->space_remaining);
}

#endif /* COGL_ENABLE_DEBUG */

CoglBool
_cogl_rectangle_map_add (CoglRectangleMap *map,
                         unsigned int width,
                         unsigned int height,
                         void *data,
                         CoglRectangleMapEntry *rectangle)
{
  unsigned int rectangle_size = width * height;
  /* Stack of nodes to search in */
  GArray *stack = map->stack;
  CoglRectangleMapNode *found_node = NULL;

  /* Zero-sized rectangles break the algorithm for removing rectangles
     so we'll disallow them */
  _COGL_RETURN_VAL_IF_FAIL (width > 0 && height > 0, FALSE);

  /* Start with the root node */
  g_array_set_size (stack, 0);
  _cogl_rectangle_map_stack_push (stack, map->root, FALSE);

  /* Depth-first search for an empty node that is big enough */
  while (stack->len > 0)
    {
      CoglRectangleMapStackEntry *stack_top;
      CoglRectangleMapNode *node;
      int next_index;

      /* Pop an entry off the stack */
      stack_top = _cogl_rectangle_map_stack_get_top (stack);
      node = stack_top->node;
      next_index = stack_top->next_index;
      _cogl_rectangle_map_stack_pop (stack);

      /* Regardless of the type of the node, there's no point
         descending any further if the new rectangle won't fit within
         it */
      if (node->rectangle.width >= width &&
          node->rectangle.height >= height &&
          node->largest_gap >= rectangle_size)
        {
          if (node->type == COGL_RECTANGLE_MAP_EMPTY_LEAF)
            {
              /* We've found a node we can use */
              found_node = node;
              break;
            }
          else if (node->type == COGL_RECTANGLE_MAP_BRANCH)
            {
              if (next_index)
                /* Try the right branch */
                _cogl_rectangle_map_stack_push (stack,
                                                node->d.branch.right,
                                                0);
              else
                {
                  /* Make sure we remember to try the right branch once
                     we've finished descending the left branch */
                  _cogl_rectangle_map_stack_push (stack,
                                                  node,
                                                  1);
                  /* Try the left branch */
                  _cogl_rectangle_map_stack_push (stack,
                                                  node->d.branch.left,
                                                  0);
                }
            }
        }
    }

  if (found_node)
    {
      CoglRectangleMapNode *node;

      /* Split according to whichever axis will leave us with the
         largest space */
      if (found_node->rectangle.width - width >
          found_node->rectangle.height - height)
        {
          found_node =
            _cogl_rectangle_map_node_split_horizontally (found_node, width);
          found_node =
            _cogl_rectangle_map_node_split_vertically (found_node, height);
        }
      else
        {
          found_node =
            _cogl_rectangle_map_node_split_vertically (found_node, height);
          found_node =
            _cogl_rectangle_map_node_split_horizontally (found_node, width);
        }

      found_node->type = COGL_RECTANGLE_MAP_FILLED_LEAF;
      found_node->d.data = data;
      found_node->largest_gap = 0;
      if (rectangle)
        *rectangle = found_node->rectangle;

      /* Walk back up the tree and update the stored largest gap for
         the node's sub tree */
      for (node = found_node->parent; node; node = node->parent)
        {
          /* This node is a parent so it should always be a branch */
          g_assert (node->type == COGL_RECTANGLE_MAP_BRANCH);

          node->largest_gap = MAX (node->d.branch.left->largest_gap,
                                   node->d.branch.right->largest_gap);
        }

      /* There is now an extra rectangle in the map */
      map->n_rectangles++;
      /* and less space */
      map->space_remaining -= rectangle_size;

#ifdef COGL_ENABLE_DEBUG
      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DUMP_ATLAS_IMAGE)))
        {
#ifdef HAVE_CAIRO
          _cogl_rectangle_map_dump_image (map);
#endif
          /* Dumping the rectangle map is really slow so we might as well
             verify the space remaining here as it is also quite slow */
          _cogl_rectangle_map_verify (map);
        }
#endif

      return TRUE;
    }
  else
    return FALSE;
}

void
_cogl_rectangle_map_remove (CoglRectangleMap *map,
                            const CoglRectangleMapEntry *rectangle)
{
  CoglRectangleMapNode *node = map->root;
  unsigned int rectangle_size = rectangle->width * rectangle->height;

  /* We can do a binary-chop down the search tree to find the rectangle */
  while (node->type == COGL_RECTANGLE_MAP_BRANCH)
    {
      CoglRectangleMapNode *left_node = node->d.branch.left;

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
  if (node->type != COGL_RECTANGLE_MAP_FILLED_LEAF ||
      node->rectangle.x != rectangle->x ||
      node->rectangle.y != rectangle->y ||
      node->rectangle.width != rectangle->width ||
      node->rectangle.height != rectangle->height)
    /* This should only happen if someone tried to remove a rectangle
       that was not in the map so something has gone wrong */
    g_return_if_reached ();
  else
    {
      /* Convert the node back to an empty node */
      if (map->value_destroy_func)
        map->value_destroy_func (node->d.data);
      node->type = COGL_RECTANGLE_MAP_EMPTY_LEAF;
      node->largest_gap = rectangle_size;

      /* Walk back up the tree combining branch nodes that have two
         empty leaves back into a single empty leaf */
      for (node = node->parent; node; node = node->parent)
        {
          /* This node is a parent so it should always be a branch */
          g_assert (node->type == COGL_RECTANGLE_MAP_BRANCH);

          if (node->d.branch.left->type == COGL_RECTANGLE_MAP_EMPTY_LEAF &&
              node->d.branch.right->type == COGL_RECTANGLE_MAP_EMPTY_LEAF)
            {
              _cogl_rectangle_map_node_free (node->d.branch.left);
              _cogl_rectangle_map_node_free (node->d.branch.right);
              node->type = COGL_RECTANGLE_MAP_EMPTY_LEAF;

              node->largest_gap = (node->rectangle.width *
                                   node->rectangle.height);
            }
          else
            break;
        }

      /* Reduce the amount of space remaining in all of the parents
         further up the chain */
      for (; node; node = node->parent)
        node->largest_gap = MAX (node->d.branch.left->largest_gap,
                                 node->d.branch.right->largest_gap);

      /* There is now one less rectangle */
      g_assert (map->n_rectangles > 0);
      map->n_rectangles--;
      /* and more space */
      map->space_remaining += rectangle_size;
    }

#ifdef COGL_ENABLE_DEBUG
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DUMP_ATLAS_IMAGE)))
    {
#ifdef HAVE_CAIRO
      _cogl_rectangle_map_dump_image (map);
#endif
      /* Dumping the rectangle map is really slow so we might as well
         verify the space remaining here as it is also quite slow */
      _cogl_rectangle_map_verify (map);
    }
#endif
}

unsigned int
_cogl_rectangle_map_get_width (CoglRectangleMap *map)
{
  return map->root->rectangle.width;
}

unsigned int
_cogl_rectangle_map_get_height (CoglRectangleMap *map)
{
  return map->root->rectangle.height;
}

unsigned int
_cogl_rectangle_map_get_remaining_space (CoglRectangleMap *map)
{
  return map->space_remaining;
}

unsigned int
_cogl_rectangle_map_get_n_rectangles (CoglRectangleMap *map)
{
  return map->n_rectangles;
}

static void
_cogl_rectangle_map_internal_foreach (CoglRectangleMap *map,
                                      CoglRectangleMapInternalForeachCb func,
                                      void *data)
{
  /* Stack of nodes to search in */
  GArray *stack = map->stack;

  /* Start with the root node */
  g_array_set_size (stack, 0);
  _cogl_rectangle_map_stack_push (stack, map->root, 0);

  /* Iterate all nodes depth-first */
  while (stack->len > 0)
    {
      CoglRectangleMapStackEntry *stack_top =
        _cogl_rectangle_map_stack_get_top (stack);
      CoglRectangleMapNode *node = stack_top->node;

      switch (node->type)
        {
        case COGL_RECTANGLE_MAP_BRANCH:
          if (stack_top->next_index == 0)
            {
              /* Next time we come back to this node, go to the right */
              stack_top->next_index = 1;

              /* Explore the left branch next */
              _cogl_rectangle_map_stack_push (stack,
                                              node->d.branch.left,
                                              0);
            }
          else if (stack_top->next_index == 1)
            {
              /* Next time we come back to this node, stop processing it */
              stack_top->next_index = 2;

              /* Explore the right branch next */
              _cogl_rectangle_map_stack_push (stack,
                                              node->d.branch.right,
                                              0);
            }
          else
            {
              /* We're finished with this node so we can call the callback */
              func (node, data);
              _cogl_rectangle_map_stack_pop (stack);
            }
          break;

        default:
          /* Some sort of leaf node, just call the callback */
          func (node, data);
          _cogl_rectangle_map_stack_pop (stack);
          break;
        }
    }

  /* The stack should now be empty */
  g_assert (stack->len == 0);
}

typedef struct _CoglRectangleMapForeachClosure
{
  CoglRectangleMapCallback callback;
  void *data;
} CoglRectangleMapForeachClosure;

static void
_cogl_rectangle_map_foreach_cb (CoglRectangleMapNode *node, void *data)
{
  CoglRectangleMapForeachClosure *closure = data;

  if (node->type == COGL_RECTANGLE_MAP_FILLED_LEAF)
    closure->callback (&node->rectangle, node->d.data, closure->data);
}

void
_cogl_rectangle_map_foreach (CoglRectangleMap *map,
                             CoglRectangleMapCallback callback,
                             void *data)
{
  CoglRectangleMapForeachClosure closure;

  closure.callback = callback;
  closure.data = data;

  _cogl_rectangle_map_internal_foreach (map,
                                        _cogl_rectangle_map_foreach_cb,
                                        &closure);
}

static void
_cogl_rectangle_map_free_cb (CoglRectangleMapNode *node, void *data)
{
  CoglRectangleMap *map = data;

  if (node->type == COGL_RECTANGLE_MAP_FILLED_LEAF && map->value_destroy_func)
    map->value_destroy_func (node->d.data);

  _cogl_rectangle_map_node_free (node);
}

void
_cogl_rectangle_map_free (CoglRectangleMap *map)
{
  _cogl_rectangle_map_internal_foreach (map,
                                        _cogl_rectangle_map_free_cb,
                                        map);

  g_array_free (map->stack, TRUE);

  g_free (map);
}

#if defined (COGL_ENABLE_DEBUG) && defined (HAVE_CAIRO)

static void
_cogl_rectangle_map_dump_image_cb (CoglRectangleMapNode *node, void *data)
{
  cairo_t *cr = data;

  if (node->type == COGL_RECTANGLE_MAP_FILLED_LEAF ||
      node->type == COGL_RECTANGLE_MAP_EMPTY_LEAF)
    {
      /* Fill the rectangle using a different colour depending on
         whether the rectangle is used */
      if (node->type == COGL_RECTANGLE_MAP_FILLED_LEAF)
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
_cogl_rectangle_map_dump_image (CoglRectangleMap *map)
{
  /* This dumps a png to help visualize the map. Each leaf rectangle
     is drawn with a white outline. Unused leaves are filled in black
     and used leaves are blue */

  cairo_surface_t *surface =
    cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                _cogl_rectangle_map_get_width (map),
                                _cogl_rectangle_map_get_height (map));
  cairo_t *cr = cairo_create (surface);

  _cogl_rectangle_map_internal_foreach (map,
                                        _cogl_rectangle_map_dump_image_cb,
                                        cr);

  cairo_destroy (cr);

  cairo_surface_write_to_png (surface, "cogl-rectangle-map-dump.png");

  cairo_surface_destroy (surface);
}

#endif /* COGL_ENABLE_DEBUG && HAVE_CAIRO */
