/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-icon-colors.c: Colors for colorizing a symbolic icon
 *
 * Copyright 2010 Red Hat, Inc.
 * Copyright 2010 Florian Müllner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "st-icon-colors.h"

/**
 * st_icon_colors_new:
 *
 * Creates a new #StIconColors. All colors are initialized to transparent black.
 *
 * Returns: a newly created #StIconColors. Free with st_icon_colors_unref()
 */
StIconColors *
st_icon_colors_new (void)
{
  return g_atomic_rc_box_new0 (StIconColors);
}

/**
 * st_icon_colors_ref:
 * @colors: a #StIconColors
 *
 * Atomically increments the reference count of @colors by one.
 *
 * Returns: the passed in #StIconColors.
 */
StIconColors *
st_icon_colors_ref (StIconColors *colors)
{
  g_return_val_if_fail (colors != NULL, NULL);

  return g_atomic_rc_box_acquire (colors);
}

/**
 * st_icon_colors_unref:
 * @colors: a #StIconColors
 *
 * Atomically decrements the reference count of @colors by one.
 * If the reference count drops to 0, all memory allocated by the
 * #StIconColors is released.
 */
void
st_icon_colors_unref (StIconColors *colors)
{
  g_return_if_fail (colors != NULL);

  g_atomic_rc_box_release (colors);
}

/**
 * st_icon_colors_copy:
 * @colors: a #StIconColors
 *
 * Creates a new StIconColors structure that is a copy of the passed
 * in @colors. You would use this function instead of st_icon_colors_ref()
 * if you were planning to change colors in the result.
 *
 * Returns: a newly created #StIconColors.
 */
StIconColors *
st_icon_colors_copy (StIconColors *colors)
{
  StIconColors *copy;

  g_return_val_if_fail (colors != NULL, NULL);

  copy = st_icon_colors_new ();

  copy->foreground = colors->foreground;
  copy->warning = colors->warning;
  copy->error = colors->error;
  copy->success = colors->success;

  return copy;
}

/**
 * st_icon_colors_equal:
 * @colors: a #StIconColors
 * @other: another #StIconColors
 *
 * Check if two #StIconColors objects are identical.
 *
 * Returns: %TRUE if the #StIconColors are equal
 */
gboolean
st_icon_colors_equal (StIconColors *colors,
                      StIconColors *other)
{
  if (colors == other)
    return TRUE;

  if (colors == NULL || other == NULL)
    return FALSE;

  return clutter_color_equal (&colors->foreground, &other->foreground) &&
         clutter_color_equal (&colors->warning, &other->warning) &&
         clutter_color_equal (&colors->error, &other->error) &&
         clutter_color_equal (&colors->success, &other->success);
}

G_DEFINE_BOXED_TYPE (StIconColors,
                     st_icon_colors,
                     st_icon_colors_ref,
                     st_icon_colors_unref)
