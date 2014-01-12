/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MetaWindowShape
 *
 * Extracted invariant window shape
 *
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __META_WINDOW_SHAPE_H__
#define __META_WINDOW_SHAPE_H__

#include <cairo.h>
#include <glib.h>

/**
 * MetaWindowShape:
 * #MetaWindowShape represents a 9-sliced region with borders on all sides that
 * are unscaled, and a constant central region that is scaled. For example,
 * the regions representing two windows that are rounded rectangles,
 * with the same corner radius but different sizes, have the
 * same MetaWindowShape.
 *
 * #MetaWindowShape is designed to be used as part of a hash table key, so has
 * efficient hash and equal functions.
 */
typedef struct _MetaWindowShape MetaWindowShape;

MetaWindowShape *  meta_window_shape_new         (cairo_region_t  *region);
MetaWindowShape *  meta_window_shape_ref         (MetaWindowShape *shape);
void               meta_window_shape_unref       (MetaWindowShape *shape);
guint              meta_window_shape_hash        (MetaWindowShape *shape);
gboolean           meta_window_shape_equal       (MetaWindowShape *shape_a,
                                                  MetaWindowShape *shape_b);
void               meta_window_shape_get_borders (MetaWindowShape *shape,
                                                  int             *border_top,
                                                  int             *border_right,
                                                  int             *border_bottom,
                                                  int             *border_left);
cairo_region_t    *meta_window_shape_to_region   (MetaWindowShape *shape,
                                                  int              center_width,
                                                  int              center_height);

#endif /* __META_WINDOW_SHAPE_H __*/

