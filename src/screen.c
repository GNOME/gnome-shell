/* Metacity X screen handler */

/* 
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
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
#include "screen.h"
#include "util.h"
#include "errors.h"
#include "window.h"
#include "frame.h"
#include "prefs.h"
#include "workspace.h"
#include "keybindings.h"
#include "stack.h"
#include "xprops.h"

#ifdef HAVE_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <X11/Xatom.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>

static char* get_screen_name (MetaDisplay *display,
                              int          number);

static void update_num_workspaces  (MetaScreen *screen);
static void update_focus_mode      (MetaScreen *screen);
static void prefs_changed_callback (MetaPreference pref,
                                    gpointer       data);

static int
set_wm_check_hint (MetaScreen *screen)
{
  unsigned long data[1];

  g_return_val_if_fail (screen->display->leader_window != None, 0);
  
  data[0] = screen->display->leader_window;

  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_net_supporting_wm_check,
                   XA_WINDOW,
                   32, PropModeReplace, (guchar*) data, 1);

  /* Legacy GNOME hint (uses cardinal, dunno why) */

  /* do this after setting up window fully, to avoid races
   * with clients listening to property notify on root.
   */
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_win_supporting_wm_check,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  
  return Success;
}

static int
set_supported_hint (MetaScreen *screen)
{
#define N_SUPPORTED 42
#define N_WIN_SUPPORTED 1
  Atom atoms[N_SUPPORTED];
  
  atoms[0] = screen->display->atom_net_wm_name;
  atoms[1] = screen->display->atom_net_close_window;
  atoms[2] = screen->display->atom_net_wm_state;
  atoms[3] = screen->display->atom_net_wm_state_shaded;
  atoms[4] = screen->display->atom_net_wm_state_maximized_vert;
  atoms[5] = screen->display->atom_net_wm_state_maximized_horz;
  atoms[6] = screen->display->atom_net_wm_desktop;
  atoms[7] = screen->display->atom_net_number_of_desktops;
  atoms[8] = screen->display->atom_net_current_desktop;
  atoms[9] = screen->display->atom_net_wm_window_type;
  atoms[10] = screen->display->atom_net_wm_window_type_desktop;
  atoms[11] = screen->display->atom_net_wm_window_type_dock;
  atoms[12] = screen->display->atom_net_wm_window_type_toolbar;
  atoms[13] = screen->display->atom_net_wm_window_type_menu;
  atoms[14] = screen->display->atom_net_wm_window_type_dialog;
  atoms[15] = screen->display->atom_net_wm_window_type_normal;
  atoms[16] = screen->display->atom_net_wm_state_modal;
  atoms[17] = screen->display->atom_net_client_list;
  atoms[18] = screen->display->atom_net_client_list_stacking;
  atoms[19] = screen->display->atom_net_wm_state_skip_taskbar;
  atoms[20] = screen->display->atom_net_wm_state_skip_pager;
  atoms[21] = screen->display->atom_net_wm_icon;
  atoms[22] = screen->display->atom_net_wm_moveresize;
  atoms[23] = screen->display->atom_net_wm_state_hidden;
  atoms[24] = screen->display->atom_net_wm_window_type_utility;
  atoms[25] = screen->display->atom_net_wm_window_type_splashscreen;
  atoms[26] = screen->display->atom_net_wm_state_fullscreen;
  atoms[27] = screen->display->atom_net_wm_ping;
  atoms[28] = screen->display->atom_net_active_window;
  atoms[29] = screen->display->atom_net_workarea;
  atoms[30] = screen->display->atom_net_showing_desktop;
  atoms[31] = screen->display->atom_net_desktop_layout;
  atoms[32] = screen->display->atom_net_desktop_names;
  atoms[33] = screen->display->atom_net_wm_allowed_actions;
  atoms[34] = screen->display->atom_net_wm_action_move;
  atoms[35] = screen->display->atom_net_wm_action_resize;
  atoms[36] = screen->display->atom_net_wm_action_shade;
  atoms[37] = screen->display->atom_net_wm_action_stick;
  atoms[38] = screen->display->atom_net_wm_action_maximize_horz;
  atoms[39] = screen->display->atom_net_wm_action_maximize_vert;
  atoms[40] = screen->display->atom_net_wm_action_change_desktop;
  atoms[41] = screen->display->atom_net_wm_action_close;
  
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_net_supported,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) atoms, N_SUPPORTED);

  /* Set legacy GNOME hints */
  atoms[0] = screen->display->atom_win_layer;
  
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_win_protocols,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) atoms, N_WIN_SUPPORTED);
  
  return Success;
#undef N_SUPPORTED
}

static int
set_wm_icon_size_hint (MetaScreen *screen)
{
#define N_VALS 6
  gulong vals[N_VALS];

  /* min width, min height, max w, max h, width inc, height inc */
  vals[0] = META_ICON_WIDTH;
  vals[1] = META_ICON_HEIGHT;
  vals[2] = META_ICON_WIDTH;
  vals[3] = META_ICON_HEIGHT;
  vals[4] = 0;
  vals[5] = 0;
  
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_wm_icon_size,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) vals, N_VALS);
  
  return Success;
#undef N_VALS
}

