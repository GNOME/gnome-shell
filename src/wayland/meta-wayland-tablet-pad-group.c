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

#include <wayland-server.h>
#include "tablet-unstable-v2-server-protocol.h"

#include "meta-surface-actor-wayland.h"
#include "meta-wayland-tablet-seat.h"
#include "meta-wayland-tablet-pad.h"
#include "meta-wayland-tablet-pad-group.h"
#include "meta-wayland-tablet-pad-ring.h"
#include "meta-wayland-tablet-pad-strip.h"

#ifdef HAVE_NATIVE_BACKEND
#include <clutter/evdev/clutter-evdev.h>
#include "backends/native/meta-backend-native.h"
#endif

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

MetaWaylandTabletPadGroup *
meta_wayland_tablet_pad_group_new (MetaWaylandTabletPad *pad)
{
  MetaWaylandTabletPadGroup *group;

  group = g_slice_new0 (MetaWaylandTabletPadGroup);
  wl_list_init (&group->resource_list);
  wl_list_init (&group->focus_resource_list);
  group->pad = pad;

  return group;
}

void
meta_wayland_tablet_pad_group_free (MetaWaylandTabletPadGroup *group)
{
  struct wl_resource *resource, *next;

  wl_resource_for_each_safe (resource, next, &group->resource_list)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_list_free (group->rings);
  g_list_free (group->strips);

  g_slice_free (MetaWaylandTabletPadGroup, group);
}

static void
tablet_pad_group_destroy (struct wl_client   *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_pad_group_v2_interface group_interface = {
  tablet_pad_group_destroy
};

struct wl_resource *
meta_wayland_tablet_pad_group_create_new_resource (MetaWaylandTabletPadGroup *group,
                                                   struct wl_client          *client,
                                                   struct wl_resource        *pad_resource,
                                                   uint32_t                   id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_pad_group_v2_interface,
                                 wl_resource_get_version (pad_resource), id);
  wl_resource_set_implementation (resource, &group_interface,
                                  group, unbind_resource);
  wl_resource_set_user_data (resource, group);
  wl_list_insert (&group->resource_list, wl_resource_get_link (resource));

  return resource;
}

struct wl_resource *
meta_wayland_tablet_pad_group_lookup_resource (MetaWaylandTabletPadGroup *group,
                                               struct wl_client          *client)
{
  struct wl_resource *resource;

  resource = wl_resource_find_for_client (&group->resource_list, client);

  if (!resource)
    resource = wl_resource_find_for_client (&group->focus_resource_list, client);

  return resource;
}

gboolean
meta_wayland_tablet_pad_group_has_button (MetaWaylandTabletPadGroup *group,
                                          guint                      button)
{
  MetaBackend *backend = meta_get_backend ();

#ifdef HAVE_NATIVE_BACKEND
  if (META_IS_BACKEND_NATIVE (backend))
    {
      struct libinput_device *libinput_device;
      struct libinput_tablet_pad_mode_group *mode_group;
      guint n_group;

      libinput_device = clutter_evdev_input_device_get_libinput_device (group->pad->device);
      n_group = g_list_index (group->pad->groups, group);
      mode_group = libinput_device_tablet_pad_get_mode_group (libinput_device, n_group);

      return libinput_tablet_pad_mode_group_has_button (mode_group, button);
    }
  else
#endif
    {
      return g_list_length (group->pad->groups) == 1;
    }
}

static void
meta_wayland_tablet_pad_group_send_buttons (MetaWaylandTabletPadGroup *group,
                                            struct wl_resource        *resource)
{
  struct wl_array buttons;
  guint i;

  wl_array_init (&buttons);

  for (i = 0; i < group->pad->n_buttons; i++)
    {
      uint32_t *pos;

      if (!meta_wayland_tablet_pad_group_has_button (group, i))
        continue;

      pos = wl_array_add (&buttons, sizeof (*pos));
      *pos = i;
    }

  zwp_tablet_pad_group_v2_send_buttons (resource, &buttons);
  wl_array_release (&buttons);
}

void
meta_wayland_tablet_pad_group_notify (MetaWaylandTabletPadGroup *group,
                                      struct wl_resource        *resource)
{
  struct wl_client *client = wl_resource_get_client (resource);
  struct wl_array buttons;
  guint n_group, n_modes;
  GList *l;

  wl_array_init (&buttons);

  /* Buttons */
  meta_wayland_tablet_pad_group_send_buttons (group, resource);

  /* Rings */
  for (l = group->rings; l; l = l->next)
    {
      MetaWaylandTabletPadRing *ring = l->data;
      struct wl_resource *ring_resource;

      ring_resource = meta_wayland_tablet_pad_ring_create_new_resource (ring,
                                                                        client,
                                                                        resource,
                                                                        0);
      zwp_tablet_pad_group_v2_send_ring (resource, ring_resource);
    }

