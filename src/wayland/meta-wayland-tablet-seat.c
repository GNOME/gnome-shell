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

#include "meta-wayland-private.h"
#include "meta-wayland-tablet-seat.h"
#include "meta-wayland-tablet.h"
#include "meta-wayland-tablet-tool.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
notify_tool_added (MetaWaylandTabletSeat *tablet_seat,
                   struct wl_resource    *client_resource,
                   MetaWaylandTabletTool *tool)
{
  struct wl_resource *tool_resource;
  struct wl_client *client;

  client = wl_resource_get_client (client_resource);
  tool_resource = meta_wayland_tablet_tool_lookup_resource (tool, client);

  if (!tool_resource)
    return;

  zwp_tablet_seat_v2_send_tool_added (client_resource, tool_resource);
}

static void
notify_tablet_added (MetaWaylandTabletSeat *tablet_seat,
                     struct wl_resource    *client_resource,
                     ClutterInputDevice    *device)
{
  struct wl_resource *resource;
  MetaWaylandTablet *tablet;
  struct wl_client *client;

  tablet = g_hash_table_lookup (tablet_seat->tablets, device);

  if (!tablet)
    return;

  client = wl_resource_get_client (client_resource);

  if (meta_wayland_tablet_lookup_resource (tablet, client))
    return;

  resource = meta_wayland_tablet_create_new_resource (tablet, client,
                                                      client_resource, 0);
  if (!resource)
    return;

  zwp_tablet_seat_v2_send_tablet_added (client_resource, resource);
  meta_wayland_tablet_notify (tablet, resource);
}

static void
broadcast_tablet_added (MetaWaylandTabletSeat *tablet_seat,
                        ClutterInputDevice    *device)
{
  struct wl_resource *resource;

  wl_resource_for_each (resource, &tablet_seat->resource_list)
    {
      notify_tablet_added (tablet_seat, resource, device);
    }
}

static void
notify_tablets (MetaWaylandTabletSeat *tablet_seat,
                struct wl_resource    *client_resource)
{
  ClutterInputDevice *device;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, tablet_seat->tablets);

  while (g_hash_table_iter_next (&iter, (gpointer *) &device, NULL))
    notify_tablet_added (tablet_seat, client_resource, device);
}

static gboolean
is_tablet_device (ClutterInputDevice *device)
{
  ClutterInputDeviceType device_type;

  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER)
    return FALSE;

  device_type = clutter_input_device_get_device_type (device);

  return (device_type == CLUTTER_TABLET_DEVICE ||
          device_type == CLUTTER_PEN_DEVICE ||
          device_type == CLUTTER_ERASER_DEVICE ||
          device_type == CLUTTER_CURSOR_DEVICE);
}

static void
meta_wayland_tablet_seat_device_added (MetaWaylandTabletSeat *tablet_seat,
                                       ClutterInputDevice    *device)
{
  MetaWaylandTablet *tablet;

  if (!is_tablet_device (device))
    return;

  tablet = meta_wayland_tablet_new (device, tablet_seat);
  g_hash_table_insert (tablet_seat->tablets, device, tablet);
  broadcast_tablet_added (tablet_seat, device);
}

static void
meta_wayland_tablet_seat_device_removed (MetaWaylandTabletSeat *tablet_seat,
                                         ClutterInputDevice    *device)
{
  g_hash_table_remove (tablet_seat->tablets, device);
}

static void
tablet_seat_destroy (struct wl_client   *client,
                     struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_seat_v2_interface tablet_seat_interface = {
  tablet_seat_destroy
};

MetaWaylandTabletSeat *
meta_wayland_tablet_seat_new (MetaWaylandTabletManager *manager)
{
  MetaWaylandTabletSeat *tablet_seat;
  const GSList *devices, *l;

  tablet_seat = g_slice_new0 (MetaWaylandTabletSeat);
  tablet_seat->manager = manager;
  tablet_seat->device_manager = clutter_device_manager_get_default ();
  tablet_seat->tablets = g_hash_table_new_full (NULL, NULL, NULL,
                                                (GDestroyNotify) meta_wayland_tablet_free);
  tablet_seat->tools = g_hash_table_new_full (NULL, NULL, NULL,
                                              (GDestroyNotify) meta_wayland_tablet_tool_free);
  wl_list_init (&tablet_seat->resource_list);

  g_signal_connect_swapped (tablet_seat->device_manager, "device-added",
                            G_CALLBACK (meta_wayland_tablet_seat_device_added),
                            tablet_seat);
  g_signal_connect_swapped (tablet_seat->device_manager, "device-removed",
                            G_CALLBACK (meta_wayland_tablet_seat_device_removed),
                            tablet_seat);

  devices = clutter_device_manager_peek_devices (tablet_seat->device_manager);

  for (l = devices; l; l = l->next)
    meta_wayland_tablet_seat_device_added (tablet_seat, l->data);

  return tablet_seat;
}

void
meta_wayland_tablet_seat_free (MetaWaylandTabletSeat *tablet_seat)
{
  struct wl_resource *resource, *next;

  wl_resource_for_each_safe (resource, next, &tablet_seat->resource_list)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_signal_handlers_disconnect_by_data (tablet_seat->device_manager,
                                        tablet_seat);
  g_hash_table_destroy (tablet_seat->tablets);
  g_hash_table_destroy (tablet_seat->tools);
  g_slice_free (MetaWaylandTabletSeat, tablet_seat);
}

struct wl_resource *
meta_wayland_tablet_seat_create_new_resource (MetaWaylandTabletSeat *tablet_seat,
                                              struct wl_client      *client,
                                              struct wl_resource    *manager_resource,
                                              uint32_t               id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_seat_v2_interface,
                                 wl_resource_get_version (manager_resource),
                                 id);
  wl_resource_set_implementation (resource, &tablet_seat_interface,
                                  tablet_seat, unbind_resource);
  wl_resource_set_user_data (resource, tablet_seat);
  wl_list_insert (&tablet_seat->resource_list, wl_resource_get_link (resource));

  /* Notify client of all available tablets */
  notify_tablets (tablet_seat, resource);

  return resource;
}

