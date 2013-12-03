/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-paint-node
 * @Title: ClutterPaintNode
 * @Short_Description: Paint objects
 *
 * #ClutterPaintNode is an element in the render graph.
 *
 * The render graph contains all the elements that need to be painted by
 * Clutter when submitting a frame to the graphics system.
 *
 * The render graph is distinct from the scene graph: the scene graph is
 * composed by actors, which can be visible or invisible; the scene graph
 * elements also respond to events. The render graph, instead, is only
 * composed by nodes that will be painted.
 *
 * Each #ClutterActor can submit multiple #ClutterPaintNode<!-- -->s to
 * the render graph.
 */

/**
 * ClutterPaintNode:
 *
 * The <structname>ClutterPaintNode</structname> structure contains only
 * private data and it should be accessed using the provided API.
 *
 * Ref Func: clutter_paint_node_ref
 * Unref Func: clutter_paint_node_unref
 * Set Value Func: clutter_value_set_paint_node
 * Get Value Func: clutter_value_get_paint_node
 *
 * Since: 1.10
 */

/**
 * ClutterPaintNodeClass:
 *
 * The <structname>ClutterPaintNodeClass</structname> structure contains
 * only private data.
 *
 * Since: 1.10
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include <pango/pango.h>
#include <cogl/cogl.h>
#include <json-glib/json-glib.h>

#include "clutter-paint-node-private.h"

#include "clutter-debug.h"
#include "clutter-private.h"

#include <gobject/gvaluecollector.h>

static inline void      clutter_paint_operation_clear   (ClutterPaintOperation *op);

static void
value_paint_node_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_paint_node_free_value (GValue *value)
{
  if (value->data[0].v_pointer != NULL)
    clutter_paint_node_unref (value->data[0].v_pointer);
}

static void
value_paint_node_copy_value (const GValue *src,
                             GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = clutter_paint_node_ref (src->data[0].v_pointer);
  else
    dst->data[0].v_pointer = NULL;
}

static gpointer
value_paint_node_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar *
value_paint_node_collect_value (GValue      *value,
                                guint        n_collect_values,
                                GTypeCValue *collect_values,
                                guint        collect_flags)
{
  ClutterPaintNode *node;

  node = collect_values[0].v_pointer;

  if (node == NULL)
    {
      value->data[0].v_pointer = NULL;
      return NULL;
    }

  if (node->parent_instance.g_class == NULL)
    return g_strconcat ("invalid unclassed ClutterPaintNode pointer for "
                        "value type '",
                        G_VALUE_TYPE_NAME (value),
                        "'",
                        NULL);

  value->data[0].v_pointer = clutter_paint_node_ref (node);

  return NULL;
}

static gchar *
value_paint_node_lcopy_value (const GValue *value,
                              guint         n_collect_values,
                              GTypeCValue  *collect_values,
                              guint         collect_flags)
{
  ClutterPaintNode **node_p = collect_values[0].v_pointer;

  if (node_p == NULL)
    return g_strconcat ("value location for '",
                        G_VALUE_TYPE_NAME (value),
                        "' passed as NULL",
                        NULL);

  if (value->data[0].v_pointer == NULL)
    *node_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *node_p = value->data[0].v_pointer;
  else
    *node_p = clutter_paint_node_ref (value->data[0].v_pointer);

  return NULL;
}

static void
clutter_paint_node_class_base_init (ClutterPaintNodeClass *klass)
{
}

static void
clutter_paint_node_class_base_finalize (ClutterPaintNodeClass *klass)
{
}

static void
clutter_paint_node_real_finalize (ClutterPaintNode *node)
{
  ClutterPaintNode *iter;

  g_free (node->name);

  if (node->operations != NULL)
    {
      guint i;

      for (i = 0; i < node->operations->len; i++)
        {
          ClutterPaintOperation *op;

          op = &g_array_index (node->operations, ClutterPaintOperation, i);
          clutter_paint_operation_clear (op);
        }

      g_array_unref (node->operations);
    }

  iter = node->first_child;
  while (iter != NULL)
    {
      ClutterPaintNode *next = iter->next_sibling;

      clutter_paint_node_remove_child (node, iter);

      iter = next;
    }

  g_type_free_instance ((GTypeInstance *) node);
}

static gboolean
clutter_paint_node_real_pre_draw (ClutterPaintNode *node)
{
  return FALSE;
}

static void
clutter_paint_node_real_draw (ClutterPaintNode *node)
{
}

static void
clutter_paint_node_real_post_draw (ClutterPaintNode *node)
{
}

static void
clutter_paint_node_class_init (ClutterPaintNodeClass *klass)
{
  klass->pre_draw = clutter_paint_node_real_pre_draw;
  klass->draw = clutter_paint_node_real_draw;
  klass->post_draw = clutter_paint_node_real_post_draw;
  klass->finalize = clutter_paint_node_real_finalize;
}

static void
clutter_paint_node_init (ClutterPaintNode *self)
{
  self->ref_count = 1;
}

GType
clutter_paint_node_get_type (void)
{
  static volatile gsize paint_node_type_id__volatile = 0;

  if (g_once_init_enter (&paint_node_type_id__volatile))
    {
      static const GTypeFundamentalInfo finfo = {
        (G_TYPE_FLAG_CLASSED |
         G_TYPE_FLAG_INSTANTIATABLE |
         G_TYPE_FLAG_DERIVABLE |
         G_TYPE_FLAG_DEEP_DERIVABLE),
      };

      static const GTypeValueTable value_table = {
        value_paint_node_init,
        value_paint_node_free_value,
        value_paint_node_copy_value,
        value_paint_node_peek_pointer,
        "p",
        value_paint_node_collect_value,
        "p",
        value_paint_node_lcopy_value,
      };

      const GTypeInfo node_info = {
        sizeof (ClutterPaintNodeClass),

        (GBaseInitFunc) clutter_paint_node_class_base_init,
        (GBaseFinalizeFunc) clutter_paint_node_class_base_finalize,
        (GClassInitFunc) clutter_paint_node_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,

        sizeof (ClutterPaintNode),
        0,
        (GInstanceInitFunc) clutter_paint_node_init,

        &value_table,
      };

      GType paint_node_type_id =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     I_("ClutterPaintNode"),
                                     &node_info, &finfo,
                                     G_TYPE_FLAG_ABSTRACT);

      g_once_init_leave (&paint_node_type_id__volatile, paint_node_type_id);
    }

  return paint_node_type_id__volatile;
}

/**
 * clutter_paint_node_set_name:
 * @node: a #ClutterPaintNode
 * @name: a string annotating the @node
 *
 * Sets a user-readable @name for @node.
 *
 * The @name will be used for debugging purposes.
 *
 * The @node will copy the passed string.
 *
 * Since: 1.10
 */
