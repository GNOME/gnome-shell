/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter fixed tooltip routine */

/* 
 * Copyright (C) 2001 Havoc Pennington
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
#include "fixedtip.h"
#include "ui.h"

/**
 * The floating rectangle.  This is a GtkWindow, and it contains
 * the "label" widget, below.
 */
static GtkWidget *tip = NULL;

/**
 * The actual text that gets displayed.
 */
static GtkWidget *label = NULL;
/*
 * X coordinate of the right-hand edge of the screen.
 *
 * \bug  This appears to be a bug; screen_right_edge is calculated only when
 *       the window is redrawn.  Actually we should never cache it because
 *       different windows are different sizes.
 */
static int screen_right_edge = 0;
/*
 * Y coordinate of the bottom edge of the screen.
 *
 * \bug  As with screen_right_edge.
 */
static int screen_bottom_edge = 0;

static gint
expose_handler (GtkTooltips *tooltips)
{
  gtk_paint_flat_box (tip->style, tip->window,
                      GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
                      NULL, tip, "tooltip",
                      0, 0, -1, -1);

  return FALSE;
}

void
meta_fixed_tip_show (Display *xdisplay, int screen_number,
                     int root_x, int root_y,
                     const char *markup_text)
{
  int w, h;
  
  if (tip == NULL)
    {      
      tip = gtk_window_new (GTK_WINDOW_POPUP);
      {
        GdkScreen *gdk_screen;
	GdkRectangle monitor;
	gint mon_num;

        gdk_screen = gdk_display_get_screen (gdk_display_get_default (),
                                             screen_number);
        gtk_window_set_screen (GTK_WINDOW (tip),
                               gdk_screen);
	mon_num = gdk_screen_get_monitor_at_point (gdk_screen, root_x, root_y);
	gdk_screen_get_monitor_geometry (gdk_screen, mon_num, &monitor);
	screen_right_edge = monitor.x + monitor.width;
	screen_bottom_edge = monitor.y + monitor.height;
      }
      
      gtk_widget_set_app_paintable (tip, TRUE);
      gtk_window_set_resizable (GTK_WINDOW (tip), FALSE);
      gtk_widget_set_name (tip, "gtk-tooltips");
      gtk_container_set_border_width (GTK_CONTAINER (tip), 4);

      g_signal_connect_swapped (tip, "expose_event",
				 G_CALLBACK (expose_handler), NULL);

      label = gtk_label_new (NULL);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_widget_show (label);
      
      gtk_container_add (GTK_CONTAINER (tip), label);

      g_signal_connect (tip, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &tip);
    }

  gtk_label_set_markup (GTK_LABEL (label), markup_text);
  
  gtk_window_get_size (GTK_WINDOW (tip), &w, &h);

  if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
      root_x = MAX(0, root_x - w);

  if ((root_x + w) > screen_right_edge)
    root_x -= (root_x + w) - screen_right_edge;
  
  gtk_window_move (GTK_WINDOW (tip), root_x, root_y);

  gtk_widget_show (tip);
}

void
meta_fixed_tip_hide (void)
{
  if (tip)
    {
      gtk_widget_destroy (tip);
      tip = NULL;
    }
}
