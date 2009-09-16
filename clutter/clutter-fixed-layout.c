/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *
 * Based on the fixed layout code inside clutter-group.c
 */

/**
 * SECTION:clutter-fixed-layout
 * @short_description: A fixed layout manager
 *
 * #ClutterFixedLayout is a layout manager implementing the same
 * layout policies as #ClutterGroup.
 *
 * #ClutterFixedLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-fixed-layout.h"
#include "clutter-private.h"

G_DEFINE_TYPE (ClutterFixedLayout,
               clutter_fixed_layout,
               CLUTTER_TYPE_LAYOUT_MANAGER);

static void
clutter_fixed_layout_get_preferred_width (ClutterLayoutManager *manager,
                                          ClutterContainer     *container,
                                          gfloat                for_height,
                                          gfloat               *min_width_p,
                                          gfloat               *nat_width_p)
{
  GList *children, *l;
  gdouble min_left, min_right;
  gdouble natural_left, natural_right;

  min_left = 0;
  min_right = 0;
  natural_left = 0;
  natural_right = 0;

  children = clutter_container_get_children (container);
  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_x, child_min, child_natural;

      child_x = clutter_actor_get_x (child);

      clutter_actor_get_preferred_size (child,
                                        &child_min, NULL,
                                        &child_natural, NULL);

      if (l == children)
        {
          /* First child */
          min_left = child_x;
          natural_left = child_x;
          min_right = min_left + child_min;
          natural_right = natural_left + child_natural;
        }
      else
        {
          /* Union of extents with previous children */
          if (child_x < min_left)
            min_left = child_x;

          if (child_x < natural_left)
            natural_left = child_x;

          if (child_x + child_min > min_right)
            min_right = child_x + child_min;

          if (child_x + child_natural > natural_right)
            natural_right = child_x + child_natural;
        }
    }

  g_list_free (children);

  /* The preferred size is defined as the width and height we want starting
   * from our origin, since our allocation will set the origin; so we now
   * need to remove any part of the request that is to the left of the origin.
   */
  if (min_left < 0)
    min_left = 0;

  if (natural_left < 0)
    natural_left = 0;

  if (min_right < 0)
    min_right = 0;

  if (natural_right < 0)
    natural_right = 0;

  g_assert (min_right >= min_left);
  g_assert (natural_right >= natural_left);

  if (min_width_p)
    *min_width_p = min_right - min_left;

  if (nat_width_p)
    *nat_width_p = natural_right - min_left;
}

static void
clutter_fixed_layout_get_preferred_height (ClutterLayoutManager *manager,
                                           ClutterContainer     *container,
                                           gfloat                for_width,
                                           gfloat               *min_height_p,
                                           gfloat               *nat_height_p)
{
  GList *children, *l;
  gdouble min_top, min_bottom;
  gdouble natural_top, natural_bottom;

  min_top = 0;
  min_bottom = 0;
  natural_top = 0;
  natural_bottom = 0;

  children = clutter_container_get_children (container);
  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_y, child_min, child_natural;

      child_y = clutter_actor_get_y (child);

      clutter_actor_get_preferred_size (child,
                                        NULL, &child_min,
                                        NULL, &child_natural);

      if (l == children)
        {
          /* First child */
          min_top = child_y;
          natural_top = child_y;
          min_bottom = min_top + child_min;
          natural_bottom = natural_top + child_natural;
        }
      else
        {
          /* Union of extents with previous children */
          if (child_y < min_top)
            min_top = child_y;

          if (child_y < natural_top)
            natural_top = child_y;

          if (child_y + child_min > min_bottom)
            min_bottom = child_y + child_min;

          if (child_y + child_natural > natural_bottom)
            natural_bottom = child_y + child_natural;
        }
    }

  g_list_free (children);

  /* The preferred size is defined as the width and height we want starting
   * from our origin, since our allocation will set the origin; so we now
   * need to remove any part of the request that is above the origin.
   */
  if (min_top < 0)
    min_top = 0;

  if (natural_top < 0)
    natural_top = 0;

  if (min_bottom < 0)
    min_bottom = 0;

  if (natural_bottom < 0)
    natural_bottom = 0;

  g_assert (min_bottom >= min_top);
  g_assert (natural_bottom >= natural_top);

  if (min_height_p)
    *min_height_p = min_bottom - min_top;

  if (nat_height_p)
    *nat_height_p = natural_bottom - min_top;
}

static void
clutter_fixed_layout_allocate (ClutterLayoutManager   *manager,
                               ClutterContainer       *container,
                               const ClutterActorBox  *allocation,
                               ClutterAllocationFlags  flags)
{
  GList *children, *l;

  children = clutter_container_get_children (container);

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      clutter_actor_allocate_preferred_size (child, flags);
    }

  g_list_free (children);
}

static void
clutter_fixed_layout_class_init (ClutterFixedLayoutClass *klass)
{
  ClutterLayoutManagerClass *manager_class =
    CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  manager_class->get_preferred_width =
    clutter_fixed_layout_get_preferred_width;
  manager_class->get_preferred_height =
    clutter_fixed_layout_get_preferred_height;
  manager_class->allocate = clutter_fixed_layout_allocate;
}

static void
clutter_fixed_layout_init (ClutterFixedLayout *self)
{
}

/**
 * clutter_fixed_layout_new:
 *
 * Creates a new #ClutterFixedLayout
 *
 * Return value: the newly created #ClutterFixedLayout
 *
 * Since: 1.2
 */
ClutterLayoutManager *
clutter_fixed_layout_new (void)
{
  return g_object_new (CLUTTER_TYPE_FIXED_LAYOUT, NULL);
}
