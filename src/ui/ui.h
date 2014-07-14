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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _MetaUI MetaUI;

typedef gboolean (* MetaEventFunc) (XEvent *xevent, gpointer data);

typedef enum
{
  META_UI_DIRECTION_LTR,
  META_UI_DIRECTION_RTL
} MetaUIDirection;

void meta_ui_init (void);

Display* meta_ui_get_display (void);

gint meta_ui_get_screen_number (void);

MetaUI* meta_ui_new (Display *xdisplay,
                     Screen  *screen);
void    meta_ui_free (MetaUI *ui);

void meta_ui_theme_get_frame_borders (MetaUI *ui,
                                      MetaFrameType      type,
                                      MetaFrameFlags     flags,
                                      MetaFrameBorders *borders);
void meta_ui_get_frame_borders (MetaUI *ui,
                                Window frame_xwindow,
                                MetaFrameBorders *borders);

void meta_ui_get_frame_mask (MetaUI *ui,
                             Window frame_xwindow,
                             guint width,
                             guint height,
                             cairo_t *cr);

Window meta_ui_create_frame_window (MetaUI *ui,
                                    Display *xdisplay,
                                    Visual *xvisual,
				    gint x,
				    gint y,
				    gint width,
				    gint height,
				    gint screen_no,
                                    gulong *create_serial);
void meta_ui_destroy_frame_window (MetaUI *ui,
				   Window  xwindow);
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

void meta_ui_unflicker_frame_bg (MetaUI *ui,
                                 Window  xwindow,
                                 int     target_width,
                                 int     target_height);
void meta_ui_reset_frame_bg     (MetaUI *ui,
                                 Window  xwindow);

cairo_region_t *meta_ui_get_frame_bounds (MetaUI  *ui,
                                          Window   xwindow,
                                          int      window_width,
                                          int      window_height);

void meta_ui_queue_frame_draw (MetaUI *ui,
                               Window xwindow);

void meta_ui_set_frame_title (MetaUI *ui,
                              Window xwindow,
                              const char *title);

void meta_ui_update_frame_style (MetaUI  *ui,
                                 Window   window);

void meta_ui_repaint_frame (MetaUI *ui,
                            Window xwindow);


/* FIXME these lack a display arg */
GdkPixbuf* meta_gdk_pixbuf_get_from_pixmap (Pixmap       xpixmap,
                                            int          src_x,
                                            int          src_y,
                                            int          width,
                                            int          height);

gboolean  meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                                 Window   xwindow);

void     meta_ui_set_current_theme (const char *name);
gboolean meta_ui_have_a_theme      (void);

gboolean meta_ui_window_is_widget (MetaUI *ui,
                                   Window  xwindow);

int      meta_ui_get_drag_threshold       (MetaUI *ui);

MetaUIDirection meta_ui_get_direction (void);

#endif
