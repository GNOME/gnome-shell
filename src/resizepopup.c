/* Metacity resizing-terminal-window feedback */

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
#include "resizepopup.h"
#include "util.h"
#include <gtk/gtkwindow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkmain.h>

struct _MetaResizePopup
{
  GtkWidget *size_window;
  GtkWidget *size_label;
  
  GSList *vertical_tick_windows;
  GSList *horizontal_tick_windows;

  GtkWidget *vertical_size_window;
  GtkWidget *horizontal_size_window;

  int vertical_size;
  int horizontal_size;

  gboolean need_vertical_feedback;
  gboolean need_horizontal_feedback;
  
  gboolean showing;
  
  int resize_gravity;
  int x;
  int y;
  int width;
  int height;
  int width_inc;
  int height_inc;
  int min_width;
  int min_height;
  int frame_left;
  int frame_right;
  int frame_top;
  int frame_bottom;
  int tick_origin_x;
  int tick_origin_y;
};

MetaResizePopup*
meta_ui_resize_popup_new (void)
{
  MetaResizePopup *popup;

  popup = g_new0 (MetaResizePopup, 1);

  popup->resize_gravity = -1;
  
  return popup;
}

static void
clear_tick_labels (MetaResizePopup *popup)
{
  if (popup->vertical_size_window)
    {
      gtk_widget_destroy (popup->vertical_size_window);
      popup->vertical_size_window = NULL;
    }

  if (popup->horizontal_size_window)
    {
      gtk_widget_destroy (popup->horizontal_size_window);
      popup->horizontal_size_window = NULL;
    }
}
  
static void
clear_tick_windows (MetaResizePopup *popup)
{  
  g_slist_foreach (popup->vertical_tick_windows,
                   (GFunc) gtk_widget_destroy,
                   NULL);

  g_slist_free (popup->vertical_tick_windows);
  popup->vertical_tick_windows = NULL;
  
  g_slist_foreach (popup->horizontal_tick_windows,
                   (GFunc) gtk_widget_destroy,
                   NULL);
  
  g_slist_free (popup->horizontal_tick_windows);
  popup->horizontal_tick_windows = NULL;
}

void
meta_ui_resize_popup_free (MetaResizePopup *popup)
{
  g_return_if_fail (popup != NULL);
  
  if (popup->size_window)
    gtk_widget_destroy (popup->size_window);

  clear_tick_windows (popup);
  clear_tick_labels (popup);
  
  g_free (popup);
}

static void
ensure_size_window (MetaResizePopup *popup)
{
  GtkWidget *frame;
  
  if (popup->size_window)
    return;

  if (!(popup->need_vertical_feedback || popup->need_horizontal_feedback))
    return;
  
  popup->size_window = gtk_window_new (GTK_WINDOW_POPUP);

  /* never shrink the size window */
  gtk_window_set_resizable (GTK_WINDOW (popup->size_window),
                            TRUE);
  
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

  gtk_container_add (GTK_CONTAINER (popup->size_window), frame);

  popup->size_label = gtk_label_new ("");
  gtk_misc_set_padding (GTK_MISC (popup->size_label), 3, 3);

  gtk_container_add (GTK_CONTAINER (frame), popup->size_label);

  gtk_widget_show_all (frame);
}

static void
update_size_window (MetaResizePopup *popup)
{
  char *str;
  int x, y;
  int width, height;

  if (!(popup->need_vertical_feedback || popup->need_horizontal_feedback))
    return;
  
  g_return_if_fail (popup->size_window != NULL);
  
  str = g_strdup_printf (_("%d x %d"),
                         popup->horizontal_size,
                         popup->vertical_size);

  gtk_label_set_text (GTK_LABEL (popup->size_label), str);

  g_free (str);

  gtk_window_get_size (GTK_WINDOW (popup->size_window), &width, &height);

  x = popup->x + (popup->width - width) / 2;
  y = popup->y + (popup->height - height) / 2;
  
  if (GTK_WIDGET_REALIZED (popup->size_window))
    {
      /* using move_resize to avoid jumpiness */
      gdk_window_move_resize (popup->size_window->window,
                              x, y,
                              width, height);
    }
  else
    {
      gtk_window_move   (GTK_WINDOW (popup->size_window),
                         x, y);
    }
}

