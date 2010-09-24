/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright (C) 2010 Red Hat Inc.
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

#ifndef __GTK3_COMPAT_H__
#define __GTK3_COMPAT_H__

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION (2, 90, 8)

#define USE_GTK3 1

#define MetaPixmap cairo_surface_t

#define meta_pixmap_new(window, w, h) gdk_window_create_similar_surface (window, CAIRO_CONTENT_COLOR, w, h)
#define meta_pixmap_free(pixmap) cairo_surface_destroy (pixmap)
#define meta_pixmap_cairo_create(pixmap) cairo_create (pixmap)
#define meta_cairo_set_source_pixmap(cr, pixmap, x, y) cairo_set_source_surface (cr, pixmap, x, y)

#define meta_paint_vline gtk_paint_vline
#define meta_paint_box gtk_paint_box
#define meta_paint_arrow gtk_paint_arrow
#define meta_paint_flat_box gtk_paint_flat_box

#else /* GTK_VERSION < 2.90.8 */

#undef USE_GTK3

#define MetaPixmap GdkPixmap

#define meta_pixmap_new(window, w, h) gdk_pixmap_new (window, w, h, -1)
#define meta_pixmap_free(pixmap) g_object_unref (pixmap)
#define meta_pixmap_cairo_create(pixmap) meta_cairo_create (pixmap)
#define meta_cairo_set_source_pixmap(cr, pixmap, x, y) gdk_cairo_set_source_pixmap (cr, pixmap, x, y)

/* This function only exists for GTK2 code. */
cairo_t *     meta_cairo_create            (GdkDrawable        *drawable);

void          meta_paint_vline             (GtkStyle           *style,
                                            cairo_t            *cr,
                                            GtkStateType        state_type,
                                            GtkWidget          *widget,
                                            const gchar        *detail,
                                            gint                y1_,
                                            gint                y2_,
                                            gint                x);
void          meta_paint_arrow             (GtkStyle           *style,
                                            cairo_t            *cr,
                                            GtkStateType        state_type,
                                            GtkShadowType       shadow_type,
                                            GtkWidget          *widget,
                                            const gchar        *detail,
                                            GtkArrowType        arrow_type,
                                            gboolean            fill,
                                            gint                x,
                                            gint                y,
                                            gint                width,
                                            gint                height);
void          meta_paint_box               (GtkStyle           *style,
                                            cairo_t            *cr,
                                            GtkStateType        state_type,
                                            GtkShadowType       shadow_type,
                                            GtkWidget          *widget,
                                            const gchar        *detail,
                                            gint                x,
                                            gint                y,
                                            gint                width,
                                            gint                height);
void          meta_paint_flat_box          (GtkStyle           *style,
                                            cairo_t            *cr,
                                            GtkStateType        state_type,
                                            GtkShadowType       shadow_type,
                                            GtkWidget          *widget,
                                            const gchar        *detail,
                                            gint                x,
                                            gint                y,
                                            gint                width,
                                            gint                height);

#endif /* GTK_VERSION < 2.90.8 */

#endif /* __GTK3_COMPAT_H__ */