static void
reload_xinerama_infos (MetaScreen *screen)
{
  MetaDisplay *display;

  display = screen->display;
  
  if (screen->xinerama_infos)
    g_free (screen->xinerama_infos);
  
  screen->xinerama_infos = NULL;
  screen->n_xinerama_infos = 0;
  screen->last_xinerama_index = 0;

  screen->display->xinerama_cache_invalidated = TRUE;
  
#ifdef HAVE_XFREE_XINERAMA
  if (XineramaIsActive (display->xdisplay))
    {
      XineramaScreenInfo *infos;
      int n_infos;
      int i;
      
      n_infos = 0;
      infos = XineramaQueryScreens (display->xdisplay, &n_infos);

      meta_topic (META_DEBUG_XINERAMA,
                  "Found %d Xinerama screens on display %s\n",
                  n_infos, display->name);

      if (n_infos > 0)
        {
          screen->xinerama_infos = g_new (MetaXineramaScreenInfo, n_infos);
          screen->n_xinerama_infos = n_infos;
          
          i = 0;
          while (i < n_infos)
            {
              screen->xinerama_infos[i].number = infos[i].screen_number;
              screen->xinerama_infos[i].x_origin = infos[i].x_org;
              screen->xinerama_infos[i].y_origin = infos[i].y_org;
              screen->xinerama_infos[i].width = infos[i].width;
              screen->xinerama_infos[i].height = infos[i].height;

              meta_topic (META_DEBUG_XINERAMA,
                          "Xinerama %d is %d,%d %d x %d\n",
                          screen->xinerama_infos[i].number,
                          screen->xinerama_infos[i].x_origin,
                          screen->xinerama_infos[i].y_origin,
                          screen->xinerama_infos[i].width,
                          screen->xinerama_infos[i].height);              
              
              ++i;
            }
        }
      
      meta_XFree (infos);
    }
  else
    {
      meta_topic (META_DEBUG_XINERAMA,
                  "No XFree86 Xinerama extension or XFree86 Xinerama inactive on display %s\n",
                  display->name);
    }
#else
  meta_topic (META_DEBUG_XINERAMA,
              "Metacity compiled without XFree86 Xinerama support\n");
#endif /* HAVE_XFREE_XINERAMA */

#ifdef HAVE_SOLARIS_XINERAMA
  /* This code from GDK, Copyright (C) 2002 Sun Microsystems */
  if (screen->n_xinerama_infos == 0 &&
      XineramaGetState (screen->display->xdisplay,
                        screen->number))
    {
      XRectangle monitors[MAXFRAMEBUFFERS];
      unsigned char hints[16];
      int result;
      int n_monitors;
      int i;

      n_monitors = 0;
      result = XineramaGetInfo (screen->display->xdisplay,
                                screen->number,
				monitors, hints,
                                &n_monitors);
      /* Yes I know it should be Success but the current implementation 
       * returns the num of monitor
       */
      if (result > 0)
	{
          g_assert (n_monitors > 0);
          
          screen->xinerama_infos = g_new (MetaXineramaScreenInfo, n_monitors);
          screen->n_xinerama_infos = n_monitors;
          
          i = 0;
          while (i < n_monitors)
            {
              screen->xinerama_infos[i].number = i;
              screen->xinerama_infos[i].x_origin = monitors[i].x;
	      screen->xinerama_infos[i].y_origin = monitors[i].y;
	      screen->xinerama_infos[i].width = monitors[i].width;
	      screen->xinerama_infos[i].height = monitors[i].height;

              meta_topic (META_DEBUG_XINERAMA,
                          "Xinerama %d is %d,%d %d x %d\n",
                          screen->xinerama_infos[i].number,
                          screen->xinerama_infos[i].x_origin,
                          screen->xinerama_infos[i].y_origin,
                          screen->xinerama_infos[i].width,
                          screen->xinerama_infos[i].height);              
              
              ++i;
            }
	}
    }
  else if (screen->n_xinerama_infos == 0)
    {
      meta_topic (META_DEBUG_XINERAMA,
                  "No Solaris Xinerama extension or Solaris Xinerama inactive on display %s\n",
                  display->name);
    }
#else
  meta_topic (META_DEBUG_XINERAMA,
              "Metacity compiled without Solaris Xinerama support\n");
#endif /* HAVE_SOLARIS_XINERAMA */

  
  /* If no Xinerama, fill in the single screen info so
   * we can use the field unconditionally
   */
  if (screen->n_xinerama_infos == 0)
    {
      if (g_getenv ("METACITY_DEBUG_XINERAMA"))
        {
          meta_topic (META_DEBUG_XINERAMA,
                      "Pretending a single monitor has two Xinerama screens\n");
          
          screen->xinerama_infos = g_new (MetaXineramaScreenInfo, 2);
          screen->n_xinerama_infos = 2;
          
          screen->xinerama_infos[0].number = 0;
          screen->xinerama_infos[0].x_origin = 0;
          screen->xinerama_infos[0].y_origin = 0;
          screen->xinerama_infos[0].width = screen->width / 2;
          screen->xinerama_infos[0].height = screen->height;

          screen->xinerama_infos[1].number = 1;
          screen->xinerama_infos[1].x_origin = screen->width / 2;
          screen->xinerama_infos[1].y_origin = 0;
          screen->xinerama_infos[1].width = screen->width / 2 + screen->width % 2;
          screen->xinerama_infos[1].height = screen->height;
        }
      else
        {
          meta_topic (META_DEBUG_XINERAMA,
                      "No Xinerama screens, using default screen info\n");
          
          screen->xinerama_infos = g_new (MetaXineramaScreenInfo, 1);
          screen->n_xinerama_infos = 1;
          
          screen->xinerama_infos[0].number = 0;
          screen->xinerama_infos[0].x_origin = 0;
          screen->xinerama_infos[0].y_origin = 0;
          screen->xinerama_infos[0].width = screen->width;
          screen->xinerama_infos[0].height = screen->height;
        }
    }

  g_assert (screen->n_xinerama_infos > 0);
  g_assert (screen->xinerama_infos != NULL);
}

