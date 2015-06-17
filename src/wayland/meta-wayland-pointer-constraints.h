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

#ifndef META_WAYLAND_POINTER_CONSTRAINTS_H
#define META_WAYLAND_POINTER_CONSTRAINTS_H

#include "meta-wayland-types.h"
#include "meta/window.h"

#include <wayland-server.h>

#define META_TYPE_WAYLAND_POINTER_CONSTRAINT (meta_wayland_pointer_constraint_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandPointerConstraint,
                      meta_wayland_pointer_constraint,
                      META, WAYLAND_POINTER_CONSTRAINT,
                      GObject);

typedef struct _MetaWaylandPointerConstraint MetaWaylandPointerConstraint;

void meta_wayland_pointer_constraints_init (MetaWaylandCompositor *compositor);

void meta_wayland_pointer_constraint_maybe_enable (MetaWaylandPointerConstraint *constraint);

void meta_wayland_pointer_constraint_destroy (MetaWaylandPointerConstraint *constraint);

MetaWaylandSeat * meta_wayland_pointer_constraint_get_seat (MetaWaylandPointerConstraint *constraint);

cairo_region_t * meta_wayland_pointer_constraint_calculate_effective_region (MetaWaylandPointerConstraint *constraint);

cairo_region_t * meta_wayland_pointer_constraint_get_region (MetaWaylandPointerConstraint *constraint);

MetaWaylandSurface * meta_wayland_pointer_constraint_get_surface (MetaWaylandPointerConstraint *constraint);

void meta_wayland_pointer_constraint_maybe_remove_for_seat (MetaWaylandSeat *seat,
                                                            MetaWindow      *focus_window);

void meta_wayland_pointer_constraint_maybe_enable_for_window (MetaWindow *window);

#endif /* META_WAYLAND_POINTER_CONSTRAINTS_H */
