/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Draw a workspace */

/* This file should not be modified to depend on other files in
 * libwnck or mutter, since it's used in both of them
 */

/* 
 * Copyright (C) 2002 Red Hat Inc.
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

#include "draw-workspace.h"


static void
get_window_rect (const WnckWindowDisplayInfo *win,
                 int                    screen_width,
                 int                    screen_height,
                 const GdkRectangle    *workspace_rect,
                 GdkRectangle          *rect)
{
  double width_ratio, height_ratio;
  int x, y, width, height;
  
  width_ratio = (double) workspace_rect->width / (double) screen_width;
  height_ratio = (double) workspace_rect->height / (double) screen_height;
  
  x = win->x;
  y = win->y;
  width = win->width;
  height = win->height;
  
  x *= width_ratio;
  y *= height_ratio;
  width *= width_ratio;
  height *= height_ratio;
  
  x += workspace_rect->x;
  y += workspace_rect->y;
  
  if (width < 3)
    width = 3;
  if (height < 3)
    height = 3;

  rect->x = x;
  rect->y = y;
  rect->width = width;
  rect->height = height;
}

static void
draw_window (GtkWidget                   *widget,
             GdkDrawable                 *drawable,
             const WnckWindowDisplayInfo *win,
             const GdkRectangle          *winrect,
             GtkStateType                state)
{
  cairo_t *cr;
  GdkPixbuf *icon;
  int icon_x, icon_y, icon_w, icon_h;
  gboolean is_active;
  GdkColor *color;

  is_active = win->is_active;
  
  cr = gdk_cairo_create (drawable);
  cairo_rectangle (cr, winrect->x, winrect->y, winrect->width, winrect->height);
  cairo_clip (cr);

  if (is_active)
    color = &widget->style->light[state];
  else
    color = &widget->style->bg[state];
  cairo_set_source_rgb (cr,
                        color->red / 65535.,
                        color->green / 65535.,
                        color->blue / 65535.);

  cairo_rectangle (cr,
                   winrect->x + 1, winrect->y + 1,
                   MAX (0, winrect->width - 2), MAX (0, winrect->height - 2));
  cairo_fill (cr);


  icon = win->icon;

  icon_w = icon_h = 0;
          
  if (icon)
    {              
      icon_w = gdk_pixbuf_get_width (icon);
      icon_h = gdk_pixbuf_get_height (icon);

      /* If the icon is too big, fall back to mini icon.
       * We don't arbitrarily scale the icon, because it's
       * just too slow on my Athlon 850.
       */
      if (icon_w > (winrect->width - 2) ||
          icon_h > (winrect->height - 2))
        {
          icon = win->mini_icon;
          if (icon)
            {
              icon_w = gdk_pixbuf_get_width (icon);
              icon_h = gdk_pixbuf_get_height (icon);
        
              /* Give up. */
              if (icon_w > (winrect->width - 2) ||
                  icon_h > (winrect->height - 2))
                icon = NULL;
            }
        }
    }

  if (icon)
    {
      icon_x = winrect->x + (winrect->width - icon_w) / 2;
      icon_y = winrect->y + (winrect->height - icon_h) / 2;
      
      cairo_save (cr);
      gdk_cairo_set_source_pixbuf (cr, icon, icon_x, icon_y);
      cairo_rectangle (cr, icon_x, icon_y, icon_w, icon_h);
      cairo_clip (cr);
      cairo_paint (cr);
      cairo_restore (cr);
    }
          
  if (is_active)
    color = &widget->style->fg[state];
  else
    color = &widget->style->fg[state];

  cairo_set_source_rgb (cr,
                        color->red / 65535.,
                        color->green / 65535.,
                        color->blue / 65535.);
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr,
                   winrect->x + 0.5, winrect->y + 0.5,
                   MAX (0, winrect->width - 1), MAX (0, winrect->height - 1));
  cairo_stroke (cr);
  
  cairo_destroy (cr);
}

void
wnck_draw_workspace (GtkWidget                   *widget,
                     GdkDrawable                 *drawable,
                     int                          x,
                     int                          y,
                     int                          width,
                     int                          height,
                     int                          screen_width,
                     int                          screen_height,
                     GdkPixbuf                   *workspace_background,
                     gboolean                     is_active,
                     const WnckWindowDisplayInfo *windows,
                     int                          n_windows)
{
  int i;
  GdkRectangle workspace_rect;
  GtkStateType state;

  workspace_rect.x = x;
  workspace_rect.y = y;
  workspace_rect.width = width;
  workspace_rect.height = height;

  if (is_active)
    state = GTK_STATE_SELECTED;
  else if (workspace_background) 
    state = GTK_STATE_PRELIGHT;
  else
    state = GTK_STATE_NORMAL;
  
  if (workspace_background)
    {
      gdk_draw_pixbuf (drawable,
                       GTK_WIDGET (widget)->style->dark_gc[state],
                       workspace_background,
                       0, 0,
                       x, y,
                       -1, -1,
                       GDK_RGB_DITHER_MAX,
                       0, 0);
    }
  else
    {
      cairo_t *cr;
      
      cr = gdk_cairo_create (widget->window);
      gdk_cairo_set_source_color (cr, &widget->style->dark[state]);
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
      cairo_destroy (cr);
    }
  
  i = 0;
  while (i < n_windows)
    {
      const WnckWindowDisplayInfo *win = &windows[i];
      GdkRectangle winrect;
      
      get_window_rect (win, screen_width,
                       screen_height, &workspace_rect, &winrect);
      
      draw_window (widget,
                   drawable,
                   win,
                   &winrect,
                   state);
      
      ++i;
    }
}
