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
  Window xwindow;
  /* may be NULL! not all windows get decorated */
  MetaFrame *frame;
  int depth;
  Visual *xvisual;
  char *desc; /* used in debug spew */
  char *title;

  /* Mapped is what we think the mapped state should be;
   * so if we get UnmapNotify and mapped == TRUE then
   * it's a withdraw, if mapped == FALSE the UnmapNotify
   * is caused by us.
   */
  guint mapped : 1;
  /* Minimize is the state controlled by the minimize button */
  guint minimized : 1;
  /* Iconic is the state in WM_STATE; happens for workspaces/shading
   * in addition to minimize
   */
  guint iconic : 1;
  /* initially_iconic is the WM_HINTS setting when we first manage
   * the window.
   */
  guint initially_iconic : 1;
  
  /* These are the two flags from WM_PROTOCOLS */
  guint take_focus : 1;
  guint delete_window : 1;
  /* Globally active / No input */
  guint input : 1;
  
  /* this flag tracks receipt of focus_in focus_out and
   * determines whether we draw the focus
   */
  guint has_focus : 1;
  
  /* The size we set the window to last. */
  MetaRectangle rect;
  
  /* Requested geometry */
  int border_width;
  /* x/y/w/h here get filled with ConfigureRequest values */
  XSizeHints size_hints;
};

MetaWindow* meta_window_new        (MetaDisplay *display,
                                    Window       xwindow);
void        meta_window_free       (MetaWindow  *window);
void        meta_window_show       (MetaWindow  *window);
void        meta_window_hide       (MetaWindow  *window);
void        meta_window_minimize   (MetaWindow  *window);
void        meta_window_unminimize (MetaWindow  *window);
void        meta_window_resize    (MetaWindow  *window,
                                   int          w,
                                   int          h);
void        meta_window_delete    (MetaWindow  *window,
                                   Time         timestamp);
void        meta_window_focus     (MetaWindow  *window,
                                   Time         timestamp);

/* Sends a client message */
void meta_window_send_icccm_message (MetaWindow *window,
                                     Atom        atom,
                                     Time        timestamp);


gboolean meta_window_configure_request (MetaWindow *window,
                                        XEvent     *event);
gboolean meta_window_property_notify   (MetaWindow *window,
                                        XEvent     *event);





#endif
