/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-container.c: Base class for St container actors
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "st-container.h"

#define ST_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),ST_TYPE_CONTAINER, StContainerPrivate))

struct _StContainerPrivate
{
  GList *children;
  ClutterActor *first_child;
  ClutterActor *last_child;
};

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (StContainer, st_container, ST_TYPE_WIDGET,
                                  G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                         clutter_container_iface_init));

static void
st_container_update_pseudo_classes (StContainer *container)
{
  GList *first_item, *last_item;
  ClutterActor *first_child, *last_child;
  StContainerPrivate *priv = container->priv;

  first_item = priv->children;
  first_child = first_item ? first_item->data : NULL;
  if (first_child != priv->first_child)
    {
      if (priv->first_child && ST_IS_WIDGET (priv->first_child))
        st_widget_remove_style_pseudo_class (ST_WIDGET (priv->first_child),
                                             "first-child");
      if (priv->first_child)
        {
          g_object_unref (priv->first_child);
          priv->first_child = NULL;
        }

      if (first_child && ST_IS_WIDGET (first_child))
        st_widget_add_style_pseudo_class (ST_WIDGET (first_child),
                                          "first-child");
      if (first_child)
        priv->first_child = g_object_ref (first_child);
    }

  last_item = g_list_last (priv->children);
  last_child = last_item ? last_item->data : NULL;
  if (last_child != priv->last_child)
    {
      if (priv->last_child && ST_IS_WIDGET (priv->last_child))
        st_widget_remove_style_pseudo_class (ST_WIDGET (priv->last_child),
                                             "last-child");
      if (priv->last_child)
        {
          g_object_unref (priv->last_child);
          priv->last_child = NULL;
        }

      if (last_child && ST_IS_WIDGET (last_child))
        st_widget_add_style_pseudo_class (ST_WIDGET (last_child),
                                          "last-child");
      if (last_child)
        priv->last_child = g_object_ref (last_child);
    }
}

/**
 * st_container_remove_all:
 * @container: An #StContainer
 *
 * Removes all child actors from @container.
 */
void
st_container_remove_all (StContainer *container)
{
  StContainerPrivate *priv = container->priv;

  /* copied from clutter_group_remove_all() */
  while (priv->children)
    {
      ClutterActor *child = priv->children->data;
      priv->children = priv->children->next;

      clutter_container_remove_actor (CLUTTER_CONTAINER (container), child);
    }
  st_container_update_pseudo_classes (container);
}

/**
 * st_container_destroy_children:
 * @container: An #StContainer
 *
 * Destroys all child actors from @container.
 */
void
st_container_destroy_children (StContainer *container)
{
  StContainerPrivate *priv = container->priv;

  while (priv->children)
    {
      ClutterActor *child = priv->children->data;
      priv->children = priv->children->next;

      clutter_actor_destroy (child);
    }
}

void
st_container_move_child (StContainer  *container,
                         ClutterActor *actor,
                         int           pos)
{
  StContainerPrivate *priv = container->priv;
  GList *item = NULL;

  item = g_list_find (priv->children, actor);

  if (item == NULL)
    {
      g_warning ("Actor of type '%s' is not a child of the %s container",
                 g_type_name (G_OBJECT_TYPE (actor)),
                 g_type_name (G_OBJECT_TYPE (container)));
      return;
    }

  priv->children = g_list_delete_link (priv->children, item);
  priv->children = g_list_insert (priv->children, actor, pos);

  st_container_update_pseudo_classes (container);

  clutter_actor_queue_relayout ((ClutterActor*) container);
}

/**
 * st_container_get_children_list:
 * @container: An #StContainer
 *
 * Get the internal list of @container's child actors. This function
 * should only be used by subclasses of StContainer
 *
 * Returns: list of @container's child actors
 */
GList *
st_container_get_children_list (StContainer *container)
{
  g_return_val_if_fail (ST_IS_CONTAINER (container), NULL);

  return container->priv->children;
}

static gint
sort_z_order (gconstpointer a,
              gconstpointer b)
{
  float depth_a, depth_b;

  depth_a = clutter_actor_get_depth (CLUTTER_ACTOR (a));
  depth_b = clutter_actor_get_depth (CLUTTER_ACTOR (b));

  if (depth_a < depth_b)
    return -1;
  if (depth_a > depth_b)
    return 1;
  return 0;
}

static void
st_container_add (ClutterContainer *container,
                  ClutterActor     *actor)
{
  StContainerPrivate *priv = ST_CONTAINER (container)->priv;

  g_object_ref (actor);

  priv->children = g_list_append (priv->children, actor);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));

  /* queue a relayout, to get the correct positioning inside
   * the ::actor-added signal handlers
   */
  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_signal_emit_by_name (container, "actor-added", actor);

  clutter_container_sort_depth_order (container);
  st_container_update_pseudo_classes (ST_CONTAINER (container));

  g_object_unref (actor);
}

