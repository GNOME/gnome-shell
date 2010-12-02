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

  GdkColor      *preview_color;
  guchar         preview_alpha;

  MetaRectangle  tile_rect;

  gboolean       has_alpha: 1;
};

static gboolean
meta_tile_preview_draw (GtkWidget *widget,
                        cairo_t   *cr,
                        gpointer   user_data)
{
  MetaTilePreview *preview = user_data;

  cairo_set_line_width (cr, 1.0);

  if (preview->has_alpha)
    {
      /* Fill the preview area with a transparent color */
      cairo_set_source_rgba (cr,
                             (double)preview->preview_color->red   / 0xFFFF,
                             (double)preview->preview_color->green / 0xFFFF,
                             (double)preview->preview_color->blue  / 0xFFFF,
                             (double)preview->preview_alpha / 0xFF);

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);

      /* Use the opaque color for the border */
      gdk_cairo_set_source_color (cr, preview->preview_color);
    }
  else
    {
      GtkStyle *style = gtk_widget_get_style (preview->preview_window);

      gdk_cairo_set_source_color (cr, &style->white);

      cairo_rectangle (cr,
                       OUTLINE_WIDTH - 0.5, OUTLINE_WIDTH - 0.5,
                       preview->tile_rect.width - 2 * (OUTLINE_WIDTH - 1) - 1,
                       preview->tile_rect.height - 2 * (OUTLINE_WIDTH - 1) - 1);
      cairo_stroke (cr);
    }

  cairo_rectangle (cr,
                   0.5, 0.5,
                   preview->tile_rect.width - 1,
                   preview->tile_rect.height - 1);
  cairo_stroke (cr);

  return FALSE;
}

static void
on_preview_window_style_set (GtkWidget *widget,
                             GtkStyle  *previous,
                             gpointer   user_data)
{
  MetaTilePreview *preview = user_data;
  GtkStyle *style;

  style = gtk_rc_get_style_by_paths (gtk_widget_get_settings (widget),
                                     "GtkWindow.GtkIconView",
                                     "GtkWindow.GtkIconView",
                                     GTK_TYPE_ICON_VIEW);

  if (style != NULL)
    g_object_ref (style);
  else
    style = gtk_style_new ();

  gtk_style_get (style, GTK_TYPE_ICON_VIEW,
                 "selection-box-color", &preview->preview_color,
                 "selection-box-alpha", &preview->preview_alpha,
                 NULL);
  if (!preview->preview_color)
    {
      GdkColor selection = style->base[GTK_STATE_SELECTED];
      preview->preview_color = gdk_color_copy (&selection);
    }

  g_object_unref (style);
}

MetaTilePreview *
meta_tile_preview_new (int      screen_number,
                       gboolean composited)
{
  MetaTilePreview *preview;
  GdkScreen *screen;

  screen = gdk_display_get_screen (gdk_display_get_default (), screen_number);

  preview = g_new (MetaTilePreview, 1);

  preview->preview_window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_screen (GTK_WINDOW (preview->preview_window), screen);
  gtk_widget_set_app_paintable (preview->preview_window, TRUE);

  preview->preview_color = NULL;
  preview->preview_alpha = 0xFF;

  preview->tile_rect.x = preview->tile_rect.y = 0;
  preview->tile_rect.width = preview->tile_rect.height = 0;

  preview->has_alpha = composited &&
                       (gdk_screen_get_rgba_visual (screen) != NULL);

  if (preview->has_alpha)
    {
      gtk_widget_set_visual (preview->preview_window,
                             gdk_screen_get_rgba_visual (screen));

      g_signal_connect (preview->preview_window, "style-set",
                        G_CALLBACK (on_preview_window_style_set), preview);
    }

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
    gdk_color_free (preview->preview_color);

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
  meta_core_lower_beneath_focus_window (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
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

  if (!preview->has_alpha)
    {
      cairo_region_t *outer_region, *inner_region;
      GdkRectangle outer_rect, inner_rect;
      GdkColor black;

      black = gtk_widget_get_style (preview->preview_window)->black;
      gdk_window_set_background (window, &black);

      outer_rect.x = outer_rect.y = 0;
      outer_rect.width = preview->tile_rect.width;
      outer_rect.height = preview->tile_rect.height;

      inner_rect.x = OUTLINE_WIDTH;
      inner_rect.y = OUTLINE_WIDTH;
      inner_rect.width = outer_rect.width - 2 * OUTLINE_WIDTH;
      inner_rect.height = outer_rect.height - 2 * OUTLINE_WIDTH;

      outer_region = cairo_region_create_rectangle (&outer_rect);
      inner_region = cairo_region_create_rectangle (&inner_rect);

      cairo_region_subtract (outer_region, inner_region);
      cairo_region_destroy (inner_region);

      gdk_window_shape_combine_region (window, outer_region, 0, 0);
      cairo_region_destroy (outer_region);
    }
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
