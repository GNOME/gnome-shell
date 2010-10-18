/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Utilities for region manipulation
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __META_REGION_UTILS_H__
#define __META_REGION_UTILS_H__

#include <clutter/clutter.h>

#include <cairo.h>
#include <glib.h>

/**
 * MetaRegionIterator:
 * @region: region being iterated
 * @rectangle: current rectangle
 * @line_start: whether the current rectangle starts a horizontal band
 * @line_end: whether the current rectangle ends a horizontal band
 *
 * cairo_region_t is a yx banded region; sometimes its useful to iterate through
 * such a region treating the start and end of each horizontal band in a distinct
 * fashion.
 *
 * Usage:
 *
 *  MetaRegionIterator iter;
 *  for (meta_region_iterator_init (&iter, region);
 *       !meta_region_iterator_at_end (&iter);
 *       meta_region_iterator_next (&iter))
 *  {
 *    [ Use iter.rectangle, iter.line_start, iter.line_end ]
 *  }
 */
typedef struct _MetaRegionIterator MetaRegionIterator;

struct _MetaRegionIterator {
  cairo_region_t *region;
  cairo_rectangle_int_t rectangle;
  gboolean line_start;
  gboolean line_end;
  int i;

  /*< private >*/
  int n_rectangles;
  cairo_rectangle_int_t next_rectangle;
};

void     meta_region_iterator_init      (MetaRegionIterator *iter,
                                         cairo_region_t     *region);
gboolean meta_region_iterator_at_end    (MetaRegionIterator *iter);
void     meta_region_iterator_next      (MetaRegionIterator *iter);

cairo_region_t *meta_make_border_region (cairo_region_t *region,
                                         int             x_amount,
                                         int             y_amount,
                                         gboolean        flip);

#endif /* __META_REGION_UTILS_H__ */
