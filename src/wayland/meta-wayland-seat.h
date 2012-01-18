/*
 * Wayland Support
 *
 * Copyright (C) 2012 Intel Corporation
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

#ifndef __META_WAYLAND_SEAT_H__
#define __META_WAYLAND_SEAT_H__

#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <clutter/clutter.h>
#include <glib.h>

#include "meta-wayland-private.h"

MetaWaylandSeat *
meta_wayland_seat_new (struct wl_display *display);

void
meta_wayland_seat_handle_event (MetaWaylandSeat *seat,
                                const ClutterEvent *event);

void
meta_wayland_seat_repick (MetaWaylandSeat *seat,
                          uint32_t time,
                          ClutterActor *actor);

void
meta_wayland_seat_update_sprite (MetaWaylandSeat *seat);

void
meta_wayland_seat_free (MetaWaylandSeat *seat);

#endif /* __META_WAYLAND_SEAT_H__ */
