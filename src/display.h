/* Metacity X display handler */

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

#ifndef META_DISPLAY_H
#define META_DISPLAY_H

#include <glib.h>
#include <X11/Xlib.h>
#include "eventqueue.h"
#include "common.h"

/* this doesn't really belong here, oh well. */
typedef struct _MetaRectangle MetaRectangle;

struct _MetaRectangle
{
  int x;
  int y;
  int width;
  int height;
};

typedef struct _MetaDisplay   MetaDisplay;
typedef struct _MetaFrame     MetaFrame;
typedef struct _MetaScreen    MetaScreen;
typedef struct _MetaStack     MetaStack;
typedef struct _MetaUISlave   MetaUISlave;
typedef struct _MetaWindow    MetaWindow;
typedef struct _MetaWorkspace MetaWorkspace;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

struct _MetaDisplay
{
  char *name;
  Display *xdisplay;

  Window leader_window;
  
  Atom atom_net_wm_name;
  Atom atom_wm_protocols;
  Atom atom_wm_take_focus;
  Atom atom_wm_delete_window;
  Atom atom_wm_state;
  Atom atom_net_close_window;
  Atom atom_net_wm_state;
  Atom atom_motif_wm_hints;
  Atom atom_net_wm_state_shaded;
  Atom atom_net_wm_state_maximized_horz;
  Atom atom_net_wm_state_maximized_vert;
  Atom atom_net_wm_desktop;
  Atom atom_net_number_of_desktops;
  Atom atom_wm_change_state;
  Atom atom_sm_client_id;
  Atom atom_wm_client_leader;
  Atom atom_wm_window_role;
  Atom atom_net_current_desktop;
  Atom atom_net_supporting_wm_check;
  Atom atom_net_wm_supported;
  Atom atom_net_wm_window_type;
  Atom atom_net_wm_window_type_desktop;
  Atom atom_net_wm_window_type_dock;
  Atom atom_net_wm_window_type_toolbar;
  Atom atom_net_wm_window_type_menu;
  Atom atom_net_wm_window_type_dialog;
  Atom atom_net_wm_window_type_normal;
  Atom atom_net_wm_state_modal;
  Atom atom_net_client_list;
  Atom atom_net_client_list_stacking;
  Atom atom_net_wm_state_skip_taskbar;
  Atom atom_net_wm_state_skip_pager;
  Atom atom_win_workspace;
  Atom atom_win_layer;
  Atom atom_win_protocols;
  Atom atom_win_supporting_wm_check;
  Atom atom_net_wm_icon_name;
  Atom atom_net_wm_icon;
  Atom atom_net_wm_icon_geometry;
  Atom atom_utf8_string;
  Atom atom_wm_icon_size;
  Atom atom_kwm_win_icon;
  
  /* This is the actual window from focus events,
   * not the one we last set
   */
  MetaWindow *focus_window;

  /* Previous focus window */
  MetaWindow *prev_focus_window;
  
  GList *workspaces;
  
  /*< private-ish >*/
  MetaEventQueue *events;
  GSList *screens;
  GHashTable *window_ids;
  GSList *error_traps;
  int server_grab_count;

  /* for double click */
  int double_click_time;
  Time last_button_time;
  Window last_button_xwindow;
  int last_button_num;
  guint is_double_click : 1;
  
  /* current window operation */
  MetaGrabOp  grab_op;
  MetaWindow *grab_window;
  int         grab_button;
  int         grab_root_x;
  int         grab_root_y;
  gulong      grab_mask;
  guint       grab_have_pointer : 1;
  guint       grab_have_keyboard : 1;
  MetaRectangle grab_initial_window_pos;
};

gboolean      meta_display_open                (const char  *name);
void          meta_display_close               (MetaDisplay *display);
MetaScreen*   meta_display_screen_for_root     (MetaDisplay *display,
                                                Window       xroot);
MetaScreen*   meta_display_screen_for_x_screen (MetaDisplay *display,
                                                Screen      *screen);
void          meta_display_grab                (MetaDisplay *display);
void          meta_display_ungrab              (MetaDisplay *display);
gboolean      meta_display_is_double_click     (MetaDisplay *display);

/* A given MetaWindow may have various X windows that "belong"
 * to it, such as the frame window.
 */
MetaWindow* meta_display_lookup_x_window     (MetaDisplay *display,
                                              Window       xwindow);
void        meta_display_register_x_window   (MetaDisplay *display,
                                              Window      *xwindowp,
                                              MetaWindow  *window);
void        meta_display_unregister_x_window (MetaDisplay *display,
                                              Window       xwindow);

GSList*     meta_display_list_windows        (MetaDisplay *display);

MetaDisplay* meta_display_for_x_display  (Display     *xdisplay);
GSList*      meta_displays_list          (void);

MetaWorkspace* meta_display_get_workspace_by_index        (MetaDisplay   *display,
                                                           int            index);
MetaWorkspace* meta_display_get_workspace_by_screen_index (MetaDisplay   *display,
                                                           MetaScreen    *screen,
                                                           int            index);

Cursor         meta_display_create_x_cursor (MetaDisplay *display,
                                             MetaCursor   cursor);

gboolean meta_display_begin_grab_op (MetaDisplay *display,
                                     MetaWindow  *window,
                                     MetaGrabOp   op,
                                     gboolean     pointer_already_grabbed,
                                     int          button,
                                     gulong       modmask,
                                     Time         timestamp,
                                     int          root_x,
                                     int          root_y);
void     meta_display_end_grab_op   (MetaDisplay *display,
                                     Time         timestamp);

void     meta_display_grab_window_buttons    (MetaDisplay *display,
                                              Window       xwindow);
void     meta_display_ungrab_window_buttons  (MetaDisplay *display,
                                              Window       xwindow);


#endif
