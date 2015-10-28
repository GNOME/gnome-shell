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
#include "meta-surface-actor-wayland.h"
#include "meta-wayland-tablet.h"
#include "meta-wayland-tablet-seat.h"
#include "meta-wayland-tablet-tool.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
move_resources (struct wl_list *destination,
                struct wl_list *source)
{
  wl_list_insert_list (destination, source);
  wl_list_init (source);
}

static void
move_resources_for_client (struct wl_list   *destination,
                           struct wl_list   *source,
                           struct wl_client *client)
{
  struct wl_resource *resource, *tmp;

  wl_resource_for_each_safe (resource, tmp, source)
    {
      if (wl_resource_get_client (resource) == client)
        {
          wl_list_remove (wl_resource_get_link (resource));
          wl_list_insert (destination, wl_resource_get_link (resource));
        }
    }
}

static void
broadcast_proximity_in (MetaWaylandTabletTool *tool)
{
  struct wl_resource *resource, *tablet_resource;
  struct wl_client *client;

  client = wl_resource_get_client (tool->focus_surface->resource);
  tablet_resource = meta_wayland_tablet_lookup_resource (tool->current_tablet,
                                                         client);

  wl_resource_for_each (resource, &tool->focus_resource_list)
    {
      zwp_tablet_tool_v1_send_proximity_in (resource, tool->proximity_serial,
                                            tablet_resource,
                                            tool->focus_surface->resource);
    }
}

static void
broadcast_proximity_out (MetaWaylandTabletTool *tool)
{
  struct wl_resource *resource;

  wl_resource_for_each (resource, &tool->focus_resource_list)
    {
      zwp_tablet_tool_v1_send_proximity_out (resource);
    }
}

static void
broadcast_frame (MetaWaylandTabletTool *tool,
                 const ClutterEvent    *event)
{
  struct wl_resource *resource;
  guint32 _time = event ? clutter_event_get_time (event) : CLUTTER_CURRENT_TIME;

  wl_resource_for_each (resource, &tool->focus_resource_list)
    {
      zwp_tablet_tool_v1_send_frame (resource, _time);
    }
}

static void
meta_wayland_tablet_tool_set_focus (MetaWaylandTabletTool *tool,
                                    MetaWaylandSurface    *surface,
                                    const ClutterEvent    *event)
{
  if (tool->focus_surface == surface)
    return;

  if (tool->focus_surface != NULL)
    {
      struct wl_list *l;

      l = &tool->focus_resource_list;
      if (!wl_list_empty (l))
        {
          broadcast_proximity_out (tool);
          broadcast_frame (tool, event);
          move_resources (&tool->resource_list, &tool->focus_resource_list);
        }

      wl_list_remove (&tool->focus_surface_destroy_listener.link);
      tool->focus_surface = NULL;
    }

  if (surface != NULL)
    {
      struct wl_client *client;
      struct wl_list *l;

      tool->focus_surface = surface;
      client = wl_resource_get_client (tool->focus_surface->resource);
      wl_resource_add_destroy_listener (tool->focus_surface->resource,
                                        &tool->focus_surface_destroy_listener);

      move_resources_for_client (&tool->focus_resource_list,
                                 &tool->resource_list, client);

      l = &tool->focus_resource_list;

      if (!wl_list_empty (l))
        {
          struct wl_client *client = wl_resource_get_client (tool->focus_surface->resource);
          struct wl_display *display = wl_client_get_display (client);

          tool->proximity_serial = wl_display_next_serial (display);

          broadcast_proximity_in (tool);
          broadcast_frame (tool, event);
        }
    }
}

static void
tablet_tool_handle_focus_surface_destroy (struct wl_listener *listener,
                                          void               *data)
{
  MetaWaylandTabletTool *tool;

  tool = wl_container_of (listener, tool, focus_surface_destroy_listener);
  meta_wayland_tablet_tool_set_focus (tool, NULL, NULL);
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
  wl_list_init (&tool->focus_resource_list);

  tool->focus_surface_destroy_listener.notify = tablet_tool_handle_focus_surface_destroy;

  return tool;
}

