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
#include "meta-wayland-tablet-manager.h"
#include "meta-wayland-tablet-seat.h"
#include "meta-wayland-tablet-tool.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
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
          device_type == CLUTTER_CURSOR_DEVICE ||
          device_type == CLUTTER_PAD_DEVICE);
}

static void
tablet_manager_get_tablet_seat (struct wl_client   *client,
                                struct wl_resource *resource,
                                guint32             id,
                                struct wl_resource *seat_resource)
{
  MetaWaylandTabletManager *tablet_manager = wl_resource_get_user_data (resource);
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandTabletSeat *tablet_seat;

  tablet_seat = meta_wayland_tablet_manager_ensure_seat (tablet_manager, seat);
  meta_wayland_tablet_seat_create_new_resource (tablet_seat, client,
                                                resource, id);
}

static void
tablet_manager_destroy (struct wl_client   *client,
                        struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_manager_v2_interface tablet_manager_interface = {
  tablet_manager_get_tablet_seat,
  tablet_manager_destroy
};

static void
bind_tablet_manager (struct wl_client *client,
                     void             *data,
                     uint32_t          version,
                     uint32_t          id)
{
  MetaWaylandCompositor *compositor = data;
  MetaWaylandTabletManager *tablet_manager = compositor->tablet_manager;
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_manager_v2_interface,
                                 MIN (version, 1), id);
  wl_resource_set_implementation (resource, &tablet_manager_interface,
                                  tablet_manager, unbind_resource);
  wl_resource_set_user_data (resource, tablet_manager);
  wl_list_insert (&tablet_manager->resource_list,
                  wl_resource_get_link (resource));
}

static MetaWaylandTabletManager *
meta_wayland_tablet_manager_new (MetaWaylandCompositor *compositor)
{
  MetaWaylandTabletManager *tablet_manager;

  tablet_manager = g_slice_new0 (MetaWaylandTabletManager);
  tablet_manager->compositor = compositor;
  tablet_manager->wl_display = compositor->wayland_display;
  tablet_manager->seats = g_hash_table_new_full (NULL, NULL, NULL,
                                                 (GDestroyNotify) meta_wayland_tablet_seat_free);
  wl_list_init (&tablet_manager->resource_list);

  wl_global_create (tablet_manager->wl_display,
                    &zwp_tablet_manager_v2_interface, 1,
                    compositor, bind_tablet_manager);

  return tablet_manager;
}

void
meta_wayland_tablet_manager_init (MetaWaylandCompositor *compositor)
{
  compositor->tablet_manager = meta_wayland_tablet_manager_new (compositor);
}

void
meta_wayland_tablet_manager_free (MetaWaylandTabletManager *tablet_manager)
{
  ClutterDeviceManager *device_manager;

  device_manager = clutter_device_manager_get_default ();
  g_signal_handlers_disconnect_by_data (device_manager, tablet_manager);

  g_hash_table_destroy (tablet_manager->seats);
  g_slice_free (MetaWaylandTabletManager, tablet_manager);
}

static MetaWaylandTabletSeat *
meta_wayland_tablet_manager_lookup_seat (MetaWaylandTabletManager *manager,
                                         ClutterInputDevice       *device)
{
  MetaWaylandTabletSeat *tablet_seat;
  MetaWaylandSeat *seat;
  GHashTableIter iter;

  if (!device || !is_tablet_device (device))
    return NULL;

  g_hash_table_iter_init (&iter, manager->seats);

  while (g_hash_table_iter_next (&iter, (gpointer*) &seat, (gpointer*) &tablet_seat))
    {
      if (meta_wayland_tablet_seat_lookup_tablet (tablet_seat, device) ||
          meta_wayland_tablet_seat_lookup_pad (tablet_seat, device))
        return tablet_seat;
    }

  return NULL;
}

gboolean
meta_wayland_tablet_manager_consumes_event (MetaWaylandTabletManager *manager,
                                            const ClutterEvent       *event)
{
  ClutterInputDevice *device = clutter_event_get_source_device (event);

  return meta_wayland_tablet_manager_lookup_seat (manager, device) != NULL;
}

void
meta_wayland_tablet_manager_update (MetaWaylandTabletManager *manager,
                                    const ClutterEvent       *event)
{
  ClutterInputDevice *device = clutter_event_get_source_device (event);
  MetaWaylandTabletSeat *tablet_seat;

  tablet_seat = meta_wayland_tablet_manager_lookup_seat (manager, device);

  if (!tablet_seat)
    return;

  switch (event->type)
    {
    case CLUTTER_PROXIMITY_IN:
    case CLUTTER_PROXIMITY_OUT:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_MOTION:
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
    case CLUTTER_PAD_RING:
    case CLUTTER_PAD_STRIP:
      meta_wayland_tablet_seat_update (tablet_seat, event);
      break;
    default:
      break;
    }
}

gboolean
meta_wayland_tablet_manager_handle_event (MetaWaylandTabletManager *manager,
                                          const ClutterEvent       *event)
{
  ClutterInputDevice *device = clutter_event_get_source_device (event);
  MetaWaylandTabletSeat *tablet_seat;

  tablet_seat = meta_wayland_tablet_manager_lookup_seat (manager, device);

  if (!tablet_seat)
    return CLUTTER_EVENT_PROPAGATE;

  switch (event->type)
    {
    case CLUTTER_PROXIMITY_IN:
    case CLUTTER_PROXIMITY_OUT:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_MOTION:
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
    case CLUTTER_PAD_RING:
    case CLUTTER_PAD_STRIP:
      return meta_wayland_tablet_seat_handle_event (tablet_seat, event);
    default:
      return CLUTTER_EVENT_PROPAGATE;
    }
}

MetaWaylandTabletSeat *
meta_wayland_tablet_manager_ensure_seat (MetaWaylandTabletManager *manager,
                                         MetaWaylandSeat          *seat)
{
  MetaWaylandTabletSeat *tablet_seat;

  tablet_seat = g_hash_table_lookup (manager->seats, seat);

  if (!tablet_seat)
    {
      tablet_seat = meta_wayland_tablet_seat_new (manager, seat);
      g_hash_table_insert (manager->seats, seat, tablet_seat);
    }

  return tablet_seat;
}

void
meta_wayland_tablet_manager_update_cursor_position (MetaWaylandTabletManager *manager,
                                                    const ClutterEvent       *event)
{
  MetaWaylandTabletSeat *tablet_seat = NULL;
  MetaWaylandTabletTool *tool = NULL;
  ClutterInputDeviceTool *device_tool;
  ClutterInputDevice *device;

  device = clutter_event_get_source_device (event);
  device_tool = clutter_event_get_device_tool (event);

  if (device)
    tablet_seat = meta_wayland_tablet_manager_lookup_seat (manager, device);

  if (tablet_seat && device_tool)
    tool = meta_wayland_tablet_seat_lookup_tool (tablet_seat, device_tool);

  if (tool)
    {
      gfloat new_x, new_y;

      clutter_event_get_coords (event, &new_x, &new_y);
      meta_wayland_tablet_tool_set_cursor_position (tool, new_x, new_y);
    }
}
