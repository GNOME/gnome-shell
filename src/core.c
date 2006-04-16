/* Metacity interface used by GTK+ UI to talk to core */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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
#include "core.h"
#include "frame.h"
#include "workspace.h"
#include "prefs.h"

void
meta_core_get_client_size (Display *xdisplay,
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

gboolean
meta_core_titlebar_is_onscreen (Display *xdisplay,
                                Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  return meta_window_titlebar_is_onscreen (window);  
}


Window
meta_core_get_client_xwindow (Display *xdisplay,
                              Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  return window->xwindow;
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

MetaFrameType
meta_core_get_frame_type (Display *xdisplay,
                          Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  MetaFrameType base_type;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);

  base_type = META_FRAME_TYPE_LAST;
  
  switch (window->type)
    {
    case META_WINDOW_NORMAL:
      base_type = META_FRAME_TYPE_NORMAL;
      break;
      
    case META_WINDOW_DIALOG:
      base_type = META_FRAME_TYPE_DIALOG;
      break;
      
    case META_WINDOW_MODAL_DIALOG:
      base_type = META_FRAME_TYPE_MODAL_DIALOG;
      break;
      
    case META_WINDOW_MENU:
      base_type = META_FRAME_TYPE_MENU;
      break;

    case META_WINDOW_UTILITY:
      base_type = META_FRAME_TYPE_UTILITY;
      break;
      
    case META_WINDOW_DESKTOP:
    case META_WINDOW_DOCK:
    case META_WINDOW_TOOLBAR:
    case META_WINDOW_SPLASHSCREEN:
      /* No frame */
      base_type = META_FRAME_TYPE_LAST;
      break;
    }

  if (base_type == META_FRAME_TYPE_LAST)
    return META_FRAME_TYPE_LAST; /* can't add border if undecorated */
  else if (window->border_only)
    return META_FRAME_TYPE_BORDER; /* override base frame type */
  else
    return base_type;
}

GdkPixbuf*
meta_core_get_mini_icon (Display *xdisplay,
                         Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);
  
  return window->mini_icon;
}

GdkPixbuf*
meta_core_get_icon (Display *xdisplay,
                    Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);
  
  return window->icon;
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
meta_core_user_lower_and_unfocus (Display *xdisplay,
                                  Window   frame_xwindow,
                                  Time     timestamp)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);
  
  meta_window_lower (window);

  if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK &&
      meta_prefs_get_raise_on_click ())
    {
      /* Move window to the back of the focusing workspace's MRU list.
       * Do extra sanity checks to avoid possible race conditions.
       * (Borrowed from window.c.)
       */
      if (window->screen->active_workspace &&
          meta_window_located_on_workspace (window, 
                                            window->screen->active_workspace))
        {
          GList* link;
          link = g_list_find (window->screen->active_workspace->mru_list, 
                              window);
          g_assert (link);

          window->screen->active_workspace->mru_list = 
            g_list_remove_link (window->screen->active_workspace->mru_list,
                                link);
          g_list_free (link);

          window->screen->active_workspace->mru_list = 
            g_list_append (window->screen->active_workspace->mru_list, 
                           window);
        }
    }

  /* focus the default window, if needed */
  if (window->has_focus)
    meta_workspace_focus_default_window (window->screen->active_workspace,
                                         NULL,
                                         timestamp);
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

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  meta_window_maximize (window, 
                        META_MAXIMIZE_HORIZONTAL | META_MAXIMIZE_VERTICAL);
}

void
meta_core_toggle_maximize (Display *xdisplay,
                           Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  if (META_WINDOW_MAXIMIZED (window))
    meta_window_unmaximize (window, 
                            META_MAXIMIZE_HORIZONTAL | META_MAXIMIZE_VERTICAL);
  else
    meta_window_maximize (window,
                          META_MAXIMIZE_HORIZONTAL | META_MAXIMIZE_VERTICAL);
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

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  meta_window_unmaximize (window,
                          META_MAXIMIZE_HORIZONTAL | META_MAXIMIZE_VERTICAL);
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
                                meta_screen_get_workspace_by_index (window->screen,
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

  return meta_workspace_index (screen->active_workspace);
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
meta_core_get_frame_extents   (Display        *xdisplay,
                               Window          frame_xwindow,
                               int            *x,
                               int            *y,
                               int            *width,
                               int            *height)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  if (x)
    *x = window->frame->rect.x;
  if (y)
    *y = window->frame->rect.y;
  if (width)
    *width = window->frame->rect.width;
  if (height)
    *height = window->frame->rect.height;
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

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);
  meta_window_focus (window, timestamp);

  meta_window_show_menu (window, root_x, root_y, button, timestamp);
}

