/*
 * GTK-Clutter.
 *
 * GTK+ widget for Clutter.
 *
 * Authored By Iain Holmes  <iain@openedhand.com>
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

#ifndef _HAVE_GTK_CLUTTER_H
#define _HAVE_GTK_CLUTTER_H

#include <gtk/gtksocket.h>

#include <clutter/clutter-element.h>

G_BEGIN_DECLS

#define GTK_TYPE_CLUTTER gtk_clutter_get_type ()

#define GTK_CLUTTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  GTK_TYPE_CLUTTER, GtkClutter))

#define GTK_CLUTTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CASE ((klass), \
  GTK_TYPE_CLUTTER, GtkClutterClass))

#define GTK_IS_CLUTTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  GTK_TYPE_CLUTTER))

#define GTK_IS_CLUTTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  GTK_TYPE_CLUTTER))

#define GTK_CLUTTER_STAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  GTK_TYPE_CLUTTER, GtkClutterClass))

typedef struct _GtkClutterPrivate GtkClutterPrivate;
typedef struct _GtkClutter GtkClutter;
typedef struct _GtkClutterClass GtkClutterClass;

struct _GtkClutter 
{
  GtkSocket parent;

  /*< private >*/
  GtkClutterPrivate *priv;
};

struct _GtkClutterClass
{
  GtkSocketClass parent_class;
};

GType gtk_clutter_get_type (void);

ClutterElement *gtk_clutter_get_stage (GtkClutter *clutter);

G_END_DECLS

#endif
