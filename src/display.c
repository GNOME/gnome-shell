/* Metacity X display handler */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
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
#include "display.h"
#include "util.h"
#include "main.h"
#include "screen.h"
#include "window.h"
#include "window-props.h"
#include "group-props.h"
#include "frame.h"
#include "errors.h"
#include "keybindings.h"
#include "prefs.h"
#include "resizepopup.h"
#include "workspace.h"
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#ifdef HAVE_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif
#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif
#include <string.h>

#define USE_GDK_DISPLAY

typedef struct 
{
  MetaDisplay *display;
  Window xwindow;
  Time timestamp;
  MetaWindowPingFunc ping_reply_func;
  MetaWindowPingFunc ping_timeout_func;
  void *user_data;
  guint ping_timeout_id;
} MetaPingData;

typedef struct 
{
  MetaDisplay *display;
  Window xwindow;
} MetaAutoRaiseData;

static GSList *all_displays = NULL;

static void   meta_spew_event           (MetaDisplay    *display,
                                         XEvent         *event);
static void   event_queue_callback      (XEvent         *event,
                                         gpointer        data);
static gboolean event_callback          (XEvent         *event,
                                         gpointer        data);
static Window event_get_modified_window (MetaDisplay    *display,
                                         XEvent         *event);
static guint32 event_get_time           (MetaDisplay    *display,
                                         XEvent         *event);
static void    process_pong_message     (MetaDisplay    *display,
                                         XEvent         *event);
static void    process_selection_request (MetaDisplay   *display,
                                          XEvent        *event);
static void    process_selection_clear   (MetaDisplay   *display,
                                          XEvent        *event);

static void    update_window_grab_modifiers (MetaDisplay *display);

static void    prefs_changed_callback    (MetaPreference pref,
                                          void          *data);

static void
set_utf8_string_hint (MetaDisplay *display,
                      Window xwindow,
                      Atom atom,
                      const char *val)
{
  meta_error_trap_push (display);
  XChangeProperty (display->xdisplay, 
                   xwindow, atom,
                   display->atom_utf8_string, 
                   8, PropModeReplace, (guchar*) val, strlen (val));
  meta_error_trap_pop (display, FALSE);
}

static void
ping_data_free (MetaPingData *ping_data)
{
  /* Remove the timeout */
  if (ping_data->ping_timeout_id != 0)
    g_source_remove (ping_data->ping_timeout_id);

  g_free (ping_data);
}

static void
remove_pending_pings_for_window (MetaDisplay *display, Window xwindow)
{
  GSList *tmp;
  GSList *dead;

  /* could obviously be more efficient, don't care */
  
  /* build list to be removed */
  dead = NULL;
  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      if (ping_data->xwindow == xwindow)
        dead = g_slist_prepend (dead, ping_data);
    }

  /* remove what we found */
  for (tmp = dead; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      display->pending_pings = g_slist_remove (display->pending_pings, ping_data);
      ping_data_free (ping_data);
    }

  g_slist_free (dead);
}


#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push (SnDisplay *sn_display,
                    Display   *xdisplay)
{
  MetaDisplay *display;
  display = meta_display_for_x_display (xdisplay);
  if (display != NULL)
    meta_error_trap_push (display);
}

static void
sn_error_trap_pop (SnDisplay *sn_display,
                   Display   *xdisplay)
{
  MetaDisplay *display;
  display = meta_display_for_x_display (xdisplay);
  if (display != NULL)
    meta_error_trap_pop (display, FALSE);
}
#endif

gboolean
meta_display_open (const char *name)
{
  MetaDisplay *display;
  Display *xdisplay;
  GSList *screens;
  GSList *tmp;
  int i;
  /* Remember to edit code that assigns each atom to display struct
   * when adding an atom name here.
   */
  char *atom_names[] = {
    "_NET_WM_NAME",
    "WM_PROTOCOLS",
    "WM_TAKE_FOCUS",
    "WM_DELETE_WINDOW",
    "WM_STATE",
    "_NET_CLOSE_WINDOW",
    "_NET_WM_STATE",
    "_MOTIF_WM_HINTS",
    "_NET_WM_STATE_SHADED",
    "_NET_WM_STATE_MAXIMIZED_HORZ",
    "_NET_WM_STATE_MAXIMIZED_VERT",
    "_NET_WM_DESKTOP",
    "_NET_NUMBER_OF_DESKTOPS",
    "WM_CHANGE_STATE",
    "SM_CLIENT_ID",
    "WM_CLIENT_LEADER",
    "WM_WINDOW_ROLE",
    "_NET_CURRENT_DESKTOP",
    "_NET_SUPPORTING_WM_CHECK",
    "_NET_SUPPORTED",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DESKTOP",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_TOOLBAR",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_STATE_MODAL",
    "_NET_CLIENT_LIST",
    "_NET_CLIENT_LIST_STACKING",
    "_NET_WM_STATE_SKIP_TASKBAR",
    "_NET_WM_STATE_SKIP_PAGER",
    "_WIN_WORKSPACE",
    "_WIN_LAYER",
    "_WIN_PROTOCOLS",
    "_WIN_SUPPORTING_WM_CHECK",
    "_NET_WM_ICON_NAME",
    "_NET_WM_ICON",
    "_NET_WM_ICON_GEOMETRY",
    "UTF8_STRING",
    "WM_ICON_SIZE",
    "_KWM_WIN_ICON",
    "_NET_WM_MOVERESIZE",
    "_NET_ACTIVE_WINDOW",
    "_METACITY_RESTART_MESSAGE",    
    "_NET_WM_STRUT",
    "_WIN_HINTS",
    "_METACITY_RELOAD_THEME_MESSAGE",
    "_METACITY_SET_KEYBINDINGS_MESSAGE",
    "_NET_WM_STATE_HIDDEN",
    "_NET_WM_WINDOW_TYPE_UTILITY",
    "_NET_WM_WINDOW_TYPE_SPLASH",
    "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_PING",
    "_NET_WM_PID",
    "WM_CLIENT_MACHINE",
    "_NET_WORKAREA",
    "_NET_SHOWING_DESKTOP",
    "_NET_DESKTOP_LAYOUT",
    "MANAGER",
    "TARGETS",
    "MULTIPLE",
    "TIMESTAMP",
    "VERSION",
    "ATOM_PAIR",
    "_NET_DESKTOP_NAMES",
    "_NET_WM_ALLOWED_ACTIONS",
    "_NET_WM_ACTION_MOVE",
    "_NET_WM_ACTION_RESIZE",
    "_NET_WM_ACTION_SHADE",
    "_NET_WM_ACTION_STICK",
    "_NET_WM_ACTION_MAXIMIZE_HORZ",
    "_NET_WM_ACTION_MAXIMIZE_VERT",
    "_NET_WM_ACTION_CHANGE_DESKTOP",
    "_NET_WM_ACTION_CLOSE",
    "_NET_WM_STATE_ABOVE",
    "_NET_WM_STATE_BELOW",
    "_NET_STARTUP_ID",
    "_METACITY_TOGGLE_VERBOSE",
    "_METACITY_UPDATE_COUNTER",
    "SYNC_COUNTER"
  };
  Atom atoms[G_N_ELEMENTS(atom_names)];
  
  meta_verbose ("Opening display '%s'\n", XDisplayName (name));

#ifdef USE_GDK_DISPLAY
  xdisplay = meta_ui_get_display (name);
#else
  xdisplay = XOpenDisplay (name);
#endif
  
  if (xdisplay == NULL)
    {
      meta_warning (_("Failed to open X Window System display '%s'\n"),
                    XDisplayName (name));
      return FALSE;
    }

  if (meta_is_syncing ())
    XSynchronize (xdisplay, True);
  
  display = g_new (MetaDisplay, 1);

  display->closing = 0;
  
  /* here we use XDisplayName which is what the user
   * probably put in, vs. DisplayString(display) which is
   * canonicalized by XOpenDisplay()
   */
  display->name = g_strdup (XDisplayName (name));
  display->xdisplay = xdisplay;
  display->error_trap_synced_at_last_pop = TRUE;
  display->error_traps = 0;
  display->error_trap_handler = NULL;
  display->server_grab_count = 0;

  display->pending_pings = NULL;
  display->autoraise_timeout_id = 0;
  display->focus_window = NULL;
  display->expected_focus_window = NULL;
  display->mru_list = NULL;

  /* FIXME copy the checks from GDK probably */
  display->static_gravity_works = g_getenv ("METACITY_USE_STATIC_GRAVITY") != NULL;
  
  /* we have to go ahead and do this so error handlers work */
  all_displays = g_slist_prepend (all_displays, display);

  meta_display_init_keys (display);

  update_window_grab_modifiers (display);

  meta_prefs_add_listener (prefs_changed_callback, display);
  
  XInternAtoms (display->xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);
  display->atom_net_wm_name = atoms[0];
  display->atom_wm_protocols = atoms[1];
  display->atom_wm_take_focus = atoms[2];
  display->atom_wm_delete_window = atoms[3];
  display->atom_wm_state = atoms[4];
  display->atom_net_close_window = atoms[5];
  display->atom_net_wm_state = atoms[6];
  display->atom_motif_wm_hints = atoms[7];
  display->atom_net_wm_state_shaded = atoms[8];
  display->atom_net_wm_state_maximized_horz = atoms[9];
  display->atom_net_wm_state_maximized_vert = atoms[10];
  display->atom_net_wm_desktop = atoms[11];
  display->atom_net_number_of_desktops = atoms[12];
  display->atom_wm_change_state = atoms[13];
  display->atom_sm_client_id = atoms[14];
  display->atom_wm_client_leader = atoms[15];
  display->atom_wm_window_role = atoms[16];
  display->atom_net_current_desktop = atoms[17];
  display->atom_net_supporting_wm_check = atoms[18];
  display->atom_net_supported = atoms[19];
  display->atom_net_wm_window_type = atoms[20];
  display->atom_net_wm_window_type_desktop = atoms[21];
  display->atom_net_wm_window_type_dock = atoms[22];
  display->atom_net_wm_window_type_toolbar = atoms[23];
  display->atom_net_wm_window_type_menu = atoms[24];
  display->atom_net_wm_window_type_dialog = atoms[25];
  display->atom_net_wm_window_type_normal = atoms[26];
  display->atom_net_wm_state_modal = atoms[27];
  display->atom_net_client_list = atoms[28];
  display->atom_net_client_list_stacking = atoms[29];
  display->atom_net_wm_state_skip_taskbar = atoms[30];
  display->atom_net_wm_state_skip_pager = atoms[31];
  display->atom_win_workspace = atoms[32];
  display->atom_win_layer = atoms[33];
  display->atom_win_protocols = atoms[34];
  display->atom_win_supporting_wm_check = atoms[35];
  display->atom_net_wm_icon_name = atoms[36];
  display->atom_net_wm_icon = atoms[37];
  display->atom_net_wm_icon_geometry = atoms[38];
  display->atom_utf8_string = atoms[39];
  display->atom_wm_icon_size = atoms[40];
  display->atom_kwm_win_icon = atoms[41];
  display->atom_net_wm_moveresize = atoms[42];
  display->atom_net_active_window = atoms[43];
  display->atom_metacity_restart_message = atoms[44];
  display->atom_net_wm_strut = atoms[45];
  display->atom_win_hints = atoms[46];
  display->atom_metacity_reload_theme_message = atoms[47];
  display->atom_metacity_set_keybindings_message = atoms[48];
  display->atom_net_wm_state_hidden = atoms[49];
  display->atom_net_wm_window_type_utility = atoms[50];
  display->atom_net_wm_window_type_splash = atoms[51];
  display->atom_net_wm_state_fullscreen = atoms[52];
  display->atom_net_wm_ping = atoms[53];
  display->atom_net_wm_pid = atoms[54];
  display->atom_wm_client_machine = atoms[55];
  display->atom_net_workarea = atoms[56];
  display->atom_net_showing_desktop = atoms[57];
  display->atom_net_desktop_layout = atoms[58];
  display->atom_manager = atoms[59];
  display->atom_targets = atoms[60];
  display->atom_multiple = atoms[61];
  display->atom_timestamp = atoms[62];
  display->atom_version = atoms[63];
  display->atom_atom_pair = atoms[64];
  display->atom_net_desktop_names = atoms[65];
  display->atom_net_wm_allowed_actions = atoms[66];
  display->atom_net_wm_action_move = atoms[67];
  display->atom_net_wm_action_resize = atoms[68];
  display->atom_net_wm_action_shade = atoms[69];
  display->atom_net_wm_action_stick = atoms[70];
  display->atom_net_wm_action_maximize_horz = atoms[71];
  display->atom_net_wm_action_maximize_vert = atoms[72];
  display->atom_net_wm_action_change_desktop = atoms[73];
  display->atom_net_wm_action_close = atoms[74];
  display->atom_net_wm_state_above = atoms[75];
  display->atom_net_wm_state_below = atoms[76];
  display->atom_net_startup_id = atoms[77];
  display->atom_metacity_toggle_verbose = atoms[78];
  display->atom_metacity_update_counter = atoms[79];
  display->atom_sync_counter = atoms[80];
  
  display->prop_hooks = NULL;
  meta_display_init_window_prop_hooks (display);
  display->group_prop_hooks = NULL;
  meta_display_init_group_prop_hooks (display);
  
  /* Offscreen unmapped window used for _NET_SUPPORTING_WM_CHECK,
   * created in screen_new
   */
  display->leader_window = None;
  display->no_focus_window = None;

  display->xinerama_cache_invalidated = TRUE;

  display->groups_by_leader = NULL;

  display->window_with_menu = NULL;
  display->window_menu = NULL;
  
  display->screens = NULL;
  
#ifdef HAVE_STARTUP_NOTIFICATION
  display->sn_display = sn_display_new (display->xdisplay,
                                        sn_error_trap_push,
                                        sn_error_trap_pop);
#endif
  
#ifdef USE_GDK_DISPLAY
  display->events = NULL;

  /* Get events */
  meta_ui_add_event_func (display->xdisplay,
                          event_callback,
                          display);
#else
  display->events = meta_event_queue_new (display->xdisplay,
                                          event_queue_callback,
                                          display);
#endif
  
  display->window_ids = g_hash_table_new (meta_unsigned_long_hash,
                                          meta_unsigned_long_equal);
  
  display->double_click_time = 250;
  display->last_button_time = 0;
  display->last_button_xwindow = None;
  display->last_button_num = 0;
  display->is_double_click = FALSE;

  i = 0;
  while (i < N_IGNORED_SERIALS)
    {
      display->ignored_serials[i] = 0;
      ++i;
    }
  display->ungrab_should_not_cause_focus_window = None;
  
  display->current_time = CurrentTime;
  
  display->grab_op = META_GRAB_OP_NONE;
  display->grab_window = NULL;
  display->grab_screen = NULL;
  display->grab_resize_popup = NULL;

#ifdef HAVE_XSYNC
  {
    int major, minor;
    
    display->xsync_error_base = 0;
    display->xsync_event_base = 0;

    /* I don't think we really have to fill these in */
    major = SYNC_MAJOR_VERSION;
    minor = SYNC_MINOR_VERSION;
    
    if (!XSyncQueryExtension (display->xdisplay,
                              &display->xsync_event_base,
                              &display->xsync_error_base) ||
        !XSyncInitialize (display->xdisplay,
                          &major, &minor))
      {
        display->xsync_error_base = 0;
        display->xsync_event_base = 0;
      }
    meta_verbose ("Attempted to init Xsync, found version %d.%d error base %d event base %d\n",
                  major, minor,
                  display->xsync_error_base,
                  display->xsync_event_base);
  }
#else  /* HAVE_XSYNC */
  meta_verbose ("Not compiled with Xsync support\n");
#endif /* !HAVE_XSYNC */


#ifdef HAVE_SHAPE
  {
    display->shape_error_base = 0;
    display->shape_event_base = 0;
    
    if (!XShapeQueryExtension (display->xdisplay,
                               &display->shape_event_base,
                               &display->shape_error_base))
      {
        display->shape_error_base = 0;
        display->shape_event_base = 0;
      }
    meta_verbose ("Attempted to init Shape, found error base %d event base %d\n",
                  display->shape_error_base,
                  display->shape_event_base);
  }
#else  /* HAVE_SHAPE */
  meta_verbose ("Not compiled with Shape support\n");
#endif /* !HAVE_SHAPE */
  
  screens = NULL;
  
#ifdef HAVE_GTK_MULTIHEAD  
  i = 0;
  while (i < ScreenCount (xdisplay))
    {
      MetaScreen *screen;

      screen = meta_screen_new (display, i);

      if (screen)
        screens = g_slist_prepend (screens, screen);
      ++i;
    }
#else
  {
    MetaScreen *screen;
    screen = meta_screen_new (display, DefaultScreen (xdisplay));
    if (screen)
      screens = g_slist_prepend (screens, screen);
  }
#endif
  
  display->screens = screens;
  
  if (screens == NULL)
    {
      /* This would typically happen because all the screens already
       * have window managers.
       */
      meta_display_close (display);
      return FALSE;
    }

  /* display->leader_window was created as a side effect of
   * initializing the screens
   */
  
  set_utf8_string_hint (display,
                        display->leader_window,
                        display->atom_net_wm_name,
                        "Metacity");

  {
    /* The legacy GNOME hint is to set a cardinal which is the window
     * id of the supporting_wm_check window on the supporting_wm_check
     * window itself
     */
    gulong data[1];

    data[0] = display->leader_window;
    XChangeProperty (display->xdisplay,
                     display->leader_window,
                     display->atom_win_supporting_wm_check,
                     XA_CARDINAL,
                     32, PropModeReplace, (guchar*) data, 1);

    XChangeProperty (display->xdisplay,
                     display->leader_window,
                     display->atom_net_supporting_wm_check,
                     XA_WINDOW,
                     32, PropModeReplace, (guchar*) data, 1);
  }
  
  meta_display_grab (display);
  
  /* Now manage all existing windows */
  tmp = display->screens;
  while (tmp != NULL)
    {
      meta_screen_manage_all_windows (tmp->data);
      tmp = tmp->next;
    }

  {
    Window focus;
    int ret_to;

    /* kinda bogus because GetInputFocus has no possible errors */
    meta_error_trap_push (display);

    focus = None;
    ret_to = RevertToPointerRoot;
    XGetInputFocus (display->xdisplay, &focus, &ret_to);

    /* Force a new FocusIn (does this work?) */
    XSetInputFocus (display->xdisplay, focus, ret_to, CurrentTime);
    
    meta_error_trap_pop (display, FALSE);
  }
  
  meta_display_ungrab (display);  
  
  return TRUE;
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;

  listp = data;
  *listp = g_slist_prepend (*listp, value);
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

GSList*
meta_display_list_windows (MetaDisplay *display)
{
  GSList *winlist;
  GSList *tmp;
  GSList *prev;
  
  winlist = NULL;
  g_hash_table_foreach (display->window_ids,
                        listify_func,
                        &winlist);

  /* Uniquify the list, since both frame windows and plain
   * windows are in the hash
   */
  winlist = g_slist_sort (winlist, ptrcmp);

  prev = NULL;
  tmp = winlist;
  while (tmp != NULL)
    {
      GSList *next;

      next = tmp->next;
      
      if (next &&
          next->data == tmp->data)
        {
          /* Delete tmp from list */

          if (prev)
            prev->next = next;

          if (tmp == winlist)
            winlist = next;
          
          g_slist_free_1 (tmp);

          /* leave prev unchanged */
        }
      else
        {
          prev = tmp;
        }
      
      tmp = next;
    }

  return winlist;
}

void
meta_display_close (MetaDisplay *display)
{
  GSList *tmp;
  
  if (display->error_traps > 0)
    meta_bug ("Display closed with error traps pending\n");

  display->closing += 1;

  meta_prefs_remove_listener (prefs_changed_callback, display);
  
  if (display->autoraise_timeout_id != 0)
    {
      g_source_remove (display->autoraise_timeout_id);
      display->autoraise_timeout_id = 0;
    }
  
#ifdef USE_GDK_DISPLAY
  /* Stop caring about events */
  meta_ui_remove_event_func (display->xdisplay,
                             event_callback,
                             display);
#endif
  
  /* Free all screens */
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
      meta_screen_free (screen);
      tmp = tmp->next;
    }

  g_slist_free (display->screens);
  display->screens = NULL;

#ifdef HAVE_STARTUP_NOTIFICATION
  if (display->sn_display)
    {
      sn_display_unref (display->sn_display);
      display->sn_display = NULL;
    }
#endif
  
  /* Must be after all calls to meta_window_free() since they
   * unregister windows
   */
  g_hash_table_destroy (display->window_ids);

  if (display->leader_window != None)
    XDestroyWindow (display->xdisplay, display->leader_window);

  XFlush (display->xdisplay);

  meta_display_free_window_prop_hooks (display);
  meta_display_free_group_prop_hooks (display);
  
#ifndef USE_GDK_DISPLAY
  meta_event_queue_free (display->events);
  XCloseDisplay (display->xdisplay);
#endif 
  g_free (display->name);

  all_displays = g_slist_remove (all_displays, display);

  meta_display_shutdown_keys (display);
  
  g_free (display);

  if (all_displays == NULL)
    {
      meta_verbose ("Last display closed, exiting\n");
      meta_quit (META_EXIT_SUCCESS);
    }
}