MetaScreen*
meta_screen_new (MetaDisplay *display,
                 int          number)
{
  MetaScreen *screen;
  Window xroot;
  Display *xdisplay;
  XWindowAttributes attr;
  Window new_wm_sn_owner;
  Window current_wm_sn_owner;
  gboolean replace_current_wm;
  Atom wm_sn_atom;
  char buf[128];
  Time manager_timestamp;
  
  replace_current_wm = meta_get_replace_current_wm ();
  
  /* Only display->name, display->xdisplay, and display->error_traps
   * can really be used in this function, since normally screens are
   * created from the MetaDisplay constructor
   */
  
  xdisplay = display->xdisplay;
  
  meta_verbose ("Trying screen %d on display '%s'\n",
                number, display->name);

  xroot = RootWindow (xdisplay, number);

  /* FVWM checks for None here, I don't know if this
   * ever actually happens
   */
  if (xroot == None)
    {
      meta_warning (_("Screen %d on display '%s' is invalid\n"),
                    number, display->name);
      return NULL;
    }

  sprintf (buf, "WM_S%d", number);
  wm_sn_atom = XInternAtom (xdisplay, buf, False);  
  
  current_wm_sn_owner = XGetSelectionOwner (xdisplay, wm_sn_atom);

  if (current_wm_sn_owner != None)
    {
      XSetWindowAttributes attrs;
      
      if (!replace_current_wm)
        {
          meta_warning (_("Screen %d on display \"%s\" already has a window manager; try using the --replace option to replace the current window manager.\n"),
                        number, display->name);

          return NULL;
        }

      /* We want to find out when the current selection owner dies */
      meta_error_trap_push (display);
      attrs.event_mask = StructureNotifyMask;
      XChangeWindowAttributes (xdisplay,
                               current_wm_sn_owner, CWEventMask, &attrs);
      if (meta_error_trap_pop (display) != Success)
        current_wm_sn_owner = None; /* don't wait for it to die later on */
    }

  new_wm_sn_owner = meta_create_offscreen_window (xdisplay, xroot);

  {
    /* Generate a timestamp */
    XSetWindowAttributes attrs;
    XEvent event;

    attrs.event_mask = PropertyChangeMask;
    XChangeWindowAttributes (xdisplay, new_wm_sn_owner, CWEventMask, &attrs);
    
    XChangeProperty (xdisplay,
                     new_wm_sn_owner, XA_WM_CLASS, XA_STRING, 8,
                     PropModeAppend, NULL, 0);
    XWindowEvent (xdisplay, new_wm_sn_owner, PropertyChangeMask, &event);
    attrs.event_mask = NoEventMask;
    XChangeWindowAttributes (display->xdisplay,
                             new_wm_sn_owner, CWEventMask, &attrs);

    manager_timestamp = event.xproperty.time;
  }
  
  XSetSelectionOwner (xdisplay, wm_sn_atom, new_wm_sn_owner,
                      manager_timestamp);

  if (XGetSelectionOwner (xdisplay, wm_sn_atom) != new_wm_sn_owner)
    {
      meta_warning (_("Could not acquire window manager selection on screen %d display \"%s\"\n"),
                    number, display->name);

      XDestroyWindow (xdisplay, new_wm_sn_owner);
      
      return NULL;
    }
  
  {
    /* Send client message indicating that we are now the WM */
    XClientMessageEvent ev;
    
    ev.type = ClientMessage;
    ev.window = xroot;
    ev.message_type = display->atom_manager;
    ev.format = 32;
    ev.data.l[0] = manager_timestamp;
    ev.data.l[1] = wm_sn_atom;

    XSendEvent (xdisplay, xroot, False, StructureNotifyMask, (XEvent*)&ev);
  }

  /* Wait for old window manager to go away */
  if (current_wm_sn_owner != None)
    {
      XEvent event;

      /* We sort of block infinitely here which is probably lame. */
      
      meta_verbose ("Waiting for old window manager to exit\n");
      do
        {
          XWindowEvent (xdisplay, current_wm_sn_owner,
                        StructureNotifyMask, &event);
        }
      while (event.type != DestroyNotify);
    }
  
  /* select our root window events */
  meta_error_trap_push (display);

  /* We need to or with the existing event mask since
   * gtk+ may be interested in other events.
   */
  XGetWindowAttributes (xdisplay, xroot, &attr);
  XSelectInput (xdisplay,
                xroot,
                SubstructureRedirectMask | SubstructureNotifyMask |
                ColormapChangeMask | PropertyChangeMask |
                LeaveWindowMask | EnterWindowMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask |
                FocusChangeMask | StructureNotifyMask |
		attr.your_event_mask);
  if (meta_error_trap_pop (display) != Success)
    {
      meta_warning (_("Screen %d on display \"%s\" already has a window manager\n"),
                    number, display->name);

      XDestroyWindow (xdisplay, new_wm_sn_owner);
      
      return NULL;
    }
  
  screen = g_new (MetaScreen, 1);
  screen->closing = 0;
  
  screen->display = display;
  screen->number = number;
  screen->screen_name = get_screen_name (display, number);
  screen->xscreen = ScreenOfDisplay (xdisplay, number);
  screen->xroot = xroot;
  screen->width = WidthOfScreen (screen->xscreen);
  screen->height = HeightOfScreen (screen->xscreen);
  screen->current_cursor = -1; /* invalid/unset */
  screen->default_xvisual = DefaultVisualOfScreen (screen->xscreen);
  screen->default_depth = DefaultDepthOfScreen (screen->xscreen);

  screen->wm_sn_selection_window = new_wm_sn_owner;
  screen->wm_sn_atom = wm_sn_atom;
  screen->wm_sn_timestamp = manager_timestamp;
  
  screen->work_area_idle = 0;

  screen->active_workspace = NULL;
  screen->rows_of_workspaces = 1;
  screen->columns_of_workspaces = -1;
  screen->vertical_workspaces = FALSE;
  screen->starting_corner = META_SCREEN_TOPLEFT;

  screen->xinerama_infos = NULL;
  screen->n_xinerama_infos = 0;
  screen->last_xinerama_index = 0;

  reload_xinerama_infos (screen);
  
  meta_screen_set_cursor (screen, META_CURSOR_DEFAULT);
  
  if (display->leader_window == None)
    display->leader_window = meta_create_offscreen_window (display->xdisplay,
                                                           screen->xroot);
  
  if (display->no_focus_window == None)
    {
      display->no_focus_window = meta_create_offscreen_window (display->xdisplay,
                                                               screen->xroot);

      XSelectInput (display->xdisplay, display->no_focus_window,
                    FocusChangeMask | KeyPressMask | KeyReleaseMask);
      XMapWindow (display->xdisplay, display->no_focus_window);
    }
  
  set_wm_icon_size_hint (screen);
  
  set_supported_hint (screen);
  
  set_wm_check_hint (screen);

  meta_screen_update_workspace_layout (screen);
  meta_screen_update_workspace_names (screen);
  
  /* Screens must have at least one workspace at all times,
   * so create that required workspace.
   */
  meta_workspace_activate (meta_workspace_new (screen));
  update_num_workspaces (screen);

  screen->all_keys_grabbed = FALSE;
  screen->keys_grabbed = FALSE;
  meta_screen_grab_keys (screen);

  screen->ui = meta_ui_new (screen->display->xdisplay,
                            screen->xscreen);

  screen->tab_popup = NULL;
  
  screen->stack = meta_stack_new (screen);

  meta_prefs_add_listener (prefs_changed_callback, screen);
  
  meta_verbose ("Added screen %d ('%s') root 0x%lx\n",
                screen->number, screen->screen_name, screen->xroot);  
  
  return screen;
}

