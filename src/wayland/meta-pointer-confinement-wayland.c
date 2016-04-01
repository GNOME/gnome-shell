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

#include "wayland/meta-pointer-confinement-wayland.h"

#include <glib-object.h>
#include <cairo.h>

#include "backends/meta-backend-private.h"
#include "core/meta-border.h"
#include "wayland/meta-wayland-seat.h"
#include "wayland/meta-wayland-pointer.h"
#include "wayland/meta-wayland-pointer-constraints.h"
#include "wayland/meta-wayland-surface.h"
#include "backends/meta-pointer-constraint.h"
#include "compositor/meta-surface-actor-wayland.h"

struct _MetaPointerConfinementWayland
{
  MetaPointerConstraint parent;

  MetaWaylandPointerConstraint *constraint;
};

typedef struct _MetaBox
{
  int x1;
  int y1;
  int x2;
  int y2;
} MetaBox;

G_DEFINE_TYPE (MetaPointerConfinementWayland, meta_pointer_confinement_wayland,
               META_TYPE_POINTER_CONSTRAINT);

static MetaBorder *
add_border (GArray *borders,
            float x1, float y1,
            float x2, float y2,
            MetaBorderMotionDirection blocking_directions)
{
  MetaBorder border;

  border = (MetaBorder) {
    .line = (MetaLine2) {
      .a = (MetaVector2) {
        .x = x1,
        .y = y1,
      },
      .b = (MetaVector2) {
        .x = x2,
        .y = y2,
      },
    },
    .blocking_directions = blocking_directions,
  };

  g_array_append_val (borders, border);

  return &g_array_index (borders, MetaBorder, borders->len - 1);
}

static gint
compare_lines_x (gconstpointer a, gconstpointer b)
{
  const MetaBorder *border_a = a;
  const MetaBorder *border_b = b;

  if (border_a->line.a.x == border_b->line.a.x)
    return border_a->line.b.x < border_b->line.b.x;
  else
    return border_a->line.a.x > border_b->line.a.x;
}

static void
add_non_overlapping_edges (MetaBox     *boxes,
                           unsigned int band_above_start,
                           unsigned int band_below_start,
                           unsigned int band_below_end,
                           GArray      *borders)
{
  unsigned int i;
  GArray *band_merge;
  MetaBorder *border;
  MetaBorder *prev_border;
  MetaBorder *new_border;

  band_merge = g_array_new (FALSE, FALSE, sizeof *border);

  /* Add bottom band of previous row, and top band of current row, and
   * sort them so lower left x coordinate comes first. If there are two
   * borders with the same left x coordinate, the wider one comes first.
   */
  for (i = band_above_start; i < band_below_start; i++)
    {
      MetaBox *box = &boxes[i];
      add_border (band_merge, box->x1, box->y2, box->x2, box->y2,
                  META_BORDER_MOTION_DIRECTION_POSITIVE_Y);
    }
  for (i = band_below_start; i < band_below_end; i++)
    {
      MetaBox *box= &boxes[i];
      add_border (band_merge, box->x1, box->y1, box->x2, box->y1,
                  META_BORDER_MOTION_DIRECTION_NEGATIVE_Y);
    }
  g_array_sort (band_merge, compare_lines_x);

  /* Combine the two combined bands so that any overlapping border is
   * eliminated. */
  prev_border = NULL;
  for (i = 0; i < band_merge->len; i++)
    {
      border = &g_array_index (band_merge, MetaBorder, i);

      g_assert (border->line.a.y == border->line.b.y);
      g_assert (!prev_border ||
                prev_border->line.a.y == border->line.a.y);
      g_assert (!prev_border ||
                (prev_border->line.a.x != border->line.a.x ||
                 prev_border->line.b.x != border->line.b.x));
      g_assert (!prev_border ||
                prev_border->line.a.x <= border->line.a.x);

      if (prev_border &&
          prev_border->line.a.x == border->line.a.x)
        {
          /*
           * ------------ +
           * -------      =
           * [     ]-----
           */
          prev_border->line.a.x = border->line.b.x;
        }
      else if (prev_border &&
               prev_border->line.b.x == border->line.b.x)
        {
          /*
           * ------------ +
           *       ------ =
           * ------[    ]
           */
          prev_border->line.b.x = border->line.a.x;
        }
      else if (prev_border &&
               prev_border->line.b.x == border->line.a.x)
        {
          /*
           * --------        +
           *         ------  =
           * --------------
           */
          prev_border->line.b.x = border->line.b.x;
        }
      else if (prev_border &&
               prev_border->line.b.x >= border->line.a.x)
        {
          /*
           * --------------- +
           *      ------     =
           * -----[    ]----
           */
          new_border = add_border (borders,
                                   border->line.b.x,
                                   border->line.b.y,
                                   prev_border->line.b.x,
                                   prev_border->line.b.y,
                                   prev_border->blocking_directions);
          prev_border->line.b.x = border->line.a.x;
          prev_border = new_border;
        }
      else
        {
          g_assert (!prev_border ||
                    prev_border->line.b.x < border->line.a.x);
          /*
           * First border or non-overlapping.
           *
           * -----           +
           *        -----    =
           * -----  -----
           */
          g_array_append_val (borders, *border);
          prev_border = &g_array_index (borders, MetaBorder, borders->len - 1);
        }
    }

  g_array_free (band_merge, FALSE);
}

