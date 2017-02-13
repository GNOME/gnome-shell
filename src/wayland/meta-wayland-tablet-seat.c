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
#include "meta-wayland-tablet-pad.h"

#ifdef HAVE_NATIVE_BACKEND
#include <clutter/evdev/clutter-evdev.h>
#include "backends/native/meta-backend-native.h"
#endif

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

static void
notify_pad_added (MetaWaylandTabletSeat *tablet_seat,
                  struct wl_resource    *tablet_seat_resource,
                  ClutterInputDevice    *device)
{
  struct wl_resource *resource;
  MetaWaylandTabletPad *pad;
  struct wl_client *client;

  pad = g_hash_table_lookup (tablet_seat->pads, device);

  if (!pad)
    return;

  client = wl_resource_get_client (tablet_seat_resource);

  if (meta_wayland_tablet_pad_lookup_resource (pad, client))
    return;

  resource = meta_wayland_tablet_pad_create_new_resource (pad, client,
                                                          tablet_seat_resource,
                                                          0);
  if (!resource)
    return;

  zwp_tablet_seat_v2_send_pad_added (tablet_seat_resource, resource);
  meta_wayland_tablet_pad_notify (pad, resource);
}

static void
broadcast_pad_added (MetaWaylandTabletSeat *tablet_seat,
                     ClutterInputDevice    *device)
{
  struct wl_resource *resource;

  wl_resource_for_each (resource, &tablet_seat->resource_list)
    {
      notify_pad_added (tablet_seat, resource, device);
    }
}

static void
notify_pads (MetaWaylandTabletSeat *tablet_seat,
             struct wl_resource    *tablet_seat_resource)
{
  ClutterInputDevice *device;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, tablet_seat->pads);

  while (g_hash_table_iter_next (&iter, (gpointer *) &device, NULL))
    notify_pad_added (tablet_seat, tablet_seat_resource, device);
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

static gboolean
is_pad_device (ClutterInputDevice *device)
{
  ClutterInputDeviceType device_type;

  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER)
    return FALSE;

  device_type = clutter_input_device_get_device_type (device);

  return device_type == CLUTTER_PAD_DEVICE;
}

static void
meta_wayland_tablet_seat_device_added (MetaWaylandTabletSeat *tablet_seat,
                                       ClutterInputDevice    *device)
{
  MetaWaylandSurface *pad_focus = tablet_seat->seat->keyboard->focus_surface;

  if (is_tablet_device (device))
    {
      MetaWaylandTablet *tablet;
      GList *pads, *l;

      tablet = meta_wayland_tablet_new (device, tablet_seat);
      g_hash_table_insert (tablet_seat->tablets, device, tablet);
      broadcast_tablet_added (tablet_seat, device);

      /* Because the insertion order is undefined, there might be already
       * pads that are logically paired to this tablet. Look those up and
       * refocus them.
       */
      pads = meta_wayland_tablet_seat_lookup_paired_pads (tablet_seat,
                                                          tablet);

      for (l = pads; l; l = l->next)
        meta_wayland_tablet_pad_set_focus (l->data, pad_focus);

      g_list_free (pads);
    }
  else if (is_pad_device (device))
    {
      MetaWaylandTabletPad *pad;

      pad = meta_wayland_tablet_pad_new (device, tablet_seat);
      g_hash_table_insert (tablet_seat->pads, device, pad);
      broadcast_pad_added (tablet_seat, device);

      meta_wayland_tablet_pad_set_focus (pad, pad_focus);
    }
}