static void
sync_showing (MetaResizePopup *popup)
{
  if (popup->showing)
    {
      if (popup->size_window)
        gtk_widget_show (popup->size_window);

      if (popup->vertical_size_window)
        gtk_widget_show (popup->vertical_size_window);

      if (popup->horizontal_size_window)
        gtk_widget_show (popup->horizontal_size_window);
      
      g_slist_foreach (popup->horizontal_tick_windows,
                       (GFunc) gtk_widget_show,
                       NULL);
      g_slist_foreach (popup->vertical_tick_windows,
                       (GFunc) gtk_widget_show,
                       NULL);
      
      if (popup->size_window && GTK_WIDGET_REALIZED (popup->size_window))
        gdk_window_raise (popup->size_window->window);
    }
  else
    {
      if (popup->size_window)
        gtk_widget_hide (popup->size_window);

      if (popup->vertical_size_window)
        gtk_widget_hide (popup->vertical_size_window);

      if (popup->horizontal_size_window)
        gtk_widget_hide (popup->horizontal_size_window);
      
      g_slist_foreach (popup->horizontal_tick_windows,
                       (GFunc) gtk_widget_hide,
                       NULL);
      g_slist_foreach (popup->vertical_tick_windows,
                       (GFunc) gtk_widget_hide,
                       NULL);
    }
}

static GtkWidget*
create_size_window (const char *str)
{
  PangoContext *context;
  PangoLayout *layout;
  GdkGC *gc;
  GdkBitmap *bitmap;
  PangoRectangle rect;
  GdkColor color;
  GtkWidget *window;
  int w, h;
  
  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_app_paintable (window, TRUE);
  gtk_window_set_resizable (GTK_WINDOW (window),
                            FALSE);
      
  context = gdk_pango_context_get ();

#if 0
  /* bitmaps have no meaningful cmap */
  gdk_pango_context_set_colormap (context,
                                  gtk_widget_get_colormap (widget));
#endif
  pango_context_set_base_dir (context,
                              gtk_widget_get_direction (window) == GTK_TEXT_DIR_LTR ?
                              PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL);
  pango_context_set_font_description (context,
                                      window->style->font_desc);
  pango_context_set_language (context, gtk_get_default_language ());

  layout = pango_layout_new (context);
  pango_layout_set_text (layout, str, -1);
      
  g_object_unref (G_OBJECT (context));

  pango_layout_get_pixel_extents (layout, NULL, &rect);

  w = rect.width;
  h = rect.height;
  gtk_widget_set_size_request (window, w, h);
      
  bitmap = gdk_pixmap_new (NULL, w, h, 1);
  gc = gdk_gc_new (bitmap);
  color.pixel = 0;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_rectangle (bitmap, gc, TRUE, 0, 0, w, h);
  color.pixel = 1;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_layout (bitmap, gc, 0, 0, layout);

  gtk_widget_shape_combine_mask (window,
                                 bitmap, 0, 0);

  g_object_unref (G_OBJECT (bitmap));
  g_object_unref (G_OBJECT (gc));
  g_object_unref (G_OBJECT (layout));

  /* After setting the size */
  gtk_widget_realize (window);
  gdk_window_set_background (window->window,
                             &window->style->black);

  return window;
}

static void
place_vertical_size_window (MetaResizePopup *popup,
                            int              x,
                            int              y,
                            double           align)
{
  int w, h;
  
  if (popup->vertical_size_window == NULL)
    {
      char *str;

      str = g_strdup_printf ("%d", popup->vertical_size);
      
      popup->vertical_size_window = create_size_window (str);

      g_free (str);
    }
  
  gtk_window_get_size (GTK_WINDOW (popup->vertical_size_window),
                       &w, &h);
  
  gtk_window_move (GTK_WINDOW (popup->vertical_size_window),
                   x - w * align,
                   y - h / 2);        
}