MetaScreen*
meta_display_screen_for_root (MetaDisplay *display,
                              Window       xroot)
{
  GSList *tmp;

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      if (xroot == screen->xroot)
        return screen;

      tmp = tmp->next;
    }

  return NULL;
}

MetaScreen*
meta_display_screen_for_xwindow (MetaDisplay *display,
                                 Window       xwindow)
{
  XWindowAttributes attr;
  int result;
  
  meta_error_trap_push (display);
  attr.screen = NULL;
  result = XGetWindowAttributes (display->xdisplay, xwindow, &attr);
  meta_error_trap_pop (display, TRUE);

  /* Note, XGetWindowAttributes is on all kinds of crack
   * and returns 1 on success 0 on failure, rather than Success
   * on success.
   */
  if (result == 0 || attr.screen == NULL)
    return NULL;
  
  return meta_display_screen_for_x_screen (display, attr.screen);
}

MetaScreen*
meta_display_screen_for_x_screen (MetaDisplay *display,
                                  Screen      *xscreen)
{
  GSList *tmp;

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      if (xscreen == screen->xscreen)
        return screen;

      tmp = tmp->next;
    }

  return NULL;
}

/* Grab/ungrab routines taken from fvwm */
void
meta_display_grab (MetaDisplay *display)
{
  if (display->server_grab_count == 0)
    {
      XGrabServer (display->xdisplay);
    }
  display->server_grab_count += 1;
  meta_verbose ("Grabbing display, grab count now %d\n",
                display->server_grab_count);
}

void
meta_display_ungrab (MetaDisplay *display)
{
  if (display->server_grab_count == 0)
    meta_bug ("Ungrabbed non-grabbed server\n");
  
  display->server_grab_count -= 1;
  if (display->server_grab_count == 0)
    {
      /* FIXME we want to purge all pending "queued" stuff
       * at this point, such as window hide/show
       */
      XUngrabServer (display->xdisplay);
      XFlush (display->xdisplay);
    }

  meta_verbose ("Ungrabbing display, grab count now %d\n",
                display->server_grab_count);
}

MetaDisplay*
meta_display_for_x_display (Display *xdisplay)
{
  GSList *tmp;

  tmp = all_displays;
  while (tmp != NULL)
    {
      MetaDisplay *display = tmp->data;

      if (display->xdisplay == xdisplay)
        return display;

      tmp = tmp->next;
    }

  meta_warning ("Could not find display for X display %p, probably going to crash\n",
                xdisplay);
  
  return NULL;
}

GSList*
meta_displays_list (void)
{
  return all_displays;
}

gboolean
meta_display_is_double_click (MetaDisplay *display)
{
  return display->is_double_click;
}

static gboolean dump_events = TRUE;


static void
event_queue_callback (XEvent         *event,
                      gpointer        data)
{
  event_callback (event, data);
}

static gboolean
grab_op_is_mouse (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:      
    case META_GRAB_OP_RESIZING_SW:      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_MOVING:
      return TRUE;
      break;

    default:
      return FALSE;
    }
}

static gboolean
grab_op_is_keyboard (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_KEYBOARD_MOVING:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
    case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
    case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
    case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
    case META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING:
      return TRUE;
      break;

    default:
      return FALSE;
    }
}

gboolean
meta_grab_op_is_resizing (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:      
    case META_GRAB_OP_RESIZING_SW:      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      return TRUE;
      break;

    default:
      return FALSE;
    }
}

gboolean
meta_grab_op_is_moving (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_KEYBOARD_MOVING:
      return TRUE;
      break;
      
    default:
      return FALSE;
    }
}

/* Get time of current event, or CurrentTime if none. */
guint32
meta_display_get_current_time (MetaDisplay *display)
{
  return display->current_time;
}

static void
add_ignored_serial (MetaDisplay  *display,
                    unsigned long serial)
{
  int i;

  /* don't add the same serial more than once */
  if (display->ignored_serials[N_IGNORED_SERIALS-1] == serial)
    return;
  
  /* shift serials to the left */
  i = 0;
  while (i < (N_IGNORED_SERIALS - 1))
    {
      display->ignored_serials[i] = display->ignored_serials[i+1];
      ++i;
    }
  /* put new one on the end */
  display->ignored_serials[i] = serial;
}

static gboolean
serial_is_ignored (MetaDisplay  *display,
                   unsigned long serial)
{
  int i;

  i = 0;
  while (i < N_IGNORED_SERIALS)
    {
      if (display->ignored_serials[i] == serial)
        return TRUE;
      ++i;
    }
  return FALSE;
}

