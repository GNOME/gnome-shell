/* Metacity X managed windows */

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

#ifndef META_WINDOW_H
#define META_WINDOW_H

#include "screen.h"
#include "util.h"
#include "stack.h"
#include "iconcache.h"
#include <X11/Xutil.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _MetaGroup MetaGroup;

typedef void (*MetaWindowForeachFunc) (MetaWindow *window,
                                       void       *data);

typedef enum
{
  META_WINDOW_NORMAL,
  META_WINDOW_DESKTOP,
  META_WINDOW_DOCK,
  META_WINDOW_DIALOG,
  META_WINDOW_MODAL_DIALOG,
  META_WINDOW_TOOLBAR,
  META_WINDOW_MENU,
  META_WINDOW_UTILITY,
  META_WINDOW_SPLASHSCREEN
} MetaWindowType;

struct _MetaWindow
{
  MetaDisplay *display;
  MetaScreen *screen;
  GList *workspaces;
  Window xwindow;
  /* may be NULL! not all windows get decorated */
  MetaFrame *frame;
  int depth;
  Visual *xvisual;
  Colormap colormap;
  char *desc; /* used in debug spew */
  char *title;

  char *icon_name;
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
  MetaIconCache icon_cache;
  Pixmap wm_hints_pixmap;
  Pixmap wm_hints_mask;
  
  MetaWindowType type;
  Atom type_atom;
  
  /* NOTE these five are not in UTF-8, we just treat them as random
   * binary data
   */
  char *res_class;
  char *res_name;
  char *role;
  char *sm_client_id;
  char *wm_client_machine;
  char *startup_id;

  int net_wm_pid;
  
  Window xtransient_for;
  Window xgroup_leader;
  Window xclient_leader;

  /* Initial workspace property */
  int initial_workspace;  
  
  /* Whether we're maximized */
  guint maximized : 1;

  /* Whether we're shaded */
  guint shaded : 1;

  /* Whether we're fullscreen */
  guint fullscreen : 1;
  
  /* Whether we're sticky in the multi-workspace sense
   * (vs. the not-scroll-with-viewport sense, we don't
   * have no stupid viewports)
   */
  guint on_all_workspaces : 1;

  /* Minimize is the state controlled by the minimize button */
  guint minimized : 1;

  /* Whether the window is mapped; actual server-side state
   * see also unmaps_pending
   */
  guint mapped : 1;
  
  /* Iconic is the state in WM_STATE; happens for workspaces/shading
   * in addition to minimize
   */
  guint iconic : 1;
  /* initially_iconic is the WM_HINTS setting when we first manage
   * the window. It's taken to mean initially minimized.
   */
  guint initially_iconic : 1;

  /* whether an initial workspace was explicitly set */
  guint initial_workspace_set : 1;
  
  /* These are the flags from WM_PROTOCOLS */
  guint take_focus : 1;
  guint delete_window : 1;
  guint net_wm_ping : 1;
  /* Globally active / No input */
  guint input : 1;
  
  /* MWM hints about features of window */
  guint mwm_decorated : 1;
  guint mwm_border_only : 1;
  guint mwm_has_close_func : 1;
  guint mwm_has_minimize_func : 1;
  guint mwm_has_maximize_func : 1;
  guint mwm_has_move_func : 1;
  guint mwm_has_resize_func : 1;
  
  /* Computed features of window */
  guint decorated : 1;
  guint border_only : 1;
  guint always_sticky : 1;
  guint has_close_func : 1;
  guint has_minimize_func : 1;
  guint has_maximize_func : 1;
  guint has_shade_func : 1;
  guint has_move_func : 1;
  guint has_resize_func : 1;
  guint has_fullscreen_func : 1;
  
  /* Weird "_NET_WM_STATE_MODAL" flag */
  guint wm_state_modal : 1;

  /* TRUE if the client forced these on */
  guint wm_state_skip_taskbar : 1;
  guint wm_state_skip_pager : 1;

  /* Computed whether to skip taskbar or not */
  guint skip_taskbar : 1;
  guint skip_pager : 1;

  /* TRUE if client set these */
  guint wm_state_above : 1;
  guint wm_state_below : 1;
  
  /* this flag tracks receipt of focus_in focus_out and
   * determines whether we draw the focus
   */
  guint has_focus : 1;
  
  /* Track whether the user has ever manually modified
   * the window; if so, we can use the saved user size/pos
   */
  guint user_has_move_resized : 1;

  /* Have we placed this window? */
  guint placed : 1;

  /* Are we in meta_window_free()? */
  guint unmanaging : 1;

  /* Are we in the calc_showing queue? */
  guint calc_showing_queued : 1;

  /* Are we in the move_resize queue? */
  guint move_resize_queued : 1;
  
  /* Used by keybindings.c */
  guint keys_grabbed : 1;     /* normal keybindings grabbed */
  guint grab_on_frame : 1;    /* grabs are on the frame */
  guint all_keys_grabbed : 1; /* AnyKey grabbed */
  
