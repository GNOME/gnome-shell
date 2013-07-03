/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 */

/**
 * SECTION:clutter-group
 * @short_description: A fixed layout container
 *
 * A #ClutterGroup is an Actor which contains multiple child actors positioned
 * relative to the #ClutterGroup position. Other operations such as scaling,
 * rotating and clipping of the group will apply to the child actors.
 *
 * A #ClutterGroup's size is defined by the size and position of its children;
 * it will be the smallest non-negative size that covers the right and bottom
 * edges of all of its children.
 *
 * Setting the size on a Group using #ClutterActor methods like
 * clutter_actor_set_size() will override the natural size of the Group,
 * however this will not affect the size of the children and they may still
 * be painted outside of the allocation of the group. One way to constrain
 * the visible area of a #ClutterGroup to a specified allocation is to
 * explicitly set the size of the #ClutterGroup and then use the
 * #ClutterActor:clip-to-allocation property.
 *
 * Deprecated: 1.10: Use #ClutterActor instead.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "clutter-group.h"

#include "clutter-actor.h"
#include "clutter-actor-private.h"
#include "clutter-container.h"
#include "clutter-fixed-layout.h"
#include "clutter-main.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#include "cogl/cogl.h"

struct _ClutterGroupPrivate
{
  GList *children;

  ClutterLayoutManager *layout;
};

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterGroup, clutter_group, CLUTTER_TYPE_ACTOR,
                         G_ADD_PRIVATE (ClutterGroup)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

static gint
sort_by_depth (gconstpointer a,
               gconstpointer b)
{
  gfloat depth_a = clutter_actor_get_depth (CLUTTER_ACTOR(a));
  gfloat depth_b = clutter_actor_get_depth (CLUTTER_ACTOR(b));

  if (depth_a < depth_b)
    return -1;

  if (depth_a > depth_b)
    return 1;

  return 0;
}

static void
clutter_group_real_add (ClutterContainer *container,
                        ClutterActor     *actor)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (container)->priv;

  g_object_ref (actor);

  priv->children = g_list_append (priv->children, actor);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_signal_emit_by_name (container, "actor-added", actor);

  clutter_container_sort_depth_order (container);

  g_object_unref (actor);
}

static void
clutter_group_real_actor_added (ClutterContainer *container,
                                ClutterActor     *actor)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (container)->priv;

  /* XXX - children added using clutter_actor_add_child() will
   * cause actor-added to be emitted without going through the
   * add() virtual function.
   *
   * if we get an actor-added for a child that is not in our
   * list of children already, then we go in compatibility
   * mode.
   */
  if (g_list_find (priv->children, actor) != NULL)
    return;

  priv->children = g_list_append (priv->children, actor);
  clutter_container_sort_depth_order (container);
}

static void
clutter_group_real_remove (ClutterContainer *container,
                           ClutterActor     *actor)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (container)->priv;

  g_object_ref (actor);

  priv->children = g_list_remove (priv->children, actor);
  clutter_actor_unparent (actor);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_signal_emit_by_name (container, "actor-removed", actor);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (container));

  g_object_unref (actor);
}

static void
clutter_group_real_actor_removed (ClutterContainer *container,
                                  ClutterActor     *actor)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (container)->priv;

  /* XXX - same compatibility mode of the ::actor-added implementation */
  if (g_list_find (priv->children, actor) == NULL)
    return;

  priv->children = g_list_remove (priv->children, actor);
}

static void
clutter_group_real_foreach (ClutterContainer *container,
                            ClutterCallback   callback,
                            gpointer          user_data)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (container)->priv;

  /* Using g_list_foreach instead of iterating the list manually
     because it has better protection against the current node being
     removed. This will happen for example if someone calls
     clutter_container_foreach(container, clutter_actor_destroy) */
  g_list_foreach (priv->children, (GFunc) callback, user_data);
}

static void
clutter_group_real_raise (ClutterContainer *container,
                          ClutterActor     *actor,
                          ClutterActor     *sibling)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (container)->priv;

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
      gint index_ = g_list_index (priv->children, sibling) + 1;

      priv->children = g_list_insert (priv->children, actor, index_);
    }

  /* set Z ordering a value below, this will then call sort
   * as values are equal ordering shouldn't change but Z
   * values will be correct.
   *
   * FIXME: get rid of this crap; this is so utterly broken and wrong on
   * so many levels it's not even funny. sadly, we get to keep this until
   * we can break API and remove Group for good.
   */
  if (sibling &&
      clutter_actor_get_depth (sibling) != clutter_actor_get_depth (actor))
    {
      clutter_actor_set_depth (actor, clutter_actor_get_depth (sibling));
    }

  clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
}