static void
add_band_bottom_edges (MetaBox *boxes,
                       int      band_start,
                       int      band_end,
                       GArray  *borders)
{
  int i;

  for (i = band_start; i < band_end; i++)
    {
      add_border (borders,
                  boxes[i].x1, boxes[i].y2,
                  boxes[i].x2, boxes[i].y2,
                  META_BORDER_MOTION_DIRECTION_POSITIVE_Y);
    }
}

static void
region_to_outline (cairo_region_t *region,
                   GArray         *borders)
{
  MetaBox *boxes;
  int num_boxes;
  int i;
  int top_most, bottom_most;
  int current_roof;
  int prev_top;
  int band_start, prev_band_start;

  /*
   * Remove any overlapping lines from the set of rectangles. Note that
   * pixman regions are grouped as rows of rectangles, where rectangles
   * in one row never touch or overlap and are all of the same height.
   *
   *             -------- ---                   -------- ---
   *             |      | | |                   |      | | |
   *   ----------====---- ---         -----------  ----- ---
   *   |            |            =>   |            |
   *   ----==========---------        -----        ----------
   *       |                 |            |                 |
   *       -------------------            -------------------
   *
   */

  num_boxes  = cairo_region_num_rectangles (region);
  boxes = g_new (MetaBox, num_boxes);
  for (i = 0; i < num_boxes; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      boxes[i] = (MetaBox) {
        .x1 = rect.x,
        .y1 = rect.y,
        .x2 = rect.x + rect.width,
        .y2 = rect.y + rect.height,
      };
    }
  prev_top = 0;
  top_most = boxes[0].y1;
  current_roof = top_most;
  bottom_most = boxes[num_boxes - 1].y2;
  band_start = 0;
  prev_band_start = 0;
  for (i = 0; i < num_boxes; i++)
    {
      /* Detect if there is a vertical empty space, and add the lower
       * level of the previous band if so was the case. */
      if (i > 0 &&
          boxes[i].y1 != prev_top &&
          boxes[i].y1 != boxes[i - 1].y2)
        {
          current_roof = boxes[i].y1;
          add_band_bottom_edges (boxes,
                                 band_start,
                                 i,
                                 borders);
        }

      /* Special case adding the last band, since it won't be handled
       * by the band change detection below. */
      if (boxes[i].y1 != current_roof && i == num_boxes - 1)
        {
          if (boxes[i].y1 != prev_top)
            {
              /* The last band is a single box, so we don't
               * have a prev_band_start to tell us when the
               * previous band started. */
              add_non_overlapping_edges (boxes,
                                         band_start,
                                         i,
                                         i + 1,
                                         borders);
            }
          else
            {
              add_non_overlapping_edges (boxes,
                                         prev_band_start,
                                         band_start,
                                         i + 1,
                                         borders);
            }
        }

      /* Detect when passing a band and combine the top border of the
       * just passed band with the bottom band of the previous band.
       */
      if (boxes[i].y1 != top_most && boxes[i].y1 != prev_top)
        {
          /* Combine the two passed bands. */
          if (prev_top != current_roof)
            {
              add_non_overlapping_edges (boxes,
                                         prev_band_start,
                                         band_start,
                                         i,
                                         borders);
            }

          prev_band_start = band_start;
          band_start = i;
        }

      /* Add the top border if the box is part of the current roof. */
      if (boxes[i].y1 == current_roof)
        {
          add_border (borders,
                      boxes[i].x1, boxes[i].y1,
                      boxes[i].x2, boxes[i].y1,
                      META_BORDER_MOTION_DIRECTION_NEGATIVE_Y);
        }

      /* Add the bottom border of the last band. */
      if (boxes[i].y2 == bottom_most)
        {
          add_border (borders,
                      boxes[i].x1, boxes[i].y2,
                      boxes[i].x2, boxes[i].y2,
                      META_BORDER_MOTION_DIRECTION_POSITIVE_Y);
        }

      /* Always add the left border. */
      add_border (borders,
                  boxes[i].x1, boxes[i].y1,
                  boxes[i].x1, boxes[i].y2,
                  META_BORDER_MOTION_DIRECTION_NEGATIVE_X);

      /* Always add the right border. */
      add_border (borders,
                  boxes[i].x2, boxes[i].y1,
                  boxes[i].x2, boxes[i].y2,
                  META_BORDER_MOTION_DIRECTION_POSITIVE_X);

      prev_top = boxes[i].y1;
    }

  g_free (boxes);
}