static void
reset_ignores (MetaDisplay *display)
{
  int i;

  i = 0;
  while (i < N_IGNORED_SERIALS)
    {
      display->ignored_serials[i] = 0;
      ++i;
    }

  display->ungrab_should_not_cause_focus_window = None;
}

static gboolean 
window_raise_with_delay_callback (void *data)
{
  MetaWindow *window;
  MetaAutoRaiseData *auto_raise;

  auto_raise = data;

  meta_topic (META_DEBUG_FOCUS, 
	      "In autoraise callback for window 0x%lx\n", 
	      auto_raise->xwindow);

  auto_raise->display->autoraise_timeout_id = 0;

  window  = meta_display_lookup_x_window (auto_raise->display, 
					  auto_raise->xwindow);
  
  if (window == NULL) 
    return FALSE;

  /* If we aren't already on top, check whether the pointer is inside
   * the window and raise the window if so.
   */      
  if (meta_stack_get_top (window->screen->stack) != window) 
    {
      int x, y, root_x, root_y;
      Window root, child;
      unsigned int mask;
      gboolean same_screen;

      meta_error_trap_push (window->display);
      same_screen = XQueryPointer (window->display->xdisplay,
				   window->xwindow,
				   &root, &child,
				   &root_x, &root_y, &x, &y, &mask);
      meta_error_trap_pop (window->display, TRUE);

      if ((window->frame && POINT_IN_RECT (root_x, root_y, window->frame->rect)) ||
	  (window->frame == NULL && POINT_IN_RECT (root_x, root_y, window->rect)))
	meta_window_raise (window);
      else
	meta_topic (META_DEBUG_FOCUS, 
		    "Pointer not inside window, not raising %s\n", 
		    window->desc);
    }

  return FALSE;
}

