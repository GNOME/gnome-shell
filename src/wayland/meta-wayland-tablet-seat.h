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

#ifndef META_WAYLAND_TABLET_SEAT_H
#define META_WAYLAND_TABLET_SEAT_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"

struct _MetaWaylandTabletSeat
{
  MetaWaylandTabletManager *manager;
  MetaWaylandSeat *seat;
  ClutterDeviceManager *device_manager;
  struct wl_list resource_list;

  GHashTable *tablets;
  GHashTable *tools;
  GHashTable *pads;
};

MetaWaylandTabletSeat *meta_wayland_tablet_seat_new  (MetaWaylandTabletManager *tablet_manager,
                                                      MetaWaylandSeat          *seat);
void                   meta_wayland_tablet_seat_free (MetaWaylandTabletSeat    *tablet_seat);

struct wl_resource    *meta_wayland_tablet_seat_create_new_resource (MetaWaylandTabletSeat *tablet_seat,
                                                                     struct wl_client      *client,
                                                                     struct wl_resource    *seat_resource,
                                                                     uint32_t               id);
struct wl_resource    *meta_wayland_tablet_seat_lookup_resource     (MetaWaylandTabletSeat *tablet_seat,
                                                                     struct wl_client      *client);

MetaWaylandTablet     *meta_wayland_tablet_seat_lookup_tablet       (MetaWaylandTabletSeat *tablet_seat,
                                                                     ClutterInputDevice    *device);

MetaWaylandTabletTool *meta_wayland_tablet_seat_lookup_tool         (MetaWaylandTabletSeat  *tablet_seat,
                                                                     ClutterInputDeviceTool *tool);

MetaWaylandTabletPad  *meta_wayland_tablet_seat_lookup_pad          (MetaWaylandTabletSeat *tablet_seat,
                                                                     ClutterInputDevice    *device);

void                   meta_wayland_tablet_seat_update              (MetaWaylandTabletSeat *tablet_seat,
                                                                     const ClutterEvent    *event);
gboolean               meta_wayland_tablet_seat_handle_event        (MetaWaylandTabletSeat *tablet_seat,
                                                                     const ClutterEvent    *event);

void                   meta_wayland_tablet_seat_notify_tool         (MetaWaylandTabletSeat *tablet_seat,
                                                                     MetaWaylandTabletTool *tool,
                                                                     struct wl_client      *client);

void                   meta_wayland_tablet_seat_set_pad_focus       (MetaWaylandTabletSeat *tablet_seat,
                                                                     MetaWaylandSurface    *surface);

MetaWaylandTablet     *meta_wayland_tablet_seat_lookup_paired_tablet (MetaWaylandTabletSeat *tablet_seat,
                                                                      MetaWaylandTabletPad  *pad);
GList                 *meta_wayland_tablet_seat_lookup_paired_pads   (MetaWaylandTabletSeat *tablet_seat,
                                                                      MetaWaylandTablet     *tablet);

#endif /* META_WAYLAND_TABLET_SEAT_H */
