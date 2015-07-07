/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
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
 *
 * Written by:
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#include "config.h"

#include "core/meta-border.h"

#include <math.h>

static inline float
meta_vector2_cross_product (const MetaVector2 a,
                            const MetaVector2 b)
{
  return a.x * b.y - a.y * b.x;
}

static inline MetaVector2
meta_vector2_add (const MetaVector2 a,
                  const MetaVector2 b)
{
  return (MetaVector2) {
    .x = a.x + b.x,
    .y = a.y + b.y,
  };
}

static inline MetaVector2
meta_vector2_multiply_constant (const float       c,
                                const MetaVector2 a)
{
  return (MetaVector2) {
    .x = c * a.x,
    .y = c * a.y,
  };
}

gboolean
meta_line2_intersects_with (const MetaLine2 *line1,
                            const MetaLine2 *line2,
                            MetaVector2     *intersection)
{
  MetaVector2 p = line1->a;
  MetaVector2 r = meta_vector2_subtract (line1->b, line1->a);
  MetaVector2 q = line2->a;
  MetaVector2 s = meta_vector2_subtract (line2->b, line2->a);
  float rxs;
  float sxr;
  float t;
  float u;

  /*
   * The line (p, r) and (q, s) intersects where
   *
   *   p + t r = q + u s
   *
   * Calculate t:
   *
   *   (p + t r) × s = (q + u s) × s
   *   p × s + t (r × s) = q × s + u (s × s)
   *   p × s + t (r × s) = q × s
   *   t (r × s) = q × s - p × s
   *   t (r × s) = (q - p) × s
   *   t = ((q - p) × s) / (r × s)
   *
   * Using the same method, for u we get:
   *
   *   u = ((p - q) × r) / (s × r)
   */

  rxs = meta_vector2_cross_product (r, s);
  sxr = meta_vector2_cross_product (s, r);

  /* If r × s = 0 then the lines are either parallel or collinear. */
  if (fabsf (rxs) < FLT_MIN)
    return FALSE;

  t = meta_vector2_cross_product (meta_vector2_subtract (q, p), s) / rxs;
  u = meta_vector2_cross_product (meta_vector2_subtract (p, q), r) / sxr;

  /* The lines only intersect if 0 ≤ t ≤ 1 and 0 ≤ u ≤ 1. */
  if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0)
    return FALSE;

  *intersection = meta_vector2_add (p, meta_vector2_multiply_constant (t, r));

  return TRUE;
}

gboolean
meta_border_is_horizontal (MetaBorder *border)
{
  return border->line.a.y == border->line.b.y;
}

gboolean
meta_border_is_blocking_directions (MetaBorder               *border,
                                    MetaBorderMotionDirection directions)
{
  if (meta_border_is_horizontal (border))
    {
      if ((directions & (META_BORDER_MOTION_DIRECTION_POSITIVE_Y |
                         META_BORDER_MOTION_DIRECTION_NEGATIVE_Y)) == 0)
        return FALSE;
    }
  else
    {
      if ((directions & (META_BORDER_MOTION_DIRECTION_POSITIVE_X |
                         META_BORDER_MOTION_DIRECTION_NEGATIVE_X)) == 0)
        return FALSE;
    }

  return (~border->blocking_directions & directions) != directions;
}

unsigned int
meta_border_get_allows_directions (MetaBorder *border)
{
  return ~border->blocking_directions &
    (META_BORDER_MOTION_DIRECTION_POSITIVE_X |
     META_BORDER_MOTION_DIRECTION_POSITIVE_Y |
     META_BORDER_MOTION_DIRECTION_NEGATIVE_X |
     META_BORDER_MOTION_DIRECTION_NEGATIVE_Y);
}

void
meta_border_set_allows_directions (MetaBorder *border, unsigned int directions)
{
  border->blocking_directions =
    ~directions & (META_BORDER_MOTION_DIRECTION_POSITIVE_X |
                   META_BORDER_MOTION_DIRECTION_POSITIVE_Y |
                   META_BORDER_MOTION_DIRECTION_NEGATIVE_X |
                   META_BORDER_MOTION_DIRECTION_NEGATIVE_Y);
}
