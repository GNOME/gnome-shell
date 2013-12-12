/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter tile-preview marks the area a window will *ehm* snap to */

/*
 * Copyright (C) 2010 Florian Müllner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>
#include <cairo.h>

#include "tile-preview.h"
#include "core.h"

#define OUTLINE_WIDTH 5  /* frame width in non-composite case */


struct _MetaTilePreview {
  GtkWidget     *preview_window;
  gulong         create_serial;

  GdkRGBA       *preview_color;

  MetaRectangle  tile_rect;
};

static gboolean
meta_tile_preview_draw (GtkWidget *widget,
                        cairo_t   *cr,
                        gpointer   user_data)
{
  MetaTilePreview *preview = user_data;

  cairo_set_line_width (cr, 1.0);

  /* Fill the preview area with a transparent color */
  gdk_cairo_set_source_rgba (cr, preview->preview_color);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  /* Use the opaque color for the border */
  cairo_set_source_rgb (cr,
                        preview->preview_color->red,
                        preview->preview_color->green,
                        preview->preview_color->blue);

  cairo_rectangle (cr,
                   0.5, 0.5,
                   preview->tile_rect.width - 1,
                   preview->tile_rect.height - 1);
  cairo_stroke (cr);

  return FALSE;
}

MetaTilePreview *
meta_tile_preview_new (int      screen_number)
{
  MetaTilePreview *preview;
  GdkScreen *screen;
  GtkStyleContext *context;
  GtkWidgetPath *path;
  guchar selection_alpha = 0xFF;

  screen = gdk_display_get_screen (gdk_display_get_default (), screen_number);

  preview = g_new (MetaTilePreview, 1);

  preview->preview_window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_screen (GTK_WINDOW (preview->preview_window), screen);
  gtk_widget_set_app_paintable (preview->preview_window, TRUE);

  preview->preview_color = NULL;

  preview->tile_rect.x = preview->tile_rect.y = 0;
  preview->tile_rect.width = preview->tile_rect.height = 0;

  gtk_widget_set_visual (preview->preview_window,
                         gdk_screen_get_rgba_visual (screen));

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);
  gtk_style_context_add_class (context,
                               GTK_STYLE_CLASS_RUBBERBAND);

  gtk_widget_path_free (path);

  gtk_style_context_get (context, GTK_STATE_FLAG_SELECTED,
                         "background-color", &preview->preview_color,
                         NULL);

  /* The background-color for the .rubberband class should probably
   * contain the correct alpha value - unfortunately, at least for now
   * it doesn't. Hopefully the following workaround can be removed
   * when GtkIconView gets ported to GtkStyleContext.
   */
  gtk_style_context_get_style (context,
                               "selection-box-alpha", &selection_alpha,
                               NULL);
  preview->preview_color->alpha = (double)selection_alpha / 0xFF;

  g_object_unref (context);

  /* We make an assumption that XCreateWindow will be the first operation
   * when calling gtk_widget_realize() (via gdk_window_new()), or that it
   * is at least "close enough".
   */
  preview->create_serial = XNextRequest (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  gtk_widget_realize (preview->preview_window);
  g_signal_connect (preview->preview_window, "draw",
                    G_CALLBACK (meta_tile_preview_draw), preview);

  return preview;
}

void
meta_tile_preview_free (MetaTilePreview *preview)
{
  gtk_widget_destroy (preview->preview_window);

  if (preview->preview_color)
    gdk_rgba_free (preview->preview_color);

  g_free (preview);
}

void
meta_tile_preview_show (MetaTilePreview *preview,
                        MetaRectangle   *tile_rect)
{
  GdkWindow *window;
  GdkRectangle old_rect;

  if (gtk_widget_get_visible (preview->preview_window)
      && preview->tile_rect.x == tile_rect->x
      && preview->tile_rect.y == tile_rect->y
      && preview->tile_rect.width == tile_rect->width
      && preview->tile_rect.height == tile_rect->height)
    return; /* nothing to do */

  gtk_widget_show (preview->preview_window);
  window = gtk_widget_get_window (preview->preview_window);
  meta_core_lower_beneath_grab_window (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                       GDK_WINDOW_XID (window),
                                       gtk_get_current_event_time ());

  old_rect.x = old_rect.y = 0;
  old_rect.width = preview->tile_rect.width;
  old_rect.height = preview->tile_rect.height;

  gdk_window_invalidate_rect (window, &old_rect, FALSE);

  preview->tile_rect = *tile_rect;

  gdk_window_move_resize (window,
                          preview->tile_rect.x, preview->tile_rect.y,
                          preview->tile_rect.width, preview->tile_rect.height);
}

void
meta_tile_preview_hide (MetaTilePreview *preview)
{
  gtk_widget_hide (preview->preview_window);
}

Window
meta_tile_preview_get_xwindow (MetaTilePreview *preview,
                               gulong          *create_serial)
{
  GdkWindow *window = gtk_widget_get_window (preview->preview_window);

  if (create_serial)
    *create_serial = preview->create_serial;

  return GDK_WINDOW_XID (window);
}
