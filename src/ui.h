/* Metacity interface for talking to GTK+ UI module */

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

#ifndef META_UI_H
#define META_UI_H

/* Don't include gtk.h here */
#include "common.h"
#include <X11/Xlib.h>
#include <glib.h>

typedef struct _MetaUI MetaUI;

typedef gboolean (* MetaEventFunc) (XEvent *xevent, gpointer data);

void meta_ui_init (int *argc, char ***argv);

Display* meta_ui_get_display       (const char *name);

void meta_ui_add_event_func    (Display       *xdisplay,
                                MetaEventFunc  func,
                                gpointer       data);
void meta_ui_remove_event_func (Display       *xdisplay,
                                MetaEventFunc  func,
                                gpointer       data);

MetaUI* meta_ui_new (Display *xdisplay,
                     Screen  *screen);
void    meta_ui_free (MetaUI *ui);

void meta_ui_get_frame_geometry (MetaUI *ui,
                                 Window frame_xwindow,
                                 int *top_height, int *bottom_height,
                                 int *left_width, int *right_width);
void meta_ui_add_frame    (MetaUI *ui,
                           Window  xwindow);
void meta_ui_remove_frame (MetaUI *ui,
                           Window  xwindow);

/* GDK insists on tracking map/unmap */
void meta_ui_map_frame   (MetaUI *ui,
                          Window  xwindow);
void meta_ui_unmap_frame (MetaUI *ui,
                          Window  xwindow);

void meta_ui_reset_frame_bg (MetaUI *ui,
                             Window xwindow);

void meta_ui_queue_frame_draw (MetaUI *ui,
                               Window xwindow);

void meta_ui_set_frame_title (MetaUI *ui,
                              Window xwindow,
                              const char *title);

#endif
