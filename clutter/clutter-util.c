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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-util
 * @short_description: Utility functions
 *
 * Various miscellaneous utilility functions.
 */

#include "clutter-util.h"
#include "clutter-main.h"

/**
 * clutter_util_next_p2:
 * @a: Value to get the next power
 *
 * Calculates the nearest power of two, greater than or equal to @a.
 *
 * Return value: The nearest power of two, greater or equal to @a.
 *
 * Deprecated: 1.2
 */
gint
clutter_util_next_p2 (gint a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}