static void
place_horizontal_size_window (MetaResizePopup *popup,
                              int              x,
                              int              y,
                              double           align)
{
  int w, h;
  
  if (popup->horizontal_size_window == NULL)
    {
      char *str;

      str = g_strdup_printf ("%d", popup->horizontal_size);
      
      popup->horizontal_size_window = create_size_window (str);

      g_free (str);
    }
  
  gtk_window_get_size (GTK_WINDOW (popup->horizontal_size_window),
                       &w, &h);
  
  gtk_window_move (GTK_WINDOW (popup->horizontal_size_window),
                   x - w / 2,
                   y - h * align);
}

static gboolean
tick_window_expose (GtkWidget      *widget,
                    GdkEventExpose *event,
                    gpointer        data)
{
  int w, h;
  
  gdk_window_get_size (widget->window, &w, &h);

#if 0
  gdk_draw_rectangle (widget->window,
                      widget->style->white_gc,
                      FALSE,
                      0, 0,
                      w - 1,
                      h - 1);
#endif
  
  return FALSE;
}

static GtkWidget*
create_tick (int w,
             int h)
{
  GtkWidget *window;
  
  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_app_paintable (window, TRUE);

  gtk_widget_set_size_request (window, w, h);
  
  gtk_widget_realize (window);
  gdk_window_set_background (window->window,
                             &window->style->black);

  g_signal_connect (G_OBJECT (window), "expose_event",
                    G_CALLBACK (tick_window_expose), NULL);

  return window;
}

#define TICK_WIDTH 1
#define TICK_LENGTH 7

static void
add_vertical_tick (MetaResizePopup *popup,
                   int              x,
                   int              y)
{
  GtkWidget *window;

  /* Create a tick for the vertical resize column of
   * tick marks.
   */
  
  window = create_tick (TICK_LENGTH, TICK_WIDTH);
  
  gtk_window_move (GTK_WINDOW (window),
                   x, y);

  popup->vertical_tick_windows =
    g_slist_prepend (popup->vertical_tick_windows,
                     window);

  /* create GdkWindow */
  gtk_widget_realize (window);

  /* Be sure the size window is above it */
  if (popup->size_window && GTK_WIDGET_REALIZED (popup->size_window))
    gdk_window_raise (popup->size_window->window);
}

static void
add_horizontal_tick (MetaResizePopup *popup,
                     int              x,
                     int              y)
{
  GtkWidget *window;

  /* Create a tick for the vertical resize column of
   * tick marks.
   */
  
  window = create_tick (TICK_WIDTH, TICK_LENGTH);
  
  gtk_window_move (GTK_WINDOW (window),
                   x, y);

  popup->horizontal_tick_windows =
    g_slist_prepend (popup->horizontal_tick_windows,
                     window);

  /* create GdkWindow */
  gtk_widget_realize (window);

  /* Be sure the size window is above it */
  if (popup->size_window && GTK_WIDGET_REALIZED (popup->size_window))
    gdk_window_raise (popup->size_window->window);
}

