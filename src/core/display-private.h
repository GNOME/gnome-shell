/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X display handler */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
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

#ifndef META_DISPLAY_PRIVATE_H
#define META_DISPLAY_PRIVATE_H

#ifndef PACKAGE
#error "config.h not included"
#endif

#include <glib.h>
#include <X11/Xlib.h>
#include <meta/common.h>
#include <meta/boxes.h>
#include <meta/display.h>
#include "keybindings-private.h"
#include <meta/prefs.h>
#include <meta/barrier.h>
#include <clutter/clutter.h>

#ifdef HAVE_STARTUP_NOTIFICATION
#include <libsn/sn.h>
#endif

#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

typedef struct _MetaStack      MetaStack;
typedef struct _MetaUISlave    MetaUISlave;

typedef struct _MetaGroupPropHooks  MetaGroupPropHooks;
typedef struct _MetaWindowPropHooks MetaWindowPropHooks;

typedef struct MetaEdgeResistanceData MetaEdgeResistanceData;

typedef void (* MetaWindowPingFunc) (MetaWindow  *window,
                                     guint32      timestamp,
                                     gpointer     user_data);

typedef enum {
  META_LIST_DEFAULT                   = 0,      /* normal windows */
  META_LIST_INCLUDE_OVERRIDE_REDIRECT = 1 << 0, /* normal and O-R */
} MetaListWindowsFlags;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

/* This is basically a bogus number, just has to be large enough
 * to handle the expected case of the alt+tab operation, where
 * we want to ignore serials from UnmapNotify on the tab popup,
 * and the LeaveNotify/EnterNotify from the pointer ungrab. It
 * also has to be big enough to hold ignored serials from the point
 * where we reshape the stage to the point where we get events back.
 */
#define N_IGNORED_CROSSING_SERIALS  10

typedef enum {
  META_TILE_NONE,
  META_TILE_LEFT,
  META_TILE_RIGHT,
  META_TILE_MAXIMIZED
} MetaTileMode;

struct _MetaDisplay
{
  GObject parent_instance;
  
  char *name;
  Display *xdisplay;

  int clutter_event_filter;

  Window leader_window;
  Window timestamp_pinging_window;

  /* Pull in all the names of atoms as fields; we will intern them when the
   * class is constructed.
   */
#define item(x)  Atom atom_##x;
#include <meta/atomnames.h>
#undef item

  /* The window and serial of the most recent FocusIn event. */
  Window server_focus_window;
  gulong server_focus_serial;

  /* Our best guess as to the "currently" focused window (that is, the
   * window that we expect will be focused at the point when the X
   * server processes our next request), and the serial of the request
   * or event that caused this.
   */
  MetaWindow *focus_window;
  /* For windows we've focused that don't necessarily have an X window,
   * like the no_focus_window or the stage X window. */
  Window focus_xwindow;
  gulong focus_serial;

  /* last timestamp passed to XSetInputFocus */
  guint32 last_focus_time;

  /* last user interaction time in any app */
  guint32 last_user_time;

  /* whether we're using mousenav (only relevant for sloppy&mouse focus modes;
   * !mouse_mode means "keynav mode")
   */
  guint mouse_mode : 1;

  /* Helper var used when focus_new_windows setting is 'strict'; only
   * relevant in 'strict' mode and if the focus window is a terminal.
   * In that case, we don't allow new windows to take focus away from
   * a terminal, but if the user explicitly did something that should
   * allow a different window to gain focus (e.g. global keybinding or
   * clicking on a dock), then we will allow the transfer.
   */
  guint allow_terminal_deactivation : 1;

  /* If true, server->focus_serial refers to us changing the focus; in
   * this case, we can ignore focus events that have exactly focus_serial,
   * since we take care to make another request immediately afterwards.
   * But if focus is being changed by another client, we have to accept
   * multiple events with the same serial.
   */
  guint focused_by_us : 1;
  
