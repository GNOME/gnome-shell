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
#include <X11/Xutil.h>

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
  char *desc; /* used in debug spew */
  char *title;

  /* NOTE these four are not in UTF-8, we just treat them as random
   * binary data
   */
  char *res_class;
  char *res_name;
  char *role;
  char *sm_client_id;

  Window xtransient_for;
  Window xgroup_leader;
  Window xclient_leader;
  
  /* Whether we're maximized */
  guint maximized : 1;

  /* Whether we're shaded */
  guint shaded : 1;
  
  /* Mapped is what we think the mapped state should be;
   * so if we get UnmapNotify and mapped == TRUE then
   * it's a withdraw, if mapped == FALSE the UnmapNotify
   * is caused by us.
   */
  guint mapped : 1  ;

  /* Minimize is the state controlled by the minimize button */
  guint minimized : 1;

  /* Iconic is the state in WM_STATE; happens for workspaces/shading
   * in addition to minimize
   */
  guint iconic : 1;
  /* initially_iconic is the WM_HINTS setting when we first manage
   * the window. It's taken to mean initially minimized.
   */
  guint initially_iconic : 1;
  
  /* These are the two flags from WM_PROTOCOLS */
  guint take_focus : 1;
  guint delete_window : 1;
  /* Globally active / No input */
  guint input : 1;

  /* MWM hints */
  guint decorated : 1;
  guint has_close_func : 1;
  guint has_minimize_func : 1;
  guint has_maximize_func : 1;

  /* this flag tracks receipt of focus_in focus_out and
   * determines whether we draw the focus
   */
  guint has_focus : 1;
  
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
  
  /* Requested geometry */
  int border_width;
  /* x/y/w/h here get filled with ConfigureRequest values */
  XSizeHints size_hints;
};

MetaWindow* meta_window_new                (MetaDisplay *display,
                                            Window       xwindow);
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

/* args to move are window pos, not frame pos */
void        meta_window_move               (MetaWindow  *window,
                                            int          root_x_nw,
                                            int          root_y_nw);
void        meta_window_resize             (MetaWindow  *window,
                                            int          w,
                                            int          h);
void        meta_window_move_resize        (MetaWindow  *window,
                                            int          root_x_nw,
                                            int          root_y_nw,
                                            int          w,
                                            int          h);
/* This recalcs the window/frame size, and recalcs the frame
 * size/contents as well.
 */
void        meta_window_queue_move_resize  (MetaWindow  *window);

/* this gets root coords */
void        meta_window_get_position       (MetaWindow  *window,
                                            int         *x,
                                            int         *y);
void        meta_window_delete             (MetaWindow  *window,
                                            Time         timestamp);
void        meta_window_focus              (MetaWindow  *window,
                                            Time         timestamp);
void        meta_window_raise              (MetaWindow  *window);


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
#endif