static void
ensure_tick_windows (MetaResizePopup *popup)
{
  int x, y;
  int max_x, max_y;
  
  if (popup->resize_gravity < 0)
    return;
  
  if (popup->horizontal_tick_windows != NULL ||
      popup->vertical_tick_windows != NULL)
    return;    

  /* FIXME the current implementation sucks too much to enable. */
  return;
  
  max_x = gdk_screen_width ();
  max_y = gdk_screen_height ();
  
  if (popup->need_vertical_feedback)
    {      
      y = popup->tick_origin_y;

      switch (popup->resize_gravity)
        {
        case NorthEastGravity:
        case EastGravity:
        case SouthEastGravity:
          /* Vertical tick column on the fixed East side */
          x = popup->x + popup->width + popup->frame_right;
          break;

        case NorthWestGravity:
        case WestGravity:
        case SouthWestGravity:
          /* Vertical ticks on the fixed West side */
          x = popup->x - TICK_LENGTH - popup->frame_left;
          break;
      
        case NorthGravity:
        case SouthGravity:
        case CenterGravity:
          /* Center the vertical ticks */
          x = popup->x + (popup->width - TICK_LENGTH) / 2;
          break;
      
        default:
          /* gcc warnings */
          x = 0;
          break;
        }

      switch (popup->resize_gravity)
        {
        case SouthGravity:
        case SouthEastGravity:
        case SouthWestGravity:
          while (y > 0)
            {
              add_vertical_tick (popup, x, y);
              y -= popup->height_inc;
            }
          break;

        case NorthGravity:
        case NorthEastGravity:
        case NorthWestGravity:
          while (y < max_y)
            {
              add_vertical_tick (popup, x, y);
              y += popup->height_inc;
            }
          break;
        }
    }

  if (popup->need_horizontal_feedback)
    {      
      x = popup->tick_origin_x;
      
      switch (popup->resize_gravity)
        {
        case SouthWestGravity:
        case SouthGravity:
        case SouthEastGravity:
          /* Horizontal tick column on the fixed South side */
          y = popup->y + popup->height + popup->frame_bottom;
          break;

        case NorthWestGravity:
        case NorthGravity:
        case NorthEastGravity:
          /* Horizontal ticks on the fixed North side */
          y = popup->y - TICK_LENGTH - popup->frame_top;
          break;
      
        case EastGravity:
        case WestGravity:
        case CenterGravity:
          /* Center the horizontal ticks */
          y = popup->y + (popup->height - TICK_LENGTH) / 2;
          break;
      
        default:
          /* gcc warnings */
          y = 0;
          break;
        }

      switch (popup->resize_gravity)
        {
        case EastGravity:
        case SouthEastGravity:
        case NorthEastGravity:
          while (x > 0)
            {
              add_horizontal_tick (popup, x, y);
              x -= popup->width_inc;
            }
          break;

        case WestGravity:
        case SouthWestGravity:
        case NorthWestGravity:
          while (x < max_x)
            {
              add_horizontal_tick (popup, x, y);
              x += popup->width_inc;
            }
          break;
        }
    }
}

