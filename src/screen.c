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

#include "screen.h"
#include "util.h"
#include "errors.h"
#include "window.h"
#include "frame.h"
#include "workspace.h"
#include "keybindings.h"
#include "stack.h"

#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <locale.h>
#include <string.h>

static char* get_screen_name (MetaDisplay *display,
                              int          number);


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
  return Success;
}

static int
set_supported_hint (MetaScreen *screen)
{
#define N_SUPPORTED 21
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
  
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_net_wm_supported,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) atoms, N_SUPPORTED);

  return Success;
#undef N_SUPPORTED
}

MetaScreen*
meta_screen_new (MetaDisplay *display,
                 int          number)
{
  MetaScreen *screen;
  Window xroot;
  Display *xdisplay;
  Cursor cursor;
  
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
                ButtonPressMask | ButtonReleaseMask);
  if (meta_error_trap_pop (display) != Success)
    {
      meta_warning (_("Screen %d on display '%s' already has a window manager\n"),
                    number, display->name);
      return NULL;
    }

  cursor = XCreateFontCursor (display->xdisplay, XC_left_ptr);
  XDefineCursor (display->xdisplay, xroot, cursor);
  XFreeCursor (display->xdisplay, cursor);
  
  screen = g_new (MetaScreen, 1);

  screen->display = display;
  screen->number = number;
  screen->screen_name = get_screen_name (display, number);
  screen->xscreen = ScreenOfDisplay (xdisplay, number);
  screen->xroot = xroot;

  if (display->leader_window == None)
    display->leader_window = XCreateSimpleWindow (display->xdisplay,
                                                  screen->xroot,
                                                  -100, -100, 1, 1, 0, 0, 0);
  
  set_supported_hint (screen);
  
  set_wm_check_hint (screen);
  
  /* Screens must have at least one workspace at all times,
   * so create that required workspace.
   */
  screen->active_workspace = meta_workspace_new (screen);
  /* FIXME, for now there are always 6 workspaces... */
  meta_workspace_new (screen);
  meta_workspace_new (screen);
  meta_workspace_new (screen);
  meta_workspace_new (screen);
  meta_workspace_new (screen);

  meta_screen_grab_keys (screen);

  screen->ui = meta_ui_new (screen->display->xdisplay,
                            screen->xscreen);

  screen->stack = meta_stack_new (screen);
  
  meta_verbose ("Added screen %d ('%s') root 0x%lx\n",
                screen->number, screen->screen_name, screen->xroot);  
  
  return screen;
}

void
meta_screen_free (MetaScreen *screen)
{  
  meta_screen_ungrab_keys (screen);
  
  meta_ui_free (screen->ui);

  meta_stack_free (screen->stack);
  
  g_free (screen->screen_name);
  g_free (screen);
}

void
meta_screen_manage_all_windows (MetaScreen *screen)
{
  Window ignored1, ignored2;
  Window *children;
  unsigned int n_children;
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
      
      tmp = tmp->data;
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