void
clutter_paint_node_set_name (ClutterPaintNode *node,
                             const char       *name)
{
  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));

  g_free (node->name);
  node->name = g_strdup (name);
}

/**
 * clutter_paint_node_ref:
 * @node: a #ClutterPaintNode
 *
 * Acquires a reference on @node.
 *
 * Return value: (transfer full): the #ClutterPaintNode
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_paint_node_ref (ClutterPaintNode *node)
{
  g_return_val_if_fail (CLUTTER_IS_PAINT_NODE (node), NULL);

  g_atomic_int_inc (&node->ref_count);

  return node;
}

/**
 * clutter_paint_node_unref:
 * @node: a #ClutterPaintNode
 *
 * Releases a reference on @node.
 *
 * Since: 1.10
 */
void
clutter_paint_node_unref (ClutterPaintNode *node)
{
  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));

  if (g_atomic_int_dec_and_test (&node->ref_count))
    {
      ClutterPaintNodeClass *klass = CLUTTER_PAINT_NODE_GET_CLASS (node);

      klass->finalize (node);
    }
}

/**
 * clutter_paint_node_add_child:
 * @node: a #ClutterPaintNode
 * @child: the child #ClutterPaintNode to add
 *
 * Adds @child to the list of children of @node.
 *
 * This function will acquire a reference on @child.
 *
 * Since: 1.10
 */
