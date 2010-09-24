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

#include <config.h>

#include "gdk2-drawing-utils.h"

#include <math.h>

#include "gdk-compat.h"

#ifndef USE_GTK3

static const cairo_user_data_key_t context_key;

cairo_t *
meta_cairo_create (GdkDrawable *drawable)
{
  cairo_t *cr;
  
  cr = gdk_cairo_create (drawable);
  cairo_set_user_data (cr, &context_key, drawable, NULL);

  return cr;
}

static GdkWindow *
extract_window (cairo_t      *cr,
                int          *dx,
                int          *dy,
                GdkRectangle *clip_area)
{
  GdkWindow *window = cairo_get_user_data (cr, &context_key);
  cairo_matrix_t matrix;

  g_assert (dx != NULL);
  g_assert (dy != NULL);
  g_assert (clip_area != NULL);

  /* lots of stuff that mustn't happen because we can't cope with it. */
  if (window == NULL)
    {
      g_error ("Could not get the GdkWindow from the cairo context passed to\n"
               "theme drawing functions. A GdkWindow must be set on all cairo\n"
               "context passed to theme drawing functions when using GTK2.\n"
               "Please use meta_cairo_create() to create the Cairo context.\n");
    }
  cairo_get_matrix (cr, &matrix);
  if (matrix.xx != 1.0 || matrix.yy != 1.0 ||
      matrix.xy != 0.0 || matrix.yx != 0.0 ||
      floor (matrix.x0) != matrix.x0 ||
      floor (matrix.y0) != matrix.y0)
    {
      g_error ("GTK2 drawing requires that the matrix set on the cairo context\n"
               "is an integer translation, however that is not the case.\n");
    }

  gdk_cairo_get_clip_rectangle (cr, clip_area);
  clip_area->x += matrix.x0;
  clip_area->y += matrix.y0;

  *dx = matrix.x0;
  *dy = matrix.y0;

  return window;
}

void
meta_paint_vline (GtkStyle           *style,
                  cairo_t            *cr,
                  GtkStateType        state_type,
                  GtkWidget          *widget,
                  const gchar        *detail,
                  gint                y1_,
                  gint                y2_,
                  gint                x)
{
  int dx, dy;
  GdkWindow *window;
  GdkRectangle area;

  window = extract_window (cr, &dx, &dy, &area);

  gtk_paint_vline (style, window, state_type, &area,
                   widget, detail, y1_ + dy, y2_ + dy, x + dx);
}

void
meta_paint_arrow (GtkStyle           *style,
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
                  gint                height)
{
  int dx, dy;
  GdkWindow *window;
  GdkRectangle area;

  window = extract_window (cr, &dx, &dy, &area);

  gtk_paint_arrow (style, window, state_type, shadow_type,
                   &area, widget, detail, arrow_type,
                   fill, x + dx, y + dy, width, height);
}

void
meta_paint_box (GtkStyle           *style,
                cairo_t            *cr,
                GtkStateType        state_type,
                GtkShadowType       shadow_type,
                GtkWidget          *widget,
                const gchar        *detail,
                gint                x,
                gint                y,
                gint                width,
                gint                height)
{
  int dx, dy;
  GdkWindow *window;
  GdkRectangle area;

  window = extract_window (cr, &dx, &dy, &area);

  gtk_paint_box (style, window, state_type, shadow_type,
                 &area, widget, detail,
                 x + dx, y + dy, width, height);
}

void
meta_paint_flat_box (GtkStyle           *style,
                     cairo_t            *cr,
                     GtkStateType        state_type,
                     GtkShadowType       shadow_type,
                     GtkWidget          *widget,
                     const gchar        *detail,
                     gint                x,
                     gint                y,
                     gint                width,
                     gint                height)
{
  int dx, dy;
  GdkWindow *window;
  GdkRectangle area;

  window = extract_window (cr, &dx, &dy, &area);

  gtk_paint_flat_box (style, window, state_type, shadow_type,
                      &area, widget, detail,
                      x + dx, y + dy, width, height);
}

#endif /* USE_GTK3 */