void
meta_screen_free (MetaScreen *screen)
{
  MetaDisplay *display;

  display = screen->display;

  screen->closing += 1;
  
  meta_display_grab (display);

  meta_display_unmanage_windows_for_screen (display, screen);
  
  meta_prefs_remove_listener (prefs_changed_callback, screen);
  
  meta_screen_ungrab_keys (screen);

  meta_ui_free (screen->ui);

  meta_stack_free (screen->stack);

  meta_error_trap_push (screen->display);
  XSelectInput (screen->display->xdisplay, screen->xroot, 0);
  if (meta_error_trap_pop (screen->display) != Success)
    meta_warning (_("Could not release screen %d on display \"%s\"\n"),
                  screen->number, screen->display->name);

  XDestroyWindow (screen->display->xdisplay,
                  screen->wm_sn_selection_window);
  
  if (screen->work_area_idle != 0)
    g_source_remove (screen->work_area_idle);
  
  g_free (screen->screen_name);
  g_free (screen);

  XFlush (display->xdisplay);
  meta_display_ungrab (display);
}

void
meta_screen_manage_all_windows (MetaScreen *screen)
{
  Window ignored1, ignored2;
  Window *children;
  int n_children;
  int i;

  /* Must grab server to avoid obvious race condition */
  meta_display_grab (screen->display);

  meta_error_trap_push (screen->display);
  
  XQueryTree (screen->display->xdisplay,
              screen->xroot,
              &ignored1, &ignored2, &children, &n_children);

  if (meta_error_trap_pop (screen->display))
    {
      meta_display_ungrab (screen->display);
      return;
    }

  meta_stack_freeze (screen->stack);
  i = 0;
  while (i < n_children)
    {
      meta_window_new (screen->display, children[i], TRUE);

      ++i;
    }
  meta_stack_thaw (screen->stack);

  meta_display_ungrab (screen->display);
  
  if (children)
    XFree (children);
}

MetaScreen*
meta_screen_for_x_screen (Screen *xscreen)
{
  MetaDisplay *display;
  
  display = meta_display_for_x_display (DisplayOfScreen (xscreen));

  if (display == NULL)
    return NULL;
  
  return meta_display_screen_for_x_screen (display, xscreen);
}