struct wl_resource *
meta_wayland_tablet_seat_lookup_resource (MetaWaylandTabletSeat *tablet_seat,
                                          struct wl_client      *client)
{
  return wl_resource_find_for_client (&tablet_seat->resource_list, client);
}

MetaWaylandTablet *
meta_wayland_tablet_seat_lookup_tablet (MetaWaylandTabletSeat *tablet_seat,
                                        ClutterInputDevice    *device)
{
  return g_hash_table_lookup (tablet_seat->tablets, device);
}

MetaWaylandTabletTool *
meta_wayland_tablet_seat_lookup_tool (MetaWaylandTabletSeat  *tablet_seat,
                                      ClutterInputDeviceTool *tool)
{
  return g_hash_table_lookup (tablet_seat->tools, tool);
}

static MetaWaylandTabletTool *
meta_wayland_tablet_seat_ensure_tool (MetaWaylandTabletSeat  *tablet_seat,
                                      ClutterInputDevice     *device,
                                      ClutterInputDeviceTool *device_tool)
{
  MetaWaylandTabletTool *tool;

  tool = g_hash_table_lookup (tablet_seat->tools, device_tool);

  if (!tool)
    {
      tool = meta_wayland_tablet_tool_new (tablet_seat, device, device_tool);
      g_hash_table_insert (tablet_seat->tools, device_tool, tool);
    }

  return tool;
}

void
meta_wayland_tablet_seat_update (MetaWaylandTabletSeat *tablet_seat,
                                 const ClutterEvent    *event)
{
  ClutterInputDevice *device;
  ClutterInputDeviceTool *device_tool;
  MetaWaylandTabletTool *tool = NULL;

  device = clutter_event_get_source_device (event);
  device_tool = clutter_event_get_device_tool (event);

  if (device && device_tool)
    tool = meta_wayland_tablet_seat_ensure_tool (tablet_seat, device, device_tool);

  if (!tool)
    return;

  switch (event->type)
    {
    case CLUTTER_PROXIMITY_IN:
    case CLUTTER_PROXIMITY_OUT:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_MOTION:
      meta_wayland_tablet_tool_update (tool, event);
      break;
    default:
      break;
    }
}

gboolean
meta_wayland_tablet_seat_handle_event (MetaWaylandTabletSeat *tablet_seat,
                                       const ClutterEvent    *event)
{
  ClutterInputDeviceTool *device_tool;
  MetaWaylandTabletTool *tool = NULL;

  device_tool = clutter_event_get_device_tool (event);

  if (device_tool)
    tool = g_hash_table_lookup (tablet_seat->tools, device_tool);

  if (!tool)
    return CLUTTER_EVENT_PROPAGATE;

  switch (event->type)
    {
    case CLUTTER_PROXIMITY_IN:
    case CLUTTER_PROXIMITY_OUT:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_MOTION:
      meta_wayland_tablet_tool_handle_event (tool, event);
      return CLUTTER_EVENT_PROPAGATE;
    default:
      return CLUTTER_EVENT_STOP;
    }
}

void
meta_wayland_tablet_seat_notify_tool (MetaWaylandTabletSeat *tablet_seat,
                                      MetaWaylandTabletTool *tool,
                                      struct wl_client      *client)
{
  struct wl_resource *resource;

  resource = wl_resource_find_for_client (&tablet_seat->resource_list, client);

  if (resource)
    notify_tool_added (tablet_seat, resource, tool);
}
