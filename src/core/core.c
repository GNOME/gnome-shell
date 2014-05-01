/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter interface used by GTK+ UI to talk to core */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "core.h"
#include "frame.h"
#include "workspace-private.h"
#include <meta/prefs.h>
#include <meta/errors.h>
#include "util-private.h"

/* Looks up the MetaWindow representing the frame of the given X window.
 * Used as a helper function by a bunch of the functions below.
 *
 * FIXME: The functions that use this function throw the result away
 * after use. Many of these functions tend to be called in small groups,
 * which results in get_window() getting called several times in succession
 * with the same parameters. We should profile to see whether this wastes
 * much time, and if it does we should look into a generalised
 * meta_core_get_window_info() which takes a bunch of pointers to variables
 * to put its results in, and only fills in the non-null ones.
 */
static MetaWindow *
get_window (Display *xdisplay,
            Window   frame_xwindow)
{
  MetaDisplay *display;
  MetaWindow *window;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, frame_xwindow);

  if (window == NULL || window->frame == NULL)
    {
      meta_bug ("No such frame window 0x%lx!\n", frame_xwindow);
      return NULL;
    }

  return window;
}

void
meta_core_get (Display *xdisplay,
    Window xwindow,
    ...)
{
  va_list args;
  MetaCoreGetType request;

  MetaDisplay *display = meta_display_for_x_display (xdisplay);
  MetaWindow *window = meta_display_lookup_x_window (display, xwindow);

  va_start (args, xwindow);

  request = va_arg (args, MetaCoreGetType);

  /* Now, we special-case the first request slightly. Mostly, requests
   * for information on windows which have no frame are errors.
   * But sometimes we may want to know *whether* a window has a frame.
   * In this case, pass the key META_CORE_WINDOW_HAS_FRAME
   * as the *first* request, with a pointer to a boolean; if the window
   * has no frame, this will be set to False and meta_core_get will
   * exit immediately (so the values of any other requests will be
   * undefined). Otherwise it will be set to True and meta_core_get will
   * continue happily on its way.
   */

  if (request != META_CORE_WINDOW_HAS_FRAME &&
      (window == NULL || window->frame == NULL)) {
    meta_bug ("No such frame window 0x%lx!\n", xwindow);
    goto out;
  }

  while (request != META_CORE_GET_END) {
    
    gpointer answer = va_arg (args, gpointer);

    switch (request) {
      case META_CORE_WINDOW_HAS_FRAME:
        *((gboolean*)answer) = window != NULL && window->frame != NULL;
        if (!*((gboolean*)answer)) goto out; /* see above */
        break; 
      case META_CORE_GET_CLIENT_WIDTH:
        *((gint*)answer) = window->rect.width;
        break;
      case META_CORE_GET_CLIENT_HEIGHT:
        *((gint*)answer) = window->rect.height;
        break;
      case META_CORE_GET_CLIENT_XWINDOW:
        *((Window*)answer) = window->xwindow;
        break;
      case META_CORE_GET_FRAME_FLAGS:
        *((MetaFrameFlags*)answer) = meta_frame_get_flags (window->frame);
        break; 
      case META_CORE_GET_FRAME_TYPE:
        *((MetaFrameType*)answer) = meta_window_get_frame_type (window);
        break;
      case META_CORE_GET_MINI_ICON:
        *((GdkPixbuf**)answer) = window->mini_icon;
        break;
      case META_CORE_GET_ICON:
        *((GdkPixbuf**)answer) = window->icon;
        break;
      case META_CORE_GET_X:
        meta_window_get_position (window, (int*)answer, NULL);
        break;
      case META_CORE_GET_Y:
        meta_window_get_position (window, NULL, (int*)answer);
        break;
      case META_CORE_GET_FRAME_WORKSPACE:
        *((gint*)answer) = meta_window_get_net_wm_desktop (window);
        break;
      case META_CORE_GET_FRAME_X:
        *((gint*)answer) = window->frame->rect.x;
        break;
      case META_CORE_GET_FRAME_Y:
        *((gint*)answer) = window->frame->rect.y;
        break;
      case META_CORE_GET_FRAME_WIDTH:
        *((gint*)answer) = window->frame->rect.width;
        break;
      case META_CORE_GET_FRAME_HEIGHT:
        *((gint*)answer) = window->frame->rect.height;
        break;
      case META_CORE_GET_THEME_VARIANT:
        *((char**)answer) = window->gtk_theme_variant;
        break;
      case META_CORE_GET_SCREEN_WIDTH:
        *((gint*)answer) = window->screen->rect.width;
        break;
      case META_CORE_GET_SCREEN_HEIGHT:
        *((gint*)answer) = window->screen->rect.height;
        break;

      default:
        meta_warning("Unknown window information request: %d\n", request);
    }

    request = va_arg (args, MetaCoreGetType);
  } 

 out:
  va_end (args);
}