static void
prefs_changed_callback (MetaPreference pref,
                        gpointer       data)
{
  MetaScreen *screen = data;
  
  if (pref == META_PREF_NUM_WORKSPACES)
    {
      update_num_workspaces (screen);
    }
  else if (pref == META_PREF_FOCUS_MODE)
    {
      update_focus_mode (screen);
    }
}


static char*
get_screen_name (MetaDisplay *display,
                 int          number)
{
  char *p;
  char *dname;
  char *scr;
  
  /* DisplayString gives us a sort of canonical display,
   * vs. the user-entered name from XDisplayName()
   */
  dname = g_strdup (DisplayString (display->xdisplay));

  /* Change display name to specify this screen.
   */
  p = strrchr (dname, ':');
  if (p)
    {
      p = strchr (p, '.');
      if (p)
        *p = '\0';
    }
  
  scr = g_strdup_printf ("%s.%d", dname, number);

  g_free (dname);

  return scr;
}

static gint
ptrcmp (gconstpointer a, gconstpointer b)
{
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;
  
  listp = data;

  *listp = g_slist_prepend (*listp, value);
}

void
meta_screen_foreach_window (MetaScreen *screen,
                            MetaScreenWindowFunc func,
                            gpointer data)
{
  GSList *winlist;
  GSList *tmp;

  /* If we end up doing this often, just keeping a list
   * of windows might be sensible.
   */
  
  winlist = NULL;
  g_hash_table_foreach (screen->display->window_ids,
                        listify_func,
                        &winlist);
  
  winlist = g_slist_sort (winlist, ptrcmp);
  
  tmp = winlist;
  while (tmp != NULL)
    {
      /* If the next node doesn't contain this window
       * a second time, delete the window.
       */
      if (tmp->next == NULL ||
          (tmp->next && tmp->next->data != tmp->data))
        {
          MetaWindow *window = tmp->data;

          if (window->screen == screen)
            (* func) (screen, window, data);
        }
      
      tmp = tmp->next;
    }
  g_slist_free (winlist);
}

static void
queue_draw (MetaScreen *screen, MetaWindow *window, gpointer data)
{
  if (window->frame)
    meta_frame_queue_draw (window->frame);
}

void
meta_screen_queue_frame_redraws (MetaScreen *screen)
{
  meta_screen_foreach_window (screen, queue_draw, NULL);
}

static void
queue_resize (MetaScreen *screen, MetaWindow *window, gpointer data)
{
  meta_window_queue_move_resize (window);
}

void
meta_screen_queue_window_resizes (MetaScreen *screen)
{
  meta_screen_foreach_window (screen, queue_resize, NULL);
}

int
meta_screen_get_n_workspaces (MetaScreen *screen)
{
  GList *tmp;
  int i;

  i = 0;
  tmp = screen->display->workspaces;
  while (tmp != NULL)
    {
      MetaWorkspace *w = tmp->data;

      if (w->screen == screen)
        ++i;
      
      tmp = tmp->next;
    }

  return i;
}

static int
set_number_of_spaces_hint (MetaScreen *screen,
			   int         n_spaces)
{
  unsigned long data[1];

  if (screen->closing > 0)
    return 0;

  data[0] = n_spaces;

  meta_verbose ("Setting _NET_NUMBER_OF_DESKTOPS to %ld\n", data[0]);

  meta_error_trap_push (screen->display);
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_net_number_of_desktops,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  return meta_error_trap_pop (screen->display);
}

static void
update_num_workspaces (MetaScreen *screen)
{
  int new_num;
  GList *tmp;
  int i;
  GList *extras;
  MetaWorkspace *last_remaining;
  gboolean need_change_space;
  
  new_num = meta_prefs_get_num_workspaces ();

  g_assert (new_num > 0);

  last_remaining = NULL;
  extras = NULL;
  i = 0;
  tmp = screen->display->workspaces;
  while (tmp != NULL)
    {
      MetaWorkspace *w = tmp->data;

      if (w->screen == screen)
        {          
          ++i;

          if (i > new_num)
            extras = g_list_prepend (extras, w);
          else
            last_remaining = w;
        }
      
      tmp = tmp->next;
    }

  g_assert (last_remaining);
  
  /* Get rid of the extra workspaces by moving all their windows
   * to last_remaining, then activating last_remaining if
   * one of the removed workspaces was active. This will be a bit
   * wacky if the config tool for changing number of workspaces
   * is on a removed workspace ;-)
   */
  need_change_space = FALSE;
  tmp = extras;
  while (tmp != NULL)
    {
      MetaWorkspace *w = tmp->data;

      meta_workspace_relocate_windows (w, last_remaining);      

      if (w == screen->active_workspace)
        need_change_space = TRUE;
      
      tmp = tmp->next;
    }

  if (need_change_space)
    meta_workspace_activate (last_remaining);

  /* Should now be safe to free the workspaces */
  tmp = extras;
  while (tmp != NULL)
    {
      MetaWorkspace *w = tmp->data;

      g_assert (w->windows == NULL);
      meta_workspace_free (w);
      
      tmp = tmp->next;
    }
  
  g_list_free (extras);
  
  while (i < new_num)
    {
      meta_workspace_new (screen);
      ++i;
    }

  set_number_of_spaces_hint (screen, new_num);

  meta_screen_queue_workarea_recalc (screen);
}

static void
update_focus_mode (MetaScreen *screen)
{
  /* nothing to do anymore */ ;
}

