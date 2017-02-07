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
#include "meta-wayland-tablet-pad-strip.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

MetaWaylandTabletPadStrip *
meta_wayland_tablet_pad_strip_new (MetaWaylandTabletPad *pad)
{
  MetaWaylandTabletPadStrip *strip;

  strip = g_slice_new0 (MetaWaylandTabletPadStrip);
  wl_list_init (&strip->resource_list);
  wl_list_init (&strip->focus_resource_list);
  strip->pad = pad;

  return strip;
}

void
meta_wayland_tablet_pad_strip_free (MetaWaylandTabletPadStrip *strip)
{
  struct wl_resource *resource, *next;

  wl_resource_for_each_safe (resource, next, &strip->resource_list)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_free (strip->feedback);
  g_slice_free (MetaWaylandTabletPadStrip, strip);
}

static void
tablet_pad_strip_set_feedback (struct wl_client   *client,
                               struct wl_resource *resource,
                               const char         *str,
                               uint32_t            serial)
{
  MetaWaylandTabletPadStrip *strip = wl_resource_get_user_data (resource);

  if (strip->group->mode_switch_serial != serial)
    return;

  strip->feedback = g_strdup (str);
}

static void
tablet_pad_strip_destroy (struct wl_client   *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_pad_strip_v2_interface strip_interface = {
  tablet_pad_strip_set_feedback,
  tablet_pad_strip_destroy,
};

struct wl_resource *
meta_wayland_tablet_pad_strip_create_new_resource (MetaWaylandTabletPadStrip *strip,
                                                   struct wl_client          *client,
                                                   struct wl_resource        *group_resource,
                                                   uint32_t                   id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_pad_strip_v2_interface,
                                 wl_resource_get_version (group_resource), id);
  wl_resource_set_implementation (resource, &strip_interface,
                                  strip, unbind_resource);
  wl_resource_set_user_data (resource, strip);
  wl_list_insert (&strip->resource_list, wl_resource_get_link (resource));

  return resource;
}

gboolean
meta_wayland_tablet_pad_strip_handle_event (MetaWaylandTabletPadStrip *strip,
                                            const ClutterEvent        *event)
{
  struct wl_list *focus_resources = &strip->focus_resource_list;
  enum zwp_tablet_pad_strip_v2_source source;
  gboolean source_known = FALSE;
  struct wl_resource *resource;

  if (wl_list_empty (focus_resources))
    return FALSE;
  if (event->type != CLUTTER_PAD_STRIP)
    return FALSE;

  if (event->pad_strip.strip_source == CLUTTER_INPUT_DEVICE_PAD_SOURCE_FINGER)
    {
      source = ZWP_TABLET_PAD_STRIP_V2_SOURCE_FINGER;
      source_known = TRUE;
    }

  wl_resource_for_each (resource, focus_resources)
    {
      gdouble value = event->pad_strip.value;

      if (source_known)
        zwp_tablet_pad_strip_v2_send_source (resource, source);

      if (value >= 0)
        zwp_tablet_pad_strip_v2_send_position (resource, (uint32_t) (value * 65535));
      else
        zwp_tablet_pad_strip_v2_send_stop (resource);

      zwp_tablet_pad_strip_v2_send_frame (resource,
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
meta_wayland_tablet_pad_strip_sync_focus (MetaWaylandTabletPadStrip *strip)
{
  g_clear_pointer (&strip->feedback, g_free);

  if (!wl_list_empty (&strip->focus_resource_list))
    {
      move_resources (&strip->resource_list, &strip->focus_resource_list);
    }

  if (strip->pad->focus_surface != NULL)
    {
      move_resources_for_client (&strip->focus_resource_list,
                                 &strip->resource_list,
                                 wl_resource_get_client (strip->pad->focus_surface->resource));
    }
}

void
meta_wayland_tablet_pad_strip_set_group (MetaWaylandTabletPadStrip *strip,
                                         MetaWaylandTabletPadGroup *group)
{
  /* Group is static, can only be set once */
  g_assert (strip->group == NULL);

  strip->group = group;
  group->strips = g_list_append (group->strips, strip);
}
