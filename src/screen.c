/* Metacity X screen handler */

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
#include "screen.h"
#include "util.h"
#include "errors.h"
#include "window.h"
#include "frame.h"
#include "prefs.h"
#include "workspace.h"
#include "keybindings.h"
#include "stack.h"

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <X11/Xatom.h>
#include <locale.h>
#include <string.h>

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

  /* legacy hint window should have property containing self */
  XChangeProperty (screen->display->xdisplay, screen->display->leader_window,
                   screen->display->atom_win_supporting_wm_check,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);

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
#define N_SUPPORTED 30
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
  atoms[29] = screen->display->atom_net_wm_workarea;
  
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

MetaScreen*
meta_screen_new (MetaDisplay *display,
                 int          number)
{
  MetaScreen *screen;
  Window xroot;
  Display *xdisplay;
  int xinerama_event_base, xinerama_error_base;
  
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

  /* Select our root window events */
  meta_error_trap_push (display);
  XSelectInput (xdisplay,
                xroot,
                SubstructureRedirectMask | SubstructureNotifyMask |
                ColormapChangeMask | PropertyChangeMask |
                LeaveWindowMask | EnterWindowMask |
                ButtonPressMask | ButtonReleaseMask |
                FocusChangeMask);
  if (meta_error_trap_pop (display) != Success)
    {
      meta_warning (_("Screen %d on display '%s' already has a window manager\n"),
                    number, display->name);
      return NULL;
    }
  
  screen = g_new (MetaScreen, 1);

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

  screen->xinerama_infos = NULL;
  screen->n_xinerama_infos = 0;

#ifdef HAVE_XINERAMA
  if (XineramaQueryExtension (display->xdisplay,
                              &xinerama_event_base,
                              &xinerama_error_base))
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
              
              ++i;
            }
        }
      
      meta_XFree (infos);
    }
  else
    {
      meta_topic (META_DEBUG_XINERAMA,
                  "No Xinerama extension on display %s\n",
                  display->name);
    }
#else
  meta_topic (META_DEBUG_XINERAMA,
              "Metacity compiled without Xinerama support\n");
#endif

  /* If no Xinerama, fill in the single screen info so
   * we can use the field unconditionally
   */
  if (screen->n_xinerama_infos == 0)
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

  g_assert (screen->n_xinerama_infos > 0);
  g_assert (screen->xinerama_infos != NULL);
  
  meta_screen_set_cursor (screen, META_CURSOR_DEFAULT);
  
  if (display->leader_window == None)
    display->leader_window = XCreateSimpleWindow (display->xdisplay,
                                                  screen->xroot,
                                                  -100, -100, 1, 1, 0, 0, 0);

  if (display->no_focus_window == None)
    {
      display->no_focus_window = XCreateSimpleWindow (display->xdisplay,
                                                      screen->xroot,
                                                      -100, -100, 1, 1, 0, 0, 0);
      XSelectInput (display->xdisplay, display->no_focus_window,
                    FocusChangeMask);
      XMapWindow (display->xdisplay, display->no_focus_window);
    }
  
  set_wm_icon_size_hint (screen);
  
  set_supported_hint (screen);
  
  set_wm_check_hint (screen);
  
  /* Screens must have at least one workspace at all times,
   * so create that required workspace.
   */
  screen->active_workspace = meta_workspace_new (screen);
  update_num_workspaces (screen);

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
  meta_prefs_remove_listener (prefs_changed_callback, screen);
  
  meta_screen_ungrab_keys (screen);

  meta_ui_free (screen->ui);

  meta_stack_free (screen->stack);

  meta_error_trap_push (screen->display);
  XSelectInput (screen->display->xdisplay, screen->xroot, 0);
  if (meta_error_trap_pop (screen->display) != Success)
    meta_warning (_("Could not release screen %d on display '%s'\n"),
                  screen->number, screen->display->name);
  
  g_free (screen->screen_name);
  g_free (screen);
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
  
  /* Add missing workspaces. FIXME This will keep setting the
   * number-of-workspaces root window property on each workspace
   * creation, kind of a lame thing
   */
  while (i < new_num)
    {
      meta_workspace_new (screen);
      ++i;
    }
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
  entries[len].xwindow = None;
  entries[len].title = NULL;
  entries[len].icon = NULL;
  
  i = 0;
  tmp = tab_list;
  while (i < len)
    {
      MetaWindow *window;
      MetaRectangle r;
      
      window = tmp->data;
      
      entries[i].xwindow = window->xwindow;
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
  
  screen->tab_popup = meta_ui_tab_popup_new (entries);
  g_free (entries);

  g_slist_free (tab_list);
  
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
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS, "No top window to focus found\n");
    }
}
