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

#endif /* __GTK3_COMPAT_H__ */
