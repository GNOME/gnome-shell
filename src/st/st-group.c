/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-group.h: A fixed layout container based on ClutterGroup
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
 *
 */

/**
 * SECTION:st-group
 * SECTION:clutter-group
 * @short_description: A fixed layout container
 *
 * A #StGroup is an Actor which contains multiple child actors positioned
 * relative to the #StGroup position. Other operations such as scaling,
 * rotating and clipping of the group will apply to the child actors.
 *
 * A #StGroup's size is defined by the size and position of its children;
 * it will be the smallest non-negative size that covers the right and bottom
 * edges of all of its children.
 *
 * Setting the size on a Group using #ClutterActor methods like
 * clutter_actor_set_size() will override the natural size of the Group,
 * however this will not affect the size of the children and they may still
 * be painted outside of the allocation of the group. One way to constrain
 * the visible area of a #StGroup to a specified allocation is to
 * explicitly set the size of the #StGroup and then use the
 * #ClutterActor:clip-to-allocation property.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter.h>

#include "st-group.h"
#include "st-enum-types.h"
#include "st-private.h"

G_DEFINE_TYPE (StGroup, st_group, ST_TYPE_CONTAINER);

static void
st_group_paint (ClutterActor *actor)
{
  CLUTTER_ACTOR_CLASS (st_group_parent_class)->paint (actor);

  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_paint),
                             NULL);
}

static void
st_group_pick (ClutterActor       *actor,
               const ClutterColor *pick)
{
  /* Chain up so we get a bounding box painted (if we are reactive) */
  CLUTTER_ACTOR_CLASS (st_group_parent_class)->pick (actor, pick);

  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_paint),
                             NULL);
}

static void
st_group_get_preferred_width (ClutterActor *actor,
                              gfloat        for_height,
                              gfloat       *min_width,
                              gfloat       *natural_width)
{
  GList *l, *children;
  gdouble min_right, natural_right;

  /* We will always be at least 0 sized (ie, if all of the actors are
     to the left of the origin we won't return a negative size) */
  min_right = 0;
  natural_right = 0;

  children = st_container_get_children_list (ST_CONTAINER (actor));

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_x, child_min, child_nat;

      child_x = clutter_actor_get_x (child);

      /* for_height is irrelevant to the fixed layout, so it's not used */
      _st_actor_get_preferred_width (child, -1, FALSE,
                                     &child_min, &child_nat);

      /* Track the rightmost edge */
      if (child_x + child_min > min_right)
        min_right = child_x + child_min;

      if (child_x + child_nat > natural_right)
        natural_right = child_x + child_nat;
    }

  /* The size is defined as the distance from the origin to the right-hand
     edge of the rightmost actor */
  if (min_width)
    *min_width = min_right;

  if (natural_width)
    *natural_width = natural_right;
}

static void
st_group_get_preferred_height (ClutterActor *actor,
                               gfloat        for_width,
                               gfloat       *min_height,
                               gfloat       *natural_height)
{
  GList *l, *children;
  gdouble min_bottom, natural_bottom;

  /* We will always be at least 0 sized (ie, if all of the actors are
     above of the origin we won't return a negative size) */
  min_bottom = 0;
  natural_bottom = 0;

  children = st_container_get_children_list (ST_CONTAINER (actor));

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_y, child_min, child_nat;

      child_y = clutter_actor_get_y (child);

      /* for_width is irrelevant to the fixed layout, so it's not used */
      _st_actor_get_preferred_height (child, -1, FALSE,
                                      &child_min, &child_nat);

      /* Track the bottommost edge */
      if (child_y + child_min > min_bottom)
        min_bottom = child_y + child_min;

      if (child_y + child_nat > natural_bottom)
        natural_bottom = child_y + child_nat;
    }

  /* The size is defined as the distance from the origin to the right-hand
     edge of the rightmost actor */
  if (min_height)
    *min_height = min_bottom;

  if (natural_height)
    *natural_height = natural_bottom;
}

static void
st_group_allocate (ClutterActor           *actor,
                   const ClutterActorBox  *box,
                   ClutterAllocationFlags  flags)
{
  GList *l, *children;

  CLUTTER_ACTOR_CLASS (st_group_parent_class)->allocate (actor, box, flags);

  children = st_container_get_children_list (ST_CONTAINER (actor));
  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      clutter_actor_allocate_preferred_size (child, flags);
    }
}

static void
st_group_show_all (ClutterActor *actor)
{
  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_show),
                             NULL);
  clutter_actor_show (actor);
}

static void
st_group_hide_all (ClutterActor *actor)
{
  clutter_actor_hide (actor);
  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_hide),
                             NULL);
}




static void
st_group_class_init (StGroupClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_preferred_width = st_group_get_preferred_width;
  actor_class->get_preferred_height = st_group_get_preferred_height;
  actor_class->allocate = st_group_allocate;
  actor_class->paint = st_group_paint;
  actor_class->pick = st_group_pick;
  actor_class->show_all = st_group_show_all;
  actor_class->hide_all = st_group_hide_all;
}

static void
st_group_init (StGroup *self)
{
}

/**
 * st_group_new:
 *
 * Create a new  #StGroup.
 *
 * Return value: the newly created #StGroup actor
 */
StWidget *
st_group_new (void)
{
  return g_object_new (ST_TYPE_GROUP, NULL);
}
