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

#include "meta-wayland-pointer-gesture-pinch.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-surface.h"
#include "pointer-gestures-server-protocol.h"

static void
handle_pinch_begin (MetaWaylandPointer *pointer,
                    const ClutterEvent *event)
{
  MetaWaylandPointerClient *pointer_client;
  struct wl_resource *resource;
  uint32_t serial;

  pointer_client = pointer->focus_client;
  serial = wl_display_next_serial (pointer->display);

  wl_resource_for_each (resource, &pointer_client->pinch_gesture_resources)
    {
      _wl_pointer_gesture_pinch_send_begin (resource, serial,
                                            clutter_event_get_time (event),
                                            pointer->focus_surface->resource,
                                            2);
    }
}

static void
handle_pinch_update (MetaWaylandPointer *pointer,
                     const ClutterEvent *event)
{
  MetaWaylandPointerClient *pointer_client;
  struct wl_resource *resource;
  gdouble dx, dy, scale, rotation;

  pointer_client = pointer->focus_client;
  clutter_event_get_gesture_motion_delta (event, &dx, &dy);
  rotation = clutter_event_get_gesture_pinch_angle_delta (event);
  scale = clutter_event_get_gesture_pinch_scale (event);

  wl_resource_for_each (resource, &pointer_client->pinch_gesture_resources)
    {
      _wl_pointer_gesture_pinch_send_update (resource,
                                             clutter_event_get_time (event),
                                             wl_fixed_from_double (dx),
                                             wl_fixed_from_double (dy),
                                             wl_fixed_from_double (scale),
                                             wl_fixed_from_double (rotation));
    }
}

static void
handle_pinch_end (MetaWaylandPointer *pointer,
                  const ClutterEvent *event)
{
  MetaWaylandPointerClient *pointer_client;
  struct wl_resource *resource;
  gboolean cancelled = FALSE;
  uint32_t serial;

  pointer_client = pointer->focus_client;
  serial = wl_display_next_serial (pointer->display);

  if (event->touchpad_pinch.phase == CLUTTER_TOUCHPAD_GESTURE_PHASE_CANCEL)
    cancelled = TRUE;

  wl_resource_for_each (resource, &pointer_client->pinch_gesture_resources)
    {
      _wl_pointer_gesture_pinch_send_end (resource, serial,
                                          clutter_event_get_time (event),
                                          cancelled);
    }
}

gboolean
meta_wayland_pointer_gesture_pinch_handle_event (MetaWaylandPointer *pointer,
                                                 const ClutterEvent *event)
{
  if (event->type != CLUTTER_TOUCHPAD_PINCH)
    return FALSE;

  if (!pointer->focus_client)
    return FALSE;

  switch (event->touchpad_pinch.phase)
    {
    case CLUTTER_TOUCHPAD_GESTURE_PHASE_BEGIN:
      handle_pinch_begin (pointer, event);
      break;
    case CLUTTER_TOUCHPAD_GESTURE_PHASE_UPDATE:
      handle_pinch_update (pointer, event);
      break;
    case CLUTTER_TOUCHPAD_GESTURE_PHASE_END:
    case CLUTTER_TOUCHPAD_GESTURE_PHASE_CANCEL:
      handle_pinch_end (pointer, event);
      break;
    default:
      return FALSE;
    }

  return TRUE;
}

static void
pointer_gesture_pinch_destroy (struct wl_client   *client,
                               struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct _wl_pointer_gesture_pinch_interface pointer_gesture_pinch_interface = {
  pointer_gesture_pinch_destroy
};

void
meta_wayland_pointer_gesture_pinch_create_new_resource (MetaWaylandPointer *pointer,
                                                        struct wl_client   *client,
                                                        struct wl_resource *gestures_resource,
                                                        uint32_t            id)
{
  MetaWaylandPointerClient *pointer_client;
  struct wl_resource *res;

  pointer_client = meta_wayland_pointer_get_pointer_client (pointer, client);
  g_return_if_fail (pointer_client != NULL);

  res = wl_resource_create (client, &_wl_pointer_gesture_pinch_interface,
                            wl_resource_get_version (gestures_resource), id);
  wl_resource_set_implementation (res, &pointer_gesture_pinch_interface, pointer,
                                  meta_wayland_pointer_unbind_pointer_client_resource);
  wl_list_insert (&pointer_client->pinch_gesture_resources,
                  wl_resource_get_link (res));
}
