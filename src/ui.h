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

/* Don't include gtk.h or gdk.h here */
#include "common.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _MetaUI MetaUI;

typedef struct _MetaImageWindow MetaImageWindow;

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

MetaWindowMenu* meta_ui_window_menu_new   (MetaUI             *ui,
                                           Window              client_xwindow,
                                           MetaMenuOp          ops,
                                           MetaMenuOp          insensitive,
                                           int                 active_workspace,
                                           int                 n_workspaces,
                                           MetaWindowMenuFunc  func,
                                           gpointer            data);
void            meta_ui_window_menu_popup (MetaWindowMenu     *menu,
                                           int                 root_x,
                                           int                 root_y,
                                           int                 button,
                                           guint32             timestamp);
void            meta_ui_window_menu_free  (MetaWindowMenu     *menu);


MetaImageWindow* meta_image_window_new          (void);
void             meta_image_window_free         (MetaImageWindow *iw);
void             meta_image_window_set_showing  (MetaImageWindow *iw,
                                                 gboolean         showing);
void             meta_image_window_set_image    (MetaImageWindow *iw,
                                                 GdkPixbuf       *pixbuf);
void             meta_image_window_set_position (MetaImageWindow *iw,
                                                 int              x,
                                                 int              y);

/* FIXME these lack a display arg */
GdkPixbuf* meta_gdk_pixbuf_get_from_window (GdkPixbuf   *dest,
                                            Window       xwindow,
                                            int          src_x,
                                            int          src_y,
                                            int          dest_x,
                                            int          dest_y,
                                            int          width,
                                            int          height);

GdkPixbuf* meta_gdk_pixbuf_get_from_pixmap (GdkPixbuf   *dest,
                                            Pixmap       xpixmap,
                                            int          src_x,
                                            int          src_y,
                                            int          dest_x,
                                            int          dest_y,
                                            int          width,
                                            int          height);

/* Used when we have a server grab and draw all over everything,
 * then we need to handle exposes after doing that, instead of
 * during it
 */
void      meta_ui_push_delay_exposes (MetaUI *ui);
void      meta_ui_pop_delay_exposes  (MetaUI *ui);

GdkPixbuf* meta_ui_get_default_window_icon (MetaUI *ui);

gboolean  meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                                 Window   xwindow);

char*     meta_text_property_to_utf8 (Display             *xdisplay,
                                      const XTextProperty *prop);

#include "tabpopup.h"

#endif