static void
clutter_group_real_lower (ClutterContainer *container,
                          ClutterActor     *actor,
                          ClutterActor     *sibling)
{
  ClutterGroup *self = CLUTTER_GROUP (container);
  ClutterGroupPrivate *priv = self->priv;

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
      gint index_ = g_list_index (priv->children, sibling);

      priv->children = g_list_insert (priv->children, actor, index_);
    }

  /* See comment in group_raise for this */
  if (sibling &&
      clutter_actor_get_depth (sibling) != clutter_actor_get_depth (actor))
    {
      clutter_actor_set_depth (actor, clutter_actor_get_depth (sibling));
    }

  clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
}

static void
clutter_group_real_sort_depth_order (ClutterContainer *container)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (container)->priv;

  priv->children = g_list_sort (priv->children, sort_by_depth);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = clutter_group_real_add;
  iface->actor_added = clutter_group_real_actor_added;
  iface->remove = clutter_group_real_remove;
  iface->actor_removed = clutter_group_real_actor_removed;
  iface->foreach = clutter_group_real_foreach;
  iface->raise = clutter_group_real_raise;
  iface->lower = clutter_group_real_lower;
  iface->sort_depth_order = clutter_group_real_sort_depth_order;
}

static void
clutter_group_real_paint (ClutterActor *actor)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (actor)->priv;

  CLUTTER_NOTE (PAINT, "ClutterGroup paint enter '%s'",
                _clutter_actor_get_debug_name (actor));

  g_list_foreach (priv->children, (GFunc) clutter_actor_paint, NULL);

  CLUTTER_NOTE (PAINT, "ClutterGroup paint leave '%s'",
                _clutter_actor_get_debug_name (actor));
}

static void
clutter_group_real_pick (ClutterActor       *actor,
                         const ClutterColor *pick)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (actor)->priv;

  /* Chain up so we get a bounding box pained (if we are reactive) */
  CLUTTER_ACTOR_CLASS (clutter_group_parent_class)->pick (actor, pick);

  g_list_foreach (priv->children, (GFunc) clutter_actor_paint, NULL);
}

static void
clutter_group_real_get_preferred_width (ClutterActor *actor,
                                        gfloat        for_height,
                                        gfloat       *min_width,
                                        gfloat       *natural_width)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (actor)->priv;

  clutter_layout_manager_get_preferred_width (priv->layout,
                                              CLUTTER_CONTAINER (actor),
                                              for_height,
                                              min_width, natural_width);
}

static void
clutter_group_real_get_preferred_height (ClutterActor *actor,
                                         gfloat        for_width,
                                         gfloat       *min_height,
                                         gfloat       *natural_height)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (actor)->priv;

  clutter_layout_manager_get_preferred_height (priv->layout,
                                               CLUTTER_CONTAINER (actor),
                                               for_width,
                                               min_height, natural_height);
}

static void
clutter_group_real_allocate (ClutterActor           *actor,
                             const ClutterActorBox  *allocation,
                             ClutterAllocationFlags  flags)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (actor)->priv;
  ClutterActorClass *klass;

  klass = CLUTTER_ACTOR_CLASS (clutter_group_parent_class);
  klass->allocate (actor, allocation, flags);

  if (priv->children == NULL)
    return;

  clutter_layout_manager_allocate (priv->layout,
                                   CLUTTER_CONTAINER (actor),
                                   allocation, flags);
}

static void
clutter_group_dispose (GObject *object)
{
  ClutterGroup *self = CLUTTER_GROUP (object);
  ClutterGroupPrivate *priv = self->priv;

  /* Note: we are careful to consider that destroying children could
   * have the side-effect of destroying other children so
   * priv->children may be modified during clutter_actor_destroy. */
  while (priv->children != NULL)
    {
      ClutterActor *child = priv->children->data;
      priv->children = g_list_delete_link (priv->children, priv->children);
      clutter_actor_destroy (child);
    }

  if (priv->layout)
    {
      clutter_layout_manager_set_container (priv->layout, NULL);
      g_object_unref (priv->layout);
      priv->layout = NULL;
    }

  G_OBJECT_CLASS (clutter_group_parent_class)->dispose (object);
}

static void
clutter_group_real_show_all (ClutterActor *actor)
{
  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_show),
                             NULL);
  clutter_actor_show (actor);
}

static void
clutter_group_real_hide_all (ClutterActor *actor)
{
  clutter_actor_hide (actor);
  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_hide),
                             NULL);
}