void
meta_core_queue_frame_resize (Display *xdisplay,
                              Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_queue (window, META_QUEUE_MOVE_RESIZE);
  meta_window_frame_size_changed (window);
}

static gboolean
lower_window_and_transients (MetaWindow *window,
                             gpointer   data)
{
  meta_window_lower (window);

  meta_window_foreach_transient (window, lower_window_and_transients, NULL);

  if (meta_prefs_get_focus_mode () == G_DESKTOP_FOCUS_MODE_CLICK &&
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

  return FALSE;
}

void
meta_core_user_lower_and_unfocus (Display *xdisplay,
                                  Window   frame_xwindow,
                                  guint32  timestamp)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  lower_window_and_transients (window, NULL);

 /* Rather than try to figure that out whether we just lowered
  * the focus window, assume that's always the case. (Typically,
  * this will be invoked via keyboard action or by a mouse action;
  * in either case the window or a modal child will have been focused.) */
  meta_workspace_focus_default_window (window->screen->active_workspace,
                                       NULL,
                                       timestamp);
}

void
meta_core_user_focus (Display *xdisplay,
                      Window   frame_xwindow,
                      guint32  timestamp)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);
  
  meta_window_focus (window, timestamp);
}

void
meta_core_minimize (Display *xdisplay,
                    Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_minimize (window);
}

void
meta_core_maximize (Display *xdisplay,
                    Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  meta_window_maximize (window, META_MAXIMIZE_BOTH);
}

void
meta_core_toggle_maximize_vertically (Display *xdisplay,
				      Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  if (META_WINDOW_MAXIMIZED_VERTICALLY (window))
    meta_window_unmaximize (window, META_MAXIMIZE_VERTICAL);
  else
    meta_window_maximize (window, META_MAXIMIZE_VERTICAL);
}

void
meta_core_toggle_maximize_horizontally (Display *xdisplay,
				        Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  if (META_WINDOW_MAXIMIZED_HORIZONTALLY (window))
    meta_window_unmaximize (window, META_MAXIMIZE_HORIZONTAL);
  else
    meta_window_maximize (window, META_MAXIMIZE_HORIZONTAL);
}

void
meta_core_toggle_maximize (Display *xdisplay,
                           Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  if (META_WINDOW_MAXIMIZED (window))
    meta_window_unmaximize (window, META_MAXIMIZE_BOTH);
  else
    meta_window_maximize (window, META_MAXIMIZE_BOTH);
}

void
meta_core_unmaximize (Display *xdisplay,
                      Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  if (meta_prefs_get_raise_on_click ())
    meta_window_raise (window);

  meta_window_unmaximize (window, META_MAXIMIZE_BOTH);
}

void
meta_core_delete (Display *xdisplay,
                  Window   frame_xwindow,
                  guint32  timestamp)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);
     
  meta_window_delete (window, timestamp);
}

void
meta_core_unshade (Display *xdisplay,
                   Window   frame_xwindow,
                   guint32  timestamp)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_unshade (window, timestamp);
}

void
meta_core_shade (Display *xdisplay,
                 Window   frame_xwindow,
                 guint32  timestamp)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);
  
  meta_window_shade (window, timestamp);
}

void
meta_core_unstick (Display *xdisplay,
                   Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_unstick (window);
}

void
meta_core_make_above (Display *xdisplay,
                      Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_make_above (window);
}

void
meta_core_unmake_above (Display *xdisplay,
                        Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_unmake_above (window);
}

void
meta_core_stick (Display *xdisplay,
                 Window   frame_xwindow)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_stick (window);
}

void
meta_core_change_workspace (Display *xdisplay,
                            Window   frame_xwindow,
                            int      new_workspace)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);

  meta_window_change_workspace (window,
                                meta_screen_get_workspace_by_index (window->screen,
                                                                    new_workspace));
}