void
clutter_paint_node_add_child (ClutterPaintNode *node,
                              ClutterPaintNode *child)
{
  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));
  g_return_if_fail (CLUTTER_IS_PAINT_NODE (child));
  g_return_if_fail (node != child);
  g_return_if_fail (child->parent == NULL);

  child->parent = node;
  clutter_paint_node_ref (child);

  node->n_children += 1;

  child->prev_sibling = node->last_child;

  if (node->last_child != NULL)
    {
      ClutterPaintNode *tmp = node->last_child;

      tmp->next_sibling = child;
    }

  if (child->prev_sibling == NULL)
    node->first_child = child;

  if (child->next_sibling == NULL)
    node->last_child = child;
}

/**
 * clutter_paint_node_remove_child:
 * @node: a #ClutterPaintNode
 * @child: the #ClutterPaintNode to remove
 *
 * Removes @child from the list of children of @node.
 *
 * This function will release the reference on @child acquired by
 * using clutter_paint_node_add_child().
 *
 * Since: 1.10
 */
void
clutter_paint_node_remove_child (ClutterPaintNode *node,
                                 ClutterPaintNode *child)
{
  ClutterPaintNode *prev, *next;

  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));
  g_return_if_fail (CLUTTER_IS_PAINT_NODE (child));
  g_return_if_fail (node != child);
  g_return_if_fail (child->parent == node);

  node->n_children -= 1;

  prev = child->prev_sibling;
  next = child->next_sibling;

  if (prev != NULL)
    prev->next_sibling = next;

  if (next != NULL)
    next->prev_sibling = prev;

  if (node->first_child == child)
    node->first_child = next;

  if (node->last_child == child)
    node->last_child = prev;

  child->prev_sibling = NULL;
  child->next_sibling = NULL;
  child->parent = NULL;

  clutter_paint_node_unref (child);
}

/**
 * clutter_paint_node_replace_child:
 * @node: a #ClutterPaintNode
 * @old_child: the child replaced by @new_child
 * @new_child: the child that replaces @old_child
 *
 * Atomically replaces @old_child with @new_child in the list of
 * children of @node.
 *
 * This function will release the reference on @old_child acquired
 * by @node, and will acquire a new reference on @new_child.
 *
 * Since: 1.10
 */
void
clutter_paint_node_replace_child (ClutterPaintNode *node,
                                  ClutterPaintNode *old_child,
                                  ClutterPaintNode *new_child)
{
  ClutterPaintNode *prev, *next;

  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));
  g_return_if_fail (CLUTTER_IS_PAINT_NODE (old_child));
  g_return_if_fail (old_child->parent == node);
  g_return_if_fail (CLUTTER_IS_PAINT_NODE (new_child));
  g_return_if_fail (new_child->parent == NULL);

  prev = old_child->prev_sibling;
  next = old_child->next_sibling;

  new_child->parent = node;
  new_child->prev_sibling = prev;
  new_child->next_sibling = next;
  clutter_paint_node_ref (new_child);

  if (prev != NULL)
    prev->next_sibling = new_child;

  if (next != NULL)
    next->prev_sibling = new_child;

  if (node->first_child == old_child)
    node->first_child = new_child;

  if (node->last_child == old_child)
    node->last_child = new_child;

  old_child->prev_sibling = NULL;
  old_child->next_sibling = NULL;
  old_child->parent = NULL;
  clutter_paint_node_unref (old_child);
}

/**
 * clutter_paint_node_remove_all:
 * @node: a #ClutterPaintNode
 *
 * Removes all children of @node.
 *
 * This function releases the reference acquired by @node on its
 * children.
 *
 * Since: 1.10
 */
void
clutter_paint_node_remove_all (ClutterPaintNode *node)
{
  ClutterPaintNode *iter;

  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));

  iter = node->first_child;
  while (iter != NULL)
    {
      ClutterPaintNode *next = iter->next_sibling;

      clutter_paint_node_remove_child (node, iter);

      iter = next;
    }
}

