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

#include <gdk/gdkx.h>

#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkwidget.h>

#include <clutter/clutter-main.h>
#include <clutter/clutter-stage.h>

#include "gtk-clutter.h"

#define GTK_CLUTTER_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_CLUTTER, GtkClutterPrivate))

struct _GtkClutterPrivate {
  ClutterActor *stage;
};

static GtkDrawingAreaClass *parent_class;

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

  req->width = 800;
  req->height = 600;
}

static void
realize (GtkWidget *widget)
{
  GtkClutter *clutter;
  GtkClutterPrivate *priv;
  XVisualInfo *xvinfo;
  GdkVisual *visual;
  GdkColormap *colormap;
  gboolean foreign_success;

  clutter = GTK_CLUTTER (widget);
  priv = GTK_CLUTTER_GET_PRIVATE (clutter);

  /* We need to use the colormap from the Clutter visual */
  xvinfo = clutter_stage_get_xvisual (CLUTTER_STAGE (priv->stage));
  visual = gdk_x11_screen_lookup_visual (gdk_screen_get_default (),
                                         xvinfo->visualid);
  colormap = gdk_colormap_new (visual, FALSE);
  gtk_widget_set_colormap (widget, colormap);

  /* And turn off double buffering, cos GL doesn't like it */
  gtk_widget_set_double_buffered (widget, FALSE);

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

  clutter = GTK_CLUTTER (widget);
  priv = GTK_CLUTTER_GET_PRIVATE (clutter);

  clutter_stage_set_xwindow_foreign (CLUTTER_STAGE (priv->stage), 
                                     GDK_WINDOW_XID (widget->window));
}

static void
gtk_clutter_class_init (GtkClutterClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  gobject_class->dispose = dispose;

  widget_class->size_request = size_request;
  widget_class->realize = realize;

  g_type_class_add_private (gobject_class, sizeof (GtkClutterPrivate));

  parent_class = g_type_class_peek_parent (klass);
}

static void
gtk_clutter_init (GtkClutter *clutter)
{
  GtkClutterPrivate *priv;

  clutter->priv = priv = GTK_CLUTTER_GET_PRIVATE (clutter);

  gtk_widget_set_double_buffered (GTK_WIDGET (clutter), FALSE);

  priv->stage = clutter_stage_get_default ();
}

G_DEFINE_TYPE (GtkClutter, gtk_clutter, GTK_TYPE_DRAWING_AREA);

/**
 * gtk_clutter_get_stage:
 * @clutter: A #GtkClutter object.
 *
 * Obtains the #ClutterStage associated with this object.
 *
 * Return value: A #ClutterActor.
 */
ClutterActor *
gtk_clutter_get_stage (GtkClutter *clutter)
{
  g_return_val_if_fail (GTK_IS_CLUTTER (clutter), NULL);

  return clutter->priv->stage;
}