void
meta_core_show_window_menu (Display *xdisplay,
                            Window   frame_xwindow,
                            int      root_x,
                            int      root_y,
                            int      button,
                            guint32  timestamp)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);
  
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
    case META_MENU_OP_NONE:
      /* No keybinding for this one */
      break;
    case META_MENU_OP_DELETE:
      name = "close";
      break;
    case META_MENU_OP_MINIMIZE:
      name = "minimize";
      break;
    case META_MENU_OP_UNMAXIMIZE:
      name = "unmaximize";
      break;
    case META_MENU_OP_MAXIMIZE:
      name = "maximize";
      break;
    case META_MENU_OP_UNSHADE:
    case META_MENU_OP_SHADE:
      name = "toggle_shaded";
      break;
    case META_MENU_OP_UNSTICK:
    case META_MENU_OP_STICK:
      name = "toggle-on-all-workspaces";
      break;
    case META_MENU_OP_ABOVE:
    case META_MENU_OP_UNABOVE:
      name = "toggle-above";
      break;
    case META_MENU_OP_WORKSPACES:
      switch (workspace)
        {
        case 1:
          name = "move-to-workspace-1";
          break;
        case 2:
          name = "move-to-workspace-2";
          break;
        case 3:
          name = "move-to-workspace-3";
          break; 
        case 4:
          name = "move-to-workspace-4";
          break; 
        case 5:
          name = "move-to-workspace-5";
          break; 
        case 6:
          name = "move-to-workspace-6";
          break; 
        case 7:
          name = "move-to-workspace-7";
          break; 
        case 8:
          name = "move-to-workspace-8";
          break; 
        case 9:
          name = "move-to-workspace-9";
          break; 
        case 10:
          name = "move-to-workspace-10";
          break;
        case 11:
          name = "move-to-workspace-11";
          break;
        case 12:
          name = "move-to-workspace-12";
          break;
        }
      break;
    case META_MENU_OP_MOVE:
      name = "begin-move";
      break;
    case META_MENU_OP_RESIZE:
      name = "begin-resize";
      break;
    case META_MENU_OP_MOVE_LEFT:
      name = "move-to-workspace-left";
      break;
    case META_MENU_OP_MOVE_RIGHT:
      name = "move-to-workspace-right";
      break;
    case META_MENU_OP_MOVE_UP:
      name = "move-to-workspace-up";
      break;
    case META_MENU_OP_MOVE_DOWN:
      name = "move-to-workspace-down";
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
  MetaWorkspace *workspace;

  display = meta_display_for_x_display (xdisplay);
  workspace = meta_screen_get_workspace_by_index (display->screen, index);
  return workspace ? meta_workspace_get_name (workspace) : NULL;
}

gboolean
meta_core_begin_grab_op (Display    *xdisplay,
                         Window      frame_xwindow,
                         MetaGrabOp  op,
                         gboolean    pointer_already_grabbed,
                         gboolean    frame_action,
                         int         button,
                         gulong      modmask,
                         guint32     timestamp,
                         int         root_x,
                         int         root_y)
{
  MetaWindow *window = get_window (xdisplay, frame_xwindow);
  MetaDisplay *display;
  MetaScreen *screen;
  
  display = meta_display_for_x_display (xdisplay);
  screen = display->screen;

  g_assert (screen != NULL);
  
  return meta_display_begin_grab_op (display, screen, window,
                                     op, pointer_already_grabbed,
                                     frame_action,
                                     button, modmask,
                                     timestamp, root_x, root_y);
}

void
meta_core_end_grab_op (Display *xdisplay,
                       guint32  timestamp)
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
  MetaWindow *window = get_window (xdisplay, frame_on_screen);

  meta_frame_set_screen_cursor (window->frame, cursor);
}

void
meta_invalidate_default_icons (void)
{
  MetaDisplay *display = meta_get_display ();
  GSList *windows;
  GSList *l;

  if (display == NULL)
    return; /* We can validly be called before the display is opened. */

  windows = meta_display_list_windows (display, META_LIST_DEFAULT);
  for (l = windows; l != NULL; l = l->next)
    {
      MetaWindow *window = (MetaWindow*)l->data;

      if (window->icon_cache.origin == USING_FALLBACK_ICON)
        {
          meta_icon_cache_free (&(window->icon_cache));
          meta_window_update_icon_now (window);
        }
    }

  g_slist_free (windows);
}

void
meta_core_add_old_event_mask (Display     *xdisplay,
                              Window       xwindow,
                              XIEventMask *mask)
{
  XIEventMask *prev;
  gint n_masks, i, j;

  prev = XIGetSelectedEvents (xdisplay, xwindow, &n_masks);

  for (i = 0; i < n_masks; i++)
    {
      if (prev[i].deviceid != XIAllMasterDevices)
        continue;

      for (j = 0; j < MIN (mask->mask_len, prev[i].mask_len); j++)
        mask->mask[j] |= prev[i].mask[j];
    }

  XFree (prev);
}
