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
#include <gdk/gdkx.h>
#include "common.h"

void meta_core_get_client_size (Display *xdisplay,
                                Window   frame_xwindow,
                                int     *width,
                                int     *height);

Window meta_core_get_client_xwindow (Display *xdisplay,
                                     Window   frame_xwindow);

MetaFrameFlags meta_core_get_frame_flags (Display *xdisplay,
                                          Window   frame_xwindow);
MetaFrameType  meta_core_get_frame_type   (Display *xdisplay,
                                           Window   frame_xwindow);

GdkPixbuf* meta_core_get_mini_icon (Display *xdisplay,
                                    Window   frame_xwindow);
GdkPixbuf* meta_core_get_icon      (Display *xdisplay,
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
                             int      gravity,
                             int      width,
                             int      height);

void meta_core_user_raise   (Display *xdisplay,
                             Window   frame_xwindow);
void meta_core_user_lower   (Display *xdisplay,
                             Window   frame_xwindow);

void meta_core_user_focus   (Display *xdisplay,
                             Window   frame_xwindow,
                             Time     timestamp);

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
void meta_core_toggle_maximize  (Display *xdisplay,
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
const char* meta_core_get_workspace_name_with_index (Display *xdisplay,
                                                     Window xroot,
                                                     int    index);

void  meta_core_get_frame_extents   (Display        *xdisplay,
                                     Window          frame_xwindow,
                                     int            *x,
                                     int            *y,
                                     int            *width,
                                     int            *height);


void meta_core_show_window_menu (Display *xdisplay,
                                 Window   frame_xwindow,
                                 int      root_x,
                                 int      root_y,
                                 int      button,
                                 Time     timestamp);

void meta_core_get_menu_accelerator (MetaMenuOp           menu_op,
                                     int                  workspace,
                                     unsigned int        *keysym,
                                     MetaVirtualModifier *modifiers);

gboolean   meta_core_begin_grab_op (Display    *xdisplay,
                                    Window      frame_xwindow,
                                    MetaGrabOp  op,
                                    gboolean    pointer_already_grabbed,
                                    int         event_serial,
                                    int         button,
                                    gulong      modmask,
                                    Time        timestamp,
                                    int         root_x,
                                    int         root_y);
void       meta_core_end_grab_op   (Display    *xdisplay,
                                    Time        timestamp);
MetaGrabOp meta_core_get_grab_op     (Display    *xdisplay);
Window     meta_core_get_grab_frame  (Display   *xdisplay);
int        meta_core_get_grab_button (Display  *xdisplay);


void       meta_core_grab_buttons  (Display *xdisplay,
                                    Window   frame_xwindow);

void       meta_core_set_screen_cursor (Display *xdisplay,
                                        Window   frame_on_screen,
                                        MetaCursor cursor);

void       meta_core_get_screen_size (Display *xdisplay,
                                      Window   frame_on_screen,
                                      int     *width,
                                      int     *height);

/* Used because we ignore EnterNotify when a window is unmapped that
 * really shouldn't cause focus changes, by comparing the event serial
 * of the EnterNotify and the UnmapNotify.
 */
void meta_core_increment_event_serial (Display *display);

int meta_ui_get_last_event_serial (Display *xdisplay);

#endif




