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

#ifndef _HAVE_CLUTTER_COLOR_H
#define _HAVE_CLUTTER_COLOR_H

#include <glib.h>

G_BEGIN_DECLS

#define clutter_color_r(col) ((col) >> 24)
#define clutter_color_g(col) (((col) >> 16) & 0xff)
#define clutter_color_b(col) (((col) >> 8) & 0xff)
#define clutter_color_a(col) ((col) & 0xff)

#define clutter_color_set_r(col,r) ((col) &= (r)) 
#define clutter_color_set_g(col,g) ((col) &= (g << 8)) 
#define clutter_color_set_b(col,b) ((col) &= (b << 16)) 
#define clutter_color_set_a(col,a) ((col) &= (a << 24)) 

typedef guint32 ClutterColor;

ClutterColor
clutter_color_new (guint8 r, guint8 g, guint8 b, guint8 a);

void
clutter_color_set (ClutterColor *color, 
		   guint8        r, 
		   guint8        g, 
		   guint8        b, 
		   guint8        a);

void
clutter_color_get (ClutterColor  color, 
		   guint8        *r, 
		   guint8        *g, 
		   guint8        *b, 
		   guint8        *a);

G_END_DECLS

#endif
