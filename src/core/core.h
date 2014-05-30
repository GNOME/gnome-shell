/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter interface used by GTK+ UI to talk to core */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005 Elijah Newren
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

#ifndef META_CORE_H
#define META_CORE_H

/* Don't include core headers here */
#include <gdk/gdkx.h>
#include <meta/common.h>
#include <meta/boxes.h>

typedef enum
{
  META_CORE_GET_END = 0,
  META_CORE_WINDOW_HAS_FRAME,
  META_CORE_GET_CLIENT_WIDTH,
  META_CORE_GET_CLIENT_HEIGHT,
  META_CORE_GET_FRAME_FLAGS,
  META_CORE_GET_FRAME_TYPE,
  META_CORE_GET_MINI_ICON,
  META_CORE_GET_ICON,
  META_CORE_GET_FRAME_RECT,
  META_CORE_GET_THEME_VARIANT,
} MetaCoreGetType;

/* General information function about the given window. Pass in a sequence of
 * pairs of MetaCoreGetTypes and pointers to variables; the variables will be
 * filled with the requested values. End the list with META_CORE_GET_END.
 * For example:
 *
 *   meta_core_get (my_display, my_window,
 *                  META_CORE_GET_FRAME_WIDTH, &width,
 *                  META_CORE_GET_FRAME_HEIGHT, &height,
 *                  META_CORE_GET_END);
 *
 * If the window doesn't have a frame, this will raise a meta_bug. To suppress
 * this behaviour, ask META_CORE_WINDOW_HAS_FRAME as the *first* question in
 * the list. If the window has no frame, the answer to this question will be
 * False, and anything else you asked will be undefined. Otherwise, the answer
 * will be True. The answer will necessarily be True if you ask the question
 * in any other position. The positions of all other questions don't matter.
 *
 * The reason for this function is that some parts of the program don't know
 * about MetaWindows. But they *can* see core.h. So we used to have a whole
 * load of functions which took a display and an X window, looked up the
 * relevant MetaWindow, and returned information about it. The trouble with
 * that is that looking up the MetaWindow is a nontrivial operation, and
 * consolidating the calls in this way makes (for example) frame exposes
 * 33% faster, according to valgrind.
 *
 * This function would perhaps be slightly better if the questions were
 * represented by pointers, perhaps gchar*s, because then we could take
 * advantage of gcc's automatic sentinel checking. On the other hand, this
 * immediately suggests string comparison, and that's slow.
 *
 * Another possible improvement is that core.h still has a bunch of
 * functions which can't be described by the formula "give a display and
 * an X window, get a single value" (meta_core_user_move, for example), but
 * which could theoretically be handled by this function if we relaxed the
 * requirement that all questions should have exactly one argument.
 */
void meta_core_get (Display *xdisplay,
                    Window window,
                    ...);

void meta_core_queue_frame_resize (Display *xdisplay,
                                   Window frame_xwindow);

void meta_core_user_lower_and_unfocus (Display *xdisplay,
                                       Window   frame_xwindow,
                                       guint32  timestamp);

void meta_core_user_focus   (Display *xdisplay,
                             Window   frame_xwindow,
                             guint32  timestamp);

void meta_core_minimize         (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_toggle_maximize  (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_toggle_maximize_horizontally  (Display *xdisplay,
                                              Window   frame_xwindow);
void meta_core_toggle_maximize_vertically    (Display *xdisplay,
                                              Window   frame_xwindow);
void meta_core_unmaximize       (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_maximize         (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_delete           (Display *xdisplay,
                                 Window   frame_xwindow,
                                 guint32  timestamp);
void meta_core_unshade          (Display *xdisplay,
                                 Window   frame_xwindow,
                                 guint32  timestamp);
void meta_core_shade            (Display *xdisplay,
                                 Window   frame_xwindow,
                                 guint32  timestamp);
void meta_core_unstick          (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_stick            (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_unmake_above     (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_make_above       (Display *xdisplay,
                                 Window   frame_xwindow);
void meta_core_change_workspace (Display *xdisplay,
                                 Window   frame_xwindow,
                                 int      new_workspace);

int meta_core_get_frame_workspace (Display *xdisplay,
                                   Window frame_xwindow);
const char* meta_core_get_workspace_name_with_index (Display *xdisplay,
                                                     Window xroot,
                                                     int    index);

void meta_core_show_window_menu (Display            *xdisplay,
                                 Window              frame_xwindow,
                                 MetaWindowMenuType  menu,
                                 int                 root_x,
                                 int                 root_y,
                                 guint32             timestamp);

void meta_core_show_window_menu_for_rect (Display            *xdisplay,
                                          Window              frame_xwindow,
                                          MetaWindowMenuType  menu,
                                          MetaRectangle      *rect,
                                          guint32             timestamp);

gboolean   meta_core_begin_grab_op (Display    *xdisplay,
                                    Window      frame_xwindow,
                                    MetaGrabOp  op,
                                    gboolean    pointer_already_grabbed,
                                    gboolean    frame_action,
                                    int         button,
                                    gulong      modmask,
                                    guint32     timestamp,
                                    int         root_x,
                                    int         root_y);
void       meta_core_end_grab_op   (Display    *xdisplay,
                                    guint32     timestamp);
MetaGrabOp meta_core_get_grab_op     (Display    *xdisplay);


void       meta_core_grab_buttons  (Display *xdisplay,
                                    Window   frame_xwindow);

void       meta_core_set_screen_cursor (Display *xdisplay,
                                        Window   frame_on_screen,
                                        MetaCursor cursor);

void meta_invalidate_default_icons (void);

void meta_core_add_old_event_mask (Display     *xdisplay,
                                   Window       xwindow,
                                   XIEventMask *mask);

#endif
