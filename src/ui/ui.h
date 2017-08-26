/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter interface for talking to GTK+ UI module */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_UI_H
#define META_UI_H

/* Don't include gtk.h or gdk.h here */
#include <meta/common.h>
#include <meta/types.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _MetaUI MetaUI;
typedef struct _MetaUIFrame MetaUIFrame;

typedef gboolean (* MetaEventFunc) (XEvent *xevent, gpointer data);

MetaUI *meta_ui_new  (MetaX11Display *x11_display);
void    meta_ui_free (MetaUI *ui);

void meta_ui_theme_get_frame_borders (MetaUI *ui,
                                      MetaFrameType      type,
                                      MetaFrameFlags     flags,
                                      MetaFrameBorders *borders);

MetaUIFrame * meta_ui_create_frame (MetaUI *ui,
                                    Display *xdisplay,
                                    MetaWindow *meta_window,
                                    Visual *xvisual,
                                    gint x,
                                    gint y,
                                    gint width,
                                    gint height,
                                    gulong *create_serial);
void meta_ui_move_resize_frame (MetaUI *ui,
				Window frame,
				int x,
				int y,
				int width,
				int height);

/* GDK insists on tracking map/unmap */
void meta_ui_map_frame   (MetaUI *ui,
                          Window  xwindow);
void meta_ui_unmap_frame (MetaUI *ui,
                          Window  xwindow);

gboolean  meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                                 Window   xwindow);

gboolean meta_ui_window_is_widget (MetaUI *ui,
                                   Window  xwindow);
gboolean meta_ui_window_is_dummy  (MetaUI *ui,
                                   Window  xwindow);

#endif