static MetaBorder *
get_closest_border (GArray    *borders,
                    MetaLine2 *motion,
                    uint32_t   directions)
{
  MetaBorder *border;
  MetaVector2 intersection;
  MetaVector2 delta;
  float distance_2;
  MetaBorder *closest_border = NULL;
  float closest_distance_2 = DBL_MAX;
  unsigned int i;

  for (i = 0; i < borders->len; i++)
    {
      border = &g_array_index (borders, MetaBorder, i);

      if (!meta_border_is_blocking_directions (border, directions))
        continue;

      if (!meta_line2_intersects_with (&border->line, motion, &intersection))
        continue;

      delta = meta_vector2_subtract (intersection, motion->a);
      distance_2 = delta.x*delta.x + delta.y*delta.y;
      if (distance_2 < closest_distance_2)
        {
          closest_border = border;
          closest_distance_2 = distance_2;
        }
    }

  return closest_border;
}

static void
clamp_to_border (MetaBorder *border,
                 MetaLine2  *motion,
                 uint32_t   *motion_dir)
{
  /*
   * When clamping either rightward or downward motions, the motion needs to be
   * clamped so that the destination coordinate does not end up on the border
   * (see weston_pointer_clamp_event_to_region). Do this by clamping such
   * motions to the border minus the smallest possible wl_fixed_t value.
   *
   * When clamping in either leftward or upward motion, the resulting coordinate
   * needs to be clamped so that it is enough on the inside to avoid the
   * inaccuracies of clutter's stage to actor transformation algorithm (the one
   * used in clutter_actor_transform_stage_point) to make it end up outside the
   * next motion. It also needs to be clamped so that to the wl_fixed_t
   * coordinate may still be right on the border (i.e. at .0). Testing shows
   * that the smallest wl_fixed_t value divided by 10 is small enough to make
   * the wl_fixed_t coordinate .0 and large enough to avoid the inaccuracies of
   * clutters transform algorithm.
   */
  if (meta_border_is_horizontal (border))
    {
      if (*motion_dir & META_BORDER_MOTION_DIRECTION_POSITIVE_Y)
        motion->b.y = border->line.a.y - wl_fixed_to_double (1);
      else
        motion->b.y = border->line.a.y + wl_fixed_to_double (1) / 10;
      *motion_dir &= ~(META_BORDER_MOTION_DIRECTION_POSITIVE_Y |
                       META_BORDER_MOTION_DIRECTION_NEGATIVE_Y);
    }
  else
    {
      if (*motion_dir & META_BORDER_MOTION_DIRECTION_POSITIVE_X)
        motion->b.x = border->line.a.x - wl_fixed_to_double (1);
      else
        motion->b.x = border->line.a.x + wl_fixed_to_double (1) / 10;
      *motion_dir &= ~(META_BORDER_MOTION_DIRECTION_POSITIVE_X |
                       META_BORDER_MOTION_DIRECTION_NEGATIVE_X);
    }
}