static void
update_tick_labels (MetaResizePopup *popup)
{
  int x, y;
  int left_edge, right_edge, top_edge, bottom_edge;
  
  if (popup->resize_gravity < 0)
    return;

  left_edge = popup->x - popup->frame_left;
  right_edge = popup->x + popup->width + popup->frame_right;
  top_edge = popup->y - popup->frame_top;
  bottom_edge = popup->y + popup->height + popup->frame_bottom;
  
  if (popup->need_vertical_feedback)
    {
      int size_x, size_y;
      double size_align;

      switch (popup->resize_gravity)
        {
        case NorthEastGravity:
        case EastGravity:
        case SouthEastGravity:
          x = popup->x + popup->width + popup->frame_right;
          size_x = x + TICK_LENGTH;
          size_align = 0.0;
          break;

        case NorthWestGravity:
        case WestGravity:
        case SouthWestGravity:
          x = popup->x - TICK_LENGTH - popup->frame_left;
          size_x = x - TICK_LENGTH;
          size_align = 1.0;
          break;
      
        case NorthGravity:
        case SouthGravity:
        case CenterGravity:
          x = popup->x + (popup->width - TICK_LENGTH) / 2;
          size_x = x - TICK_LENGTH / 2 - 1;
          size_align = 1.0;
          break;
      
        default:
          /* gcc warnings */
          x = 0;
          size_x = 0;
          size_align = 0.5;
          break;
        }

      switch (popup->resize_gravity)
        {
        case SouthGravity:
        case SouthEastGravity:
        case SouthWestGravity:
          size_y = top_edge;
          break;

        case NorthGravity:
        case NorthEastGravity:
        case NorthWestGravity:
          size_y = bottom_edge;
          break;

        default:
          size_y = 0;
          break;
        }

      place_vertical_size_window (popup, size_x, size_y, size_align);
    }

  if (popup->need_horizontal_feedback)
    {
      int size_x, size_y;
      double size_align;

      switch (popup->resize_gravity)
        {
        case SouthWestGravity:
        case SouthGravity:
        case SouthEastGravity:
          y = popup->y + popup->height + popup->frame_bottom;
          size_y = y + TICK_LENGTH;
          size_align = 0.0;
          break;

        case NorthWestGravity:
        case NorthGravity:
        case NorthEastGravity:
          y = popup->y - TICK_LENGTH - popup->frame_top;
          size_y = y - TICK_LENGTH;
          size_align = 1.0;
          break;
      
        case EastGravity:
        case WestGravity:
        case CenterGravity:
          y = popup->y + (popup->height - TICK_LENGTH) / 2;
          size_y = y - TICK_LENGTH / 2 - 1;
          size_align = 1.0;
          break;
      
        default:
          /* gcc warnings */
          y = 0;
          size_y = 0;
          size_align = 0.5;
          break;
        }

      switch (popup->resize_gravity)
        {
        case WestGravity:
        case NorthWestGravity:
        case SouthWestGravity:
          size_x = right_edge;
          break;

        case EastGravity:
        case NorthEastGravity:
        case SouthEastGravity:
          size_x = left_edge;
          break;

        default:
          size_x = 0;
          break;
        }

      place_horizontal_size_window (popup, size_x, size_y, size_align);
    }
}

static void
get_tick_origin (int  resize_gravity,
                 int  x,
                 int  y,
                 int  width,
                 int  height,
                 int  min_width,
                 int  min_height,
                 int  frame_left,
                 int  frame_right,
                 int  frame_top,
                 int  frame_bottom,
                 int *origin_x,
                 int *origin_y)
{
  *origin_x = 0;
  *origin_y = 0;
  
  switch (resize_gravity)
    {
      /* If client is staying fixed on the east during resize, then we
       * have to move the west edge. Which means ticks originate
       * on the east.
       */
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      *origin_x = x + width - min_width - frame_right + TICK_WIDTH / 2;
      break;

    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      *origin_x = x + min_width + frame_left - TICK_WIDTH / 2 - 1;
      break;
      
      /* centered horizontally */
    case NorthGravity:
    case SouthGravity:
    case CenterGravity:
      /* Not going to draw horizontal ticks */
      *origin_x = 0;
      break;
      
    default:
      break;
    }

  switch (resize_gravity)
    {
      /* If client is staying fixed on the south during resize,
       * we have to move the north edge, so ticks originate on the
       * south.
       */
    case SouthGravity:
    case SouthEastGravity:
    case SouthWestGravity:
      *origin_y = y + height - frame_top - min_height + TICK_WIDTH / 2;
      break;

      /* staying fixed on the north */
    case NorthGravity:
    case NorthEastGravity:
    case NorthWestGravity:
      *origin_y = y + min_height + frame_bottom - TICK_WIDTH / 2 - 1;
      break;
      
      /* centered vertically */
    case EastGravity:
    case WestGravity:
    case CenterGravity:
      *origin_y = 0;
      break;
      
    default:
      break;
    }
}