static gboolean
event_callback (XEvent   *event,
                gpointer  data)
{
  MetaWindow *window;
  MetaDisplay *display;
  Window modified;
  gboolean frame_was_receiver;
  gboolean filter_out_event;
  
  display = data;
  
  if (dump_events)
    meta_spew_event (display, event);

#ifdef HAVE_STARTUP_NOTIFICATION
  sn_display_process_event (display->sn_display, event);
#endif
  
  filter_out_event = FALSE;
  display->current_time = event_get_time (display, event);
  display->xinerama_cache_invalidated = TRUE;
  
  modified = event_get_modified_window (display, event);
  
  if (event->type == ButtonPress)
    {
      /* filter out scrollwheel */
      if (event->xbutton.button == 4 ||
	  event->xbutton.button == 5)
	return FALSE;

      /* mark double click events, kind of a hack, oh well. */
      if (((int)event->xbutton.button) ==  display->last_button_num &&
          event->xbutton.window == display->last_button_xwindow &&
          event->xbutton.time < (display->last_button_time + display->double_click_time))
        {
          display->is_double_click = TRUE;
          meta_topic (META_DEBUG_EVENTS,
                      "This was the second click of a double click\n");

        }
      else
        {
          display->is_double_click = FALSE;
        }

      display->last_button_num = event->xbutton.button;
      display->last_button_xwindow = event->xbutton.window;
      display->last_button_time = event->xbutton.time;
    }
  else if (event->type == UnmapNotify)
    {
      if (meta_ui_window_should_not_cause_focus (display->xdisplay,
                                                 modified))
        {
          add_ignored_serial (display, event->xany.serial);
          meta_topic (META_DEBUG_FOCUS,
                      "Adding EnterNotify serial %lu to ignored focus serials\n",
                      event->xany.serial);
        }
    }
  else if (event->type == LeaveNotify &&
           event->xcrossing.mode == NotifyUngrab &&
           modified == display->ungrab_should_not_cause_focus_window)
    {
      add_ignored_serial (display, event->xany.serial);
      meta_topic (META_DEBUG_FOCUS,
                  "Adding LeaveNotify serial %lu to ignored focus serials\n",
                  event->xany.serial);
    }

  if (modified != None)
    window = meta_display_lookup_x_window (display, modified);
  else
    window = NULL;

  frame_was_receiver = FALSE;
  if (window &&
      window->frame &&
      modified == window->frame->xwindow)
    {
      /* Note that if the frame and the client both have an
       * XGrabButton (as is normal with our setup), the event
       * goes to the frame.
       */
      frame_was_receiver = TRUE;
      meta_topic (META_DEBUG_EVENTS, "Frame was receiver of event\n");
    }

#ifdef HAVE_XSYNC
  if (META_DISPLAY_HAS_XSYNC (display) && 
      event->type == (display->xsync_event_base + XSyncAlarmNotify) &&
      ((XSyncAlarmNotifyEvent*)event)->alarm == display->grab_update_alarm)
    {
      filter_out_event = TRUE; /* GTK doesn't want to see this really */
      
      if (display->grab_op != META_GRAB_OP_NONE &&
          display->grab_window != NULL &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (display->grab_window, event);
    }
#endif /* HAVE_XSYNC */

#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (display) && 
      event->type == (display->shape_event_base + ShapeNotify))
    {
      filter_out_event = TRUE; /* GTK doesn't want to see this really */
      
      if (window && !frame_was_receiver)
        {
          XShapeEvent *sev = (XShapeEvent*) event;

          if (sev->kind == ShapeBounding)
            {
              if (sev->shaped && !window->has_shape)
                {
                  window->has_shape = TRUE;                  
                  meta_topic (META_DEBUG_SHAPES,
                              "Window %s now has a shape\n",
                              window->desc);
                }
              else if (!sev->shaped && window->has_shape)
                {
                  window->has_shape = FALSE;
                  meta_topic (META_DEBUG_SHAPES,
                              "Window %s no longer has a shape\n",
                              window->desc);
                }
              else
                {
                  meta_topic (META_DEBUG_SHAPES,
                              "Window %s shape changed\n",
                              window->desc);
                }

              if (window->frame)
                {
                  window->frame->need_reapply_frame_shape = TRUE;
                  meta_window_queue_move_resize (window);
                }
            }
        }
      else
        {
          meta_topic (META_DEBUG_SHAPES,
                      "ShapeNotify not on a client window (window %s frame_was_receiver = %d)\n",
                      window ? window->desc : "(none)",
                      frame_was_receiver);
        }
    }
#endif /* HAVE_SHAPE */
  
  switch (event->type)
    {
    case KeyPress:
    case KeyRelease:
      meta_display_process_key_event (display, window, event);
      break;
    case ButtonPress:
      if ((window &&
           grab_op_is_mouse (display->grab_op) &&
           display->grab_button != (int) event->xbutton.button && 
           display->grab_window == window) ||
          grab_op_is_keyboard (display->grab_op))
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "Ending grab op %d on window %s due to button press\n",
                      display->grab_op,
                      (display->grab_window ?
                       display->grab_window->desc : 
                       "none"));
          meta_display_end_grab_op (display,
                                    event->xbutton.time);
        }
      else if (window && display->grab_op == META_GRAB_OP_NONE)
        {
          gboolean begin_move = FALSE;
          unsigned int grab_mask;
          gboolean unmodified;

          grab_mask = display->window_grab_modifiers;
          if (g_getenv ("METACITY_DEBUG_BUTTON_GRABS"))
            grab_mask |= ControlMask;

          /* Two possible sources of an unmodified event; one is a
           * client that's letting button presses pass through to the
           * frame, the other is our focus_window_grab on unmodified
           * button 1.  So for all such events we focus the window.
           */
          unmodified = (event->xbutton.state & grab_mask) == 0;
          
          if (unmodified ||
              event->xbutton.button == 1)
            {
              if (!frame_was_receiver)
                {
                  /* don't focus if frame received, will be lowered in
                   * frames.c or special-cased if the click was on a
                   * minimize/close button.
                   */
                  meta_window_raise (window);
                  
                  meta_topic (META_DEBUG_FOCUS,
                              "Focusing %s due to unmodified button %d press (display.c)\n",
                              window->desc, event->xbutton.button);
                  meta_window_focus (window, event->xbutton.time);
                }
              
              /* you can move on alt-click but not on
               * the click-to-focus
               */
              if (!unmodified)
                begin_move = TRUE;
            }
          else if (!unmodified && event->xbutton.button == 2)
            {
              if (window->has_resize_func)
                {
                  gboolean north;
                  gboolean west;
                  int root_x, root_y;
                  MetaGrabOp op;

                  meta_window_get_position (window, &root_x, &root_y);

                  west = event->xbutton.x_root < (root_x + window->rect.width / 2);
                  north = event->xbutton.y_root < (root_y + window->rect.height / 2);

                  if (west && north)
                    op = META_GRAB_OP_RESIZING_NW;
                  else if (west)
                    op = META_GRAB_OP_RESIZING_SW;
                  else if (north)
                    op = META_GRAB_OP_RESIZING_NE;
                  else
                    op = META_GRAB_OP_RESIZING_SE;
                  
                  meta_display_begin_grab_op (display,
                                              window->screen,
                                              window,
                                              op,
                                              TRUE,
                                              event->xbutton.button,
                                              0,
                                              event->xbutton.time,
                                              event->xbutton.x_root,
                                              event->xbutton.y_root);
                }
            }
          else if (event->xbutton.button == 3)
            {
              meta_window_show_menu (window,
                                     event->xbutton.x_root,
                                     event->xbutton.y_root,
                                     event->xbutton.button,
                                     event->xbutton.time);
            }

          if (!frame_was_receiver && unmodified)
            {
              /* This is from our synchronous grab since
               * it has no modifiers and was on the client window
               */
              int mode;
              
              /* When clicking a different app in click-to-focus
               * in application-based mode, and the different
               * app is not a dock or desktop, eat the focus click.
               */
              if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK &&
                  meta_prefs_get_application_based () &&
                  !window->has_focus &&
                  window->type != META_WINDOW_DOCK &&
                  window->type != META_WINDOW_DESKTOP &&
                  (display->focus_window == NULL ||
                   !meta_window_same_application (window,
                                                  display->focus_window)))
                mode = AsyncPointer; /* eat focus click */
              else
                mode = ReplayPointer; /* give event back */
              
              XAllowEvents (display->xdisplay,
                            mode, event->xbutton.time);
            }
          
          if (begin_move && window->has_move_func)
            {
              meta_display_begin_grab_op (display,
                                          window->screen,
                                          window,
                                          META_GRAB_OP_MOVING,
                                          TRUE,
                                          event->xbutton.button,
                                          0,
                                          event->xbutton.time,
                                          event->xbutton.x_root,
                                          event->xbutton.y_root);
            }
        }
      break;
    case ButtonRelease:
      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (window, event);
      break;
    case MotionNotify:
      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (window, event);
      break;
    case EnterNotify:
      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (window, event);
      /* do this even if window->has_focus to avoid races */
      else if (window && !serial_is_ignored (display, event->xany.serial) &&
               event->xcrossing.detail != NotifyInferior)
        {
          switch (meta_prefs_get_focus_mode ())
            {
            case META_FOCUS_MODE_SLOPPY:
            case META_FOCUS_MODE_MOUSE:
              if (window->type != META_WINDOW_DOCK &&
                  window->type != META_WINDOW_DESKTOP)
                {
                  meta_topic (META_DEBUG_FOCUS,
                              "Focusing %s due to enter notify with serial %lu\n",
                              window->desc, event->xany.serial);

		  meta_window_focus (window, event->xcrossing.time);

		  /* stop ignoring stuff */
		  reset_ignores (display);
		  
		  if (meta_prefs_get_auto_raise ()) 
		    {
		      MetaAutoRaiseData *auto_raise_data;

		      meta_topic (META_DEBUG_FOCUS, 
				  "Queuing an autoraise timeout for %s with delay %d\n", 
				  window->desc, 
				  meta_prefs_get_auto_raise_delay ());
		      
		      auto_raise_data = g_new (MetaAutoRaiseData, 1);
		      auto_raise_data->display = window->display;
		      auto_raise_data->xwindow = window->xwindow;
		      
		      if (display->autoraise_timeout_id != 0)
			g_source_remove (display->autoraise_timeout_id);

		      display->autoraise_timeout_id = 
			g_timeout_add_full (G_PRIORITY_DEFAULT,
					    meta_prefs_get_auto_raise_delay (),
					    window_raise_with_delay_callback,
					    auto_raise_data,
					    g_free);
		    }
		  else
		    {
		      meta_topic (META_DEBUG_FOCUS,
				  "Auto raise is disabled\n");		      
		    }
                }
              break;
            case META_FOCUS_MODE_CLICK:
              break;
            }
          
          if (window->type == META_WINDOW_DOCK)
            meta_window_raise (window);
        }
      break;
    case LeaveNotify:
      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (window, event);
      else if (window != NULL)
        {
          switch (meta_prefs_get_focus_mode ())
            {
            case META_FOCUS_MODE_MOUSE:
              /* This is kind of questionable; but we normally
               * set focus to RevertToPointerRoot, so I guess
               * leaving it on PointerRoot when nothing is focused
               * is probably right. Anyway, unfocus the
               * focused window.
               */
              if (window->has_focus &&
		  event->xcrossing.mode != NotifyGrab && 
		  event->xcrossing.mode != NotifyUngrab &&
		  event->xcrossing.detail != NotifyInferior)
                {
                  meta_verbose ("Unsetting focus from %s due to LeaveNotify\n",
                                window->desc);
                  XSetInputFocus (display->xdisplay,
                                  display->no_focus_window,
                                  RevertToPointerRoot,
                                  event->xcrossing.time);
                }
              break;
            case META_FOCUS_MODE_SLOPPY:
            case META_FOCUS_MODE_CLICK:
              break;
            }
          
          if (window->type == META_WINDOW_DOCK &&
              event->xcrossing.mode != NotifyGrab &&
              event->xcrossing.mode != NotifyUngrab &&
              !window->has_focus)
            meta_window_lower (window);
        }
      break;
    case FocusIn:
    case FocusOut:
      if (window)
        {
          meta_window_notify_focus (window, event);
        }
      else if (event->xany.window == display->no_focus_window)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focus %s event received on no_focus_window 0x%lx "
                      "mode %s detail %s\n",
                      event->type == FocusIn ? "in" :
                      event->type == FocusOut ? "out" :
                      "???",
                      event->xany.window,
                      meta_event_mode_to_string (event->xfocus.mode),
                      meta_event_detail_to_string (event->xfocus.mode));
        }
      else if (meta_display_screen_for_root (display,
                                             event->xany.window) != NULL)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focus %s event received on root window 0x%lx "
                      "mode %s detail %s\n",
                      event->type == FocusIn ? "in" :
                      event->type == FocusOut ? "out" :
                      "???",
                      event->xany.window,
                      meta_event_mode_to_string (event->xfocus.mode),
                      meta_event_detail_to_string (event->xfocus.mode));
        }
      break;
    case KeymapNotify:
      break;
    case Expose:
      break;
    case GraphicsExpose:
      break;
    case NoExpose:
      break;
    case VisibilityNotify:
      break;
    case CreateNotify:
      break;
    case DestroyNotify:
      if (window)
        {
          if (display->grab_op != META_GRAB_OP_NONE &&
              display->grab_window == window)
            meta_display_end_grab_op (display, CurrentTime);
          
          if (frame_was_receiver)
            {
              meta_warning ("Unexpected destruction of frame 0x%lx, not sure if this should silently fail or be considered a bug\n",
                            window->frame->xwindow);
              meta_error_trap_push (display);
              meta_window_destroy_frame (window->frame->window);
              meta_error_trap_pop (display, FALSE);
            }
          else
            {
              meta_window_free (window); /* Unmanage destroyed window */
            }
        }
      break;
    case UnmapNotify:
      if (window)
        {
          if (display->grab_op != META_GRAB_OP_NONE &&
              display->grab_window == window)
            meta_display_end_grab_op (display, CurrentTime);      
      
          if (!frame_was_receiver)
            {
              if (window->unmaps_pending == 0)
                {
                  meta_topic (META_DEBUG_WINDOW_STATE,
                              "Window %s withdrawn\n",
                              window->desc);
                  window->withdrawn = TRUE;
                  meta_window_free (window); /* Unmanage withdrawn window */
                  window = NULL;
                }
              else
                {
                  window->unmaps_pending -= 1;
                  meta_topic (META_DEBUG_WINDOW_STATE,
                              "Received pending unmap, %d now pending\n",
                              window->unmaps_pending);
                }
            }

          /* Unfocus on UnmapNotify, do this after the possible
           * window_free above so that window_free can see if window->has_focus
           * and move focus to another window
           */
          if (window)
            meta_window_notify_focus (window, event);
        }
      break;
    case MapNotify:
      break;
    case MapRequest:
      if (window == NULL)
        {
          window = meta_window_new (display, event->xmaprequest.window,
                                    FALSE);
        }
      /* if frame was receiver it's some malicious send event or something */
      else if (!frame_was_receiver && window)        
        {
          meta_verbose ("MapRequest on %s mapped = %d minimized = %d\n",
                        window->desc, window->mapped, window->minimized);
          if (window->minimized)
            {
              meta_window_unminimize (window);
              if (!meta_workspace_contains_window (window->screen->active_workspace,
                                                   window))
                {
                  meta_verbose ("Changing workspace due to MapRequest mapped = %d minimized = %d\n",
                                window->mapped, window->minimized);
                  meta_window_change_workspace (window,
                                                window->screen->active_workspace);
                }
            }
        }
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:
      /* Handle screen resize */
      {
	MetaScreen *screen;
        
        screen = meta_display_screen_for_root (display,
                                               event->xconfigure.window);

	if (screen != NULL)
          {
#ifdef HAVE_RANDR
            /* do the resize the official way */
            XRRUpdateConfiguration (event);
#else
            /* poke around in Xlib */
            screen->xscreen->width   = event->xconfigure.width;
            screen->xscreen->height  = event->xconfigure.height;
#endif
            
            meta_screen_resize (screen, 
                                event->xconfigure.width,
                                event->xconfigure.height);
          }
      }
      break;
    case ConfigureRequest:
      /* This comment and code is found in both twm and fvwm */
      /*
       * According to the July 27, 1988 ICCCM draft, we should ignore size and
       * position fields in the WM_NORMAL_HINTS property when we map a window.
       * Instead, we'll read the current geometry.  Therefore, we should respond
       * to configuration requests for windows which have never been mapped.
       */
      if (window == NULL)
        {
          unsigned int xwcm;
          XWindowChanges xwc;
          
          xwcm = event->xconfigurerequest.value_mask &
            (CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

          xwc.x = event->xconfigurerequest.x;
          xwc.y = event->xconfigurerequest.y;
          xwc.width = event->xconfigurerequest.width;
          xwc.height = event->xconfigurerequest.height;
          xwc.border_width = event->xconfigurerequest.border_width;

          meta_verbose ("Configuring withdrawn window to %d,%d %dx%d border %d (some values may not be in mask)\n",
                        xwc.x, xwc.y, xwc.width, xwc.height, xwc.border_width);
          meta_error_trap_push (display);
          XConfigureWindow (display->xdisplay, event->xconfigurerequest.window,
                            xwcm, &xwc);
          meta_error_trap_pop (display, FALSE);
        }
      else
        {
          if (!frame_was_receiver)
            meta_window_configure_request (window, event);
        }
      break;
    case GravityNotify:
      break;
    case ResizeRequest:
      break;
    case CirculateNotify:
      break;
    case CirculateRequest:
      break;
    case PropertyNotify:
      {
        MetaGroup *group;
        MetaScreen *screen;
        
        if (window && !frame_was_receiver)
          meta_window_property_notify (window, event);

        group = meta_display_lookup_group (display,
                                           event->xproperty.window);
        if (group != NULL)
          meta_group_property_notify (group, event);
        
        screen = NULL;
        if (window == NULL &&
            group == NULL) /* window/group != NULL means it wasn't a root window */
          screen = meta_display_screen_for_root (display,
                                                 event->xproperty.window);
            
        if (screen != NULL)
          {
            if (event->xproperty.atom ==
                display->atom_net_desktop_layout)
              meta_screen_update_workspace_layout (screen);
            else if (event->xproperty.atom ==
                     display->atom_net_desktop_names)
              meta_screen_update_workspace_names (screen);
          }
      }
      break;
    case SelectionClear:
      /* do this here instead of at end of function
       * so we can return
       */
      display->current_time = CurrentTime;
      process_selection_clear (display, event);
      /* Note that processing that may have resulted in
       * closing the display... so return right away.
       */
      return FALSE;
      break;
    case SelectionRequest:
      process_selection_request (display, event);
      break;
    case SelectionNotify:
      break;
    case ColormapNotify:
      if (window && !frame_was_receiver)
        window->colormap = event->xcolormap.colormap;
      break;
    case ClientMessage:
      if (window)
        {
          if (!frame_was_receiver)
            meta_window_client_message (window, event);
        }
      else
        {
          MetaScreen *screen;

          screen = meta_display_screen_for_root (display,
                                                 event->xclient.window);
          
          if (screen)
            {
              if (event->xclient.message_type ==
                  display->atom_net_current_desktop)
                {
                  int space;
                  MetaWorkspace *workspace;
              
                  space = event->xclient.data.l[0];
              
                  meta_verbose ("Request to change current workspace to %d\n",
                                space);
              
                  workspace =
                    meta_screen_get_workspace_by_index (screen,
                                                        space);

                  if (workspace)
                    meta_workspace_activate (workspace);
                  else
                    meta_verbose ("Don't know about workspace %d\n", space);
                }
              else if (event->xclient.message_type ==
                       display->atom_net_number_of_desktops)
                {
                  int num_spaces;
              
                  num_spaces = event->xclient.data.l[0];
              
                  meta_verbose ("Request to set number of workspaces to %d\n",
                                num_spaces);

                  meta_prefs_set_num_workspaces (num_spaces);
                }
	      else if (event->xclient.message_type ==
		       display->atom_net_showing_desktop)
		{
		  gboolean showing_desktop;
                  
		  showing_desktop = event->xclient.data.l[0] != 0;
		  meta_verbose ("Request to %s desktop\n", showing_desktop ? "show" : "hide");
                  
		  if (showing_desktop)
		    meta_screen_show_desktop (screen);
		  else
		    meta_screen_unshow_desktop (screen);
		}
              else if (event->xclient.message_type ==
                       display->atom_metacity_restart_message)
                {
                  meta_verbose ("Received restart request\n");
                  meta_restart ();
                }
              else if (event->xclient.message_type ==
                       display->atom_metacity_reload_theme_message)
                {
                  meta_verbose ("Received reload theme request\n");
                  meta_ui_set_current_theme (meta_prefs_get_theme (),
                                             TRUE);
                  meta_display_retheme_all ();
                }
              else if (event->xclient.message_type ==
                       display->atom_metacity_set_keybindings_message)
                {
                  meta_verbose ("Received set keybindings request = %d\n",
                                (int) event->xclient.data.l[0]);
                  meta_set_keybindings_disabled (!event->xclient.data.l[0]);
                }
              else if (event->xclient.message_type ==
                       display->atom_metacity_toggle_verbose)
                {
                  meta_verbose ("Received toggle verbose message\n");
                  meta_set_verbose (!meta_is_verbose ());
                }
	      else if (event->xclient.message_type ==
		       display->atom_wm_protocols) 
		{
                  meta_verbose ("Received WM_PROTOCOLS message\n");
                  
		  if ((Atom)event->xclient.data.l[0] == display->atom_net_wm_ping)
                    {
                      process_pong_message (display, event);

                      /* We don't want ping reply events going into
                       * the GTK+ event loop because gtk+ will treat
                       * them as ping requests and send more replies.
                       */
                      filter_out_event = TRUE;
                    }
		}
            }
        }
      break;
    case MappingNotify:
      {
        gboolean ignore_current;

        ignore_current = FALSE;
        
        /* Check whether the next event is an identical MappingNotify
         * event.  If it is, ignore the current event, we'll update
         * when we get the next one.
         */
	if (XPending (display->xdisplay))
          {
            XEvent next_event;
            
            XPeekEvent (display->xdisplay, &next_event);
            
            if (next_event.type == MappingNotify &&
                next_event.xmapping.request == event->xmapping.request)
              ignore_current = TRUE;
          }

        if (!ignore_current)
          {
            /* Let XLib know that there is a new keyboard mapping.
             */
            XRefreshKeyboardMapping (&event->xmapping);
            meta_display_process_mapping_event (display, event);
          }
      }
      break;
    default:
      break;
    }

  display->current_time = CurrentTime;
  return filter_out_event;
}

/* Return the window this has to do with, if any, rather
 * than the frame or root window that was selecting
 * for substructure
 */
static Window
event_get_modified_window (MetaDisplay *display,
                           XEvent *event)
{
  switch (event->type)
    {
    case KeyPress:
    case KeyRelease:
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
    case FocusIn:
    case FocusOut:
    case KeymapNotify:
    case Expose:
    case GraphicsExpose:
    case NoExpose:
    case VisibilityNotify:
    case ResizeRequest:
    case PropertyNotify:
    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
    case ColormapNotify:
    case ClientMessage:
    case EnterNotify:
    case LeaveNotify:
      return event->xany.window;
      
    case CreateNotify:
      return event->xcreatewindow.window;
      
    case DestroyNotify:
      return event->xdestroywindow.window;

    case UnmapNotify:
      return event->xunmap.window;

    case MapNotify:
      return event->xmap.window;

    case MapRequest:
      return event->xmaprequest.window;

    case ReparentNotify:
     return event->xreparent.window;
      
    case ConfigureNotify:
      return event->xconfigure.window;
      
    case ConfigureRequest:
      return event->xconfigurerequest.window;

    case GravityNotify:
      return event->xgravity.window;

    case CirculateNotify:
      return event->xcirculate.window;

    case CirculateRequest:
      return event->xcirculaterequest.window;

    case MappingNotify:
      return None;

    default:
#ifdef HAVE_SHAPE
      if (META_DISPLAY_HAS_SHAPE (display) && 
          event->type == (display->shape_event_base + ShapeNotify))
        {
          XShapeEvent *sev = (XShapeEvent*) event;
          return sev->window;
        }
#endif

      return None;
    }
}

