/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Utilities for use with Cogl
 *
 * Copyright 2010 Red Hat, Inc.
 * Copyright 2010 Intel Corporation
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

#include "clutter-utils.h"
#include <math.h>

/* This file uses pixel-aligned region computation to determine what
 * can be clipped out. This only really works if everything is aligned
 * to the pixel grid - not scaled or rotated and at integer offsets.
 *
 * (This could be relaxed - if we turned off filtering for unscaled
 * windows then windows would be, by definition aligned to the pixel
 * grid. And for rectangular windows without a shape, the outline that
 * we draw for an unrotated window is always a rectangle because we
 * don't use antialasing for the window boundary - with or without
 * filtering, with or without a scale. But figuring out exactly
 * what pixels will be drawn by the graphics system in these cases
 * gets tricky, so we just go for the easiest part - no scale,
 * and at integer offsets.)
 *
 * The way we check for pixel-aligned is by looking at the
 * transformation into screen space of the allocation box of an actor
 * and and checking if the corners are "close enough" to integral
 * pixel values.
 */

/* The definition of "close enough" to integral pixel values is
 * equality when we convert to 24.8 fixed-point.
 */
static inline int
round_to_fixed (float x)
{
  return roundf (x * 256);
}

/* This helper function checks if (according to our fixed point precision)
 * the vertices @verts form a box of width @widthf and height @heightf
 * located at integral coordinates. These coordinates are returned
 * in @x_origin and @y_origin.
 */
gboolean
meta_actor_vertices_are_untransformed (ClutterVertex *verts,
                                       float          widthf,
                                       float          heightf,
                                       int           *x_origin,
                                       int           *y_origin)
{
  int width, height;
  int v0x, v0y, v1x, v1y, v2x, v2y, v3x, v3y;
  int x, y;

  width = round_to_fixed (widthf); height = round_to_fixed (heightf);

  v0x = round_to_fixed (verts[0].x); v0y = round_to_fixed (verts[0].y);
  v1x = round_to_fixed (verts[1].x); v1y = round_to_fixed (verts[1].y);
  v2x = round_to_fixed (verts[2].x); v2y = round_to_fixed (verts[2].y);
  v3x = round_to_fixed (verts[3].x); v3y = round_to_fixed (verts[3].y);

  /* Using shifting for converting fixed => int, gets things right for
   * negative values. / 256. wouldn't do the same
   */
  x = v0x >> 8;
  y = v0y >> 8;

  /* At integral coordinates? */
  if (x * 256 != v0x || y * 256 != v0y)
    return FALSE;

  /* Not scaled? */
  if (v1x - v0x != width || v2y - v0y != height)
    return FALSE;

  /* Not rotated/skewed? */
  if (v0x != v2x || v0y != v1y ||
      v3x != v1x || v3y != v2y)
    return FALSE;

  *x_origin = x;
  *y_origin = y;

  return TRUE;
}

/* Check if an actor is "untransformed" - which actually means transformed by
 * at most a integer-translation. The integer translation, if any, is returned.
 */
gboolean
meta_actor_is_untransformed (ClutterActor *actor,
                             int          *x_origin,
                             int          *y_origin)
{
  gfloat widthf, heightf;
  ClutterVertex verts[4];

  clutter_actor_get_size (actor, &widthf, &heightf);
  clutter_actor_get_abs_allocation_vertices (actor, verts);

  return meta_actor_vertices_are_untransformed (verts, widthf, heightf, x_origin, y_origin);
}

