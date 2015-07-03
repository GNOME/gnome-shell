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

#ifndef META_BORDER_H
#define META_BORDER_H

#include <glib.h>

typedef enum
{
  META_BORDER_MOTION_DIRECTION_POSITIVE_X = 1 << 0,
  META_BORDER_MOTION_DIRECTION_POSITIVE_Y = 1 << 1,
  META_BORDER_MOTION_DIRECTION_NEGATIVE_X = 1 << 2,
  META_BORDER_MOTION_DIRECTION_NEGATIVE_Y = 1 << 3,
} MetaBorderMotionDirection;

typedef struct _MetaVector2
{
  float x;
  float y;
} MetaVector2;

typedef struct _MetaLine2
{
  MetaVector2 a;
  MetaVector2 b;
} MetaLine2;

typedef struct _MetaBorder
{
  MetaLine2 line;
  MetaBorderMotionDirection blocking_directions;
} MetaBorder;

static inline MetaVector2
meta_vector2_subtract (const MetaVector2 a,
                       const MetaVector2 b)
{
  return (MetaVector2) {
    .x = a.x - b.x,
    .y = a.y - b.y,
  };
}

gboolean
meta_line2_intersects_with (const MetaLine2 *line1,
                            const MetaLine2 *line2,
                            MetaVector2     *intersection);

gboolean
meta_border_is_horizontal (MetaBorder *border);

gboolean
meta_border_is_blocking_directions (MetaBorder               *border,
                                    MetaBorderMotionDirection directions);

unsigned int
meta_border_get_allows_directions (MetaBorder *border);

void
meta_border_set_allows_directions (MetaBorder *border, unsigned int directions);

#endif /* META_BORDER_H */