void
meta_ui_resize_popup_set (MetaResizePopup *popup,
                          int              resize_gravity,
                          int              x,
                          int              y,
                          int              width,
                          int              height,
                          int              base_width,
                          int              base_height,
                          int              min_width,
                          int              min_height,
                          int              width_inc,
                          int              height_inc,
                          int              frame_left,
                          int              frame_right,
                          int              frame_top,
                          int              frame_bottom)
{
  gboolean need_update_size;
  gboolean need_update_ticks;
  gboolean need_update_tick_labels;
  int tick_x, tick_y;
  int display_w, display_h;
  gboolean need_vertical, need_horizontal;
  
  g_return_if_fail (popup != NULL);

  need_update_size = FALSE;
  need_update_ticks = FALSE;
  need_update_tick_labels = FALSE;

  switch (popup->resize_gravity)
    {
    case SouthGravity:
    case SouthEastGravity:
    case SouthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
    case NorthWestGravity:
      need_vertical = TRUE;
      break;
    default:
      need_vertical = FALSE;
      break;
    }

  if (height_inc <= (TICK_WIDTH + 1))
    need_vertical = FALSE;
  
  switch (popup->resize_gravity)
    {
    case EastGravity:
    case SouthEastGravity:
    case NorthEastGravity:
    case WestGravity:
    case SouthWestGravity:
    case NorthWestGravity:
      need_horizontal = TRUE;
      break;
    default:
      need_horizontal = FALSE;
      break;
    }

  if (popup->width_inc <= (TICK_WIDTH + 1))
    need_horizontal = FALSE;
  
  display_w = width - base_width;
  if (width_inc > 0)
    display_w /= width_inc;

  display_h = height - base_height;
  if (height_inc > 0)
    display_h /= height_inc;
  
  if (popup->x != x ||
      popup->y != y ||
      popup->width != width ||
      popup->height != height ||
      display_w != popup->horizontal_size ||
      display_h != popup->vertical_size)
    need_update_size = TRUE;

  get_tick_origin (resize_gravity, x, y, width, height,
                   min_width, min_height,
                   frame_left, frame_right,
                   frame_top, frame_bottom,
                   &tick_x, &tick_y);
  
  if (popup->tick_origin_x != tick_x ||
      popup->tick_origin_y != tick_y ||
      popup->frame_left != frame_left ||
      popup->frame_right != frame_right ||
      popup->frame_top != frame_top ||
      popup->frame_bottom != frame_bottom)
    need_update_ticks = TRUE;
  
  if (need_update_ticks ||
      display_w != popup->horizontal_size ||
      display_h != popup->vertical_size)
    need_update_tick_labels = TRUE;

  if (need_horizontal != popup->need_horizontal_feedback ||
      need_vertical != popup->need_vertical_feedback)
    {
      need_update_size = TRUE;
      need_update_ticks = TRUE;
      need_update_tick_labels = TRUE;
    }
  
  popup->resize_gravity = resize_gravity;
  popup->x = x;
  popup->y = y;
  popup->width = width;
  popup->height = height;
  popup->min_width = min_width;
  popup->min_height = min_height;
  popup->width_inc = width_inc;
  popup->height_inc = height_inc;
  popup->tick_origin_x = tick_x;
  popup->tick_origin_y = tick_y;
  popup->frame_left = frame_left;
  popup->frame_right = frame_right;
  popup->frame_top = frame_top;
  popup->frame_bottom = frame_bottom;
  popup->vertical_size = display_h;
  popup->horizontal_size = display_w;
  popup->need_vertical_feedback = need_vertical;
  popup->need_horizontal_feedback = need_horizontal;
  
  if (need_update_tick_labels)
    {
      clear_tick_labels (popup);
      update_tick_labels (popup);
    }
  
  if (need_update_ticks)
    {
      clear_tick_windows (popup);
      ensure_tick_windows (popup);
    }
  
  if (need_update_size)
    {
      ensure_size_window (popup);
      update_size_window (popup);
    }
      
  sync_showing (popup);
}

void
meta_ui_resize_popup_set_showing  (MetaResizePopup *popup,
                                   gboolean         showing)
{
  g_return_if_fail (popup != NULL);
  
  if (showing == popup->showing)
    return;

  popup->showing = !!showing;

  if (popup->showing)
    {
      ensure_size_window (popup);
      ensure_tick_windows (popup);
      update_size_window (popup);
    }
  
  sync_showing (popup);
}