/**
 * clutter_paint_node_get_first_child:
 * @node: a #ClutterPaintNode
 *
 * Retrieves the first child of the @node.
 *
 * Return value: (transfer none): a pointer to the first child of
 *   the #ClutterPaintNode.
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_paint_node_get_first_child (ClutterPaintNode *node)
{
  g_return_val_if_fail (CLUTTER_IS_PAINT_NODE (node), NULL);

  return node->first_child;
}

/**
 * clutter_paint_node_get_previous_sibling:
 * @node: a #ClutterPaintNode
 *
 * Retrieves the previous sibling of @node.
 *
 * Return value: (transfer none): a pointer to the previous sibling
 *   of the #ClutterPaintNode.
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_paint_node_get_previous_sibling (ClutterPaintNode *node)
{
  g_return_val_if_fail (CLUTTER_IS_PAINT_NODE (node), NULL);

  return node->prev_sibling;
}

/**
 * clutter_paint_node_get_next_sibling:
 * @node: a #ClutterPaintNode
 *
 * Retrieves the next sibling of @node.
 *
 * Return value: (transfer none): a pointer to the next sibling
 *   of a #ClutterPaintNode
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_paint_node_get_next_sibling (ClutterPaintNode *node)
{
  g_return_val_if_fail (CLUTTER_IS_PAINT_NODE (node), NULL);

  return node->next_sibling;
}

/**
 * clutter_paint_node_get_last_child:
 * @node: a #ClutterPaintNode
 *
 * Retrieves the last child of @node.
 *
 * Return value: (transfer none): a pointer to the last child
 *   of a #ClutterPaintNode
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_paint_node_get_last_child (ClutterPaintNode *node)
{
  g_return_val_if_fail (CLUTTER_IS_PAINT_NODE (node), NULL);

  return node->last_child;
}

/**
 * clutter_paint_node_get_parent:
 * @node: a #ClutterPaintNode
 *
 * Retrieves the parent of @node.
 *
 * Return value: (transfer none): a pointer to the parent of
 *   a #ClutterPaintNode
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_paint_node_get_parent (ClutterPaintNode *node)
{
  g_return_val_if_fail (CLUTTER_IS_PAINT_NODE (node), NULL);

  return node->parent;
}

/**
 * clutter_paint_node_get_n_children:
 * @node: a #ClutterPaintNode
 *
 * Retrieves the number of children of @node.
 *
 * Return value: the number of children of a #ClutterPaintNode
 *
 * Since: 1.10
 */
guint
clutter_paint_node_get_n_children (ClutterPaintNode *node)
{
  g_return_val_if_fail (CLUTTER_IS_PAINT_NODE (node), 0);

  return node->n_children;
}

/**
 * clutter_value_set_paint_node:
 * @value: a #GValue initialized with %CLUTTER_TYPE_PAINT_NODE
 * @node: (type Clutter.PaintNode) (allow-none): a #ClutterPaintNode, or %NULL
 *
 * Sets the contents of a #GValue initialized with %CLUTTER_TYPE_PAINT_NODE.
 *
 * This function increased the reference count of @node; if you do not wish
 * to increase the reference count, use clutter_value_take_paint_node()
 * instead. The reference count will be released by g_value_unset().
 *
 * Since: 1.10
 */
void
clutter_value_set_paint_node (GValue   *value,
                              gpointer  node)
{
  ClutterPaintNode *old_node;

  g_return_if_fail (CLUTTER_VALUE_HOLDS_PAINT_NODE (value));

  old_node = value->data[0].v_pointer;

  if (node != NULL)
    {
      g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));

      value->data[0].v_pointer = clutter_paint_node_ref (node);
    }
  else
    value->data[0].v_pointer = NULL;

  if (old_node != NULL)
    clutter_paint_node_unref (old_node);
}

/**
 * clutter_value_take_paint_node:
 * @value: a #GValue, initialized with %CLUTTER_TYPE_PAINT_NODE
 * @node: (type Clutter.PaintNode) (allow-none): a #ClutterPaintNode, or %NULL
 *
 * Sets the contents of a #GValue initialized with %CLUTTER_TYPE_PAINT_NODE.
 *
 * Unlike clutter_value_set_paint_node(), this function will not take a
 * reference on the passed @node: instead, it will take ownership of the
 * current reference count.
 *
 * Since: 1.10
 */