static guint32
event_get_time (MetaDisplay *display,
                XEvent      *event)
{
  switch (event->type)
    {
    case KeyPress:
    case KeyRelease:
      return event->xkey.time;
      
    case ButtonPress:
    case ButtonRelease:
      return event->xbutton.time;
      
    case MotionNotify:
      return event->xmotion.time;

    case PropertyNotify:
      return event->xproperty.time;

    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
      return event->xselection.time;

    case EnterNotify:
    case LeaveNotify:
      return event->xcrossing.time;

    case FocusIn:
    case FocusOut:
    case KeymapNotify:      
    case Expose:
    case GraphicsExpose:
    case NoExpose:
    case MapNotify:
    case UnmapNotify:
    case VisibilityNotify:
    case ResizeRequest:
    case ColormapNotify:
    case ClientMessage:
    case CreateNotify:
    case DestroyNotify:
    case MapRequest:
    case ReparentNotify:
    case ConfigureNotify:
    case ConfigureRequest:
    case GravityNotify:
    case CirculateNotify:
    case CirculateRequest:
    case MappingNotify:
    default:
      return CurrentTime;
    }
}

#ifdef WITH_VERBOSE_MODE
const char*
meta_event_detail_to_string (int d)
{
  const char *detail = "???";
  switch (d)
    {
      /* We are an ancestor in the A<->B focus change relationship */
    case NotifyAncestor:
      detail = "NotifyAncestor";
      break;
    case NotifyDetailNone:
      detail = "NotifyDetailNone";
      break;
      /* We are a descendant in the A<->B focus change relationship */
    case NotifyInferior:
      detail = "NotifyInferior";
      break;
    case NotifyNonlinear:
      detail = "NotifyNonlinear";
      break;
    case NotifyNonlinearVirtual:
      detail = "NotifyNonlinearVirtual";
      break;
    case NotifyPointer:
      detail = "NotifyPointer";
      break;
    case NotifyPointerRoot:
      detail = "NotifyPointerRoot";
      break;
    case NotifyVirtual:
      detail = "NotifyVirtual";
      break;
    }

  return detail;
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
const char*
meta_event_mode_to_string (int m)
{
  const char *mode = "???";
  switch (m)
    {
    case NotifyNormal:
      mode = "NotifyNormal";
      break;
    case NotifyGrab:
      mode = "NotifyGrab";
      break;
    case NotifyUngrab:
      mode = "NotifyUngrab";
      break;
      /* not sure any X implementations are missing this, but
       * it seems to be absent from some docs.
       */
#ifdef NotifyWhileGrabbed
    case NotifyWhileGrabbed:
      mode = "NotifyWhileGrabbed";
      break;
#endif
    }

  return mode;
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
static const char*
stack_mode_to_string (int mode)
{
  switch (mode)
    {
    case Above:
      return "Above";
    case Below:
      return "Below";
    case TopIf:
      return "TopIf";
    case BottomIf:
      return "BottomIf";
    case Opposite:
      return "Opposite";      
    }

  return "Unknown";
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
static char*
key_event_description (Display *xdisplay,
                       XEvent  *event)
{
  KeySym keysym;
  const char *str;
  
  keysym = XKeycodeToKeysym (xdisplay, event->xkey.keycode, 0);  

  str = XKeysymToString (keysym);
  
  return g_strdup_printf ("Key '%s' state 0x%x", 
                          str ? str : "none", event->xkey.state);
}
#endif /* WITH_VERBOSE_MODE */

#ifdef HAVE_XSYNC
#ifdef WITH_VERBOSE_MODE
static gint64
sync_value_to_64 (const XSyncValue *value)
{
  gint64 v;

  v = XSyncValueLow32 (*value);
  v |= (((gint64)XSyncValueHigh32 (*value)) << 32);
  
  return v;
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
static const char*
alarm_state_to_string (XSyncAlarmState state)
{
  switch (state)
    {
    case XSyncAlarmActive:
      return "Active";
    case XSyncAlarmInactive:
      return "Inactive";
    case XSyncAlarmDestroyed:
      return "Destroyed";
    default:
      return "(unknown)";
    }
}
#endif /* WITH_VERBOSE_MODE */

#endif /* HAVE_XSYNC */

#ifdef WITH_VERBOSE_MODE
static void
meta_spew_event (MetaDisplay *display,
                 XEvent      *event)
{
  const char *name = NULL;
  char *extra = NULL;
  char *winname;
  MetaScreen *screen;

  /* filter overnumerous events */
  if (event->type == Expose || event->type == MotionNotify ||
      event->type == NoExpose)
    return;
      
  switch (event->type)
    {
    case KeyPress:
      name = "KeyPress";
      extra = key_event_description (display->xdisplay, event);
      break;
    case KeyRelease:
      name = "KeyRelease";
      extra = key_event_description (display->xdisplay, event);
      break;
    case ButtonPress:
      name = "ButtonPress";
      extra = g_strdup_printf ("button %d state 0x%x x %d y %d root 0x%lx same_screen %d",
                               event->xbutton.button,
                               event->xbutton.state,
                               event->xbutton.x,
                               event->xbutton.y,
                               event->xbutton.root,
                               event->xbutton.same_screen);
      break;
    case ButtonRelease:
      name = "ButtonRelease";
      extra = g_strdup_printf ("button %d state 0x%x x %d y %d root 0x%lx same_screen %d",
                               event->xbutton.button,
                               event->xbutton.state,
                               event->xbutton.x,
                               event->xbutton.y,
                               event->xbutton.root,
                               event->xbutton.same_screen);
      break;
    case MotionNotify:
      name = "MotionNotify";
      extra = g_strdup_printf ("win: 0x%lx x: %d y: %d",
                               event->xmotion.window,
                               event->xmotion.x,
                               event->xmotion.y);
      break;
    case EnterNotify:
      name = "EnterNotify";
      extra = g_strdup_printf ("win: 0x%lx root: 0x%lx subwindow: 0x%lx mode: %s detail: %s focus: %d x: %d y: %d",
                               event->xcrossing.window,
                               event->xcrossing.root,
                               event->xcrossing.subwindow,
                               meta_event_mode_to_string (event->xcrossing.mode),
                               meta_event_detail_to_string (event->xcrossing.detail),
                               event->xcrossing.focus,
                               event->xcrossing.x,
                               event->xcrossing.y);
      break;
    case LeaveNotify:
      name = "LeaveNotify";
      extra = g_strdup_printf ("win: 0x%lx root: 0x%lx subwindow: 0x%lx mode: %s detail: %s focus: %d x: %d y: %d",
                               event->xcrossing.window,
                               event->xcrossing.root,
                               event->xcrossing.subwindow,
                               meta_event_mode_to_string (event->xcrossing.mode),
                               meta_event_detail_to_string (event->xcrossing.detail),
                               event->xcrossing.focus,
                               event->xcrossing.x,
                               event->xcrossing.y);
      break;
    case FocusIn:
      name = "FocusIn";
      extra = g_strdup_printf ("detail: %s mode: %s\n",
                               meta_event_detail_to_string (event->xfocus.detail),
                               meta_event_mode_to_string (event->xfocus.mode));
      break;
    case FocusOut:
      name = "FocusOut";
      extra = g_strdup_printf ("detail: %s mode: %s\n",
                               meta_event_detail_to_string (event->xfocus.detail),
                               meta_event_mode_to_string (event->xfocus.mode));
      break;
    case KeymapNotify:
      name = "KeymapNotify";
      break;
    case Expose:
      name = "Expose";
      break;
    case GraphicsExpose:
      name = "GraphicsExpose";
      break;
    case NoExpose:
      name = "NoExpose";
      break;
    case VisibilityNotify:
      name = "VisibilityNotify";
      break;
    case CreateNotify:
      name = "CreateNotify";
      break;
    case DestroyNotify:
      name = "DestroyNotify";
      break;
    case UnmapNotify:
      name = "UnmapNotify";
      extra = g_strdup_printf ("event: 0x%lx window: 0x%lx from_configure: %d",
                               event->xunmap.event,
                               event->xunmap.window,
                               event->xunmap.from_configure);
      break;
    case MapNotify:
      name = "MapNotify";
      extra = g_strdup_printf ("event: 0x%lx window: 0x%lx override_redirect: %d",
                               event->xmap.event,
                               event->xmap.window,
                               event->xmap.override_redirect);
      break;
    case MapRequest:
      name = "MapRequest";
      extra = g_strdup_printf ("window: 0x%lx parent: 0x%lx\n",
                               event->xmaprequest.window,
                               event->xmaprequest.parent);
      break;
    case ReparentNotify:
      name = "ReparentNotify";
      break;
    case ConfigureNotify:
      name = "ConfigureNotify";
      extra = g_strdup_printf ("x: %d y: %d w: %d h: %d above: 0x%lx override_redirect: %d",
                               event->xconfigure.x,
                               event->xconfigure.y,
                               event->xconfigure.width,
                               event->xconfigure.height,
                               event->xconfigure.above,
                               event->xconfigure.override_redirect);
      break;
    case ConfigureRequest:
      name = "ConfigureRequest";
      extra = g_strdup_printf ("parent: 0x%lx window: 0x%lx x: %d %sy: %d %sw: %d %sh: %d %sborder: %d %sabove: %lx %sstackmode: %s %s",
                               event->xconfigurerequest.parent,
                               event->xconfigurerequest.window,
                               event->xconfigurerequest.x,
                               event->xconfigurerequest.value_mask &
                               CWX ? "" : "(unset) ",
                               event->xconfigurerequest.y,
                               event->xconfigurerequest.value_mask &
                               CWY ? "" : "(unset) ",
                               event->xconfigurerequest.width,
                               event->xconfigurerequest.value_mask &
                               CWWidth ? "" : "(unset) ",
                               event->xconfigurerequest.height,
                               event->xconfigurerequest.value_mask &
                               CWHeight ? "" : "(unset) ",
                               event->xconfigurerequest.border_width,
                               event->xconfigurerequest.value_mask &
                               CWBorderWidth ? "" : "(unset)",
                               event->xconfigurerequest.above,
                               event->xconfigurerequest.value_mask &
                               CWSibling ? "" : "(unset)",
                               stack_mode_to_string (event->xconfigurerequest.detail),
                               event->xconfigurerequest.value_mask &
                               CWStackMode ? "" : "(unset)");
      break;
    case GravityNotify:
      name = "GravityNotify";
      break;
    case ResizeRequest:
      name = "ResizeRequest";
      extra = g_strdup_printf ("width = %d height = %d",
                               event->xresizerequest.width,
                               event->xresizerequest.height);
      break;
    case CirculateNotify:
      name = "CirculateNotify";
      break;
    case CirculateRequest:
      name = "CirculateRequest";
      break;
    case PropertyNotify:
      {
        char *str;
        const char *state;
            
        name = "PropertyNotify";
            
        meta_error_trap_push (display);
        str = XGetAtomName (display->xdisplay,
                            event->xproperty.atom);
        meta_error_trap_pop (display, TRUE);

        if (event->xproperty.state == PropertyNewValue)
          state = "PropertyNewValue";
        else if (event->xproperty.state == PropertyDelete)
          state = "PropertyDelete";
        else
          state = "???";
            
        extra = g_strdup_printf ("atom: %s state: %s",
                                 str ? str : "(unknown atom)",
                                 state);
        meta_XFree (str);
      }
      break;
    case SelectionClear:
      name = "SelectionClear";
      break;
    case SelectionRequest:
      name = "SelectionRequest";
      break;
    case SelectionNotify:
      name = "SelectionNotify";
      break;
    case ColormapNotify:
      name = "ColormapNotify";
      break;
    case ClientMessage:
      {
        char *str;
        name = "ClientMessage";
        meta_error_trap_push (display);
        str = XGetAtomName (display->xdisplay,
                            event->xclient.message_type);
        meta_error_trap_pop (display, TRUE);
        extra = g_strdup_printf ("type: %s format: %d\n",
                                 str ? str : "(unknown atom)",
                                 event->xclient.format);
        meta_XFree (str);
      }
      break;
    case MappingNotify:
      name = "MappingNotify";
      break;
    default:
#ifdef HAVE_XSYNC
      if (META_DISPLAY_HAS_XSYNC (display) && 
          event->type == (display->xsync_event_base + XSyncAlarmNotify))
        {
          XSyncAlarmNotifyEvent *aevent = (XSyncAlarmNotifyEvent*) event;
          
          name = "XSyncAlarmNotify";
          extra =
            g_strdup_printf ("alarm: 0x%lx"
                             " counter_value: %" G_GINT64_FORMAT
                             " alarm_value: %" G_GINT64_FORMAT
                             " time: %u alarm state: %s",
                             aevent->alarm,
                             (gint64) sync_value_to_64 (&aevent->counter_value),
                             (gint64) sync_value_to_64 (&aevent->alarm_value),
                             (unsigned int) aevent->time,
                             alarm_state_to_string (aevent->state));
        }
      else
#endif /* HAVE_XSYNC */
#ifdef HAVE_SHAPE
        if (META_DISPLAY_HAS_SHAPE (display) && 
            event->type == (display->shape_event_base + ShapeNotify))
          {
            XShapeEvent *sev = (XShapeEvent*) event;

            name = "ShapeNotify";

            extra =
              g_strdup_printf ("kind: %s "
                               "x: %d y: %d w: %d h: %d "
                               "shaped: %d",
                               sev->kind == ShapeBounding ?
                               "ShapeBounding" :
                               (sev->kind == ShapeClip ?
                               "ShapeClip" : "(unknown)"),
                               sev->x, sev->y, sev->width, sev->height,
                               sev->shaped);
          }
        else
#endif /* HAVE_SHAPE */      
        {
          name = "(Unknown event)";
          extra = g_strdup_printf ("type: %d", event->xany.type);
        }
      break;
    }

  screen = meta_display_screen_for_root (display, event->xany.window);
      
  if (screen)
    winname = g_strdup_printf ("root %d", screen->number);
  else
    winname = g_strdup_printf ("0x%lx", event->xany.window);
      
  meta_topic (META_DEBUG_EVENTS,
              "%s on %s%s %s %sserial %lu\n", name, winname,
              extra ? ":" : "", extra ? extra : "",
              event->xany.send_event ? "SEND " : "",
              event->xany.serial);

  g_free (winname);

  if (extra)
    g_free (extra);
}
#endif /* WITH_VERBOSE_MODE */

MetaWindow*
meta_display_lookup_x_window (MetaDisplay *display,
                              Window       xwindow)
{
  return g_hash_table_lookup (display->window_ids, &xwindow);
}

void
meta_display_register_x_window (MetaDisplay *display,
                                Window      *xwindowp,
                                MetaWindow  *window)
{
  g_return_if_fail (g_hash_table_lookup (display->window_ids, xwindowp) == NULL);
  
  g_hash_table_insert (display->window_ids, xwindowp, window);
}

void
meta_display_unregister_x_window (MetaDisplay *display,
                                  Window       xwindow)
{
  g_return_if_fail (g_hash_table_lookup (display->window_ids, &xwindow) != NULL);

  g_hash_table_remove (display->window_ids, &xwindow);

  /* Remove any pending pings */
  remove_pending_pings_for_window (display, xwindow);
}

Cursor
meta_display_create_x_cursor (MetaDisplay *display,
                              MetaCursor cursor)
{
  Cursor xcursor;
  guint glyph;

  switch (cursor)
    {
    case META_CURSOR_DEFAULT:
      glyph = XC_left_ptr;
      break;
    case META_CURSOR_NORTH_RESIZE:
      glyph = XC_top_side;
      break;
    case META_CURSOR_SOUTH_RESIZE:
      glyph = XC_bottom_side;
      break;
    case META_CURSOR_WEST_RESIZE:
      glyph = XC_left_side;
      break;
    case META_CURSOR_EAST_RESIZE:
      glyph = XC_right_side;
      break;
    case META_CURSOR_SE_RESIZE:
      glyph = XC_bottom_right_corner;
      break;
    case META_CURSOR_SW_RESIZE:
      glyph = XC_bottom_left_corner;
      break;
    case META_CURSOR_NE_RESIZE:
      glyph = XC_top_right_corner;
      break;
    case META_CURSOR_NW_RESIZE:
      glyph = XC_top_left_corner;
      break;
    case META_CURSOR_MOVE_WINDOW:
      glyph = XC_plus;
      break;
    case META_CURSOR_RESIZE_WINDOW:
      glyph = XC_fleur;
      break;
    case META_CURSOR_BUSY:
      glyph = XC_watch;
      break;
      
    default:
      g_assert_not_reached ();
      glyph = 0; /* silence compiler */
      break;
    }
  
  xcursor = XCreateFontCursor (display->xdisplay, glyph);

  return xcursor;
}

static Cursor
xcursor_for_op (MetaDisplay *display,
                MetaGrabOp   op)
{
  MetaCursor cursor = META_CURSOR_DEFAULT;
  
  switch (op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
      cursor = META_CURSOR_SE_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
      cursor = META_CURSOR_SOUTH_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
      cursor = META_CURSOR_SW_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
      cursor = META_CURSOR_NORTH_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
      cursor = META_CURSOR_NE_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      cursor = META_CURSOR_NW_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      cursor = META_CURSOR_WEST_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      cursor = META_CURSOR_EAST_RESIZE;
      break;
    case META_GRAB_OP_KEYBOARD_MOVING:
      cursor = META_CURSOR_MOVE_WINDOW;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      cursor = META_CURSOR_RESIZE_WINDOW;
      break;
      
    default:
      break;
    }

  if (cursor == META_CURSOR_DEFAULT)
    return None;
  return meta_display_create_x_cursor (display, cursor);
}

void
meta_display_set_grab_op_cursor (MetaDisplay *display,
                                 MetaScreen  *screen,
                                 MetaGrabOp   op,
                                 gboolean     change_pointer,
                                 Window       grab_xwindow,
                                 Time         timestamp)
{
  Cursor cursor;

  cursor = xcursor_for_op (display, op);

#define GRAB_MASK (PointerMotionMask |                          \
                   ButtonPressMask | ButtonReleaseMask |        \
		   EnterWindowMask | LeaveWindowMask)

  if (change_pointer)
    {
      meta_error_trap_push_with_return (display);
      XChangeActivePointerGrab (display->xdisplay,
                                GRAB_MASK,
                                cursor,
                                timestamp);

      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Changed pointer with XChangeActivePointerGrab()\n");

      if (meta_error_trap_pop_with_return (display, FALSE) != Success)
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "Error trapped from XChangeActivePointerGrab()\n");
          if (display->grab_have_pointer)
            display->grab_have_pointer = FALSE;
        }
    }
  else
    {
      g_assert (screen != NULL);

      meta_error_trap_push (display);
      if (XGrabPointer (display->xdisplay,
                        grab_xwindow,
                        False,
                        GRAB_MASK,
                        GrabModeAsync, GrabModeAsync,
                        screen->xroot,
                        cursor,
                        timestamp) == GrabSuccess)
        {
          display->grab_have_pointer = TRUE;
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "XGrabPointer() returned GrabSuccess time 0x%lu\n",
                      timestamp);
        }
      else
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "XGrabPointer() failed time 0x%lu\n",
                      timestamp);
        }
      meta_error_trap_pop (display, TRUE);
    }

