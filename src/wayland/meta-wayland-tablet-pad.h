/*
 * Wayland Support
 *
 * Copyright (C) 2016 Red Hat
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

#ifndef META_WAYLAND_TABLET_PAD_H
#define META_WAYLAND_TABLET_PAD_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"
#include "meta-cursor-renderer.h"

struct _MetaWaylandTabletPad
{
  MetaWaylandTabletSeat *tablet_seat;
  ClutterInputDevice *device;

  struct wl_list resource_list;
  struct wl_list focus_resource_list;

  MetaWaylandSurface *focus_surface;
  struct wl_listener focus_surface_listener;
  uint32_t focus_serial;

  uint32_t n_buttons;
  GList *groups;
  GList *rings;
  GList *strips;

  GHashTable *feedback;

  MetaWaylandSurface *focus;
};

MetaWaylandTabletPad * meta_wayland_tablet_pad_new       (ClutterInputDevice    *device,
                                                          MetaWaylandTabletSeat *tablet_seat);
void                   meta_wayland_tablet_pad_free      (MetaWaylandTabletPad  *pad);

struct wl_resource *
             meta_wayland_tablet_pad_create_new_resource (MetaWaylandTabletPad *pad,
                                                          struct wl_client     *client,
                                                          struct wl_resource   *seat_resource,
                                                          uint32_t              id);
struct wl_resource *
             meta_wayland_tablet_pad_lookup_resource     (MetaWaylandTabletPad *pad,
                                                          struct wl_client     *client);

void         meta_wayland_tablet_pad_notify              (MetaWaylandTabletPad *pad,
                                                          struct wl_resource   *resource);

void         meta_wayland_tablet_pad_update              (MetaWaylandTabletPad *pad,
                                                          const ClutterEvent   *event);
gboolean     meta_wayland_tablet_pad_handle_event        (MetaWaylandTabletPad *pad,
                                                          const ClutterEvent   *event);

void         meta_wayland_tablet_pad_set_focus           (MetaWaylandTabletPad *pad,
                                                          MetaWaylandSurface   *surface);

gchar *      meta_wayland_tablet_pad_get_label           (MetaWaylandTabletPad *pad,
							  MetaPadActionType     type,
							  guint                 action);

#endif /* META_WAYLAND_TABLET_PAD_H */