void
clutter_value_take_paint_node (GValue   *value,
                               gpointer  node)
{
  ClutterPaintNode *old_node;

  g_return_if_fail (CLUTTER_VALUE_HOLDS_PAINT_NODE (value));

  old_node = value->data[0].v_pointer;

  if (node != NULL)
    {
      g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));

      /* take over ownership */
      value->data[0].v_pointer = node;
    }
  else
    value->data[0].v_pointer = NULL;

  if (old_node != NULL)
    clutter_paint_node_unref (old_node);
}

/**
 * clutter_value_get_paint_node:
 * @value: a #GValue initialized with %CLUTTER_TYPE_PAINT_NODE
 *
 * Retrieves a pointer to the #ClutterPaintNode contained inside
 * the passed #GValue.
 *
 * Return value: (transfer none) (type Clutter.PaintNode): a pointer to
 *   a #ClutterPaintNode, or %NULL
 *
 * Since: 1.10
 */
gpointer
clutter_value_get_paint_node (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_PAINT_NODE (value), NULL);

  return value->data[0].v_pointer;
}

/**
 * clutter_value_dup_paint_node:
 * @value: a #GValue initialized with %CLUTTER_TYPE_PAINT_NODE
 *
 * Retrieves a pointer to the #ClutterPaintNode contained inside
 * the passed #GValue, and if not %NULL it will increase the
 * reference count.
 *
 * Return value: (transfer full) (type Clutter.PaintNode): a pointer
 *   to the #ClutterPaintNode, with its reference count increased,
 *   or %NULL
 *
 * Since: 1.10
 */
gpointer
clutter_value_dup_paint_node (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_PAINT_NODE (value), NULL);

  if (value->data[0].v_pointer != NULL)
    return clutter_paint_node_ref (value->data[0].v_pointer);

  return NULL;
}

static inline void
clutter_paint_operation_clear (ClutterPaintOperation *op)
{
  switch (op->opcode)
    {
    case PAINT_OP_INVALID:
      break;

    case PAINT_OP_TEX_RECT:
      break;

    case PAINT_OP_PATH:
      if (op->op.path != NULL)
        cogl_object_unref (op->op.path);
      break;

    case PAINT_OP_PRIMITIVE:
      if (op->op.primitive != NULL)
        cogl_object_unref (op->op.primitive);
      break;
    }
}

static inline void
clutter_paint_op_init_tex_rect (ClutterPaintOperation *op,
                                const ClutterActorBox *rect,
                                float                  x_1,
                                float                  y_1,
                                float                  x_2,
                                float                  y_2)
{
  clutter_paint_operation_clear (op);

  op->opcode = PAINT_OP_TEX_RECT;
  op->op.texrect[0] = rect->x1;
  op->op.texrect[1] = rect->y1;
  op->op.texrect[2] = rect->x2;
  op->op.texrect[3] = rect->y2;
  op->op.texrect[4] = x_1;
  op->op.texrect[5] = y_1;
  op->op.texrect[6] = x_2;
  op->op.texrect[7] = y_2;
}

static inline void
clutter_paint_op_init_path (ClutterPaintOperation *op,
                            CoglPath              *path)
{
  clutter_paint_operation_clear (op);

  op->opcode = PAINT_OP_PATH;
  op->op.path = cogl_object_ref (path);
}

static inline void
clutter_paint_op_init_primitive (ClutterPaintOperation *op,
                                 CoglPrimitive         *primitive)
{
  clutter_paint_operation_clear (op);

  op->opcode = PAINT_OP_PRIMITIVE;
  op->op.primitive = cogl_object_ref (primitive);
}

static inline void
clutter_paint_node_maybe_init_operations (ClutterPaintNode *node)
{
  if (node->operations != NULL)
    return;

  node->operations =
    g_array_new (FALSE, FALSE, sizeof (ClutterPaintOperation));
}

/**
 * clutter_paint_node_add_rectangle:
 * @node: a #ClutterPaintNode
 * @rect: a #ClutterActorBox
 *
 * Adds a rectangle region to the @node, as described by the
 * passed @rect.
 *
 * Since: 1.10
 */
void
clutter_paint_node_add_rectangle (ClutterPaintNode      *node,
                                  const ClutterActorBox *rect)
{
  ClutterPaintOperation operation = PAINT_OP_INIT;

  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));
  g_return_if_fail (rect != NULL);

  clutter_paint_node_maybe_init_operations (node);

  clutter_paint_op_init_tex_rect (&operation, rect, 0.0, 0.0, 1.0, 1.0);
  g_array_append_val (node->operations, operation);
}