void
meta_screen_set_cursor (MetaScreen *screen,
                        MetaCursor  cursor)
{
  Cursor xcursor;

  if (cursor == screen->current_cursor)
    return;

  screen->current_cursor = cursor;
  
  xcursor = meta_display_create_x_cursor (screen->display, cursor);
  XDefineCursor (screen->display->xdisplay, screen->xroot, xcursor);
  XFreeCursor (screen->display->xdisplay, xcursor);
}

void
meta_screen_ensure_tab_popup (MetaScreen *screen,
                              MetaTabList type)
{
  MetaTabEntry *entries;
  GSList *tab_list;
  GSList *tmp;
  int len;
  int i;

  if (screen->tab_popup)
    return;

  tab_list = meta_display_get_tab_list (screen->display,
                                        type,
                                        screen,
                                        screen->active_workspace);
  
  len = g_slist_length (tab_list);

  entries = g_new (MetaTabEntry, len + 1);
  entries[len].key = NULL;
  entries[len].title = NULL;
  entries[len].icon = NULL;
  
  i = 0;
  tmp = tab_list;
  while (i < len)
    {
      MetaWindow *window;
      MetaRectangle r;
      
      window = tmp->data;
      
      entries[i].key = (MetaTabEntryKey) window->xwindow;
      entries[i].title = window->title;
      entries[i].icon = window->icon;
      meta_window_get_outer_rect (window, &r);
      entries[i].x = r.x;
      entries[i].y = r.y;
      entries[i].width = r.width;
      entries[i].height = r.height;

      /* Find inside of highlight rectangle to be used
       * when window is outlined for tabbing.
       * This should be the size of the east/west frame,
       * and the size of the south frame, on those sides.
       * on the top it should be the size of the south frame
       * edge.
       */
      if (window->frame)
        {
          int south = window->frame->rect.height - window->frame->child_y -
            window->rect.height;
          int east = window->frame->child_x;
          entries[i].inner_x = east;
          entries[i].inner_y = south;
          entries[i].inner_width = window->rect.width;
          entries[i].inner_height = window->frame->rect.height - south * 2;
        }
      else
        {
          /* Use an arbitrary border size */
#define OUTLINE_WIDTH 5
          entries[i].inner_x = OUTLINE_WIDTH;
          entries[i].inner_y = OUTLINE_WIDTH;
          entries[i].inner_width = window->rect.width - OUTLINE_WIDTH * 2;
          entries[i].inner_height = window->rect.height - OUTLINE_WIDTH * 2;
        }
      
      ++i;
      tmp = tmp->next;
    }

  screen->tab_popup = meta_ui_tab_popup_new (entries, 
                                             screen->number,
                                             len,
                                             5, /* FIXME */
                                             TRUE);
  g_free (entries);

  g_slist_free (tab_list);
  
  /* don't show tab popup, since proper window isn't selected yet */
}

void
meta_screen_ensure_workspace_popup (MetaScreen *screen)
{
  MetaTabEntry *entries;
  int len, rows, cols;
  int i;

  if (screen->tab_popup)
    return;

  len = meta_screen_get_n_workspaces (screen);

  entries = g_new (MetaTabEntry, len + 1);
  entries[len].key = NULL;
  entries[len].title = NULL;
  entries[len].icon = NULL;
  
  meta_screen_calc_workspace_layout (screen, len, &rows, &cols);

  /* FIXME: handle screen->starting_corner
   */
  if (screen->vertical_workspaces)
    {
      int j, k, iter;

      for (i = 0, iter = 0; i < rows; ++i)
        {
          for (j = 0; j < cols; ++j)
            {
              MetaWorkspace *workspace;

              k = i + (j * rows);
              if (k >= len)
                break;

              workspace = meta_display_get_workspace_by_index (screen->display, k);
              g_assert (workspace);

              entries[iter].key = (MetaTabEntryKey) workspace;
              entries[iter].title = workspace->name;
              entries[iter].icon = NULL;
              iter++;
            }
        }

      g_assert (iter == len);
    }
  else
    {
      for (i = 0; i < len; ++i)
        {
          MetaWorkspace *workspace;

          workspace = meta_display_get_workspace_by_index (screen->display, i);

          g_assert (workspace);

          entries[i].key = (MetaTabEntryKey) workspace;
          entries[i].title = workspace->name;
          entries[i].icon = NULL;
        }
    }

  screen->tab_popup = meta_ui_tab_popup_new (entries, 
                                             screen->number,
                                             len,
                                             cols,
                                             FALSE);      

  g_free (entries);

  /* don't show tab popup, since proper window isn't selected yet */
}

/* Focus top window on active workspace */
void
meta_screen_focus_top_window (MetaScreen *screen,
                              MetaWindow *not_this_one)
{
  MetaWindow *window;

  if (not_this_one)
    meta_topic (META_DEBUG_FOCUS,
                "Focusing top window excluding %s\n", not_this_one->desc);
  
  window = meta_stack_get_default_focus_window (screen->stack,
                                                screen->active_workspace,
                                                not_this_one);

  /* FIXME I'm a loser on the CurrentTime front */
  if (window)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing top window %s\n", window->desc);

      meta_window_focus (window, meta_display_get_current_time (screen->display));

      /* Also raise the window if in click-to-focus */
      if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK)
        meta_window_raise (window);
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS, "No top window to focus found\n");
    }
}

