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
  gdouble min_right;
  gdouble natural_right;

  min_right = 0;
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

      if (child_x + child_min > min_right)
        min_right = child_x + child_min;

      if (child_x + child_natural > natural_right)
        natural_right = child_x + child_natural;
    }

  g_list_free (children);

  if (min_width_p)
    *min_width_p = min_right;

  if (nat_width_p)
    *nat_width_p = natural_right;
}

static void
clutter_fixed_layout_get_preferred_height (ClutterLayoutManager *manager,
                                           ClutterContainer     *container,
                                           gfloat                for_width,
                                           gfloat               *min_height_p,
                                           gfloat               *nat_height_p)
{
  GList *children, *l;
  gdouble min_bottom;
  gdouble natural_bottom;

  min_bottom = 0;
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

      if (child_y + child_min > min_bottom)
        min_bottom = child_y + child_min;

      if (child_y + child_natural > natural_bottom)
        natural_bottom = child_y + child_natural;
    }

  g_list_free (children);

  if (min_height_p)
    *min_height_p = min_bottom;

  if (nat_height_p)
    *nat_height_p = natural_bottom;
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