void
meta_core_get_menu_accelerator (MetaMenuOp           menu_op,
                                int                  workspace,
                                unsigned int        *keysym,
                                MetaVirtualModifier *modifiers)
{
  const char *name;

  name = NULL;
  
  switch (menu_op)
    {
    case META_MENU_OP_DELETE:
      name = META_KEYBINDING_CLOSE;
      break;
    case META_MENU_OP_MINIMIZE:
      name = META_KEYBINDING_MINIMIZE;
      break;
    case META_MENU_OP_UNMAXIMIZE:
      name = META_KEYBINDING_UNMAXIMIZE;
      break;
    case META_MENU_OP_MAXIMIZE:
      name = META_KEYBINDING_MAXIMIZE;
      break;
    case META_MENU_OP_UNSHADE:
    case META_MENU_OP_SHADE:
      name = META_KEYBINDING_TOGGLE_SHADE;
      break;
    case META_MENU_OP_UNSTICK:
    case META_MENU_OP_STICK:
      name = META_KEYBINDING_TOGGLE_STICKY;
      break;
    case META_MENU_OP_ABOVE:
    case META_MENU_OP_UNABOVE:
      name = META_KEYBINDING_TOGGLE_ABOVE;
      break;
    case META_MENU_OP_WORKSPACES:
      switch (workspace)
        {
        case 1:
          name = META_KEYBINDING_MOVE_WORKSPACE_1;
          break;
        case 2:
          name = META_KEYBINDING_MOVE_WORKSPACE_2;
          break;
        case 3:
          name = META_KEYBINDING_MOVE_WORKSPACE_3;
          break; 
        case 4:
          name = META_KEYBINDING_MOVE_WORKSPACE_4;
          break; 
        case 5:
          name = META_KEYBINDING_MOVE_WORKSPACE_5;
          break; 
        case 6:
          name = META_KEYBINDING_MOVE_WORKSPACE_6;
          break; 
        case 7:
          name = META_KEYBINDING_MOVE_WORKSPACE_7;
          break; 
        case 8:
          name = META_KEYBINDING_MOVE_WORKSPACE_8;
          break; 
        case 9:
          name = META_KEYBINDING_MOVE_WORKSPACE_9;
          break; 
        case 10:
          name = META_KEYBINDING_MOVE_WORKSPACE_10;
          break;
        case 11:
          name = META_KEYBINDING_MOVE_WORKSPACE_11;
          break;
        case 12:
          name = META_KEYBINDING_MOVE_WORKSPACE_12;
          break;
        }
      break;
    case META_MENU_OP_MOVE:
      name = META_KEYBINDING_BEGIN_MOVE;
      break;
    case META_MENU_OP_RESIZE:
      name = META_KEYBINDING_BEGIN_RESIZE;
      break;
    case META_MENU_OP_MOVE_LEFT:
      name = META_KEYBINDING_MOVE_WORKSPACE_LEFT;
      break;
    case META_MENU_OP_MOVE_RIGHT:
      name = META_KEYBINDING_MOVE_WORKSPACE_RIGHT;
      break;
    case META_MENU_OP_MOVE_UP:
      name = META_KEYBINDING_MOVE_WORKSPACE_UP;
      break;
    case META_MENU_OP_MOVE_DOWN:
      name = META_KEYBINDING_MOVE_WORKSPACE_DOWN;
      break;
    case META_MENU_OP_RECOVER:
      /* No keybinding for this one */
      break;
    }

  if (name)
    {
      meta_prefs_get_window_binding (name, keysym, modifiers);
    }
  else
    {
      *keysym = 0;
      *modifiers = 0;
    }
}

const char*
meta_core_get_workspace_name_with_index (Display *xdisplay,
                                         Window   xroot,
                                         int      index)
{
  MetaDisplay *display;
  MetaScreen *screen;
  MetaWorkspace *workspace;

  display = meta_display_for_x_display (xdisplay);
  screen = meta_display_screen_for_root (display, xroot);
  g_assert (screen != NULL);
  workspace = meta_screen_get_workspace_by_index (screen, index);
  return workspace ? meta_workspace_get_name (workspace) : NULL;
}

gboolean
meta_core_begin_grab_op (Display    *xdisplay,
                         Window      frame_xwindow,
                         MetaGrabOp  op,
                         gboolean    pointer_already_grabbed,
                         int         event_serial,
                         int         button,
                         gulong      modmask,
                         Time        timestamp,
                         int         root_x,
                         int         root_y)
{
  MetaDisplay *display;
  MetaWindow *window;
  MetaScreen *screen;
  
  display = meta_display_for_x_display (xdisplay);
  screen = meta_display_screen_for_xwindow (display, frame_xwindow);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  g_assert (screen != NULL);
  
  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);  

  return meta_display_begin_grab_op (display, screen, window,
                                     op, pointer_already_grabbed,
                                     event_serial,
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

  g_assert (display != NULL);
  g_assert (display->grab_op == META_GRAB_OP_NONE || 
            display->grab_screen != NULL);
  g_assert (display->grab_op == META_GRAB_OP_NONE ||
            display->grab_screen->display->xdisplay == xdisplay);
  
  if (display->grab_op != META_GRAB_OP_NONE &&
      display->grab_window &&
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

  meta_verbose ("Grabbing buttons on frame 0x%lx\n", frame_xwindow);
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

  meta_frame_set_screen_cursor (window->frame, cursor);
}

void
meta_core_get_screen_size (Display *xdisplay,
                           Window   frame_on_screen,
                           int     *width,
                           int     *height)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_on_screen);

  if (window == NULL || window->frame == NULL)
    meta_bug ("No such frame window 0x%lx!\n", frame_on_screen);  

  if (width)
    *width = window->screen->rect.width;
  if (height)
    *height = window->screen->rect.height;
}

void
meta_core_increment_event_serial (Display *xdisplay)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (xdisplay);

  meta_display_increment_event_serial (display);
}