static uint32_t
get_motion_directions (MetaLine2 *motion)
{
  uint32_t directions = 0;

  if (motion->a.x < motion->b.x)
    directions |= META_BORDER_MOTION_DIRECTION_POSITIVE_X;
  else if (motion->a.x > motion->b.x)
    directions |= META_BORDER_MOTION_DIRECTION_NEGATIVE_X;
  if (motion->a.y < motion->b.y)
    directions |= META_BORDER_MOTION_DIRECTION_POSITIVE_Y;
  else if (motion->a.y > motion->b.y)
    directions |= META_BORDER_MOTION_DIRECTION_NEGATIVE_Y;

  return directions;
}

static void
meta_pointer_confinement_wayland_constrain (MetaPointerConstraint *constraint,
                                            ClutterInputDevice    *device,
                                            guint32                time,
                                            float                  prev_x,
                                            float                  prev_y,
                                            float                  *x,
                                            float                  *y)
{
  MetaPointerConfinementWayland *self =
    META_POINTER_CONFINEMENT_WAYLAND (constraint);
  MetaWaylandSurface *surface;
  cairo_region_t *region;
  float sx, sy;
  float prev_sx, prev_sy;
  GArray *borders;
  MetaLine2 motion;
  MetaBorder *closest_border;
  uint32_t directions;

  surface = meta_wayland_pointer_constraint_get_surface (self->constraint);

  meta_wayland_surface_get_relative_coordinates (surface, *x, *y, &sx, &sy);
  meta_wayland_surface_get_relative_coordinates (surface, prev_x, prev_y,
                                                 &prev_sx, &prev_sy);

  /* For motions in a positive direction on any axis, append the smallest
   * possible value representable in a Wayland absolute coordinate.  This is
   * in order to avoid not clamping motion that as a floating point number
   * won't be clamped, but will be rounded up to be outside of the range
   * of wl_fixed_t. */
  if (sx > prev_sx)
    sx += (float)wl_fixed_to_double(1);
  if (sy > prev_sy)
    sy += (float)wl_fixed_to_double(1);

  borders = g_array_new (FALSE, FALSE, sizeof (MetaBorder));

  /*
   * Generate borders given the confine region we are to use. The borders
   * are defined to be the outer region of the allowed area. This means
   * top/left borders are "within" the allowed area, while bottom/right
   * borders are outside. This needs to be considered when clamping
   * confined motion vectors.
   */
  region =
    meta_wayland_pointer_constraint_calculate_effective_region (self->constraint);
  region_to_outline (region, borders);
  cairo_region_destroy (region);

  motion = (MetaLine2) {
    .a = (MetaVector2) {
      .x = prev_sx,
      .y = prev_sy,
    },
    .b = (MetaVector2) {
      .x = sx,
      .y = sy,
    },
  };
  directions = get_motion_directions (&motion);

  while (directions)
    {
      closest_border = get_closest_border (borders,
                                           &motion,
                                           directions);
      if (closest_border)
        clamp_to_border (closest_border, &motion, &directions);
      else
        break;
    }

  meta_wayland_surface_get_absolute_coordinates (surface,
                                                 motion.b.x, motion.b.y,
                                                 x, y);

  g_array_free (borders, FALSE);
}

static float
point_to_border_distance_2 (MetaBorder *border,
                            float       x,
                            float       y)
{
  float orig_x, orig_y;
  float dx, dy;

  if (meta_border_is_horizontal (border))
    {
      if (x < border->line.a.x)
        orig_x = border->line.a.x;
      else if (x > border->line.b.x)
        orig_x = border->line.b.x;
      else
        orig_x = x;
      orig_y = border->line.a.y;
    }
  else
    {
      if (y < border->line.a.y)
        orig_y = border->line.a.y;
      else if (y > border->line.b.y)
        orig_y = border->line.b.y;
      else
        orig_y = y;
      orig_x = border->line.a.x;
    }

  dx = fabsf (orig_x - x);
  dy = fabsf (orig_y - y);
  return dx*dx + dy*dy;
}