static gboolean
clutter_group_real_get_paint_volume (ClutterActor       *actor,
                                     ClutterPaintVolume *volume)
{
  ClutterGroupPrivate *priv = CLUTTER_GROUP (actor)->priv;
  GList *l;

  if (priv->children == NULL)
    return TRUE;

  for (l = priv->children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      const ClutterPaintVolume *child_volume;

      /* This gets the paint volume of the child transformed into the
       * group's coordinate space... */
      child_volume = clutter_actor_get_transformed_paint_volume (child, actor);
      if (!child_volume)
        return FALSE;

      clutter_paint_volume_union (volume, child_volume);
    }

  return TRUE;
}

static void
clutter_group_class_init (ClutterGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_preferred_width = clutter_group_real_get_preferred_width;
  actor_class->get_preferred_height = clutter_group_real_get_preferred_height;
  actor_class->allocate = clutter_group_real_allocate;
  actor_class->paint = clutter_group_real_paint;
  actor_class->pick = clutter_group_real_pick;
  actor_class->show_all = clutter_group_real_show_all;
  actor_class->hide_all = clutter_group_real_hide_all;
  actor_class->get_paint_volume = clutter_group_real_get_paint_volume;

  gobject_class->dispose = clutter_group_dispose;
}

static void
clutter_group_init (ClutterGroup *self)
{
  ClutterActor *actor = CLUTTER_ACTOR (self);

  self->priv = clutter_group_get_instance_private (self);

  /* turn on some optimization
   *
   * XXX - these so-called "optimizations" are insane and should have never
   * been used. they introduce some weird behaviour that breaks invariants
   * and has to be explicitly worked around.
   *
   * this flag was set by the ClutterFixedLayout, but since that layout
   * manager is now the default for ClutterActor, we set the flag explicitly
   * here, to avoid breaking perfectly working actors overriding the
   * allocate() virtual function.
   *
   * also, we keep this flag here so that it can die once we get rid of
   * ClutterGroup.
   */
  clutter_actor_set_flags (actor, CLUTTER_ACTOR_NO_LAYOUT);

  self->priv->layout = clutter_fixed_layout_new ();
  g_object_ref_sink (self->priv->layout);

  clutter_actor_set_layout_manager (actor, self->priv->layout);
}

/**
 * clutter_group_new:
 *
 * Create a new  #ClutterGroup.
 *
 * Return value: the newly created #ClutterGroup actor
 *
 * Deprecated: 1.10: Use clutter_actor_new() instead.
 */
ClutterActor *
clutter_group_new (void)
{
  return g_object_new (CLUTTER_TYPE_GROUP, NULL);
}

/**
 * clutter_group_remove_all:
 * @self: A #ClutterGroup
 *
 * Removes all children actors from the #ClutterGroup.
 *
 * Deprecated: 1.10: Use clutter_actor_remove_all_children() instead.
 */
void
clutter_group_remove_all (ClutterGroup *self)
{
  g_return_if_fail (CLUTTER_IS_GROUP (self));

  clutter_actor_remove_all_children (CLUTTER_ACTOR (self));
}

/**
 * clutter_group_get_n_children:
 * @self: A #ClutterGroup
 *
 * Gets the number of actors held in the group.
 *
 * Return value: The number of child actors held in the group.
 *
 * Since: 0.2
 *
 * Deprecated: 1.10: Use clutter_actor_get_n_children() instead.
 */
gint
clutter_group_get_n_children (ClutterGroup *self)
{
  g_return_val_if_fail (CLUTTER_IS_GROUP (self), 0);

  return clutter_actor_get_n_children (CLUTTER_ACTOR (self));
}

/**
 * clutter_group_get_nth_child:
 * @self: A #ClutterGroup
 * @index_: the position of the requested actor.
 *
 * Gets a groups child held at @index_ in stack.
 *
 * Return value: (transfer none): A Clutter actor, or %NULL if
 *   @index_ is invalid.
 *
 * Since: 0.2
 *
 * Deprecated: 1.10: Use clutter_actor_get_child_at_index() instead.
 */
ClutterActor *
clutter_group_get_nth_child (ClutterGroup *self,
			     gint          index_)
{
  ClutterActor *actor;

  g_return_val_if_fail (CLUTTER_IS_GROUP (self), NULL);

  actor = CLUTTER_ACTOR (self);
  g_return_val_if_fail (index_ <= clutter_actor_get_n_children (actor), NULL);

  return clutter_actor_get_child_at_index (actor, index_);
}