  /* Set if the reason for unmanaging the window is that
   * it was withdrawn
   */
  guint withdrawn : 1;

  /* TRUE if constrain_position should calc placement.
   * only relevant if !window->placed
   */
  guint calc_placement : 1;

  /* Has nonzero struts */
  guint has_struts : 1; 
  /* Struts are from the _WIN_HINTS do not cover deal */
  guint do_not_cover : 1;

  /* Transient parent is a root window */
  guint transient_parent_is_root_window : 1;

  /* Info on which props we got our attributes from */
  guint using_net_wm_name : 1; /* vs. plain wm_name */
  guint using_net_wm_icon_name : 1; /* vs. plain wm_icon_name */
  
  /* Number of UnmapNotify that are caused by us, if
   * we get UnmapNotify with none pending then the client
   * is withdrawing the window.
   */
  int unmaps_pending;
  
  /* The size we set the window to last (i.e. what we believe
   * to be its actual size on the server). The x, y are
   * the actual server-side x,y so are relative to the frame
   * or the root window as appropriate.
   */
  MetaRectangle rect;

  /* The geometry to restore when we unmaximize.
   * The position is in root window coords, even if
   * there's a frame, which contrasts with window->rect
   * above.
   */
  MetaRectangle saved_rect;

  /* This is the geometry the window had after the last user-initiated
   * move/resize operations. We use this whenever we are moving the
   * implicitly (for example, if we move to avoid a panel, we
   * can snap back to this position if the panel moves again)
   *
   * Position valid if user_has_moved, size valid if user_has_resized
   *
   * Position always in root coords, unlike window->rect
   */
  MetaRectangle user_rect;
  
  /* Requested geometry */
  int border_width;
  /* x/y/w/h here get filled with ConfigureRequest values */
  XSizeHints size_hints;

  /* struts */
  int left_strut;
  int right_strut;
  int top_strut;
  int bottom_strut;
  
  /* Managed by stack.c */
  MetaStackLayer layer;
  int stack_position; /* see comment in stack.h */
  
  /* Current dialog open for this window */
  int dialog_pid;
  int dialog_pipe;

  /* maintained by group.c */
  MetaGroup *cached_group;
};

#define META_WINDOW_ALLOWS_MOVE(w)     ((w)->has_move_func && !(w)->maximized && !(w)->fullscreen)
#define META_WINDOW_ALLOWS_RESIZE_EXCEPT_HINTS(w)   ((w)->has_resize_func && !(w)->maximized && !(w)->fullscreen && !(w)->shaded)
#define META_WINDOW_ALLOWS_RESIZE(w)   (META_WINDOW_ALLOWS_RESIZE_EXCEPT_HINTS (w) &&                \
                                        (((w)->size_hints.min_width < (w)->size_hints.max_width) ||  \
                                         ((w)->size_hints.min_height < (w)->size_hints.max_height)))
#define META_WINDOW_ALLOWS_HORIZONTAL_RESIZE(w) (META_WINDOW_ALLOWS_RESIZE_EXCEPT_HINTS (w) && (w)->size_hints.min_width < (w)->size_hints.max_width)
#define META_WINDOW_ALLOWS_VERTICAL_RESIZE(w)   (META_WINDOW_ALLOWS_RESIZE_EXCEPT_HINTS (w) && (w)->size_hints.min_height < (w)->size_hints.max_height)

MetaWindow* meta_window_new                (MetaDisplay *display,
                                            Window       xwindow,
                                            gboolean     must_be_viewable);
void        meta_window_free               (MetaWindow  *window);
void        meta_window_calc_showing       (MetaWindow  *window);
void        meta_window_queue_calc_showing (MetaWindow  *window);
void        meta_window_minimize           (MetaWindow  *window);
void        meta_window_unminimize         (MetaWindow  *window);
void        meta_window_maximize           (MetaWindow  *window);
void        meta_window_unmaximize         (MetaWindow  *window);
void        meta_window_shade              (MetaWindow  *window);
void        meta_window_unshade            (MetaWindow  *window);
void        meta_window_change_workspace   (MetaWindow  *window,
                                            MetaWorkspace *workspace);
void        meta_window_stick              (MetaWindow  *window);
void        meta_window_unstick            (MetaWindow  *window);

void        meta_window_activate           (MetaWindow  *window,
                                            guint32      current_time);
void        meta_window_make_fullscreen    (MetaWindow  *window);
void        meta_window_unmake_fullscreen  (MetaWindow  *window);

/* args to move are window pos, not frame pos */
void        meta_window_move               (MetaWindow  *window,
                                            gboolean     user_op,
                                            int          root_x_nw,
                                            int          root_y_nw);
void        meta_window_resize             (MetaWindow  *window,
                                            gboolean     user_op,
                                            int          w,
                                            int          h);