  /*< private-ish >*/
  guint error_trap_synced_at_last_pop : 1;
  GSList *screens;
  MetaScreen *active_screen;
  GHashTable *xids;
  GHashTable *wayland_windows;
  int error_traps;
  int (* error_trap_handler) (Display     *display,
                              XErrorEvent *error);  
  int server_grab_count;

  /* serials of leave/unmap events that may
   * correspond to an enter event we should
   * ignore
   */
  unsigned long ignored_crossing_serials[N_IGNORED_CROSSING_SERIALS];
  Window ungrab_should_not_cause_focus_window;
  
  guint32 current_time;

  /* We maintain a sequence counter, incremented for each #MetaWindow
   * created.  This is exposed by meta_window_get_stable_sequence()
   * but is otherwise not used inside mutter.
   *
   * It can be useful to plugins which want to sort windows in a
   * stable fashion.
   */
  guint32 window_sequence_counter;

  /* Pings which we're waiting for a reply from */
  GSList     *pending_pings;

  /* Pending focus change */
  guint       focus_timeout_id;

  /* Pending autoraise */
  guint       autoraise_timeout_id;
  MetaWindow* autoraise_window;

  /* Alt+click button grabs */
  ClutterModifierType window_grab_modifiers;
  
  /* current window operation */
  MetaGrabOp  grab_op;
  MetaScreen *grab_screen;
  MetaWindow *grab_window;
  Window      grab_xwindow;
  int         grab_button;
  int         grab_anchor_root_x;
  int         grab_anchor_root_y;
  MetaRectangle grab_anchor_window_pos;
  MetaTileMode  grab_tile_mode;
  int           grab_tile_monitor_number;
  int         grab_latest_motion_x;
  int         grab_latest_motion_y;
  gulong      grab_mask;
  guint       grab_have_pointer : 1;
  guint       grab_have_keyboard : 1;
  guint       grab_frame_action : 1;
  /* During a resize operation, the directions in which we've broken
   * out of the initial maximization state */
  guint       grab_resize_unmaximize : 2; /* MetaMaximizeFlags */
  MetaRectangle grab_initial_window_pos;
  int         grab_initial_x, grab_initial_y;  /* These are only relevant for */
  gboolean    grab_threshold_movement_reached; /* raise_on_click == FALSE.    */
  MetaResizePopup *grab_resize_popup;
  GTimeVal    grab_last_moveresize_time;
  GList*      grab_old_window_stacking;
  MetaEdgeResistanceData *grab_edge_resistance_data;
  unsigned int grab_last_user_action_was_snap;
  guint32     grab_timestamp;

  /* we use property updates as sentinels for certain window focus events
   * to avoid some race conditions on EnterNotify events
   */
  int         sentinel_counter;

#ifdef HAVE_XKB
  int         xkb_base_event_type;
  guint32     last_bell_time;
#endif
  int	      grab_resize_timeout_id;

  /* Keybindings stuff */
  GHashTable     *key_bindings;
  GHashTable     *key_bindings_index;
  int             min_keycode;
  int             max_keycode;
  KeySym *keymap;
  int keysyms_per_keycode;
  XModifierKeymap *modmap;
  unsigned int above_tab_keycode;
  unsigned int ignored_modifier_mask;
  unsigned int num_lock_mask;
  unsigned int scroll_lock_mask;
  unsigned int hyper_mask;
  unsigned int super_mask;
  unsigned int meta_mask;
  MetaKeyCombo overlay_key_combo;
  gboolean overlay_key_only_pressed;
  MetaKeyCombo *iso_next_group_combos;
  int n_iso_next_group_combos;
  
  /* Monitor cache */
  unsigned int monitor_cache_invalidated : 1;

  /* Opening the display */
  unsigned int display_opening : 1;

  /* Closing down the display */
  int closing;

  /* Managed by group.c */
  GHashTable *groups_by_leader;

  /* currently-active window menu if any */
  MetaWindowMenu *window_menu;
  MetaWindow *window_with_menu;

  /* Managed by window-props.c */
  MetaWindowPropHooks *prop_hooks_table;
  GHashTable *prop_hooks;
  int n_prop_hooks;

  /* Managed by group-props.c */
  MetaGroupPropHooks *group_prop_hooks;