#undef GRAB_MASK
  
  if (cursor != None)
    XFreeCursor (display->xdisplay, cursor);
}

gboolean
meta_display_begin_grab_op (MetaDisplay *display,
			    MetaScreen  *screen,
                            MetaWindow  *window,
                            MetaGrabOp   op,
                            gboolean     pointer_already_grabbed,
                            int          button,
                            gulong       modmask,
                            Time         timestamp,
                            int          root_x,
                            int          root_y)
{
  Window grab_xwindow;
  
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Doing grab op %d on window %s button %d pointer already grabbed: %d\n",
              op, window ? window->desc : "none", button, pointer_already_grabbed);
  
  if (display->grab_op != META_GRAB_OP_NONE)
    {
      meta_warning ("Attempt to perform window operation %d on window %s when operation %d on %s already in effect\n",
                    op, window ? window->desc : "none", display->grab_op,
                    display->grab_window ? display->grab_window->desc : "none");
      return FALSE;
    }

  /* FIXME:
   *   If we have no MetaWindow we do our best
   *   and try to do the grab on the RootWindow.
   *   This will fail if anyone else has any
   *   key grab on the RootWindow.
   */
  if (window)
    grab_xwindow = window->frame ? window->frame->xwindow : window->xwindow;
  else
    grab_xwindow = screen->xroot;
  
  if (pointer_already_grabbed)
    display->grab_have_pointer = TRUE;
      
  meta_display_set_grab_op_cursor (display, screen, op, FALSE, grab_xwindow,
                                   timestamp);

  if (!display->grab_have_pointer)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "XGrabPointer() failed\n");
      return FALSE;
    }

  if (grab_op_is_keyboard (op))
    {
      if (window)
        display->grab_have_keyboard =
                     meta_window_grab_all_keys (window);

      else
        display->grab_have_keyboard =
                     meta_screen_grab_all_keys (screen);
      
      if (!display->grab_have_keyboard)
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "grabbing all keys failed, ungrabbing pointer\n");
          XUngrabPointer (display->xdisplay, CurrentTime);
	  display->grab_have_pointer = FALSE;
          return FALSE;
        }
    }
  
  display->grab_op = op;
  display->grab_window = window;
  display->grab_screen = screen;
  display->grab_xwindow = grab_xwindow;
  display->grab_button = button;
  display->grab_mask = modmask;
  display->grab_initial_root_x = root_x;
  display->grab_initial_root_y = root_y;
  display->grab_current_root_x = root_x;
  display->grab_current_root_y = root_y;
  display->grab_latest_motion_x = root_x;
  display->grab_latest_motion_y = root_y;
  display->grab_last_moveresize_time.tv_sec = 0;
  display->grab_last_moveresize_time.tv_usec = 0;
  display->grab_motion_notify_time = 0;
#ifdef HAVE_XSYNC
  display->grab_update_alarm = None;
#endif
  
  if (display->grab_window)
    {
      display->grab_initial_window_pos = display->grab_window->rect;
      meta_window_get_position (display->grab_window,
                                &display->grab_initial_window_pos.x,
                                &display->grab_initial_window_pos.y);
      display->grab_current_window_pos = display->grab_initial_window_pos;

#ifdef HAVE_XSYNC
      if (meta_grab_op_is_resizing (display->grab_op) &&
          display->grab_window->update_counter != None)
        {
          XSyncAlarmAttributes values;

          /* trigger when we make a positive transition to a value
           * one higher than the current value.
           */
          values.trigger.counter = display->grab_window->update_counter;
          values.trigger.value_type = XSyncRelative;
          values.trigger.test_type = XSyncPositiveTransition;
          XSyncIntToValue (&values.trigger.wait_value, 1);

          /* After triggering, increment test_value by this.
           * (NOT wait_value above)
           */
          XSyncIntToValue (&values.delta, 1);

          /* we want events (on by default anyway) */
          values.events = True;
          
          meta_error_trap_push_with_return (display);
          display->grab_update_alarm = XSyncCreateAlarm (display->xdisplay,
                                                         XSyncCACounter |
                                                         XSyncCAValueType |
                                                         XSyncCAValue |
                                                         XSyncCATestType |
                                                         XSyncCADelta |
                                                         XSyncCAEvents,
                                                         &values);
          if (meta_error_trap_pop_with_return (display, FALSE) != Success)
            display->grab_update_alarm = None;

          meta_topic (META_DEBUG_RESIZING,
                      "Created update alarm 0x%lx\n",
                      display->grab_update_alarm);
        }
#endif
    }
  
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Grab op %d on window %s successful\n",
              display->grab_op, window ? window->desc : "(null)");

  g_assert (display->grab_window != NULL || display->grab_screen != NULL);
  g_assert (display->grab_op != META_GRAB_OP_NONE);

  /* Do this last, after everything is set up. */
  switch (op)
    {
    case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
    case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
      meta_screen_ensure_tab_popup (screen,
                                    META_TAB_LIST_NORMAL);
      break;

    case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
    case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
      meta_screen_ensure_tab_popup (screen,
                                    META_TAB_LIST_DOCKS);
      break;
      
    case META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING:
      meta_screen_ensure_workspace_popup (screen);
      break;

    default:
      break;
    }

  if (display->grab_window)
    meta_window_refresh_resize_popup (display->grab_window);
  
  return TRUE;
}

void
meta_display_end_grab_op (MetaDisplay *display,
                          Time         timestamp)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Ending grab op %d at time %lu\n", display->grab_op,
              (unsigned long) timestamp);
  
  if (display->grab_op == META_GRAB_OP_NONE)
    return;

  if (display->grab_op == META_GRAB_OP_KEYBOARD_TABBING_NORMAL ||
      display->grab_op == META_GRAB_OP_KEYBOARD_TABBING_DOCK ||
      display->grab_op == META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL ||
      display->grab_op == META_GRAB_OP_KEYBOARD_ESCAPING_DOCK ||
      display->grab_op == META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING)
    {
      meta_ui_tab_popup_free (display->grab_screen->tab_popup);
      display->grab_screen->tab_popup = NULL;

      /* If the ungrab here causes an EnterNotify, ignore it for
       * sloppy focus
       */
      display->ungrab_should_not_cause_focus_window = display->grab_xwindow;
    }
  
  if (display->grab_have_pointer)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Ungrabbing pointer with timestamp %lu\n",
                  timestamp);
      XUngrabPointer (display->xdisplay, timestamp);
    }

  if (display->grab_have_keyboard)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Ungrabbing all keys timestamp %lu\n", timestamp);
      if (display->grab_window)
        meta_window_ungrab_all_keys (display->grab_window);
      else
        meta_screen_ungrab_all_keys (display->grab_screen);
    }