/**
 * clutter_paint_node_add_texture_rectangle:
 * @node: a #ClutterPaintNode
 * @rect: a #ClutterActorBox
 * @x_1: the left X coordinate of the texture
 * @y_1: the top Y coordinate of the texture
 * @x_2: the right X coordinate of the texture
 * @y_2: the bottom Y coordinate of the texture
 *
 * Adds a rectangle region to the @node, with texture coordinates.
 *
 * Since: 1.10
 */
void
clutter_paint_node_add_texture_rectangle (ClutterPaintNode      *node,
                                          const ClutterActorBox *rect,
                                          float                  x_1,
                                          float                  y_1,
                                          float                  x_2,
                                          float                  y_2)
{
  ClutterPaintOperation operation = PAINT_OP_INIT;

  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));
  g_return_if_fail (rect != NULL);

  clutter_paint_node_maybe_init_operations (node);

  clutter_paint_op_init_tex_rect (&operation, rect, x_1, y_1, x_2, y_2);
  g_array_append_val (node->operations, operation);
}

/**
 * clutter_paint_node_add_path:
 * @node: a #ClutterPaintNode
 * @path: a Cogl path
 *
 * Adds a region described as a path to the @node.
 *
 * This function acquires a reference on the passed @path, so it
 * is safe to call cogl_object_unref() when it returns.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
clutter_paint_node_add_path (ClutterPaintNode *node,
                             CoglPath         *path)
{
  ClutterPaintOperation operation = PAINT_OP_INIT;

  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));
  g_return_if_fail (cogl_is_path (path));

  clutter_paint_node_maybe_init_operations (node);

  clutter_paint_op_init_path (&operation, path);
  g_array_append_val (node->operations, operation);
}

/**
 * clutter_paint_node_add_primitive:
 * @node: a #ClutterPaintNode
 * @primitive: a Cogl primitive
 *
 * Adds a region described by a Cogl primitive to the @node.
 *
 * This function acquires a reference on @primitive, so it is safe
 * to call cogl_object_unref() when it returns.
 *
 * Since: 1.10
 */
void
clutter_paint_node_add_primitive (ClutterPaintNode *node,
                                  CoglPrimitive    *primitive)
{
  ClutterPaintOperation operation = PAINT_OP_INIT;

  g_return_if_fail (CLUTTER_IS_PAINT_NODE (node));
  g_return_if_fail (cogl_is_primitive (primitive));

  clutter_paint_node_maybe_init_operations (node);

  clutter_paint_op_init_primitive (&operation, primitive);
  g_array_append_val (node->operations, operation);
}

/*< private >
 * _clutter_paint_node_paint:
 * @node: a #ClutterPaintNode
 *
 * Paints the @node using the class implementation, traversing
 * its children, if any.
 */
void
_clutter_paint_node_paint (ClutterPaintNode *node)
{
  ClutterPaintNodeClass *klass = CLUTTER_PAINT_NODE_GET_CLASS (node);
  ClutterPaintNode *iter;
  gboolean res;

  res = klass->pre_draw (node);

  if (res)
    {
      klass->draw (node);
    }

  for (iter = node->first_child;
       iter != NULL;
       iter = iter->next_sibling)
    {
      _clutter_paint_node_paint (iter);
    }

  if (res)
    {
      klass->post_draw (node);
    }
}

#ifdef CLUTTER_ENABLE_DEBUG
static JsonNode *
clutter_paint_node_serialize (ClutterPaintNode *node)
{
  ClutterPaintNodeClass *klass = CLUTTER_PAINT_NODE_GET_CLASS (node);

  if (klass->serialize != NULL)
    return klass->serialize (node);

  return json_node_new (JSON_NODE_NULL);
}

