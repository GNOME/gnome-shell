/* Draw a workspace */

/* This file should not be modified to depend on other files in
 * libwnck or metacity, since it's used in both of them
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
draw_window (GtkWidget             *widget,
             GdkDrawable           *drawable,
             const WnckWindowDisplayInfo *win,
             const GdkRectangle    *winrect)
{
  GdkPixbuf *icon;
  int icon_x, icon_y, icon_w, icon_h;
          
  gdk_draw_rectangle (drawable,
                      win->is_active ?
                      widget->style->bg_gc[GTK_STATE_SELECTED] :
                      widget->style->bg_gc[GTK_STATE_NORMAL],
                      TRUE,
                      winrect->x + 1, winrect->y + 1,
                      winrect->width - 2, winrect->height - 2);

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
                
      {
        /* render_to_drawable should take a clip rect to save
         * us this mess...
         */
        GdkRectangle pixbuf_rect;
        GdkRectangle draw_rect;

        pixbuf_rect.x = icon_x;
        pixbuf_rect.y = icon_y;
        pixbuf_rect.width = icon_w;
        pixbuf_rect.height = icon_h;
        
        if (gdk_rectangle_intersect ((GdkRectangle *)winrect, &pixbuf_rect,
                                     &draw_rect))
          {
            gdk_pixbuf_render_to_drawable_alpha (icon,
                                                 drawable,
                                                 draw_rect.x - pixbuf_rect.x,
                                                 draw_rect.y - pixbuf_rect.y,
                                                 draw_rect.x, draw_rect.y,
                                                 draw_rect.width,
                                                 draw_rect.height,
                                                 GDK_PIXBUF_ALPHA_FULL,
                                                 128,
                                                 GDK_RGB_DITHER_NORMAL,
                                                 0, 0);
          }
      }
    }
          
  gdk_draw_rectangle (drawable,
                      win->is_active ?
                      widget->style->fg_gc[GTK_STATE_SELECTED] :
                      widget->style->fg_gc[GTK_STATE_NORMAL],
                      FALSE,
                      winrect->x, winrect->y,
                      winrect->width - 1, winrect->height - 1);
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
  
  workspace_rect.x = x;
  workspace_rect.y = y;
  workspace_rect.width = width;
  workspace_rect.height = height;

  
  if (is_active)
    gdk_draw_rectangle (drawable,
                        GTK_WIDGET (widget)->style->dark_gc[GTK_STATE_SELECTED],
                        TRUE,
                        x, y, width, height);
  else if (workspace_background)
    {
      gdk_pixbuf_render_to_drawable (workspace_background,
                                     drawable,
                                     GTK_WIDGET (widget)->style->dark_gc[GTK_STATE_SELECTED],
                                     0, 0,
                                     x, y,
                                     -1, -1,
                                     GDK_RGB_DITHER_MAX,
                                     0, 0);
    }
  else
    gdk_draw_rectangle (drawable,
                        GTK_WIDGET (widget)->style->dark_gc[GTK_STATE_NORMAL],
                        TRUE,
                        x, y, width, height);


  i = 0;
  while (i < n_windows)
    {
      const WnckWindowDisplayInfo *win = &windows[i];
      GdkRectangle winrect;
      
      get_window_rect (win, screen_width, screen_height, &workspace_rect, &winrect);
      
      draw_window (widget,
                   drawable,
                   win,
                   &winrect);
      
      ++i;
    }
}
