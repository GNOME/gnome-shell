/* Metacity interface used by GTK+ UI to talk to core */

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

#include "core.h"
#include "frame.h"

void
meta_core_get_frame_size (Display *xdisplay,
                          Window   frame_xwindow,
                          int     *width,
                          int     *height)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);
  
  if (width)
    *width = window->frame->rect.width;
  if (height)
    *height = window->frame->rect.height;
}

MetaFrameFlags
meta_core_get_frame_flags (Display *xdisplay,
                           Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  return meta_frame_get_flags (window->frame);
}

void
meta_core_queue_frame_resize (Display *xdisplay,
                              Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_queue_move_resize (window);
}

void
meta_core_user_move (Display *xdisplay,
                     Window   frame_xwindow,
                     int      x,
                     int      y)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  window->user_has_moved = TRUE;
  meta_window_move (window, x, y);
}

void
meta_core_user_resize  (Display *xdisplay,
                        Window   frame_xwindow,
                        int      width,
                        int      height)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  window->user_has_resized = TRUE;
  meta_window_resize (window, width, height);
}

void
meta_core_user_raise (Display *xdisplay,
                      Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);
  
  meta_window_raise (window);
}

void
meta_core_get_position (Display *xdisplay,
                        Window   frame_xwindow,
                        int     *x,
                        int     *y)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_get_position (window, x, y);
}

void
meta_core_get_size (Display *xdisplay,
                    Window   frame_xwindow,
                    int     *width,
                    int     *height)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  if (width)
    *width = window->rect.width;
  if (height)
    *height = window->rect.height;
}


void
meta_core_minimize (Display *xdisplay,
                    Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_minimize (window);
}

void
meta_core_maximize (Display *xdisplay,
                    Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_maximize (window);
}

void
meta_core_unmaximize (Display *xdisplay,
                      Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_unmaximize (window);
}

void
meta_core_delete (Display *xdisplay,
                  Window   frame_xwindow,
                  guint32  timestamp)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);
  
  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  
     
  meta_window_delete (window, timestamp);
}

