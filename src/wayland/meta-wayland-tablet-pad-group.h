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

#ifndef META_WAYLAND_TABLET_PAD_GROUP_H
#define META_WAYLAND_TABLET_PAD_GROUP_H

#include <wayland-server.h>

#include <glib.h>

#include "clutter/clutter.h"
#include "meta-wayland-types.h"

struct _MetaWaylandTabletPadGroup
{
  MetaWaylandTabletPad *pad;
  GArray *buttons;
  uint32_t n_modes;
  uint32_t current_mode;

  struct wl_list resource_list;
  struct wl_list focus_resource_list;
  uint32_t mode_switch_serial;

  GList *strips;
  GList *rings;
};

MetaWaylandTabletPadGroup * meta_wayland_tablet_pad_group_new  (MetaWaylandTabletPad      *pad);
void                        meta_wayland_tablet_pad_group_free (MetaWaylandTabletPadGroup *group);

struct wl_resource *
             meta_wayland_tablet_pad_group_create_new_resource (MetaWaylandTabletPadGroup *group,
                                                                struct wl_client          *client,
                                                                struct wl_resource        *pad_resource,
                                                                uint32_t                   id);
struct wl_resource *
             meta_wayland_tablet_pad_group_lookup_resource     (MetaWaylandTabletPadGroup *group,
                                                                struct wl_client          *client);

void         meta_wayland_tablet_pad_group_notify              (MetaWaylandTabletPadGroup *group,
                                                                struct wl_resource        *resource);

void         meta_wayland_tablet_pad_group_update              (MetaWaylandTabletPadGroup *group,
                                                                const ClutterEvent        *event);
gboolean     meta_wayland_tablet_pad_group_handle_event        (MetaWaylandTabletPadGroup *group,
                                                                const ClutterEvent        *event);

void         meta_wayland_tablet_pad_group_sync_focus          (MetaWaylandTabletPadGroup *group);

gboolean     meta_wayland_tablet_pad_group_has_button            (MetaWaylandTabletPadGroup *group,
                                                                  guint                      button);
gboolean     meta_wayland_tablet_pad_group_is_mode_switch_button (MetaWaylandTabletPadGroup *group,
                                                                  guint                      button);

#endif /* META_WAYLAND_TABLET_PAD_GROUP_H */
