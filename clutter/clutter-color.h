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

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_COLOR	(clutter_color_get_type ())

typedef struct _ClutterColor ClutterColor;

struct _ClutterColor
{
  guint8 red;
  guint8 green;
  guint8 blue;
  
  guint8 alpha;
};

GType   clutter_color_get_type   (void) G_GNUC_CONST;

void    clutter_color_add        (const ClutterColor *src1,
			          const ClutterColor *src2,
			          ClutterColor       *dest);
void    clutter_color_subtract   (const ClutterColor *src1,
			          const ClutterColor *src2,
			          ClutterColor       *dest);

void    clutter_color_lighten    (const ClutterColor *src,
			          ClutterColor       *dest);
void    clutter_color_darken     (const ClutterColor *src,
			          ClutterColor       *dest);
void    clutter_color_shade      (const ClutterColor *src,
			          ClutterColor       *dest,
			          gdouble             shade);

void    clutter_color_to_hls     (const ClutterColor *src,
			          guint8             *hue,
			          guint8             *luminance,
			          guint8             *saturation);
void    clutter_color_from_hls   (ClutterColor       *dest,
			          guint8              hue,
			          guint8              luminance,
			          guint8              saturation);

guint32 clutter_color_to_pixel   (const ClutterColor *src);
void    clutter_color_from_pixel (ClutterColor       *dest,
				  guint32             pixel);

G_END_DECLS

#endif
