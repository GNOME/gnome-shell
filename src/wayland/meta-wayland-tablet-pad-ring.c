/*
 * Wayland Support
 *
 * Copyright (C) 2016 Red Hat
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

#include "meta-surface-actor-wayland.h"
#include "meta-wayland-private.h"
#include "meta-wayland-tablet-pad.h"
#include "meta-wayland-tablet-pad-group.h"
#include "meta-wayland-tablet-pad-ring.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

MetaWaylandTabletPadRing *
meta_wayland_tablet_pad_ring_new (MetaWaylandTabletPad *pad)
{
  MetaWaylandTabletPadRing *ring;

  ring = g_slice_new0 (MetaWaylandTabletPadRing);
  wl_list_init (&ring->resource_list);
  wl_list_init (&ring->focus_resource_list);
  ring->pad = pad;

  return ring;
}

void
meta_wayland_tablet_pad_ring_free (MetaWaylandTabletPadRing *ring)
{
  struct wl_resource *resource, *next;

  wl_resource_for_each_safe (resource, next, &ring->resource_list)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_free (ring->feedback);
  g_slice_free (MetaWaylandTabletPadRing, ring);
}

static void
tablet_pad_ring_set_feedback (struct wl_client   *client,
                              struct wl_resource *resource,
                              const char         *str,
                              uint32_t            serial)
{
  MetaWaylandTabletPadRing *ring = wl_resource_get_user_data (resource);

  if (ring->group->mode_switch_serial != serial)
    return;

  ring->feedback = g_strdup (str);
}

static void
tablet_pad_ring_destroy (struct wl_client   *client,
                         struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_pad_ring_v2_interface ring_interface = {
  tablet_pad_ring_set_feedback,
  tablet_pad_ring_destroy,
};

struct wl_resource *
meta_wayland_tablet_pad_ring_create_new_resource (MetaWaylandTabletPadRing *ring,
                                                  struct wl_client         *client,
                                                  struct wl_resource       *group_resource,
                                                  uint32_t                  id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_pad_ring_v2_interface,
                                 wl_resource_get_version (group_resource), id);
  wl_resource_set_implementation (resource, &ring_interface,
                                  ring, unbind_resource);
  wl_resource_set_user_data (resource, ring);
  wl_list_insert (&ring->resource_list, wl_resource_get_link (resource));

  return resource;
}

gboolean
meta_wayland_tablet_pad_ring_handle_event (MetaWaylandTabletPadRing *ring,
                                           const ClutterEvent       *event)
{
  struct wl_list *focus_resources = &ring->focus_resource_list;
  enum zwp_tablet_pad_ring_v2_source source;
  gboolean source_known = FALSE;
  struct wl_resource *resource;

  if (wl_list_empty (focus_resources))
    return FALSE;
  if (event->type != CLUTTER_PAD_RING)
    return FALSE;

  if (event->pad_ring.ring_source == CLUTTER_INPUT_DEVICE_PAD_SOURCE_FINGER)
    {
      source = ZWP_TABLET_PAD_RING_V2_SOURCE_FINGER;
      source_known = TRUE;
    }

  wl_resource_for_each (resource, focus_resources)
    {
      gdouble angle = event->pad_ring.angle;

      if (source_known)
        zwp_tablet_pad_ring_v2_send_source (resource, source);

      if (angle >= 0)
        zwp_tablet_pad_ring_v2_send_angle (resource,
                                           wl_fixed_from_double (angle));
      else
        zwp_tablet_pad_ring_v2_send_stop (resource);

      zwp_tablet_pad_ring_v2_send_frame (resource,
                                         clutter_event_get_time (event));
    }

  return TRUE;
}

static void
move_resources (struct wl_list *destination, struct wl_list *source)
{
  wl_list_insert_list (destination, source);
  wl_list_init (source);
}

static void
move_resources_for_client (struct wl_list *destination,
			   struct wl_list *source,
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

void
meta_wayland_tablet_pad_ring_sync_focus (MetaWaylandTabletPadRing *ring)
{
  g_clear_pointer (&ring->feedback, g_free);

  if (!wl_list_empty (&ring->focus_resource_list))
    {
      move_resources (&ring->resource_list, &ring->focus_resource_list);
    }

  if (ring->pad->focus_surface != NULL)
    {
      move_resources_for_client (&ring->focus_resource_list,
                                 &ring->resource_list,
                                 wl_resource_get_client (ring->pad->focus_surface->resource));
    }
}

void
meta_wayland_tablet_pad_ring_set_group (MetaWaylandTabletPadRing  *ring,
					MetaWaylandTabletPadGroup *group)
{
  /* Group is static, can only be set once */
  g_assert (ring->group == NULL);

  ring->group = group;
  group->rings = g_list_append (group->rings, ring);
}
