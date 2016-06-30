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

#ifndef META_WAYLAND_TABLET_PAD_STRIP_H
#define META_WAYLAND_TABLET_PAD_STRIP_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"
#include "meta-cursor-renderer.h"

struct _MetaWaylandTabletPadStrip
{
  MetaWaylandTabletPad *pad;
  MetaWaylandTabletPadGroup *group;

  struct wl_list resource_list;
  struct wl_list focus_resource_list;

  gchar *feedback;
};

MetaWaylandTabletPadStrip * meta_wayland_tablet_pad_strip_new  (MetaWaylandTabletPad      *pad);
void                        meta_wayland_tablet_pad_strip_free (MetaWaylandTabletPadStrip *strip);

void                        meta_wayland_tablet_pad_strip_set_group (MetaWaylandTabletPadStrip *strip,
                                                                     MetaWaylandTabletPadGroup *group);

struct wl_resource *
             meta_wayland_tablet_pad_strip_create_new_resource (MetaWaylandTabletPadStrip *strip,
                                                                struct wl_client          *client,
                                                                struct wl_resource        *group_resource,
                                                                uint32_t                   id);

gboolean     meta_wayland_tablet_pad_strip_handle_event        (MetaWaylandTabletPadStrip *strip,
                                                                const ClutterEvent        *event);

void         meta_wayland_tablet_pad_strip_sync_focus          (MetaWaylandTabletPadStrip *strip);

#endif /* META_WAYLAND_TABLET_PAD_STRIP_H */