static void
st_container_remove (ClutterContainer *container,
                     ClutterActor     *actor)
{
  StContainerPrivate *priv = ST_CONTAINER (container)->priv;

  g_object_ref (actor);

  priv->children = g_list_remove (priv->children, actor);
  clutter_actor_unparent (actor);

  /* queue a relayout, to get the correct positioning inside
   * the ::actor-removed signal handlers
   */
  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  /* at this point, the actor passed to the "actor-removed" signal
   * handlers is not parented anymore to the container but since we
   * are holding a reference on it, it's still valid
   */
  g_signal_emit_by_name (container, "actor-removed", actor);

  st_container_update_pseudo_classes (ST_CONTAINER (container));

  if (CLUTTER_ACTOR_IS_VISIBLE (container))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (container));

  g_object_unref (actor);
}

static void
st_container_foreach (ClutterContainer *container,
                      ClutterCallback   callback,
                      gpointer          user_data)
{
  StContainerPrivate *priv = ST_CONTAINER (container)->priv;

  /* Using g_list_foreach instead of iterating the list manually
   * because it has better protection against the current node being
   * removed. This will happen for example if someone calls
   * clutter_container_foreach(container, clutter_actor_destroy)
   */
  g_list_foreach (priv->children, (GFunc) callback, user_data);
}

static void
st_container_raise (ClutterContainer *container,
                    ClutterActor     *actor,
                    ClutterActor     *sibling)
{
  StContainerPrivate *priv = ST_CONTAINER (container)->priv;

  priv->children = g_list_remove (priv->children, actor);

  /* Raise at the top */
  if (!sibling)
    {
      GList *last_item;

      last_item = g_list_last (priv->children);

      if (last_item)
        sibling = last_item->data;

      priv->children = g_list_append (priv->children, actor);
    }
  else
    {
      gint pos;

      pos = g_list_index (priv->children, sibling) + 1;

      priv->children = g_list_insert (priv->children, actor, pos);
    }

  /* set Z ordering a value below, this will then call sort
   * as values are equal ordering shouldn't change but Z
   * values will be correct.
   *
   * FIXME: optimise
   */
  if (sibling &&
      clutter_actor_get_depth (sibling) != clutter_actor_get_depth (actor))
    {
      clutter_actor_set_depth (actor, clutter_actor_get_depth (sibling));
    }

  st_container_update_pseudo_classes (ST_CONTAINER (container));

  if (CLUTTER_ACTOR_IS_VISIBLE (container))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
}

static void
st_container_lower (ClutterContainer *container,
                    ClutterActor     *actor,
                    ClutterActor     *sibling)
{
  StContainerPrivate *priv = ST_CONTAINER (container)->priv;

  priv->children = g_list_remove (priv->children, actor);

  /* Push to bottom */
  if (!sibling)
    {
      GList *last_item;

      last_item = g_list_first (priv->children);

      if (last_item)
        sibling = last_item->data;

      priv->children = g_list_prepend (priv->children, actor);
    }
  else
    {
      gint pos;

      pos = g_list_index (priv->children, sibling);

      priv->children = g_list_insert (priv->children, actor, pos);
    }

  /* See comment in st_container_raise() for this */
  if (sibling &&
      clutter_actor_get_depth (sibling) != clutter_actor_get_depth (actor))
    {
      clutter_actor_set_depth (actor, clutter_actor_get_depth (sibling));
    }

  st_container_update_pseudo_classes (ST_CONTAINER (container));

  if (CLUTTER_ACTOR_IS_VISIBLE (container))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
}

static void
st_container_sort_depth_order (ClutterContainer *container)
{
  StContainerPrivate *priv = ST_CONTAINER (container)->priv;

  priv->children = g_list_sort (priv->children, sort_z_order);

  if (CLUTTER_ACTOR_IS_VISIBLE (container))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
}

static void
st_container_dispose (GObject *object)
{
  StContainerPrivate *priv = ST_CONTAINER (object)->priv;

  if (priv->children)
    {
      g_list_foreach (priv->children, (GFunc) clutter_actor_destroy, NULL);
      g_list_free (priv->children);

      priv->children = NULL;
    }

  if (priv->first_child)
    g_object_unref (priv->first_child);
  priv->first_child = NULL;

  if (priv->last_child)
    g_object_unref (priv->last_child);
  priv->last_child = NULL;

  G_OBJECT_CLASS (st_container_parent_class)->dispose (object);
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = st_container_add;
  iface->remove = st_container_remove;
  iface->foreach = st_container_foreach;
  iface->raise = st_container_raise;
  iface->lower = st_container_lower;
  iface->sort_depth_order = st_container_sort_depth_order;
}

static void
st_container_init (StContainer *container)
{
  container->priv = ST_CONTAINER_GET_PRIVATE (container);
}

static void
st_container_class_init (StContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (StContainerPrivate));

  object_class->dispose = st_container_dispose;
}
