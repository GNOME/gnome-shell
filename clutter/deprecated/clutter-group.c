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

G_DEFINE_TYPE (ClutterGroup, clutter_group, CLUTTER_TYPE_ACTOR)

static void
clutter_group_real_show_all (ClutterActor *actor)
{
  ClutterActor *iter;

  for (iter = clutter_actor_get_first_child (actor);
       iter != NULL;
       iter = clutter_actor_get_next_sibling (iter))
    clutter_actor_show (iter);

  clutter_actor_show (actor);
}

static void
clutter_group_real_hide_all (ClutterActor *actor)
{
  ClutterActor *iter;

  clutter_actor_hide (actor);

  for (iter = clutter_actor_get_first_child (actor);
       iter != NULL;
       iter = clutter_actor_get_next_sibling (iter))
    clutter_actor_hide (iter);
}

static gboolean
clutter_group_get_paint_volume (ClutterActor *actor,
                                ClutterPaintVolume *volume)
{
  ClutterActor *child;
  gboolean retval;

  /* bail out early if we don't have any child */
  if (clutter_actor_get_n_children (actor) == 0)
    return FALSE;

  retval = TRUE;

  /* otherwise, union the paint volumes of our children, in case
   * any one of them decides to paint outside the parent's allocation
   */
  for (child = clutter_actor_get_first_child (actor);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      const ClutterPaintVolume *child_volume;

      /* This gets the paint volume of the child transformed into the
       * group's coordinate space... */
      child_volume = clutter_actor_get_transformed_paint_volume (child, actor);
      if (!child_volume)
        return FALSE;

      clutter_paint_volume_union (volume, child_volume);
    }

  return retval;
}

static void
clutter_group_class_init (ClutterGroupClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->show_all = clutter_group_real_show_all;
  actor_class->hide_all = clutter_group_real_hide_all;
  actor_class->get_paint_volume = clutter_group_get_paint_volume;
}

static void
clutter_group_init (ClutterGroup *self)
{
  clutter_actor_set_layout_manager (CLUTTER_ACTOR (self),
                                    clutter_fixed_layout_new ());
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
