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

#ifndef META_DISPLAY_H
#define META_DISPLAY_H

#ifndef PACKAGE
#error "config.h not included"
#endif

#include <glib.h>
#include <X11/Xlib.h>
#include "eventqueue.h"
#include "common.h"

#ifdef HAVE_STARTUP_NOTIFICATION
#include <libsn/sn.h>
#endif

#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

#define meta_XFree(p) do { if ((p)) XFree ((p)); } while (0)

/* this doesn't really belong here, oh well. */
typedef struct _MetaRectangle MetaRectangle;

struct _MetaRectangle
{
  int x;
  int y;
  int width;
  int height;
};

typedef struct _MetaDisplay    MetaDisplay;
typedef struct _MetaFrame      MetaFrame;
typedef struct _MetaKeyBinding MetaKeyBinding;
typedef struct _MetaScreen     MetaScreen;
typedef struct _MetaStack      MetaStack;
typedef struct _MetaUISlave    MetaUISlave;
typedef struct _MetaWindow     MetaWindow;
typedef struct _MetaWorkspace  MetaWorkspace;

typedef struct _MetaWindowPropHooks MetaWindowPropHooks;
typedef struct _MetaGroupPropHooks  MetaGroupPropHooks;

typedef void (* MetaWindowPingFunc) (MetaDisplay *display,
				     Window       xwindow,
				     gpointer     user_data);


#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

/* This is basically a bogus number, just has to be large enough
 * to handle the expected case of the alt+tab operation, where
 * we want to ignore serials from UnmapNotify on the tab popup,
 * and the LeaveNotify/EnterNotify from the pointer ungrab
 */
#define N_IGNORED_SERIALS           4

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
  Atom atom_net_supported;
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
  Atom atom_net_wm_moveresize;
  Atom atom_net_active_window;
  Atom atom_metacity_restart_message;
  Atom atom_net_wm_strut;
  Atom atom_win_hints;
  Atom atom_metacity_reload_theme_message;
  Atom atom_metacity_set_keybindings_message;
  Atom atom_net_wm_state_hidden;
  Atom atom_net_wm_window_type_utility;
  Atom atom_net_wm_window_type_splash;
  Atom atom_net_wm_ping;
  Atom atom_net_wm_pid;
  Atom atom_wm_client_machine;
  Atom atom_net_wm_state_fullscreen;
  Atom atom_net_workarea;
  Atom atom_net_showing_desktop;
  Atom atom_net_desktop_layout;
  Atom atom_manager;
  Atom atom_targets;
  Atom atom_multiple;
  Atom atom_timestamp;
  Atom atom_version;
  Atom atom_atom_pair;
  Atom atom_net_desktop_names;
  Atom atom_net_wm_allowed_actions;
  Atom atom_net_wm_action_move;
  Atom atom_net_wm_action_resize;
  Atom atom_net_wm_action_shade;
  Atom atom_net_wm_action_stick;
  Atom atom_net_wm_action_maximize_horz;
  Atom atom_net_wm_action_maximize_vert;
  Atom atom_net_wm_action_change_desktop;
  Atom atom_net_wm_action_close;
  Atom atom_net_wm_state_above;
  Atom atom_net_wm_state_below;
  Atom atom_net_startup_id;
  Atom atom_metacity_toggle_verbose;
  Atom atom_metacity_update_counter;
  Atom atom_sync_counter;
  
  /* This is the actual window from focus events,
   * not the one we last set
   */
  MetaWindow *focus_window;

  /* window we are expecting a FocusIn event for
   */
  MetaWindow *expected_focus_window;

  /* Most recently focused list. Always contains all
   * live windows.
   */
  GList *mru_list;  

  guint static_gravity_works : 1;
  
  /*< private-ish >*/
  guint error_trap_synced_at_last_pop : 1;
  MetaEventQueue *events;
  GSList *screens;
  GHashTable *window_ids;
  int error_traps;
  int (* error_trap_handler) (Display     *display,
                              XErrorEvent *error);  
  int server_grab_count;

  /* This window holds the focus when we don't want to focus
   * any actual clients
   */
  Window no_focus_window;
  
  /* for double click */
  int double_click_time;
  Time last_button_time;
  Window last_button_xwindow;
  int last_button_num;
  guint is_double_click : 1;

  /* serials of leave/unmap events that may
   * correspond to an enter event we should
   * ignore
   */
  unsigned long ignored_serials[N_IGNORED_SERIALS];
  Window ungrab_should_not_cause_focus_window;
  
  guint32 current_time;

  /* Pings which we're waiting for a reply from */
  GSList     *pending_pings;

  /* Pending autoraise */
  guint       autoraise_timeout_id;

  /* Alt+click button grabs */
  unsigned int window_grab_modifiers;
  
  /* current window operation */
  MetaGrabOp  grab_op;
  MetaScreen *grab_screen;
  MetaWindow *grab_window;
  Window      grab_xwindow;
  int         grab_button;
  int         grab_initial_root_x;
  int         grab_initial_root_y;
  int         grab_current_root_x;
  int         grab_current_root_y;
  int         grab_latest_motion_x;
  int         grab_latest_motion_y;
  gulong      grab_mask;
  guint       grab_have_pointer : 1;
  guint       grab_have_keyboard : 1;
  MetaRectangle grab_initial_window_pos;
  MetaRectangle grab_current_window_pos;
  MetaResizePopup *grab_resize_popup;
  GTimeVal    grab_last_moveresize_time;