static JsonNode *
clutter_paint_node_to_json (ClutterPaintNode *node)
{
  JsonBuilder *builder;
  JsonNode *res;

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, g_type_name (G_TYPE_FROM_INSTANCE (node)));

  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, node->name);

  json_builder_set_member_name (builder, "node-data");
  json_builder_add_value (builder, clutter_paint_node_serialize (node));

  json_builder_set_member_name (builder, "operations");
  json_builder_begin_array (builder);

  if (node->operations != NULL)
    {
      guint i;

      for (i = 0; i < node->operations->len; i++)
        {
          const ClutterPaintOperation *op;

          op = &g_array_index (node->operations, ClutterPaintOperation, i);
          json_builder_begin_object (builder);

          switch (op->opcode)
            {
            case PAINT_OP_TEX_RECT:
              json_builder_set_member_name (builder, "texrect");
              json_builder_begin_array (builder);
              json_builder_add_double_value (builder, op->op.texrect[0]);
              json_builder_add_double_value (builder, op->op.texrect[1]);
              json_builder_add_double_value (builder, op->op.texrect[2]);
              json_builder_add_double_value (builder, op->op.texrect[3]);
              json_builder_add_double_value (builder, op->op.texrect[4]);
              json_builder_add_double_value (builder, op->op.texrect[5]);
              json_builder_add_double_value (builder, op->op.texrect[6]);
              json_builder_add_double_value (builder, op->op.texrect[7]);
              json_builder_end_array (builder);
              break;

            case PAINT_OP_PATH:
              json_builder_set_member_name (builder, "path");
              json_builder_add_int_value (builder, (gint64) op->op.path);
              break;

            case PAINT_OP_PRIMITIVE:
              json_builder_set_member_name (builder, "primitive");
              json_builder_add_int_value (builder, (gint64) op->op.primitive);
              break;

            case PAINT_OP_INVALID:
              break;
            }

          json_builder_end_object (builder);
        }
    }

  json_builder_end_array (builder);

  json_builder_set_member_name (builder, "children");
  json_builder_begin_array (builder);

  if (node->first_child != NULL)
    {
      ClutterPaintNode *child;

      for (child = node->first_child;
           child != NULL;
           child = child->next_sibling)
        {
          JsonNode *n = clutter_paint_node_to_json (child);

          json_builder_add_value (builder, n);
        }
    }

  json_builder_end_array (builder);

  json_builder_end_object (builder);

  res = json_builder_get_root (builder);

  g_object_unref (builder);

  return res;
}
#endif /* CLUTTER_ENABLE_DEBUG */

void
_clutter_paint_node_dump_tree (ClutterPaintNode *node)
{
#ifdef CLUTTER_ENABLE_DEBUG
  JsonGenerator *gen = json_generator_new ();
  char *str;
  gsize len;

  json_generator_set_root (gen, clutter_paint_node_to_json (node));
  str = json_generator_to_data (gen, &len);

  g_print ("Render tree starting from %p:\n%s\n", node, str);

  g_free (str);
#endif /* CLUTTER_ENABLE_DEBUG */
}

/*< private >
 * _clutter_paint_node_create:
 * @gtype: a #ClutterPaintNode type
 *
 * Creates a new #ClutterPaintNode instance using @gtype
 *
 * Return value: (transfer full): the newly created #ClutterPaintNode
 *   sub-class instance; use clutter_paint_node_unref() when done
 */
gpointer
_clutter_paint_node_create (GType gtype)
{
  g_return_val_if_fail (g_type_is_a (gtype, CLUTTER_TYPE_PAINT_NODE), NULL);

  _clutter_paint_node_init_types ();

  return (gpointer) g_type_create_instance (gtype);
}

static ClutterPaintNode *
clutter_paint_node_get_root (ClutterPaintNode *node)
{
  ClutterPaintNode *iter;

  iter = node;
  while (iter != NULL && iter->parent != NULL)
    iter = iter->parent;

  return iter;
}

CoglFramebuffer *
clutter_paint_node_get_framebuffer (ClutterPaintNode *node)
{
  ClutterPaintNode *root = clutter_paint_node_get_root (node);
  ClutterPaintNodeClass *klass;

  if (root == NULL)
    return NULL;

  klass = CLUTTER_PAINT_NODE_GET_CLASS (root);
  if (klass->get_framebuffer != NULL)
    return klass->get_framebuffer (root);

  return cogl_get_draw_framebuffer ();
}