static void
warp_to_behind_border (MetaBorder *border,
                       float      *sx,
                       float      *sy)
{
  switch (border->blocking_directions)
    {
    case META_BORDER_MOTION_DIRECTION_POSITIVE_X:
    case META_BORDER_MOTION_DIRECTION_NEGATIVE_X:
      if (border->blocking_directions == META_BORDER_MOTION_DIRECTION_POSITIVE_X)
        *sx = border->line.a.x - wl_fixed_to_double (1);
      else
        *sx = border->line.a.x + wl_fixed_to_double (1);
      if (*sy < border->line.a.y)
        *sy = border->line.a.y + wl_fixed_to_double (1);
      else if (*sy > border->line.b.y)
        *sy = border->line.b.y - wl_fixed_to_double (1);
      break;
    case META_BORDER_MOTION_DIRECTION_POSITIVE_Y:
    case META_BORDER_MOTION_DIRECTION_NEGATIVE_Y:
      if (border->blocking_directions == META_BORDER_MOTION_DIRECTION_POSITIVE_Y)
        *sy = border->line.a.y - wl_fixed_to_double (1);
      else
        *sy = border->line.a.y + wl_fixed_to_double (1);
      if (*sx < border->line.a.x)
        *sx = border->line.a.x + wl_fixed_to_double (1);
      else if (*sx > (border->line.b.x))
        *sx = border->line.b.x - wl_fixed_to_double (1);
      break;
    }
}

static void
meta_pointer_confinement_wayland_maybe_warp (MetaPointerConfinementWayland *self)
{
  MetaWaylandSeat *seat;
  MetaWaylandSurface *surface;
  ClutterPoint point;
  float sx;
  float sy;
  cairo_region_t *region;

  seat = meta_wayland_pointer_constraint_get_seat (self->constraint);
  surface = meta_wayland_pointer_constraint_get_surface (self->constraint);

  clutter_input_device_get_coords (seat->pointer->device, NULL, &point);
  meta_wayland_surface_get_relative_coordinates (surface,
                                                 point.x, point.y,
                                                 &sx, &sy);

  region =
    meta_wayland_pointer_constraint_calculate_effective_region (self->constraint);

  if (!cairo_region_contains_point (region, (int)sx, (int)sy))
    {
      GArray *borders;
      float closest_distance_2 = FLT_MAX;
      MetaBorder *closest_border = NULL;
      unsigned int i;
      float x;
      float y;

      borders = g_array_new (FALSE, FALSE, sizeof (MetaBorder));

      region_to_outline (region, borders);

      for (i = 0; i < borders->len; i++)
        {
          MetaBorder *border = &g_array_index (borders, MetaBorder, i);
          float distance_2;

          distance_2 = point_to_border_distance_2 (border, sx, sy);
          if (distance_2 < closest_distance_2)
            {
              closest_border = border;
              closest_distance_2 = distance_2;
            }
        }

      warp_to_behind_border (closest_border, &sx, &sy);

      meta_wayland_surface_get_absolute_coordinates (surface, sx, sy, &x, &y);
      meta_backend_warp_pointer (meta_get_backend (), (int)x, (int)y);
    }

  cairo_region_destroy (region);
}

static void
surface_actor_painting (MetaSurfaceActorWayland       *surface_actor,
                        MetaPointerConfinementWayland *self)
{
  meta_pointer_confinement_wayland_maybe_warp (self);
}

MetaPointerConstraint *
meta_pointer_confinement_wayland_new (MetaWaylandPointerConstraint *constraint)
{
  GObject *object;
  MetaPointerConfinementWayland *confinement;
  MetaWaylandSurface *surface;

  object = g_object_new (META_TYPE_POINTER_CONFINEMENT_WAYLAND, NULL);
  confinement = META_POINTER_CONFINEMENT_WAYLAND (object);

  confinement->constraint = constraint;

  surface = meta_wayland_pointer_constraint_get_surface (constraint);
  g_signal_connect_object (surface->surface_actor,
                           "painting",
                           G_CALLBACK (surface_actor_painting),
                           confinement,
                           0);

  return META_POINTER_CONSTRAINT (confinement);
}

static void
meta_pointer_confinement_wayland_init (MetaPointerConfinementWayland *confinement_wayland)
{
}

static void
meta_pointer_confinement_wayland_class_init (MetaPointerConfinementWaylandClass *klass)
{
  MetaPointerConstraintClass *pointer_constraint_class =
    META_POINTER_CONSTRAINT_CLASS (klass);

  pointer_constraint_class->constrain = meta_pointer_confinement_wayland_constrain;
}