static void
meta_wayland_tablet_seat_device_removed (MetaWaylandTabletSeat *tablet_seat,
                                         ClutterInputDevice    *device)
{
  g_hash_table_remove (tablet_seat->tablets, device);
  g_hash_table_remove (tablet_seat->pads, device);
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
meta_wayland_tablet_seat_new (MetaWaylandTabletManager *manager,
                              MetaWaylandSeat          *seat)
{
  MetaWaylandTabletSeat *tablet_seat;
  const GSList *devices, *l;

  tablet_seat = g_slice_new0 (MetaWaylandTabletSeat);
  tablet_seat->manager = manager;
  tablet_seat->seat = seat;
  tablet_seat->device_manager = clutter_device_manager_get_default ();
  tablet_seat->tablets = g_hash_table_new_full (NULL, NULL, NULL,
                                                (GDestroyNotify) meta_wayland_tablet_free);
  tablet_seat->tools = g_hash_table_new_full (NULL, NULL, NULL,
                                              (GDestroyNotify) meta_wayland_tablet_tool_free);
  tablet_seat->pads = g_hash_table_new_full (NULL, NULL, NULL,
                                             (GDestroyNotify) meta_wayland_tablet_pad_free);
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
  g_hash_table_destroy (tablet_seat->pads);
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

  /* Notify client of all available tablets/pads */
  notify_tablets (tablet_seat, resource);
  notify_pads (tablet_seat, resource);

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

MetaWaylandTabletPad *
meta_wayland_tablet_seat_lookup_pad (MetaWaylandTabletSeat *tablet_seat,
                                     ClutterInputDevice    *device)
{
  return g_hash_table_lookup (tablet_seat->pads, device);
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
  MetaWaylandTabletPad *pad = NULL;

  device = clutter_event_get_source_device (event);

  switch (event->type)
    {
    case CLUTTER_PROXIMITY_IN:
    case CLUTTER_PROXIMITY_OUT:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_MOTION:
      device_tool = clutter_event_get_device_tool (event);

      if (device && device_tool)
        tool = meta_wayland_tablet_seat_ensure_tool (tablet_seat, device, device_tool);

      if (!tool)
        return;

      meta_wayland_tablet_tool_update (tool, event);
      break;
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
    case CLUTTER_PAD_RING:
    case CLUTTER_PAD_STRIP:
      pad = g_hash_table_lookup (tablet_seat->pads, device);
      if (!pad)
        return;

      return meta_wayland_tablet_pad_update (pad, event);
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
  MetaWaylandTabletPad *pad = NULL;

  switch (event->type)
    {
    case CLUTTER_PROXIMITY_IN:
    case CLUTTER_PROXIMITY_OUT:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_MOTION:
      device_tool = clutter_event_get_device_tool (event);

      if (device_tool)
        tool = g_hash_table_lookup (tablet_seat->tools, device_tool);

      if (!tool)
        return CLUTTER_EVENT_PROPAGATE;

      meta_wayland_tablet_tool_handle_event (tool, event);
      return CLUTTER_EVENT_PROPAGATE;
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
    case CLUTTER_PAD_RING:
    case CLUTTER_PAD_STRIP:
      pad = g_hash_table_lookup (tablet_seat->pads,
                                 clutter_event_get_source_device (event));
      if (!pad)
        return CLUTTER_EVENT_PROPAGATE;

      return meta_wayland_tablet_pad_handle_event (pad, event);
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

static GList *
lookup_grouped_devices (ClutterInputDevice     *device,
                        ClutterInputDeviceType  type)
{
  ClutterDeviceManager *device_manager;
  const GSList *devices, *l;
  GList *group = NULL;

  device_manager = clutter_device_manager_get_default ();
  devices = clutter_device_manager_peek_devices (device_manager);

  for (l = devices; l; l = l->next)
    {
      if (l->data == device)
        continue;
      if (clutter_input_device_get_device_type (l->data) != type)
        continue;

      if (!clutter_input_device_is_grouped (device, l->data))
        continue;

      group = g_list_prepend (group, l->data);
    }

  return group;
}

MetaWaylandTablet *
meta_wayland_tablet_seat_lookup_paired_tablet (MetaWaylandTabletSeat *tablet_seat,
                                               MetaWaylandTabletPad  *pad)
{
  MetaWaylandTablet *tablet;
  GList *devices;

  devices = lookup_grouped_devices (pad->device, CLUTTER_TABLET_DEVICE);

  if (!devices)
    return NULL;

  /* We only accept one device here */
  g_warn_if_fail (!devices->next);

  tablet = meta_wayland_tablet_seat_lookup_tablet (pad->tablet_seat,
                                                   devices->data);
  g_list_free (devices);

  return tablet;
}

GList *
meta_wayland_tablet_seat_lookup_paired_pads (MetaWaylandTabletSeat *tablet_seat,
                                             MetaWaylandTablet     *tablet)
{
  GList *l, *devices, *pads = NULL;
  MetaWaylandTabletPad *pad;

  devices = lookup_grouped_devices (tablet->device, CLUTTER_PAD_DEVICE);

  for (l = devices; l; l = l->next)
    {
      pad = meta_wayland_tablet_seat_lookup_pad (tablet_seat, l->data);
      if (pad)
        pads = g_list_prepend (pads, pad);
    }

  return pads;
}

void
meta_wayland_tablet_seat_set_pad_focus (MetaWaylandTabletSeat *tablet_seat,
                                        MetaWaylandSurface    *surface)
{
  MetaWaylandTabletPad *pad;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, tablet_seat->pads);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &pad))
    meta_wayland_tablet_pad_set_focus (pad, surface);
}