#ifdef HAVE_XSYNC
  /* alarm monitoring client's _METACITY_UPDATE_COUNTER */
  XSyncAlarm  grab_update_alarm;
#endif

  /* Keybindings stuff */
  MetaKeyBinding *screen_bindings;
  int             n_screen_bindings;
  MetaKeyBinding *window_bindings;
  int             n_window_bindings;
  int min_keycode;
  int max_keycode;
  KeySym *keymap;
  int keysyms_per_keycode;
  XModifierKeymap *modmap;
  unsigned int ignored_modifier_mask;
  unsigned int num_lock_mask;
  unsigned int scroll_lock_mask;
  unsigned int hyper_mask;
  unsigned int super_mask;
  unsigned int meta_mask;
  
  /* Xinerama cache */
  unsigned int xinerama_cache_invalidated : 1;

  /* Closing down the display */
  int closing;

  /* Managed by group.c */
  GHashTable *groups_by_leader;

  /* currently-active window menu if any */
  MetaWindowMenu *window_menu;
  MetaWindow *window_with_menu;

  /* Managed by window-props.c */
  MetaWindowPropHooks *prop_hooks;

  /* Managed by group-props.c */
  MetaGroupPropHooks *group_prop_hooks;
  
#ifdef HAVE_STARTUP_NOTIFICATION
  SnDisplay *sn_display;
#endif
#ifdef HAVE_XSYNC
  int xsync_event_base;
  int xsync_error_base;
#define META_DISPLAY_HAS_XSYNC(display) ((display)->xsync_event_base != 0)
#else
#define META_DISPLAY_HAS_XSYNC(display) FALSE
#endif
#ifdef HAVE_SHAPE
  int shape_event_base;
  int shape_error_base;
#define META_DISPLAY_HAS_SHAPE(display) ((display)->shape_event_base != 0)
#else
#define META_DISPLAY_HAS_SHAPE(display) FALSE
#endif
};

gboolean      meta_display_open                (const char  *name);
void          meta_display_close               (MetaDisplay *display);
MetaScreen*   meta_display_screen_for_root     (MetaDisplay *display,
                                                Window       xroot);
MetaScreen*   meta_display_screen_for_x_screen (MetaDisplay *display,
                                                Screen      *screen);
MetaScreen*   meta_display_screen_for_xwindow  (MetaDisplay *display,
                                                Window       xindow);
void          meta_display_grab                (MetaDisplay *display);
void          meta_display_ungrab              (MetaDisplay *display);
gboolean      meta_display_is_double_click     (MetaDisplay *display);

void          meta_display_unmanage_screen     (MetaDisplay *display,
                                                MetaScreen  *screen);

void          meta_display_unmanage_windows_for_screen (MetaDisplay *display,
                                                        MetaScreen  *screen);

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

Cursor         meta_display_create_x_cursor (MetaDisplay *display,
                                             MetaCursor   cursor);

void     meta_display_set_grab_op_cursor (MetaDisplay *display,
                                          MetaScreen  *screen,
                                          MetaGrabOp   op,
                                          gboolean     change_pointer,
                                          Window       grab_xwindow,
                                          Time         timestamp);

gboolean meta_display_begin_grab_op (MetaDisplay *display,
                                     MetaScreen  *screen,
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

void meta_display_grab_focus_window_button   (MetaDisplay *display,
                                              Window       xwindow);
void meta_display_ungrab_focus_window_button (MetaDisplay *display,
                                              Window       xwindow);

/* make a request to ensure the event serial has changed */
void     meta_display_increment_event_serial (MetaDisplay *display);

void     meta_display_update_active_window_hint (MetaDisplay *display);

guint32  meta_display_get_current_time (MetaDisplay *display);

/* utility goo */
const char* meta_event_mode_to_string   (int m);
const char* meta_event_detail_to_string (int d);

void meta_display_queue_retheme_all_windows (MetaDisplay *display);
void meta_display_retheme_all (void);

void     meta_display_ping_window              (MetaDisplay        *display,
						MetaWindow         *window,
						Time                timestamp,
						MetaWindowPingFunc  ping_reply_func,
						MetaWindowPingFunc  ping_timeout_func,
						void               *user_data);
gboolean meta_display_window_has_pending_pings (MetaDisplay        *display,
						MetaWindow         *window);

typedef enum
{
  META_TAB_LIST_NORMAL,
  META_TAB_LIST_DOCKS
  

} MetaTabList;

GSList* meta_display_get_tab_list (MetaDisplay   *display,
                                   MetaTabList    type,
                                   MetaScreen    *screen,
                                   MetaWorkspace *workspace);

MetaWindow* meta_display_get_tab_next (MetaDisplay   *display,
                                       MetaTabList    type,
				       MetaScreen    *screen,
                                       MetaWorkspace *workspace,
                                       MetaWindow    *window,
                                       gboolean       backward);

MetaWindow* meta_display_get_tab_current (MetaDisplay   *display,
                                          MetaTabList    type,
                                          MetaScreen    *screen,
                                          MetaWorkspace *workspace);

int meta_resize_gravity_from_grab_op (MetaGrabOp op);

gboolean meta_grab_op_is_moving   (MetaGrabOp op);
gboolean meta_grab_op_is_resizing (MetaGrabOp op);

gboolean meta_rectangle_intersect (MetaRectangle *src1,
                                   MetaRectangle *src2,
                                   MetaRectangle *dest);

void meta_display_devirtualize_modifiers (MetaDisplay        *display,
                                          MetaVirtualModifier modifiers,
                                          unsigned int       *mask);

#endif
