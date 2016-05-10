/*
 * Wayland Support
 *
 * Copyright (C) 2015 Red Hat
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef META_WAYLAND_TABLET_H
#define META_WAYLAND_TABLET_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"
#include "meta-cursor-renderer.h"

struct _MetaWaylandTablet
{
  MetaWaylandTabletSeat *tablet_seat;
  ClutterInputDevice *device;

  struct wl_list resource_list;

  MetaWaylandSurface *current;
};

MetaWaylandTablet * meta_wayland_tablet_new          (ClutterInputDevice       *device,
                                                      MetaWaylandTabletSeat    *tablet_seat);
void                meta_wayland_tablet_free         (MetaWaylandTablet        *tablet);

struct wl_resource *
             meta_wayland_tablet_create_new_resource (MetaWaylandTablet  *tablet,
                                                      struct wl_client   *client,
                                                      struct wl_resource *seat_resource,
                                                      uint32_t            id);
struct wl_resource *
             meta_wayland_tablet_lookup_resource     (MetaWaylandTablet  *tablet,
                                                      struct wl_client   *client);

void         meta_wayland_tablet_notify              (MetaWaylandTablet  *tablet,
                                                      struct wl_resource *resource);

#endif /* META_WAYLAND_TABLET_H */
