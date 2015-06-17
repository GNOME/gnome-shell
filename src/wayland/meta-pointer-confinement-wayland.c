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

G_DEFINE_TYPE (MetaPointerConfinementWayland, meta_pointer_confinement_wayland,
               META_TYPE_POINTER_CONSTRAINT);

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
  cairo_rectangle_int_t extents;
  float sx, sy;
  float min_sx, max_sx;
  float min_sy, max_sy;

  region =
    meta_wayland_pointer_constraint_calculate_effective_region (self->constraint);
  cairo_region_get_extents (region, &extents);
  cairo_region_destroy (region);

  min_sx = extents.x;
  max_sx = extents.x + extents.width - 1;
  max_sy = extents.y + extents.height - 1;
  min_sy = extents.y;

  surface = meta_wayland_pointer_constraint_get_surface (self->constraint);
  meta_wayland_surface_get_relative_coordinates (surface, *x, *y, &sx, &sy);

  if (sx < min_sx)
    sx = min_sx;
  else if (sx > max_sx)
    sx = max_sx;

  if (sy < min_sy)
    sy = min_sy;
  else if (sy > max_sy)
    sy = max_sy;

  meta_wayland_surface_get_absolute_coordinates (surface, sx, sy, x, y);
}

static void
meta_pointer_confinement_wayland_maybe_warp (MetaPointerConfinementWayland *self)
{
  MetaWaylandSeat *seat;
  MetaWaylandSurface *surface;
  wl_fixed_t sx;
  wl_fixed_t sy;
  cairo_region_t *region;

  seat = meta_wayland_pointer_constraint_get_seat (self->constraint);
  surface = meta_wayland_pointer_constraint_get_surface (self->constraint);
  meta_wayland_pointer_get_relative_coordinates (&seat->pointer,
                                                 surface,
                                                 &sx, &sy);

  region =
    meta_wayland_pointer_constraint_calculate_effective_region (self->constraint);

  if (!cairo_region_contains_point (region,
                                    wl_fixed_to_int (sx),
                                    wl_fixed_to_int (sy)))
    {
      cairo_rectangle_int_t extents;
      wl_fixed_t min_sx;
      wl_fixed_t max_sx;
      wl_fixed_t max_sy;
      wl_fixed_t min_sy;
      gboolean x_changed = TRUE, y_changed = TRUE;

      cairo_region_get_extents (region, &extents);

      min_sx = wl_fixed_from_int (extents.x);
      max_sx = wl_fixed_from_int (extents.x + extents.width - 1);
      max_sy = wl_fixed_from_int (extents.y + extents.height - 1);
      min_sy = wl_fixed_from_int (extents.y);

      if (sx < min_sx)
        sx = min_sx;
      else if (sx > max_sx)
        sx = max_sx;
      else
        x_changed = FALSE;

      if (sy < min_sy)
        sy = min_sy;
      else if (sy > max_sy)
        sy = max_sy;
      else
        y_changed = FALSE;

      if (x_changed || y_changed)
        {
          float x, y;

          meta_wayland_surface_get_absolute_coordinates (surface,
                                                         wl_fixed_to_double (sx),
                                                         wl_fixed_to_double (sy),
                                                         &x, &y);
          meta_backend_warp_pointer (meta_get_backend (), (int)x, (int)y);
        }
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