const MetaXineramaScreenInfo*
meta_screen_get_xinerama_for_window (MetaScreen *screen,
				     MetaWindow *window)
{
  int i;
  int best_xinerama, xinerama_score;
  MetaRectangle window_rect;

  if (screen->n_xinerama_infos == 1)
    return &screen->xinerama_infos[0];
  
  meta_window_get_outer_rect (window, &window_rect);
  
  best_xinerama = 0;
  xinerama_score = 0;

  i = 0;
  while (i < screen->n_xinerama_infos)
    {
      MetaRectangle dest, screen_info;
      
      screen_info.x = screen->xinerama_infos[i].x_origin;
      screen_info.y = screen->xinerama_infos[i].y_origin;
      screen_info.width = screen->xinerama_infos[i].width;
      screen_info.height = screen->xinerama_infos[i].height;
      
      if (meta_rectangle_intersect (&screen_info, &window_rect, &dest))
        {
          if (dest.width * dest.height > xinerama_score)
            {
              xinerama_score = dest.width * dest.height;
              best_xinerama = i;
            }
        }

      ++i;
    }

  return &screen->xinerama_infos[best_xinerama];
}

const MetaXineramaScreenInfo*
meta_screen_get_current_xinerama (MetaScreen *screen)
{
  if (screen->n_xinerama_infos == 1)
    return &screen->xinerama_infos[0];

  /* Sadly, we have to do it this way. Yuck.
   */
  
  if (screen->display->xinerama_cache_invalidated)
    {
      Window root_return, child_return;
      int root_x_return, root_y_return;
      int win_x_return, win_y_return;
      unsigned int mask_return;
      int i;
      
      screen->display->xinerama_cache_invalidated = FALSE;
      
      XQueryPointer (screen->display->xdisplay,
                     screen->xroot,
                     &root_return,
                     &child_return,
                     &root_x_return,
                     &root_y_return,
                     &win_x_return,
                     &win_y_return,
                     &mask_return);

      screen->last_xinerama_index = 0;
      i = 0;
      while (i < screen->n_xinerama_infos)
        {
          if ((root_x_return >= screen->xinerama_infos[i].x_origin &&
               root_x_return < (screen->xinerama_infos[i].x_origin + screen->xinerama_infos[i].width) &&
               root_y_return >= screen->xinerama_infos[i].y_origin &&
               root_y_return < (screen->xinerama_infos[i].y_origin + screen->xinerama_infos[i].height)))
          {
            screen->last_xinerama_index = i;
            break;
          }
          
          ++i;
        }
      
      meta_topic (META_DEBUG_XINERAMA,
                  "Rechecked current Xinerama, now %d\n",
                  screen->last_xinerama_index);
    }

  return &screen->xinerama_infos[screen->last_xinerama_index];
}

#define _NET_WM_ORIENTATION_HORZ 0
#define _NET_WM_ORIENTATION_VERT 1

#define _NET_WM_TOPLEFT     0
#define _NET_WM_TOPRIGHT    1
#define _NET_WM_BOTTOMRIGHT 2
#define _NET_WM_BOTTOMLEFT  3

void
meta_screen_update_workspace_layout (MetaScreen *screen)
{
  gulong *list;
  int n_items;
  
  list = NULL;
  n_items = 0;

  if (meta_prop_get_cardinal_list (screen->display,
                                   screen->xroot,
                                   screen->display->atom_net_desktop_layout,
                                   &list, &n_items))
    {
      if (n_items == 3 || n_items == 4)
        {
          int cols, rows;
          
          switch (list[0])
            {
            case _NET_WM_ORIENTATION_HORZ:
              screen->vertical_workspaces = FALSE;
              break;
            case _NET_WM_ORIENTATION_VERT:
              screen->vertical_workspaces = TRUE;
              break;
            default:
              meta_warning ("Someone set a weird orientation in _NET_DESKTOP_LAYOUT\n");
              break;
            }

          cols = list[1];
          rows = list[2];

          if (rows <= 0 && cols <= 0)
            {
              meta_warning ("Columns = %d rows = %d in _NET_DESKTOP_LAYOUT makes no sense\n", rows, cols);
            }
          else
            {
              if (rows > 0)
                screen->rows_of_workspaces = rows;
              else
                screen->rows_of_workspaces = -1;
              
              if (cols > 0)
                screen->columns_of_workspaces = cols;
              else
                screen->columns_of_workspaces = -1;
            }

          if (n_items == 4)
            {
              switch (list[3])
                {
                  case _NET_WM_TOPLEFT:
                    screen->starting_corner = META_SCREEN_TOPLEFT;
                    break;
                  case _NET_WM_TOPRIGHT:
                    screen->starting_corner = META_SCREEN_TOPRIGHT;
                    break;
                  case _NET_WM_BOTTOMRIGHT:
                    screen->starting_corner = META_SCREEN_BOTTOMRIGHT;
                    break;
                  case _NET_WM_BOTTOMLEFT:
                    screen->starting_corner = META_SCREEN_BOTTOMLEFT;
                    break;
                  default:
                    meta_warning ("Someone set a weird starting corner in _NET_DESKTOP_LAYOUT\n");
                    break;
                }
            }
        }
      else
        {
          meta_warning ("Someone set _NET_DESKTOP_LAYOUT to %d integers instead of 4 "
                        "(3 is accepted for backwards compat)\n", n_items);
        }

      meta_XFree (list);
    }

  meta_verbose ("Workspace layout rows = %d cols = %d orientation = %d starting corner = %d\n",
                screen->rows_of_workspaces,
                screen->columns_of_workspaces,
                screen->vertical_workspaces,
                screen->starting_corner);
}

