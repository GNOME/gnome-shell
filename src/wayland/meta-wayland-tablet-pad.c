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

static void
pad_handle_focus_surface_destroy (struct wl_listener *listener,
                                  void               *data)
{
  MetaWaylandTabletPad *pad = wl_container_of (listener, pad, focus_surface_listener);

  meta_wayland_tablet_pad_set_focus (pad, NULL);
}

MetaWaylandTabletPad *
meta_wayland_tablet_pad_new (ClutterInputDevice    *device,
                             MetaWaylandTabletSeat *tablet_seat)
{
  MetaBackend *backend = meta_get_backend ();
  MetaWaylandTabletPad *pad;
  guint n_mode_groups, i;

  pad = g_slice_new0 (MetaWaylandTabletPad);
  wl_list_init (&pad->resource_list);
  wl_list_init (&pad->focus_resource_list);
  pad->focus_surface_listener.notify = pad_handle_focus_surface_destroy;
  pad->device = device;
  pad->tablet_seat = tablet_seat;

  pad->feedback = g_hash_table_new_full (NULL, NULL, NULL,
                                         (GDestroyNotify) g_free);

#ifdef HAVE_NATIVE_BACKEND
  /* Buttons, only can be honored this with the native backend */
  if (META_IS_BACKEND_NATIVE (backend))
    {
      struct libinput_device *libinput_device;

      libinput_device = clutter_evdev_input_device_get_libinput_device (device);
      pad->n_buttons = libinput_device_tablet_pad_get_num_buttons (libinput_device);
    }
#endif

  n_mode_groups = clutter_input_device_get_n_mode_groups (pad->device);

  for (i = 0; i < n_mode_groups; i++)
    {
      pad->groups = g_list_prepend (pad->groups,
                                    meta_wayland_tablet_pad_group_new (pad));
    }

  return pad;
}

void
meta_wayland_tablet_pad_free (MetaWaylandTabletPad *pad)
{
  struct wl_resource *resource, *next;

  meta_wayland_tablet_pad_set_focus (pad, NULL);

  wl_resource_for_each_safe (resource, next, &pad->resource_list)
    {
      zwp_tablet_pad_v2_send_removed (resource);
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_list_free_full (pad->groups,
                    (GDestroyNotify) meta_wayland_tablet_pad_group_free);

  g_hash_table_destroy (pad->feedback);

  g_slice_free (MetaWaylandTabletPad, pad);
}

static void
tablet_pad_set_feedback (struct wl_client   *client,
                         struct wl_resource *resource,
                         uint32_t            button,
                         const char         *str,
                         uint32_t            serial)
{
  MetaWaylandTabletPad *pad = wl_resource_get_user_data (resource);

  /* FIXME: check serial */

  g_hash_table_insert (pad->feedback, GUINT_TO_POINTER (button), g_strdup (str));
}

static void
tablet_pad_destroy (struct wl_client   *client,
                    struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_pad_v2_interface pad_interface = {
  tablet_pad_set_feedback,
  tablet_pad_destroy,
};

void
meta_wayland_tablet_pad_notify (MetaWaylandTabletPad  *pad,
                                struct wl_resource    *resource)
{
  struct wl_client *client = wl_resource_get_client (resource);
  GList *l;

  zwp_tablet_pad_v2_send_path (resource, clutter_input_device_get_device_node (pad->device));
  zwp_tablet_pad_v2_send_buttons (resource, pad->n_buttons);

  for (l = pad->groups; l; l = l->next)
    {
      MetaWaylandTabletPadGroup *group = l->data;
      struct wl_resource *group_resource;

      group_resource = meta_wayland_tablet_pad_group_create_new_resource (group,
                                                                          client,
                                                                          resource,
                                                                          0);
      zwp_tablet_pad_v2_send_group (resource, group_resource);
      meta_wayland_tablet_pad_group_notify (group, group_resource);
    }

  zwp_tablet_pad_v2_send_done (resource);
}

struct wl_resource *
meta_wayland_tablet_pad_create_new_resource (MetaWaylandTabletPad *pad,
                                             struct wl_client     *client,
                                             struct wl_resource   *seat_resource,
                                             uint32_t              id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_pad_v2_interface,
                                 wl_resource_get_version (seat_resource), id);
  wl_resource_set_implementation (resource, &pad_interface,
                                  pad, unbind_resource);
  wl_resource_set_user_data (resource, pad);
  wl_list_insert (&pad->resource_list, wl_resource_get_link (resource));

  return resource;
}

struct wl_resource *
meta_wayland_tablet_pad_lookup_resource (MetaWaylandTabletPad *pad,
                                         struct wl_client     *client)
{
  struct wl_resource *resource;

  resource = wl_resource_find_for_client (&pad->resource_list, client);

  if (!resource)
    resource = wl_resource_find_for_client (&pad->focus_resource_list, client);

  return resource;
}

static gboolean
handle_pad_button_event (MetaWaylandTabletPad *pad,
                         const ClutterEvent   *event)
{
  enum zwp_tablet_pad_v2_button_state button_state;
  struct wl_list *focus_resources = &pad->focus_resource_list;
  struct wl_resource *resource;

  if (wl_list_empty (focus_resources))
    return FALSE;

  if (event->type == CLUTTER_PAD_BUTTON_PRESS)
    button_state = ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED;
  else if (event->type == CLUTTER_PAD_BUTTON_RELEASE)
    button_state = ZWP_TABLET_TOOL_V2_BUTTON_STATE_RELEASED;
  else
    return FALSE;

  wl_resource_for_each (resource, focus_resources)
    {
      zwp_tablet_pad_v2_send_button (resource,
                                     clutter_event_get_time (event),
                                     event->pad_button.button, button_state);
    }

  return TRUE;
}

gboolean
meta_wayland_tablet_pad_handle_event (MetaWaylandTabletPad *pad,
                                      const ClutterEvent   *event)
{
  MetaWaylandTabletPadGroup *group;
  gboolean handled = FALSE;
  guint n_group;

  n_group = clutter_event_get_mode_group (event);
  group = g_list_nth_data (pad->groups, n_group);

  switch (clutter_event_type (event))
    {
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
      if (group)
        handled |= meta_wayland_tablet_pad_group_handle_event (group, event);

      if (handled)
        return TRUE;

      return handle_pad_button_event (pad, event);
    case CLUTTER_PAD_RING:
    case CLUTTER_PAD_STRIP:
      if (group)
        return meta_wayland_tablet_pad_group_handle_event (group, event);
    default:
      return FALSE;
    }
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

static void
meta_wayland_tablet_pad_update_groups_focus (MetaWaylandTabletPad *pad)
{
  GList *l;

  for (l = pad->groups; l; l = l->next)
    meta_wayland_tablet_pad_group_sync_focus (l->data);
}

void
meta_wayland_tablet_pad_set_focus (MetaWaylandTabletPad *pad,
                                   MetaWaylandSurface   *surface)
{
}