#ifdef HAVE_XSYNC
  if (display->grab_update_alarm != None)
    {
      XSyncDestroyAlarm (display->xdisplay,
                         display->grab_update_alarm);
    }
#endif /* HAVE_XSYNC */
  
  display->grab_window = NULL;
  display->grab_screen = NULL;
  display->grab_xwindow = None;
  display->grab_op = META_GRAB_OP_NONE;

  if (display->grab_resize_popup)
    {
      meta_ui_resize_popup_free (display->grab_resize_popup);
      display->grab_resize_popup = NULL;
    }
}

static void
meta_change_button_grab (MetaDisplay *display,
                         Window       xwindow,
                         gboolean     grab,
                         gboolean     sync,
                         int          button,
                         int          modmask)
{
  int ignored_mask;
  
  meta_error_trap_push (display);
  
  ignored_mask = 0;
  while (ignored_mask < (int) display->ignored_modifier_mask)
    {
      if (ignored_mask & ~(display->ignored_modifier_mask))
        {
          /* Not a combination of ignored modifiers
           * (it contains some non-ignored modifiers)
           */
          ++ignored_mask;
          continue;
        }

      if (meta_is_debugging ())
        meta_error_trap_push_with_return (display);
      
      if (grab)
        XGrabButton (display->xdisplay, button, modmask | ignored_mask,
                     xwindow, False,
                     ButtonPressMask | ButtonReleaseMask |    
                     PointerMotionMask | PointerMotionHintMask,
                     sync ? GrabModeSync : GrabModeAsync,
                     GrabModeAsync,
                     False, None);
      else
        XUngrabButton (display->xdisplay, button, modmask | ignored_mask,
                       xwindow);

      if (meta_is_debugging ())
        {
          int result;
          
          result = meta_error_trap_pop_with_return (display, FALSE);
          
          if (result != Success)
            meta_verbose ("Failed to %s button %d with mask 0x%x for window 0x%lx error code %d\n",
                          grab ? "grab" : "ungrab",
                          button, modmask | ignored_mask, xwindow, result);
        }
      
      ++ignored_mask;
    }

  meta_error_trap_pop (display, FALSE);
}

void
meta_display_grab_window_buttons (MetaDisplay *display,
                                  Window       xwindow)
{  
  /* Grab Alt + button1 and Alt + button2 for moving window,
   * and Alt + button3 for popping up window menu.
   */
  meta_verbose ("Grabbing window buttons for 0x%lx\n", xwindow);
  
  /* FIXME If we ignored errors here instead of spewing, we could
   * put one big error trap around the loop and avoid a bunch of
   * XSync()
   */

  if (display->window_grab_modifiers != 0)
    {
      gboolean debug = g_getenv ("METACITY_DEBUG_BUTTON_GRABS") != NULL;
      int i = 1;
      while (i < 4)
        {
          meta_change_button_grab (display,
                                   xwindow,
                                   TRUE,
                                   FALSE,
                                   i, display->window_grab_modifiers);  
          
          /* This is for debugging, since I end up moving the Xnest
           * otherwise ;-)
           */
          if (debug)
            meta_change_button_grab (display, xwindow,
                                     TRUE,
                                     FALSE,
                                     i, ControlMask);
          
          ++i;
        }
    }
}

void
meta_display_ungrab_window_buttons  (MetaDisplay *display,
                                     Window       xwindow)
{
  gboolean debug;
  int i;

  if (display->window_grab_modifiers == 0)
    return;
  
  debug = g_getenv ("METACITY_DEBUG_BUTTON_GRABS") != NULL;
  i = 1;
  while (i < 4)
    {
      meta_change_button_grab (display, xwindow,
                               FALSE, FALSE, i,
                               display->window_grab_modifiers);
      
      if (debug)
        meta_change_button_grab (display, xwindow,
                                 FALSE, FALSE, i, ControlMask);
      
      ++i;
    }
}

/* Grab buttons we only grab while unfocused in click-to-focus mode */
#define MAX_FOCUS_BUTTON 4
void
meta_display_grab_focus_window_button (MetaDisplay *display,
                                       Window       xwindow)
{
  /* Grab button 1 for activating unfocused windows */
  meta_verbose ("Grabbing unfocused window buttons for 0x%lx\n", xwindow);

  /* FIXME If we ignored errors here instead of spewing, we could
   * put one big error trap around the loop and avoid a bunch of
   * XSync()
   */
  
  {
    int i = 1;
    while (i < MAX_FOCUS_BUTTON)
      {
        meta_change_button_grab (display,
                                 xwindow,
                                 TRUE, TRUE, i, 0);
        
        ++i;
      }
  }
}

void
meta_display_ungrab_focus_window_button (MetaDisplay *display,
                                         Window       xwindow)
{
  meta_verbose ("Ungrabbing unfocused window buttons for 0x%lx\n", xwindow);

  {
    int i = 1;
    while (i < MAX_FOCUS_BUTTON)
      {
        meta_change_button_grab (display, xwindow,
                                 FALSE, TRUE, i, 0);
        
        ++i;
      }
  }
}

void
meta_display_increment_event_serial (MetaDisplay *display)
{
  /* We just make some random X request */
  XDeleteProperty (display->xdisplay, display->leader_window,
                   display->atom_motif_wm_hints);
}

void
meta_display_update_active_window_hint (MetaDisplay *display)
{
  GSList *tmp;
  
  unsigned long data[2];

  if (display->focus_window)
    data[0] = display->focus_window->xwindow;
  else
    data[0] = None;
  data[1] = None;
  
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
      
      meta_error_trap_push (display);
      XChangeProperty (display->xdisplay, screen->xroot,
                       display->atom_net_active_window,
                       XA_WINDOW,
                       32, PropModeReplace, (guchar*) data, 2);
      meta_error_trap_pop (display, FALSE);

      tmp = tmp->next;
    }
}

void
meta_display_queue_retheme_all_windows (MetaDisplay *display)
{
  GSList* windows;
  GSList *tmp;

  windows = meta_display_list_windows (display);
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;
      
      meta_window_queue_move_resize (window);
      if (window->frame)
        {
          window->frame->need_reapply_frame_shape = TRUE;
          
          meta_frame_queue_draw (window->frame);
        }
      
      tmp = tmp->next;
    }

  g_slist_free (windows);
}

void
meta_display_retheme_all (void)
{
  GSList *tmp;
  
  tmp = meta_displays_list ();
  while (tmp != NULL)
    {
      MetaDisplay *display = tmp->data;
      meta_display_queue_retheme_all_windows (display);
      tmp = tmp->next;
    }
}

static gboolean is_syncing = FALSE;

gboolean
meta_is_syncing (void)
{
  return is_syncing;
}

void
meta_set_syncing (gboolean setting)
{
  if (setting != is_syncing)
    {
      GSList *tmp;
      
      is_syncing = setting;

      tmp = meta_displays_list ();
      while (tmp != NULL)
        {
          MetaDisplay *display = tmp->data;
          XSynchronize (display->xdisplay, is_syncing);
          tmp = tmp->next;
        }
    }
}

#define PING_TIMEOUT_DELAY 2250

static gboolean
meta_display_ping_timeout (gpointer data)
{
  MetaPingData *ping_data;

  ping_data = data;

  ping_data->ping_timeout_id = 0;

  meta_topic (META_DEBUG_PING,
              "Ping %lu on window %lx timed out\n",
              ping_data->timestamp, ping_data->xwindow);
  
  (* ping_data->ping_timeout_func) (ping_data->display, ping_data->xwindow,
                                    ping_data->user_data);

  ping_data->display->pending_pings =
    g_slist_remove (ping_data->display->pending_pings,
                    ping_data);
  ping_data_free (ping_data);
  
  return FALSE;
}

void
meta_display_ping_window (MetaDisplay       *display,
			  MetaWindow        *window,
			  Time               timestamp,
			  MetaWindowPingFunc ping_reply_func,
			  MetaWindowPingFunc ping_timeout_func,
			  gpointer           user_data)
{
  MetaPingData *ping_data;

  if (timestamp == CurrentTime)
    {
      meta_warning ("Tried to ping a window with CurrentTime! Not allowed.\n");
      return;
    }

  if (!window->net_wm_ping)
    {
      if (ping_reply_func)
        (* ping_reply_func) (display, window->xwindow, user_data);

      return;
    }
  
  ping_data = g_new (MetaPingData, 1);
  ping_data->display = display;
  ping_data->xwindow = window->xwindow;
  ping_data->timestamp = timestamp;
  ping_data->ping_reply_func = ping_reply_func;
  ping_data->ping_timeout_func = ping_timeout_func;
  ping_data->user_data = user_data;
  ping_data->ping_timeout_id = g_timeout_add (PING_TIMEOUT_DELAY,
					      meta_display_ping_timeout,
					      ping_data);
  
  display->pending_pings = g_slist_prepend (display->pending_pings, ping_data);

  meta_topic (META_DEBUG_PING,
              "Sending ping with timestamp %lu to window %s\n",
              timestamp, window->desc);
  meta_window_send_icccm_message (window,
				  display->atom_net_wm_ping,
				  timestamp);
}

/* process the pong from our ping */
static void
process_pong_message (MetaDisplay    *display,
                      XEvent         *event)
{
  GSList *tmp;

  meta_topic (META_DEBUG_PING, "Received a pong with timestamp %lu\n",
              (Time) event->xclient.data.l[1]);
  
  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;
			  
      if ((Time)event->xclient.data.l[1] == ping_data->timestamp)
        {
          meta_topic (META_DEBUG_PING,
                      "Matching ping found for pong %lu\n",
                      ping_data->timestamp);

          /* Remove the ping data from the list */
          display->pending_pings = g_slist_remove (display->pending_pings,
                                                   ping_data);

          /* Remove the timeout */
          if (ping_data->ping_timeout_id != 0)
            {
              g_source_remove (ping_data->ping_timeout_id);
              ping_data->ping_timeout_id = 0;
            }
          
          /* Call callback */
          (* ping_data->ping_reply_func) (display, ping_data->xwindow,
                                          ping_data->user_data);
			      
          ping_data_free (ping_data);

          break;
        }
    }
}

gboolean
meta_display_window_has_pending_pings (MetaDisplay *display,
				       MetaWindow  *window)
{
  GSList *tmp;

  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      if (ping_data->xwindow == window->xwindow) 
        return TRUE;
    }

  return FALSE;
}

#define IN_TAB_CHAIN(w,t) (((t) == META_TAB_LIST_NORMAL && META_WINDOW_IN_NORMAL_TAB_CHAIN (w)) || ((t) == META_TAB_LIST_DOCKS && META_WINDOW_IN_DOCK_TAB_CHAIN (w)))

static MetaWindow*
find_tab_forward (MetaDisplay   *display,
                  MetaTabList    type,
		  MetaScreen    *screen, 
                  MetaWorkspace *workspace,
                  GList         *start)
{
  GList *tmp;

  g_return_val_if_fail (start != NULL, NULL);

  tmp = start->next;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      if (window->screen == screen &&
	  IN_TAB_CHAIN (window, type) &&
          (workspace == NULL ||
           meta_window_visible_on_workspace (window, workspace)))
        return window;

      tmp = tmp->next;
    }

  tmp = display->mru_list;
  while (tmp != start)
    {
      MetaWindow *window = tmp->data;

      if (IN_TAB_CHAIN (window, type) &&
          (workspace == NULL ||
           meta_window_visible_on_workspace (window, workspace)))
        return window;

      tmp = tmp->next;
    }  

  return NULL;
}

static MetaWindow*
find_tab_backward (MetaDisplay   *display,
                   MetaTabList    type,
		   MetaScreen    *screen, 
                   MetaWorkspace *workspace,
                   GList         *start)
{
  GList *tmp;

  g_return_val_if_fail (start != NULL, NULL);
  
  tmp = start->prev;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      if (window->screen == screen &&
	  IN_TAB_CHAIN (window, type) &&
          (workspace == NULL ||
           meta_window_visible_on_workspace (window, workspace)))
        return window;

      tmp = tmp->prev;
    }

  tmp = g_list_last (display->mru_list);
  while (tmp != start)
    {
      MetaWindow *window = tmp->data;

      if (IN_TAB_CHAIN (window, type) &&
          (workspace == NULL ||
           meta_window_visible_on_workspace (window, workspace)))
        return window;

      tmp = tmp->prev;
    }

  return NULL;
}