void
meta_screen_update_workspace_names (MetaScreen *screen)
{
  char **names;
  int n_names;
  int i;
  GList *tmp;
  
  names = NULL;
  n_names = 0;
  if (!meta_prop_get_utf8_list (screen->display,
                                screen->xroot,
                                screen->display->atom_net_desktop_names,
                                &names, &n_names))
    {
      meta_verbose ("Failed to get workspace names from root window %d\n",
                    screen->number);
      return;
    }

  i = 0;
  tmp = screen->display->workspaces;
  while (tmp != NULL && i < n_names)
    {
      MetaWorkspace *w = tmp->data;

      if (w->screen == screen)
        {
          meta_workspace_set_name (w, names[i]);
          
          ++i;
        }
      
      tmp = tmp->next;
    }
  
  g_strfreev (names);
}

Window
meta_create_offscreen_window (Display *xdisplay,
                              Window   parent)
{
  XSetWindowAttributes attrs;

  /* we want to be override redirect because sometimes we
   * create a window on a screen we aren't managing.
   * (but on a display we are managing at least one screen for)
   */
  attrs.override_redirect = True;
  
  return XCreateWindow (xdisplay,
                        parent,
                        -100, -100, 1, 1,
                        0,
                        CopyFromParent,
                        CopyFromParent,
                        CopyFromParent,
                        CWOverrideRedirect,
                        &attrs);
}

static int
set_work_area_hint (MetaScreen *screen)
{
  int num_workspaces;
  GList *tmp_list;
  unsigned long *data, *tmp;
  MetaRectangle area;
  
  num_workspaces = meta_screen_get_n_workspaces (screen);
  data = g_new (unsigned long, num_workspaces * 4);
  tmp_list = screen->display->workspaces;
  tmp = data;
  
  while (tmp_list != NULL)
    {
      MetaWorkspace *workspace = tmp_list->data;

      if (workspace->screen == screen)
        {
          meta_workspace_get_work_area (workspace, &area);
          tmp[0] = area.x;
          tmp[1] = area.y;
          tmp[2] = area.width;
          tmp[3] = area.height;

	  tmp += 4;
        }
      
      tmp_list = tmp_list->next;
    }
  
  meta_error_trap_push (screen->display);
  XChangeProperty (screen->display->xdisplay, screen->xroot,
		   screen->display->atom_net_workarea,
		   XA_CARDINAL, 32, PropModeReplace,
		   (guchar*) data, num_workspaces*4);
  g_free (data);
  return meta_error_trap_pop (screen->display);
}

static gboolean
set_work_area_idle_func (MetaScreen *screen)
{
  meta_topic (META_DEBUG_WORKAREA,
              "Running work area idle function\n");
  
  screen->work_area_idle = 0;
  
  set_work_area_hint (screen);
  
  return FALSE;
}

void
meta_screen_queue_workarea_recalc (MetaScreen *screen)
{
  /* Recompute work area in an idle */
  if (screen->work_area_idle == 0)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Adding work area hint idle function\n");
      screen->work_area_idle =
        g_idle_add_full (META_PRIORITY_WORK_AREA_HINT,
                         (GSourceFunc) set_work_area_idle_func,
                         screen,
                         NULL);
    }
}

void
meta_screen_calc_workspace_layout (MetaScreen *screen,
                                   int         num_workspaces,
                                   int        *r,
                                   int        *c)
{
  int cols, rows;

  /*
   * 3 rows, 4 columns, horizontal layout
   * and starting from top left:
   *  +--+--+--+--+
   *  | 1| 2| 3| 4|
   *  +--+--+--+--+
   *  | 5| 6| 7| 8|
   *  +--+--+--+--+
   *  | 9|10|11|12|
   *  +--+--+--+--+
   *
   * vertical layout:
   *  +--+--+--+--+
   *  | 1| 4| 7|10|
   *  +--+--+--+--+
   *  | 2| 5| 8|11|
   *  +--+--+--+--+
   *  | 3| 6| 9|12|
   *  +--+--+--+--+
   *
   */
     
  rows = screen->rows_of_workspaces;
  cols = screen->columns_of_workspaces;
  if (rows <= 0 && cols <= 0)
    cols = num_workspaces;

  if (rows <= 0)
    rows = num_workspaces / cols + ((num_workspaces % cols) > 0 ? 1 : 0);
  if (cols <= 0)
    cols = num_workspaces / rows + ((num_workspaces % rows) > 0 ? 1 : 0);

  /* paranoia */
  if (rows < 1)
    rows = 1;
  if (cols < 1)
    cols = 1;

  *r = rows;
  *c = cols;
}

void
meta_screen_resize_func (MetaScreen *screen,
                         MetaWindow *window,
                         void       *user_data)
{
  meta_window_queue_move_resize (window);
}

void
meta_screen_resize (MetaScreen *screen,
                    int         width,
                    int         height)
{  
  screen->width = width;
  screen->height = height;

  reload_xinerama_infos (screen);
  
  /* Queue a resize on all the windows */
  meta_screen_foreach_window (screen, meta_screen_resize_func, 0);
}
