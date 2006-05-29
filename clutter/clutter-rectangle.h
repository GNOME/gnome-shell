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

#ifndef _HAVE_CLUTTER_RECTANGLE_H
#define _HAVE_CLUTTER_RECTANGLE_H

#include <glib-object.h>
#include <clutter/clutter-element.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_RECTANGLE clutter_rectangle_get_type()

#define CLUTTER_RECTANGLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_RECTANGLE, ClutterRectangle))

#define CLUTTER_RECTANGLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_RECTANGLE, ClutterRectangleClass))

#define CLUTTER_IS_RECTANGLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_RECTANGLE))

#define CLUTTER_IS_RECTANGLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_RECTANGLE))

#define CLUTTER_RECTANGLE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_RECTANGLE, ClutterRectangleClass))

typedef struct _ClutterRectanglePrivate ClutterRectanglePrivate;
typedef struct _ClutterRectangle        ClutterRectangle;
typedef struct _ClutterRectangleClass   ClutterRectangleClass;

struct _ClutterRectangle
{
  ClutterElement           parent;

  /*< priv >*/
  ClutterRectanglePrivate *priv;
}; 

struct _ClutterRectangleClass 
{
  ClutterElementClass parent_class;
};

GType clutter_rectangle_get_type (void);

ClutterElement*
clutter_rectangle_new (guint32 col);

G_END_DECLS

#endif