void
meta_wayland_tablet_tool_free (MetaWaylandTabletTool *tool)
{
  struct wl_resource *resource, *next;

  meta_wayland_tablet_tool_set_focus (tool, NULL, NULL);

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

static void
emit_proximity_in (MetaWaylandTabletTool *tool,
                   struct wl_resource    *resource)
{
  struct wl_resource *tablet_resource;
  struct wl_client *client;

  if (!tool->focus_surface)
    return;

  client = wl_resource_get_client (resource);
  tablet_resource = meta_wayland_tablet_lookup_resource (tool->current_tablet,
                                                         client);

  zwp_tablet_tool_v1_send_proximity_in (resource, tool->proximity_serial,
                                        tablet_resource, tool->focus_surface->resource);
}

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

  if (tool->focus_surface &&
      wl_resource_get_client (tool->focus_surface->resource) == client)
    {
      wl_list_insert (&tool->focus_resource_list, wl_resource_get_link (resource));
      emit_proximity_in (tool, resource);
    }
  else
    {
      wl_list_insert (&tool->resource_list, wl_resource_get_link (resource));
    }

  return resource;
}

struct wl_resource *
meta_wayland_tablet_tool_lookup_resource (MetaWaylandTabletTool *tool,
                                          struct wl_client      *client)
{
  struct wl_resource *resource = NULL;

  if (!wl_list_empty (&tool->resource_list))
    resource = wl_resource_find_for_client (&tool->resource_list, client);

  if (!wl_list_empty (&tool->focus_resource_list))
    resource = wl_resource_find_for_client (&tool->focus_resource_list, client);

  return resource;
}

static void
meta_wayland_tablet_tool_account_button (MetaWaylandTabletTool *tool,
                                         const ClutterEvent    *event)
{
  if (event->type == CLUTTER_BUTTON_PRESS)
    tool->pressed_buttons |= 1 << (event->button.button - 1);
  else if (event->type == CLUTTER_BUTTON_RELEASE)
    tool->pressed_buttons &= ~(1 << (event->button.button - 1));
}

static void
sync_focus_surface (MetaWaylandTabletTool *tool,
                    const ClutterEvent    *event)
{
  MetaDisplay *display = meta_get_display ();

  switch (display->event_route)
    {
    case META_EVENT_ROUTE_WINDOW_OP:
    case META_EVENT_ROUTE_COMPOSITOR_GRAB:
    case META_EVENT_ROUTE_FRAME_BUTTON:
      /* The compositor has a grab, so remove our focus */
      meta_wayland_tablet_tool_set_focus (tool, NULL, event);
      break;

    case META_EVENT_ROUTE_NORMAL:
    case META_EVENT_ROUTE_WAYLAND_POPUP:
      meta_wayland_tablet_tool_set_focus (tool, tool->current, event);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
repick_for_event (MetaWaylandTabletTool *tool,
                  const ClutterEvent    *for_event)
{
  ClutterActor *actor = NULL;

  actor = clutter_event_get_source (for_event);

  if (META_IS_SURFACE_ACTOR_WAYLAND (actor))
    tool->current = meta_surface_actor_wayland_get_surface (META_SURFACE_ACTOR_WAYLAND (actor));
  else
    tool->current = NULL;

  sync_focus_surface (tool, for_event);
}

void
meta_wayland_tablet_tool_update (MetaWaylandTabletTool *tool,
                                 const ClutterEvent    *event)
{
  switch (event->type)
    {
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      meta_wayland_tablet_tool_account_button (tool, event);
      break;
    case CLUTTER_MOTION:
      if (!tool->pressed_buttons)
        repick_for_event (tool, event);
      break;
    case CLUTTER_PROXIMITY_IN:
      tool->current_tablet =
        meta_wayland_tablet_seat_lookup_tablet (tool->seat,
                                                clutter_event_get_source_device (event));
      break;
    default:
      break;
    }
}

gboolean
meta_wayland_tablet_tool_handle_event (MetaWaylandTabletTool *tool,
                                       const ClutterEvent    *event)
{
  switch (event->type)
    {
    case CLUTTER_PROXIMITY_IN:
      /* We don't have much info here to make anything useful out of it,
       * wait until the first motion event so we have both coordinates
       * and tool.
       */
      break;
    case CLUTTER_PROXIMITY_OUT:
      meta_wayland_tablet_tool_set_focus (tool, NULL, event);
      break;
    default:
      return CLUTTER_EVENT_PROPAGATE;
    }

  return CLUTTER_EVENT_STOP;
}