  /* Managed by compositor.c */
  MetaCompositor *compositor;

  int render_event_base;
  int render_error_base;

  int composite_event_base;
  int composite_error_base;
  int composite_major_version;
  int composite_minor_version;
  int damage_event_base;
  int damage_error_base;
  int xfixes_event_base;
  int xfixes_error_base;
  int xinput_error_base;
  int xinput_event_base;
  int xinput_opcode;

#ifdef HAVE_STARTUP_NOTIFICATION
  SnDisplay *sn_display;
#endif
#ifdef HAVE_XSYNC
  int xsync_event_base;
  int xsync_error_base;
#endif
#ifdef HAVE_SHAPE
  int shape_event_base;
  int shape_error_base;
#endif
#ifdef HAVE_XSYNC
  unsigned int have_xsync : 1;
#define META_DISPLAY_HAS_XSYNC(display) ((display)->have_xsync)
#else
#define META_DISPLAY_HAS_XSYNC(display) FALSE
#endif
#ifdef HAVE_SHAPE
  unsigned int have_shape : 1;
#define META_DISPLAY_HAS_SHAPE(display) ((display)->have_shape)
#else
#define META_DISPLAY_HAS_SHAPE(display) FALSE
#endif
  unsigned int have_render : 1;
#define META_DISPLAY_HAS_RENDER(display) ((display)->have_render)
  unsigned int have_composite : 1;
  unsigned int have_damage : 1;
#define META_DISPLAY_HAS_COMPOSITE(display) ((display)->have_composite)
#define META_DISPLAY_HAS_DAMAGE(display) ((display)->have_damage)
#ifdef HAVE_XI23
  gboolean have_xinput_23 : 1;
#define META_DISPLAY_HAS_XINPUT_23(display) ((display)->have_xinput_23)
#else
#define META_DISPLAY_HAS_XINPUT_23(display) FALSE
#endif /* HAVE_XI23 */
};

struct _MetaDisplayClass
{
  GObjectClass parent_class;
};

#define XSERVER_TIME_IS_BEFORE_ASSUMING_REAL_TIMESTAMPS(time1, time2) \
  ( (( (time1) < (time2) ) && ( (time2) - (time1) < ((guint32)-1)/2 )) ||     \
    (( (time1) > (time2) ) && ( (time1) - (time2) > ((guint32)-1)/2 ))        \
  )
/**
 * XSERVER_TIME_IS_BEFORE:
 *
 * See the docs for meta_display_xserver_time_is_before().
 */
#define XSERVER_TIME_IS_BEFORE(time1, time2)                          \
  ( (time1) == 0 ||                                                     \
    (XSERVER_TIME_IS_BEFORE_ASSUMING_REAL_TIMESTAMPS(time1, time2) && \
     (time2) != 0)                                                      \
  )

gboolean      meta_display_open                (void);
void          meta_display_close               (MetaDisplay *display,
                                                guint32      timestamp);
MetaScreen*   meta_display_screen_for_x_screen (MetaDisplay *display,
                                                Screen      *screen);
MetaScreen*   meta_display_screen_for_xwindow  (MetaDisplay *display,
                                                Window       xindow);
void          meta_display_grab                (MetaDisplay *display);
void          meta_display_ungrab              (MetaDisplay *display);

void          meta_display_unmanage_windows_for_screen (MetaDisplay *display,
                                                        MetaScreen  *screen,
                                                        guint32      timestamp);

/* Utility function to compare the stacking of two windows */
int           meta_display_stack_cmp           (const void *a,
                                                const void *b);

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

void        meta_display_register_wayland_window   (MetaDisplay *display,
                                                    MetaWindow  *window);
void        meta_display_unregister_wayland_window (MetaDisplay *display,
                                                    MetaWindow  *window);

#ifdef HAVE_XSYNC
MetaWindow* meta_display_lookup_sync_alarm     (MetaDisplay *display,
                                                XSyncAlarm   alarm);
