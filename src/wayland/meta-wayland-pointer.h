/*
 * Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __META_WAYLAND_POINTER_H__
#define __META_WAYLAND_POINTER_H__

#include <wayland-server.h>

#include "meta-wayland-seat.h"

void
meta_wayland_pointer_init (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_release (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface,
                                wl_fixed_t sx,
                                wl_fixed_t sy);

void
meta_wayland_pointer_destroy_focus (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_start_grab (MetaWaylandPointer *pointer,
                                 MetaWaylandPointerGrab *grab);

void
meta_wayland_pointer_end_grab (MetaWaylandPointer *pointer);

gboolean
meta_wayland_pointer_begin_modal (MetaWaylandPointer *pointer);
void
meta_wayland_pointer_end_modal   (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_set_current (MetaWaylandPointer *pointer,
                                  MetaWaylandSurface *surface);

#endif /* __META_WAYLAND_POINTER_H__ */