  /* Strips */
  for (l = group->strips; l; l = l->next)
    {
      MetaWaylandTabletPadStrip *strip = l->data;
      struct wl_resource *strip_resource;

      strip_resource = meta_wayland_tablet_pad_strip_create_new_resource (strip,
                                                                          client,
                                                                          resource,
                                                                          0);
      zwp_tablet_pad_group_v2_send_strip (resource, strip_resource);
    }

  n_group = g_list_index (group->pad->groups, group);
  n_modes = clutter_input_device_get_group_n_modes (group->pad->device,
                                                    n_group);

  zwp_tablet_pad_group_v2_send_modes (resource, n_modes);
  zwp_tablet_pad_group_v2_send_done (resource);
}

void
meta_wayland_tablet_pad_group_update (MetaWaylandTabletPadGroup *group,
                                      const ClutterEvent        *event)
{
  switch (event->type)
    {
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
      if (meta_wayland_tablet_pad_group_is_mode_switch_button (group, event->pad_button.button))
        group->current_mode = event->pad_button.mode;
      break;
    default:
      break;
    }
}

static gboolean
handle_pad_ring_event (MetaWaylandTabletPadGroup *group,
                       const ClutterEvent        *event)
{
  MetaWaylandTabletPadRing *ring;

  if (event->type != CLUTTER_PAD_RING)
    return FALSE;

  ring = g_list_nth_data (group->rings, event->pad_ring.ring_number);

  if (!ring)
    return FALSE;

  return meta_wayland_tablet_pad_ring_handle_event (ring, event);
}

static gboolean
handle_pad_strip_event (MetaWaylandTabletPadGroup *group,
                        const ClutterEvent        *event)
{
  MetaWaylandTabletPadStrip *strip;

  if (event->type != CLUTTER_PAD_STRIP)
    return FALSE;

  strip = g_list_nth_data (group->strips, event->pad_strip.strip_number);

  if (!strip)
    return FALSE;

  return meta_wayland_tablet_pad_strip_handle_event (strip, event);
}

static void
broadcast_group_mode (MetaWaylandTabletPadGroup *group,
                      uint32_t                   time)
{
  struct wl_display *display = group->pad->tablet_seat->seat->wl_display;
  struct wl_resource *resource;

  group->mode_switch_serial = wl_display_next_serial (display);

  wl_resource_for_each (resource, &group->focus_resource_list)
    {
      zwp_tablet_pad_group_v2_send_mode_switch (resource, time,
                                                group->mode_switch_serial,
                                                group->current_mode);
    }
}

static void
broadcast_group_buttons (MetaWaylandTabletPadGroup *group)
{
  struct wl_resource *resource;

  wl_resource_for_each (resource, &group->focus_resource_list)
    {
      meta_wayland_tablet_pad_group_send_buttons (group, resource);
    }
}

gboolean
meta_wayland_tablet_pad_group_handle_event (MetaWaylandTabletPadGroup *group,
                                            const ClutterEvent        *event)
{
  switch (clutter_event_type (event))
    {
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
      if (meta_wayland_tablet_pad_group_is_mode_switch_button (group, event->pad_button.button))
        {
          if (event->type == CLUTTER_PAD_BUTTON_PRESS)
            broadcast_group_mode (group, clutter_event_get_time (event));
          return TRUE;
        }
      else
        {
          return FALSE;
        }
      break;
    case CLUTTER_PAD_RING:
      return handle_pad_ring_event (group, event);
    case CLUTTER_PAD_STRIP:
      return handle_pad_strip_event (group, event);
    default:
      return FALSE;
    }
}

static void
meta_wayland_tablet_pad_group_update_rings_focus (MetaWaylandTabletPadGroup *group)
{
  GList *l;

  for (l = group->rings; l; l = l->next)
    meta_wayland_tablet_pad_ring_sync_focus (l->data);
}

static void
meta_wayland_tablet_pad_group_update_strips_focus (MetaWaylandTabletPadGroup *group)
{
  GList *l;

  for (l = group->strips; l; l = l->next)
    meta_wayland_tablet_pad_strip_sync_focus (l->data);
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
meta_wayland_tablet_pad_group_sync_focus (MetaWaylandTabletPadGroup *group)
{
  if (!wl_list_empty (&group->focus_resource_list))
    {
      move_resources (&group->resource_list, &group->focus_resource_list);
    }

  if (group->pad->focus_surface != NULL)
    {
      move_resources_for_client (&group->focus_resource_list,
                                 &group->resource_list,
                                 wl_resource_get_client (group->pad->focus_surface->resource));
    }

  meta_wayland_tablet_pad_group_update_rings_focus (group);
  meta_wayland_tablet_pad_group_update_strips_focus (group);

  if (!wl_list_empty (&group->focus_resource_list))
    {
      broadcast_group_mode (group, clutter_get_current_event_time ());
      broadcast_group_buttons (group);
    }
}

gboolean
meta_wayland_tablet_pad_group_is_mode_switch_button (MetaWaylandTabletPadGroup *group,
                                                     guint                      button)
{
  gint n_group = g_list_index (group->pad->groups, group);

  g_assert (n_group >= 0);

  return clutter_input_device_is_mode_switch_button (group->pad->device,
                                                     n_group, button);
}
