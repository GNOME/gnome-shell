/* Metacity interface used by GTK+ UI to talk to core */

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

#ifndef META_CORE_H
#define META_CORE_H

/* Don't include core headers here */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "frames.h"
#include "common.h"

void meta_core_get_frame_size (Display *xdisplay,
                               Window   frame_xwindow,
                               int     *width,
                               int     *height);

MetaFrameFlags meta_core_get_frame_flags (Display *xdisplay,
                                          Window   frame_xwindow);

void meta_core_queue_frame_resize (Display *xdisplay,
                                   Window frame_xwindow);

/* Move as a result of user operation */
void meta_core_user_move    (Display *xdisplay,
                             Window   frame_xwindow,
                             int      x,
                             int      y);
void meta_core_user_resize  (Display *xdisplay,
                             Window   frame_xwindow,
                             int      width,
                             int      height);

void meta_core_user_raise   (Display *xdisplay,
                             Window   frame_xwindow);

/* get position of client, same coord space expected by move */
void meta_core_get_position (Display *xdisplay,
                             Window   frame_xwindow,
                             int     *x,
                             int     *y);

void meta_core_get_size     (Display *xdisplay,
                             Window   frame_xwindow,
                             int     *width,
                             int     *height);

void meta_core_minimize         (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_unmaximize       (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_maximize         (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_delete           (Display *xdisplay,
                                 Window   frame_xwindow,
                                 guint32  timestamp);
void meta_core_unshade          (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_shade            (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_unstick          (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_stick            (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_change_workspace (Display *xdisplay,
                                 Window   frame_xwindow,
                                 int      new_workspace);


int meta_core_get_num_workspaces (Screen  *xscreen);
int meta_core_get_active_workspace (Screen *xscreen);
int meta_core_get_frame_workspace (Display *xdisplay,
                                   Window frame_xwindow);


void meta_core_show_window_menu (Display *xdisplay,
                                 Window   frame_xwindow,
                                 int      root_x,
                                 int      root_y,
                                 int      button,
                                 Time     timestamp);

#endif