void        meta_window_move_resize        (MetaWindow  *window,
                                            gboolean     user_op,
                                            int          root_x_nw,
                                            int          root_y_nw,
                                            int          w,
                                            int          h);
void        meta_window_resize_with_gravity (MetaWindow  *window,
                                             gboolean     user_op,
                                             int          w,
                                             int          h,
                                             int          gravity);

/* This recalcs the window/frame size, and recalcs the frame
 * size/contents as well.
 */
void        meta_window_queue_move_resize  (MetaWindow  *window);

/* this gets root coords */
void        meta_window_get_position       (MetaWindow  *window,
                                            int         *x,
                                            int         *y);
void        meta_window_get_user_position  (MetaWindow  *window,
                                            int         *x,
                                            int         *y);
/* gets position we need to set to stay in current position,
 * assuming position will be gravity-compensated. i.e.
 * this is the position a client would send in a configure
 * request.
 */
void        meta_window_get_gravity_position (MetaWindow  *window,
                                              int         *x,
                                              int         *y);
/* Get geometry for saving in the session; x/y are gravity
 * position, and w/h are in resize inc above the base size.
 */
void        meta_window_get_geometry         (MetaWindow  *window,
                                              int         *x,
                                              int         *y,
                                              int         *width,
                                              int         *height);
void        meta_window_get_outer_rect       (MetaWindow    *window,
                                              MetaRectangle *rect);
void        meta_window_delete             (MetaWindow  *window,
                                            Time         timestamp);
void        meta_window_kill               (MetaWindow  *window);
void        meta_window_focus              (MetaWindow  *window,
                                            Time         timestamp);
void        meta_window_raise              (MetaWindow  *window);
void        meta_window_lower              (MetaWindow  *window);

void        meta_window_update_unfocused_button_grabs (MetaWindow *window);

/* Sends a client message */
void meta_window_send_icccm_message (MetaWindow *window,
                                     Atom        atom,
                                     Time        timestamp);


gboolean meta_window_configure_request (MetaWindow *window,
                                        XEvent     *event);
gboolean meta_window_property_notify   (MetaWindow *window,
                                        XEvent     *event);
gboolean meta_window_client_message    (MetaWindow *window,
                                        XEvent     *event);
gboolean meta_window_notify_focus      (MetaWindow *window,
                                        XEvent     *event);

void     meta_window_set_current_workspace_hint (MetaWindow *window);

unsigned long meta_window_get_net_wm_desktop (MetaWindow *window);

void meta_window_show_menu (MetaWindow *window,
                            int         root_x,
                            int         root_y,
                            int         button,
                            Time        timestamp);

gboolean meta_window_shares_some_workspace (MetaWindow *window,
                                            MetaWindow *with);

void meta_window_set_gravity (MetaWindow *window,
                              int         gravity);

void meta_window_handle_mouse_grab_op_event (MetaWindow *window,
                                             XEvent     *event);

gboolean meta_window_visible_on_workspace (MetaWindow    *window,
                                           MetaWorkspace *workspace);

/* Get minimum work area for all workspaces we're on */
void meta_window_get_work_area (MetaWindow    *window,
                                gboolean       for_current_xinerama,
                                MetaRectangle *area);

gboolean meta_window_same_application (MetaWindow *window,
                                       MetaWindow *other_window);

#define META_WINDOW_IN_NORMAL_TAB_CHAIN_TYPE(w) \
  ((w)->type != META_WINDOW_DOCK && (w)->type != META_WINDOW_DESKTOP)
#define META_WINDOW_IN_NORMAL_TAB_CHAIN(w) \
  (((w)->input || (w)->take_focus) && META_WINDOW_IN_NORMAL_TAB_CHAIN_TYPE (w))
#define META_WINDOW_IN_DOCK_TAB_CHAIN(w) \
  (((w)->input || (w)->take_focus) && ! META_WINDOW_IN_NORMAL_TAB_CHAIN_TYPE (w))

void meta_window_refresh_resize_popup (MetaWindow *window);

void meta_window_free_delete_dialog (MetaWindow *window);

void meta_window_foreach_transient (MetaWindow            *window,
                                    MetaWindowForeachFunc  func,
                                    void                  *data);
gboolean meta_window_is_ancestor_of_transient (MetaWindow *window,
                                               MetaWindow *transient);

gboolean meta_window_warp_pointer (MetaWindow *window,
                                   MetaGrabOp  grab_op);

void meta_window_begin_grab_op (MetaWindow *window,
                                MetaGrabOp  op,
                                Time        timestamp);

void meta_window_update_resize_grab_op (MetaWindow *window,
                                        gboolean    update_cursor);

void meta_window_update_layer (MetaWindow *window);

gboolean meta_window_get_icon_geometry (MetaWindow    *window,
                                        MetaRectangle *rect);

const char* meta_window_get_startup_id (MetaWindow *window);

#endif
