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

#ifndef META_WAYLAND_TABLET_PAD_RING_H
#define META_WAYLAND_TABLET_PAD_RING_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"
#include "meta-cursor-renderer.h"

struct _MetaWaylandTabletPadRing
{
  MetaWaylandTabletPad *pad;
  MetaWaylandTabletPadGroup *group;

  struct wl_list resource_list;
  struct wl_list focus_resource_list;

  gchar *feedback;
};

MetaWaylandTabletPadRing * meta_wayland_tablet_pad_ring_new  (MetaWaylandTabletPad *pad);
void                       meta_wayland_tablet_pad_ring_free (MetaWaylandTabletPadRing *ring);

void                       meta_wayland_tablet_pad_ring_set_group (MetaWaylandTabletPadRing  *ring,
								   MetaWaylandTabletPadGroup *group);
struct wl_resource *
             meta_wayland_tablet_pad_ring_create_new_resource (MetaWaylandTabletPadRing *ring,
                                                               struct wl_client         *client,
                                                               struct wl_resource       *group_resource,
                                                               uint32_t                  id);

gboolean     meta_wayland_tablet_pad_ring_handle_event        (MetaWaylandTabletPadRing *ring,
                                                               const ClutterEvent       *event);

void         meta_wayland_tablet_pad_ring_sync_focus          (MetaWaylandTabletPadRing *ring);

#endif /* META_WAYLAND_TABLET_PAD_RING_H */

