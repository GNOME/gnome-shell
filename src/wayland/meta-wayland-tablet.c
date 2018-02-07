/*
 * Wayland Support
 *
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#define _GNU_SOURCE

#include "config.h"

#include <glib.h>

#include <wayland-server.h>
#include "tablet-unstable-v2-server-protocol.h"

#include "meta-surface-actor-wayland.h"
#include "meta-wayland-private.h"
#include "meta-wayland-tablet.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

MetaWaylandTablet *
meta_wayland_tablet_new (ClutterInputDevice    *device,
                         MetaWaylandTabletSeat *tablet_seat)
{
  MetaWaylandTablet *tablet;

  tablet = g_slice_new0 (MetaWaylandTablet);
  wl_list_init (&tablet->resource_list);
  tablet->device = device;
  tablet->tablet_seat = tablet_seat;

  return tablet;
}

void
meta_wayland_tablet_free (MetaWaylandTablet *tablet)
{
  struct wl_resource *resource, *next;

  wl_resource_for_each_safe (resource, next, &tablet->resource_list)
    {
      zwp_tablet_v2_send_removed (resource);
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_slice_free (MetaWaylandTablet, tablet);
}

static void
tablet_destroy (struct wl_client   *client,
                struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_v2_interface tablet_interface = {
  tablet_destroy
};

void
meta_wayland_tablet_notify (MetaWaylandTablet  *tablet,
                            struct wl_resource *resource)
{
  ClutterInputDevice *device = tablet->device;
  const gchar *node_path, *vendor, *product;
  guint vid, pid;

  zwp_tablet_v2_send_name (resource, clutter_input_device_get_device_name (device));

  node_path = clutter_input_device_get_device_node (device);
  if (node_path)
    zwp_tablet_v2_send_path (resource, node_path);

  vendor = clutter_input_device_get_vendor_id (device);
  product = clutter_input_device_get_product_id (device);

  if (vendor && sscanf (vendor, "%x", &vid) == 1 &&
      product && sscanf (product, "%x", &pid) == 1)
    zwp_tablet_v2_send_id (resource, vid, pid);

  zwp_tablet_v2_send_done (resource);
}

struct wl_resource *
meta_wayland_tablet_create_new_resource (MetaWaylandTablet  *tablet,
                                         struct wl_client   *client,
                                         struct wl_resource *seat_resource,
                                         uint32_t            id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_v2_interface,
                                 wl_resource_get_version (seat_resource), id);
  wl_resource_set_implementation (resource, &tablet_interface,
                                  tablet, unbind_resource);
  wl_resource_set_user_data (resource, tablet);
  wl_list_insert (&tablet->resource_list, wl_resource_get_link (resource));

  return resource;
}

struct wl_resource *
meta_wayland_tablet_lookup_resource (MetaWaylandTablet *tablet,
                                     struct wl_client  *client)
{
  return wl_resource_find_for_client (&tablet->resource_list, client);
}