GSList*
meta_display_get_tab_list (MetaDisplay   *display,
                           MetaTabList    type,
                           MetaScreen    *screen,
                           MetaWorkspace *workspace)
{
  GSList *tab_list;

  /* workspace can be NULL for all workspaces */  

  /* Windows sellout mode - MRU order. Collect unminimized windows
   * then minimized so minimized windows aren't in the way so much.
   */
  {
    GList *tmp;
    
    tab_list = NULL;
    tmp = screen->display->mru_list;
    while (tmp != NULL)
      {
        MetaWindow *window = tmp->data;
        
        if (!window->minimized &&
            window->screen == screen &&
            IN_TAB_CHAIN (window, type) &&
            (workspace == NULL ||
             meta_window_visible_on_workspace (window, workspace)))
          tab_list = g_slist_prepend (tab_list, window);
        
        tmp = tmp->next;
      }
  }

  {
    GList *tmp;
    
    tmp = screen->display->mru_list;
    while (tmp != NULL)
      {
        MetaWindow *window = tmp->data;
        
        if (window->minimized &&
            window->screen == screen &&
            IN_TAB_CHAIN (window, type) &&
            (workspace == NULL ||
             meta_window_visible_on_workspace (window, workspace)))
          tab_list = g_slist_prepend (tab_list, window);
        
        tmp = tmp->next;
      }
  }

  tab_list = g_slist_reverse (tab_list);
  
  return tab_list;
}

MetaWindow*
meta_display_get_tab_next (MetaDisplay   *display,
                           MetaTabList    type,
			   MetaScreen    *screen,
                           MetaWorkspace *workspace,
                           MetaWindow    *window,
                           gboolean       backward)
{
  if (display->mru_list == NULL)
    return NULL;
  
  if (window != NULL)
    {
      g_assert (window->display == display);
      
      if (backward)
        return find_tab_backward (display, type, screen, workspace,
                                  g_list_find (display->mru_list,
                                               window));
      else
        return find_tab_forward (display, type, screen, workspace,
                                 g_list_find (display->mru_list,
                                              window));
    }
  
  if (backward)
    return find_tab_backward (display, type, screen, workspace,
                              g_list_last (display->mru_list));
  else
    return find_tab_forward (display, type, screen, workspace,
                             display->mru_list);
}

MetaWindow*
meta_display_get_tab_current (MetaDisplay   *display,
                              MetaTabList    type,
                              MetaScreen    *screen,
                              MetaWorkspace *workspace)
{
  MetaWindow *window;

  window = display->focus_window;
  
  if (window != NULL &&
      window->screen == screen &&
      IN_TAB_CHAIN (window, type) &&
      (workspace == NULL ||
       meta_window_visible_on_workspace (window, workspace)))
    return window;
  else
    return NULL;
}

int
meta_resize_gravity_from_grab_op (MetaGrabOp op)
{
  int gravity;
  
  gravity = -1;
  switch (op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
      gravity = NorthWestGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_RESIZING_S:
      gravity = NorthGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_RESIZING_SW:
      gravity = NorthEastGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_RESIZING_N:
      gravity = SouthGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NE:
      gravity = SouthWestGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_RESIZING_NW:
      gravity = SouthEastGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_RESIZING_E:
      gravity = WestGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_RESIZING_W:
      gravity = EastGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      gravity = CenterGravity;
      break;
    default:
      break;
    }

  return gravity;
}

gboolean
meta_rectangle_intersect (MetaRectangle *src1,
			  MetaRectangle *src2,
			  MetaRectangle *dest)
{
  int dest_x, dest_y;
  int dest_w, dest_h;
  int return_val;

  g_return_val_if_fail (src1 != NULL, FALSE);
  g_return_val_if_fail (src2 != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  return_val = FALSE;

  dest_x = MAX (src1->x, src2->x);
  dest_y = MAX (src1->y, src2->y);
  dest_w = MIN (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest_h = MIN (src1->y + src1->height, src2->y + src2->height) - dest_y;
  
  if (dest_w > 0 && dest_h > 0)
    {
      dest->x = dest_x;
      dest->y = dest_y;
      dest->width = dest_w;
      dest->height = dest_h;
      return_val = TRUE;
    }
  else
    {
      dest->width = 0;
      dest->height = 0;
    }

  return return_val;
}

static MetaScreen*
find_screen_for_selection (MetaDisplay *display,
                           Window       owner,
                           Atom         selection)
{  
  GSList *tmp;  
  
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
      
      if (screen->wm_sn_selection_window == owner &&
          screen->wm_sn_atom == selection)
        return screen;
  
      tmp = tmp->next;
    }

  return NULL;
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static gboolean
convert_property (MetaDisplay *display,
                  MetaScreen  *screen,
                  Window       w,
                  Atom         target,
                  Atom         property)
{
#define N_TARGETS 4
  Atom conversion_targets[N_TARGETS];
  long icccm_version[] = { 2, 0 };

  conversion_targets[0] = display->atom_targets;
  conversion_targets[1] = display->atom_multiple;
  conversion_targets[2] = display->atom_timestamp;
  conversion_targets[3] = display->atom_version;

  meta_error_trap_push_with_return (display);
  if (target == display->atom_targets)
    XChangeProperty (display->xdisplay, w, property,
		     XA_ATOM, 32, PropModeReplace,
		     (unsigned char *)conversion_targets, N_TARGETS);
  else if (target == display->atom_timestamp)
    XChangeProperty (display->xdisplay, w, property,
		     XA_INTEGER, 32, PropModeReplace,
		     (unsigned char *)&screen->wm_sn_timestamp, 1);
  else if (target == display->atom_version)
    XChangeProperty (display->xdisplay, w, property,
		     XA_INTEGER, 32, PropModeReplace,
		     (unsigned char *)icccm_version, 2);
  else
    {
      meta_error_trap_pop_with_return (display, FALSE);
      return FALSE;
    }
  
  if (meta_error_trap_pop_with_return (display, FALSE) != Success)
    return FALSE;

  /* Be sure the PropertyNotify has arrived so we
   * can send SelectionNotify
   */
  /* FIXME the error trap pop synced anyway, right? */
  meta_topic (META_DEBUG_SYNC, "Syncing on %s\n", G_GNUC_FUNCTION);
  XSync (display->xdisplay, False);

  return TRUE;
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static void
process_selection_request (MetaDisplay   *display,
                           XEvent        *event)
{
  XSelectionEvent reply;
  MetaScreen *screen;

  screen = find_screen_for_selection (display,
                                      event->xselectionrequest.owner,
                                      event->xselectionrequest.selection);

  if (screen == NULL)
    {
      char *str;
      
      meta_error_trap_push (display);
      str = XGetAtomName (display->xdisplay,
                          event->xselectionrequest.selection);
      meta_error_trap_pop (display, TRUE);
      
      meta_verbose ("Selection request with selection %s window 0x%lx not a WM_Sn selection we recognize\n",
                    str ? str : "(bad atom)", event->xselectionrequest.owner);
      
      meta_XFree (str);

      return;
    }
  
  reply.type = SelectionNotify;
  reply.display = display->xdisplay;
  reply.requestor = event->xselectionrequest.requestor;
  reply.selection = event->xselectionrequest.selection;
  reply.target = event->xselectionrequest.target;
  reply.property = None;
  reply.time = event->xselectionrequest.time;

  if (event->xselectionrequest.target == display->atom_multiple)
    {
      if (event->xselectionrequest.property != None)
        {
          Atom type, *adata;
          int i, format;
          unsigned long num, rest;
          unsigned char *data;

          meta_error_trap_push_with_return (display);
          if (XGetWindowProperty (display->xdisplay,
                                  event->xselectionrequest.requestor,
                                  event->xselectionrequest.property, 0, 256, False,
                                  display->atom_atom_pair,
                                  &type, &format, &num, &rest, &data) != Success)
            {
              meta_error_trap_pop_with_return (display, TRUE);
              return;
            }
          
          if (meta_error_trap_pop_with_return (display, TRUE) == Success)
            {              
              /* FIXME: to be 100% correct, should deal with rest > 0,
               * but since we have 4 possible targets, we will hardly ever
               * meet multiple requests with a length > 8
               */
              adata = (Atom*)data;
              i = 0;
              while (i < (int) num)
                {
                  if (!convert_property (display, screen,
                                         event->xselectionrequest.requestor,
                                         adata[i], adata[i+1]))
                    adata[i+1] = None;
                  i += 2;
                }

              meta_error_trap_push (display);
              XChangeProperty (display->xdisplay,
                               event->xselectionrequest.requestor,
                               event->xselectionrequest.property,
                               display->atom_atom_pair,
                               32, PropModeReplace, data, num);
              meta_error_trap_pop (display, FALSE);
              meta_XFree (data);
            }
        }
    }
  else
    {
      if (event->xselectionrequest.property == None)
        event->xselectionrequest.property = event->xselectionrequest.target;
      
      if (convert_property (display, screen,
                            event->xselectionrequest.requestor,
                            event->xselectionrequest.target,
                            event->xselectionrequest.property))
        reply.property = event->xselectionrequest.property;
    }

  XSendEvent (display->xdisplay,
              event->xselectionrequest.requestor,
              False, 0L, (XEvent*)&reply);

  meta_verbose ("Handled selection request\n");
}

static void
process_selection_clear (MetaDisplay   *display,
                         XEvent        *event)
{
  /* We need to unmanage the screen on which we lost the selection */
  MetaScreen *screen;

  screen = find_screen_for_selection (display,
                                      event->xselectionclear.window,
                                      event->xselectionclear.selection);
  

  if (screen != NULL)
    {
      meta_verbose ("Got selection clear for screen %d on display %s\n",
                    screen->number, display->name);
      
      meta_display_unmanage_screen (display, screen);

      /* display and screen may both be invalid memory... */
      
      return;
    }

  {
    char *str;
            
    meta_error_trap_push (display);
    str = XGetAtomName (display->xdisplay,
                        event->xselectionclear.selection);
    meta_error_trap_pop (display, TRUE);

    meta_verbose ("Selection clear with selection %s window 0x%lx not a WM_Sn selection we recognize\n",
                  str ? str : "(bad atom)", event->xselectionclear.window);

    meta_XFree (str);
  }
}

void
meta_display_unmanage_screen (MetaDisplay *display,
                              MetaScreen  *screen)
{
  meta_verbose ("Unmanaging screen %d on display %s\n",
                screen->number, display->name);
  
  g_return_if_fail (g_slist_find (display->screens, screen) != NULL);
  
  meta_screen_free (screen);
  display->screens = g_slist_remove (display->screens, screen);

  if (display->screens == NULL)
    meta_display_close (display);
}

void
meta_display_unmanage_windows_for_screen (MetaDisplay *display,
                                          MetaScreen  *screen)
{
  GSList *tmp;
  GSList *winlist;

  winlist = meta_display_list_windows (display);

  /* Unmanage all windows */
  tmp = winlist;
  while (tmp != NULL)
    {      
      meta_window_free (tmp->data);
      
      tmp = tmp->next;
    }
  g_slist_free (winlist);
}

void
meta_display_devirtualize_modifiers (MetaDisplay        *display,
                                     MetaVirtualModifier modifiers,
                                     unsigned int       *mask)
{
  *mask = 0;
  
  if (modifiers & META_VIRTUAL_SHIFT_MASK)
    *mask |= ShiftMask;
  if (modifiers & META_VIRTUAL_CONTROL_MASK)
    *mask |= ControlMask;
  if (modifiers & META_VIRTUAL_ALT_MASK)
    *mask |= Mod1Mask;
  if (modifiers & META_VIRTUAL_META_MASK)
    *mask |= display->meta_mask;
  if (modifiers & META_VIRTUAL_HYPER_MASK)
    *mask |= display->hyper_mask;
  if (modifiers & META_VIRTUAL_SUPER_MASK)
    *mask |= display->super_mask;
  if (modifiers & META_VIRTUAL_MOD2_MASK)
    *mask |= Mod2Mask;
  if (modifiers & META_VIRTUAL_MOD3_MASK)
    *mask |= Mod3Mask;
  if (modifiers & META_VIRTUAL_MOD4_MASK)
    *mask |= Mod4Mask;
  if (modifiers & META_VIRTUAL_MOD5_MASK)
    *mask |= Mod5Mask;  
}

static void
update_window_grab_modifiers (MetaDisplay *display)
     
{
  MetaVirtualModifier virtual_mods;
  unsigned int mods;
    
  virtual_mods = meta_prefs_get_mouse_button_mods ();
  meta_display_devirtualize_modifiers (display, virtual_mods,
                                       &mods);
    
  display->window_grab_modifiers = mods;
}

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  if (pref == META_PREF_MOUSE_BUTTON_MODS)
    {
      MetaDisplay *display = data;
      GSList *windows;
      GSList *tmp;

      windows = meta_display_list_windows (display);
      
      /* Ungrab all */
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;
          meta_display_ungrab_window_buttons (display, w->xwindow);
          meta_display_ungrab_focus_window_button (display, w->xwindow);
          tmp = tmp->next;
        }
      
      /* change our modifier */
      update_window_grab_modifiers (display);

      /* Grab all */
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;
          meta_display_grab_focus_window_button (display, w->xwindow);
          meta_display_grab_window_buttons (display, w->xwindow);
          tmp = tmp->next;
        }

      g_slist_free (windows);
    }
}
