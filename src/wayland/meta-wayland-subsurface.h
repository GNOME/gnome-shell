/*
 * Copyright (C) 2012,2013 Intel Corporation
 * Copyright (C) 2013-2017 Red Hat, Inc.
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

#ifndef META_WAYLAND_SUBSURFACE_H
#define META_WAYLAND_SUBSURFACE_H

#include "wayland/meta-wayland-actor-surface.h"

#define META_TYPE_WAYLAND_SUBSURFACE (meta_wayland_subsurface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSubsurface,
                      meta_wayland_subsurface,
                      META, WAYLAND_SUBSURFACE,
                      MetaWaylandActorSurface)

void meta_wayland_subsurface_parent_state_applied (MetaWaylandSubsurface *subsurface);

void meta_wayland_subsurface_union_geometry (MetaWaylandSubsurface *subsurface,
                                             int                    parent_x,
                                             int                    parent_y,
                                             MetaRectangle         *out_geometry);

void meta_wayland_subsurfaces_init (MetaWaylandCompositor *compositor);

#endif /* META_WAYLAND_SUBSURFACE_H */
