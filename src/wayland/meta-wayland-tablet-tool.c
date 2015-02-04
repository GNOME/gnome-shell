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
#include "tablet-unstable-v1-server-protocol.h"
#include "meta-wayland-private.h"
#include "meta-wayland-tablet-tool.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

MetaWaylandTabletTool *
meta_wayland_tablet_tool_new (MetaWaylandTabletSeat  *seat,
                              ClutterInputDevice     *device,
                              ClutterInputDeviceTool *device_tool)
{
  MetaWaylandTabletTool *tool;

  tool = g_slice_new0 (MetaWaylandTabletTool);
  tool->seat = seat;
  tool->device = device;
  tool->device_tool = device_tool;
  wl_list_init (&tool->resource_list);

  return tool;
}

void
meta_wayland_tablet_tool_free (MetaWaylandTabletTool *tool)
{
  struct wl_resource *resource, *next;

  wl_resource_for_each_safe (resource, next, &tool->resource_list)
    {
      zwp_tablet_tool_v1_send_removed (resource);
    }

  g_slice_free (MetaWaylandTabletTool, tool);
}

static void
tool_set_cursor (struct wl_client   *client,
                 struct wl_resource *resource,
                 uint32_t            serial,
                 struct wl_resource *surface_resource,
                 int32_t             hotspot_x,
                 int32_t             hotspot_y)
{
}

static void
tool_destroy (struct wl_client   *client,
              struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_tool_v1_interface tool_interface = {
  tool_set_cursor,
  tool_destroy
};

struct wl_resource *
meta_wayland_tablet_tool_create_new_resource (MetaWaylandTabletTool *tool,
                                              struct wl_client      *client,
                                              struct wl_resource    *seat_resource,
                                              uint32_t               id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_tool_v1_interface,
                                 wl_resource_get_version (seat_resource), id);
  wl_resource_set_implementation (resource, &tool_interface,
                                  tool, unbind_resource);
  wl_resource_set_user_data (resource, tool);
  wl_list_insert (&tool->resource_list, wl_resource_get_link (resource));

  return resource;
}

struct wl_resource *
meta_wayland_tablet_tool_lookup_resource (MetaWaylandTabletTool *tool,
                                          struct wl_client      *client)
{
  if (wl_list_empty (&tool->resource_list))
    return NULL;

  return wl_resource_find_for_client (&tool->resource_list, client);
}

void
meta_wayland_tablet_tool_update (MetaWaylandTabletTool *tool,
                                 const ClutterEvent    *event)
{
}

gboolean
meta_wayland_tablet_tool_handle_event (MetaWaylandTabletTool *tool,
                                       const ClutterEvent    *event)
{
  return CLUTTER_EVENT_STOP;
}
