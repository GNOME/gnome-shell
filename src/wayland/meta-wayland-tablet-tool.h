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

#ifndef META_WAYLAND_TABLET_TOOL_H
#define META_WAYLAND_TABLET_TOOL_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"
#include "meta-cursor-renderer.h"

struct _MetaWaylandTabletTool
{
  MetaWaylandTabletSeat *seat;
  ClutterInputDevice *device;
  ClutterInputDeviceTool *device_tool;
  struct wl_list resource_list;
  struct wl_list focus_resource_list;

  MetaWaylandSurface *focus_surface;
  struct wl_listener focus_surface_destroy_listener;

  MetaWaylandSurface *cursor_surface;
  struct wl_listener cursor_surface_destroy_listener;
  MetaCursorRenderer *cursor_renderer;
  MetaCursorSprite *default_sprite;
  guint prepare_at_signal_id;

  MetaWaylandSurface *current;
  guint32 pressed_buttons;
  guint32 button_count;

  guint32 proximity_serial;
  guint32 down_serial;
  guint32 button_serial;

  float grab_x, grab_y;

  MetaWaylandTablet *current_tablet;
};

MetaWaylandTabletTool * meta_wayland_tablet_tool_new  (MetaWaylandTabletSeat  *seat,
                                                       ClutterInputDevice     *device,
                                                       ClutterInputDeviceTool *device_tool);
void                    meta_wayland_tablet_tool_free (MetaWaylandTabletTool  *tool);

struct wl_resource *
         meta_wayland_tablet_tool_create_new_resource (MetaWaylandTabletTool  *tool,
                                                       struct wl_client       *client,
                                                       struct wl_resource     *seat_resource,
                                                       uint32_t                id);
struct wl_resource *
         meta_wayland_tablet_tool_lookup_resource     (MetaWaylandTabletTool  *tool,
                                                       struct wl_client       *client);

void     meta_wayland_tablet_tool_update              (MetaWaylandTabletTool  *tool,
                                                       const ClutterEvent     *event);
gboolean meta_wayland_tablet_tool_handle_event        (MetaWaylandTabletTool  *tool,
                                                       const ClutterEvent     *event);

void     meta_wayland_tablet_tool_set_cursor_position (MetaWaylandTabletTool  *tool,
                                                       float                   new_x,
                                                       float                   new_y);

gboolean meta_wayland_tablet_tool_can_grab_surface (MetaWaylandTabletTool *tool,
                                                    MetaWaylandSurface    *surface,
                                                    uint32_t               serial);

#endif /* META_WAYLAND_TABLET_TOOL_H */
