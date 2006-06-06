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

#include "config.h"

#include <gtk/gtksocket.h>
#include <gtk/gtkwidget.h>

#include <clutter/clutter-stage.h>

#include "gtk-clutter.h"

#define GTK_CLUTTER_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_CLUTTER, GtkClutterPrivate))

struct _GtkClutterPrivate {
  ClutterElement *stage;
  gboolean anchored;
};

static GtkSocketClass *parent_class;

static void
dispose (GObject *object)
{
  GtkClutter *clutter;
  GtkClutterPrivate *priv;

  clutter = GTK_CLUTTER (object);
  priv = GTK_CLUTTER_GET_PRIVATE (clutter);

  if (priv->stage) {
    g_object_unref (G_OBJECT (priv->stage));
    priv->stage = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
size_request (GtkWidget *widget,
              GtkRequisition *req)
{
  GtkClutter *clutter;
  GtkClutterPrivate *priv;

  clutter = GTK_CLUTTER (widget);
  priv = GTK_CLUTTER_GET_PRIVATE (clutter);

  req->width = clutter_element_get_width (priv->stage);
  req->height = clutter_element_get_height (priv->stage);
}

static void
hierarchy_changed (GtkWidget *widget,
                   GtkWidget *parent)
{
  GtkClutter *clutter;
  GtkClutterPrivate *priv;

  clutter = GTK_CLUTTER (widget);
  priv = GTK_CLUTTER_GET_PRIVATE (clutter);

  if (!priv->anchored) {
    /* Now we can add our stage to the socket */
    gtk_socket_add_id 
      (GTK_SOCKET (clutter), (GdkNativeWindow) clutter_stage_get_xwindow (CLUTTER_STAGE (priv->stage)));

    priv->anchored = TRUE;
  }
}

static void
gtk_clutter_class_init (GtkClutterClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  gobject_class->dispose = dispose;

  widget_class->size_request = size_request;
  widget_class->hierarchy_changed = hierarchy_changed;

  g_type_class_add_private (gobject_class, sizeof (GtkClutterPrivate));

  parent_class = g_type_class_peek_parent (klass);
}

static void
gtk_clutter_init (GtkClutter *clutter)
{
  GtkClutterPrivate *priv;

  clutter->priv = priv = GTK_CLUTTER_GET_PRIVATE (clutter);
  
  priv->stage = clutter_stage_get_default ();
  priv->anchored = FALSE;
}

G_DEFINE_TYPE (GtkClutter, gtk_clutter, GTK_TYPE_SOCKET);

ClutterElement *
gtk_clutter_get_stage (GtkClutter *clutter)
{
  g_return_val_if_fail (GTK_IS_CLUTTER (clutter), NULL);

  return clutter->priv->stage;
}
