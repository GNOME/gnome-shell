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
#include "workspace.h"

void
meta_core_get_outer_rect (Display      *xdisplay,
                          Window        frame_xwindow,
                          GdkRectangle *rect)
{
  MetaDisplay *display;
  MetaWindow *window;
  MetaRectangle r;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  
}

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

  meta_window_move (window, TRUE, x, y);
}

void
meta_core_user_resize  (Display *xdisplay,
                        Window   frame_xwindow,
                        int      gravity,
                        int      width,
                        int      height)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  meta_window_resize_with_gravity (window, TRUE, width, height, gravity);
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
meta_core_user_focus (Display *xdisplay,
                      Window   frame_xwindow,
                      Time     timestamp)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);
  
  meta_window_focus (window, timestamp);
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

void
meta_core_unshade (Display *xdisplay,
                   Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_unshade (window);
}

void
meta_core_shade (Display *xdisplay,
                 Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_shade (window);
}

void
meta_core_unstick (Display *xdisplay,
                   Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_unstick (window);
}

void
meta_core_stick (Display *xdisplay,
                 Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_stick (window);
}

void
meta_core_change_workspace (Display *xdisplay,
                            Window   frame_xwindow,
                            int      new_workspace)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_change_workspace (window,
                                meta_display_get_workspace_by_screen_index (display,
                                                                            window->screen,
                                                                            new_workspace));
}

int
meta_core_get_num_workspaces (Screen  *xscreen)
{
  MetaScreen *screen;

  screen = meta_screen_for_x_screen (xscreen);

  return meta_screen_get_n_workspaces (screen);
}

int
meta_core_get_active_workspace (Screen *xscreen)
{
  MetaScreen *screen;

  screen = meta_screen_for_x_screen (xscreen);

  return meta_workspace_screen_index (screen->active_workspace);
}

int
meta_core_get_frame_workspace (Display *xdisplay,
                               Window frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  return meta_window_get_net_wm_desktop (window);
}

void
meta_core_show_window_menu (Display *xdisplay,
                            Window   frame_xwindow,
                            int      root_x,
                            int      root_y,
                            int      button,
                            Time     timestamp)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  meta_window_show_menu (window, root_x, root_y, button, timestamp);
}

gboolean
meta_core_begin_grab_op (Display    *xdisplay,
                         Window      frame_xwindow,
                         MetaGrabOp  op,
                         gboolean    pointer_already_grabbed,
                         int         button,
                         gulong      modmask,
                         Time        timestamp,
                         int         root_x,
                         int         root_y)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  return meta_display_begin_grab_op (display, window,
                                     op, pointer_already_grabbed,
                                     button, modmask,
                                     timestamp, root_x, root_y);
}

void
meta_core_end_grab_op (Display *xdisplay,
                       Time     timestamp)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (xdisplay);

  meta_display_end_grab_op (display, timestamp);
}

MetaGrabOp
meta_core_get_grab_op (Display *xdisplay)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (xdisplay);

  return display->grab_op;
}

Window
meta_core_get_grab_frame (Display *xdisplay)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (xdisplay);

  if (display->grab_op != META_GRAB_OP_NONE &&
      display->grab_window->frame)
    return display->grab_window->frame->xwindow;
  else
    return None;
}

int
meta_core_get_grab_button (Display  *xdisplay)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (xdisplay);

  if (display->grab_op == META_GRAB_OP_NONE)
    return -1;
  
  return display->grab_button;
}

void
meta_core_grab_buttons  (Display *xdisplay,
                         Window   frame_xwindow)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (xdisplay);
  
  meta_display_grab_window_buttons (display, frame_xwindow);
}

void
meta_core_set_screen_cursor (Display *xdisplay,
                             Window   frame_on_screen,
                             MetaCursor cursor)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_on_screen);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_on_screen);  

  meta_screen_set_cursor (window->screen, cursor);
}