void        meta_display_register_sync_alarm   (MetaDisplay *display,
                                                XSyncAlarm  *alarmp,
                                                MetaWindow  *window);
void        meta_display_unregister_sync_alarm (MetaDisplay *display,
                                                XSyncAlarm   alarm);
#endif /* HAVE_XSYNC */

void        meta_display_notify_window_created (MetaDisplay  *display,
                                                MetaWindow   *window);

GSList*     meta_display_list_windows        (MetaDisplay          *display,
                                              MetaListWindowsFlags  flags);

MetaDisplay* meta_display_for_x_display  (Display     *xdisplay);
MetaDisplay* meta_get_display            (void);

Cursor         meta_display_create_x_cursor (MetaDisplay *display,
                                             MetaCursor   cursor);

void     meta_display_set_grab_op_cursor (MetaDisplay *display,
                                          MetaScreen  *screen,
                                          MetaGrabOp   op,
                                          Window       grab_xwindow,
                                          guint32      timestamp);

void    meta_display_check_threshold_reached (MetaDisplay *display,
                                              int          x,
                                              int          y);
void     meta_display_grab_window_buttons    (MetaDisplay *display,
                                              Window       xwindow);
void     meta_display_ungrab_window_buttons  (MetaDisplay *display,
                                              Window       xwindow);

void meta_display_grab_focus_window_button   (MetaDisplay *display,
                                              MetaWindow  *window);
void meta_display_ungrab_focus_window_button (MetaDisplay *display,
                                              MetaWindow  *window);

/* Next function is defined in edge-resistance.c */
void meta_display_cleanup_edges              (MetaDisplay *display);

/* make a request to ensure the event serial has changed */
void     meta_display_increment_event_serial (MetaDisplay *display);

void     meta_display_update_active_window_hint (MetaDisplay *display);

/* utility goo */
const char* meta_event_mode_to_string   (int m);
const char* meta_event_detail_to_string (int d);

void meta_display_queue_retheme_all_windows (MetaDisplay *display);
void meta_display_retheme_all (void);

void meta_display_set_cursor_theme (const char *theme, 
				    int         size);

void meta_display_ping_window      (MetaWindow         *window,
                                    guint32             timestamp,
                                    MetaWindowPingFunc  ping_reply_func,
                                    MetaWindowPingFunc  ping_timeout_func,
                                    void               *user_data);
void meta_display_pong_for_serial  (MetaDisplay        *display,
                                    guint32             serial);

int meta_resize_gravity_from_grab_op (MetaGrabOp op);

gboolean meta_grab_op_is_moving   (MetaGrabOp op);
gboolean meta_grab_op_is_resizing (MetaGrabOp op);
gboolean meta_grab_op_is_mouse    (MetaGrabOp op);

void meta_display_devirtualize_modifiers (MetaDisplay        *display,
                                          MetaVirtualModifier modifiers,
                                          unsigned int       *mask);

void meta_display_increment_focus_sentinel (MetaDisplay *display);
void meta_display_decrement_focus_sentinel (MetaDisplay *display);
gboolean meta_display_focus_sentinel_clear (MetaDisplay *display);

void meta_display_queue_autoraise_callback  (MetaDisplay *display,
                                             MetaWindow  *window);
void meta_display_remove_autoraise_callback (MetaDisplay *display);

void meta_display_overlay_key_activate (MetaDisplay *display);
void meta_display_accelerator_activate (MetaDisplay     *display,
                                        guint            action,
                                        ClutterKeyEvent *event);
gboolean meta_display_modifiers_accelerator_activate (MetaDisplay *display);

/* In above-tab-keycode.c */
guint meta_display_get_above_tab_keycode (MetaDisplay *display);

#ifdef HAVE_XI23
gboolean meta_display_process_barrier_event (MetaDisplay *display,
                                             XIEvent     *event);
#endif /* HAVE_XI23 */

void meta_display_set_input_focus_xwindow (MetaDisplay *display,
                                           MetaScreen  *screen,
                                           Window       window,
                                           guint32      timestamp);

void meta_display_sync_wayland_input_focus (MetaDisplay *display);

#endif
