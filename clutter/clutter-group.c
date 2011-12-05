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
 *
 *
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include "clutter-group.h"

#include "clutter-container.h"
#include "clutter-fixed-layout.h"
#include "clutter-main.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#include "cogl/cogl.h"

#define CLUTTER_GROUP_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_GROUP, ClutterGroupPrivate))

struct _ClutterGroupPrivate
{
  ClutterLayoutManager *layout;
};

G_DEFINE_TYPE (ClutterGroup, clutter_group, CLUTTER_TYPE_ACTOR)

static void
clutter_group_real_pick (ClutterActor       *actor,
                         const ClutterColor *pick)
{
  GList *children = clutter_actor_get_children (actor);

  /* Chain up so we get a bounding box pained (if we are reactive) */
  CLUTTER_ACTOR_CLASS (clutter_group_parent_class)->pick (actor, pick);

  g_list_foreach (children, (GFunc) clutter_actor_paint, NULL);
  g_list_free (children);
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

  clutter_layout_manager_allocate (priv->layout,
                                   CLUTTER_CONTAINER (actor),
                                   allocation, flags);
}

static void
clutter_group_dispose (GObject *object)
{
  ClutterGroup *self = CLUTTER_GROUP (object);
  ClutterGroupPrivate *priv = self->priv;

  if (priv->layout != NULL)
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

static void
clutter_group_class_init (ClutterGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterGroupPrivate));

  actor_class->get_preferred_width = clutter_group_real_get_preferred_width;
  actor_class->get_preferred_height = clutter_group_real_get_preferred_height;
  actor_class->allocate = clutter_group_real_allocate;
  actor_class->pick = clutter_group_real_pick;
  actor_class->show_all = clutter_group_real_show_all;
  actor_class->hide_all = clutter_group_real_hide_all;

  gobject_class->dispose = clutter_group_dispose;

}

static void
clutter_group_init (ClutterGroup *self)
{
  self->priv = CLUTTER_GROUP_GET_PRIVATE (self);

  self->priv->layout = clutter_fixed_layout_new ();
  g_object_ref_sink (self->priv->layout);

  clutter_layout_manager_set_container (self->priv->layout,
                                        CLUTTER_CONTAINER (self));
}

/**
 * clutter_group_new:
 *
 * Create a new  #ClutterGroup.
 *
 * Return value: the newly created #ClutterGroup actor
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
 */
void
clutter_group_remove_all (ClutterGroup *self)
{
  ClutterActor *actor;
  GList *children;

  g_return_if_fail (CLUTTER_IS_GROUP (self));

  actor = CLUTTER_ACTOR (self);
  children = clutter_actor_get_children (actor);

  while (children != NULL)
    {
      ClutterActor *child = children->data;
      children = children->next;

      clutter_actor_remove_child (actor, child);
    }
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
 */
gint
clutter_group_get_n_children (ClutterGroup *self)
{
  GList *children;
  gint retval;

  g_return_val_if_fail (CLUTTER_IS_GROUP (self), 0);

  children = clutter_actor_get_children (CLUTTER_ACTOR (self));
  retval = g_list_length (children);

  g_list_free (children);

  return retval;
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
 */
ClutterActor *
clutter_group_get_nth_child (ClutterGroup *self,
			     gint          index_)
{
  ClutterActor *retval;
  GList *children;

  g_return_val_if_fail (CLUTTER_IS_GROUP (self), NULL);

  children = clutter_actor_get_children (CLUTTER_ACTOR (self));
  if (children == NULL)
    return NULL;

  retval = g_list_nth_data (children, index_);

  g_list_free (children);

  return retval;
}
