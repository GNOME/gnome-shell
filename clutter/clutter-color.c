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

#include "clutter-color.h"

ClutterColor
clutter_color_new (guint8 r, guint8 g, guint8 b, guint8 a)
{
  return ( r | g << 8 | b << 16 | a << 24 );
}

void
clutter_color_set (ClutterColor *color, 
		   guint8        r, 
		   guint8        g, 
		   guint8        b, 
		   guint8        a)
{
  *color = ( r | g << 8 | b << 16 | a << 24 );
}

void
clutter_color_get (ClutterColor  color, 
		   guint8        *r, 
		   guint8        *g, 
		   guint8        *b, 
		   guint8        *a)
{
  if (r)
    *r = clutter_color_r(color);
  if (g)
    *g = clutter_color_g(color);
  if (b)
    *b = clutter_color_b(color);
  if (a)
    *a = clutter_color_a(color);
}


