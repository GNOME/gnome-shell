/*
 * Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
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
 */

#include "config.h"

#include <cogl/cogl-wayland-server.h>
#include <clutter/clutter.h>
#include <clutter/wayland/clutter-wayland-compositor.h>
#include <clutter/wayland/clutter-wayland-surface.h>
#include <linux/input.h>
#include <stdlib.h>
#include <string.h>
#include "meta-wayland-seat.h"
#include "meta-wayland-private.h"
#include "meta-wayland-keyboard.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-data-device.h"
#include "meta-window-actor-private.h"
#include "meta/meta-shaped-texture.h"
#include "meta-shaped-texture-private.h"
#include "meta-wayland-stage.h"
#include "meta-cursor-tracker-private.h"

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
seat_get_pointer (struct wl_client *client,
                  struct wl_resource *resource,
                  uint32_t id)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (resource);
  MetaWaylandPointer *pointer = &seat->pointer;

  meta_wayland_pointer_create_new_resource (pointer, client, resource, id);
}

static void
keyboard_release (struct wl_client *client,
                  struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct wl_keyboard_interface keyboard_interface = {
  keyboard_release,
};

static void
seat_get_keyboard (struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t id)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (resource);
  MetaWaylandKeyboard *keyboard = &seat->keyboard;
  struct wl_resource *cr;

  cr = wl_resource_create (client, &wl_keyboard_interface,
			   MIN (META_WL_KEYBOARD_VERSION, wl_resource_get_version (resource)), id);
  wl_resource_set_implementation (cr, NULL, seat, unbind_resource);
  wl_list_insert (&keyboard->resource_list, wl_resource_get_link (cr));

  wl_keyboard_send_keymap (cr,
                           WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                           keyboard->xkb_info.keymap_fd,
                           keyboard->xkb_info.keymap_size);

  if (keyboard->focus_surface && wl_resource_get_client (keyboard->focus_surface->resource) == client)
    meta_wayland_keyboard_set_focus (keyboard, keyboard->focus_surface);
}

static void
seat_get_touch (struct wl_client *client,
                struct wl_resource *resource,
                uint32_t id)
{
  /* Touch not supported */
}

static const struct wl_seat_interface seat_interface = {
  seat_get_pointer,
  seat_get_keyboard,
  seat_get_touch
};

static void
bind_seat (struct wl_client *client,
           void *data,
           guint32 version,
           guint32 id)
{
  MetaWaylandSeat *seat = data;
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_seat_interface,
				 MIN (META_WL_SEAT_VERSION, version), id);
  wl_resource_set_implementation (resource, &seat_interface, seat, unbind_resource);
  wl_list_insert (&seat->base_resource_list, wl_resource_get_link (resource));

  wl_seat_send_capabilities (resource,
                             WL_SEAT_CAPABILITY_POINTER |
                             WL_SEAT_CAPABILITY_KEYBOARD);

  if (version >= META_WL_SEAT_HAS_NAME)
    wl_seat_send_name (resource, "seat0");
}

MetaWaylandSeat *
meta_wayland_seat_new (struct wl_display *display)
{
  MetaWaylandSeat *seat = g_new0 (MetaWaylandSeat, 1);

  seat->selection_data_source = NULL;
  wl_list_init (&seat->base_resource_list);
  wl_list_init (&seat->data_device_resource_list);

  meta_wayland_pointer_init (&seat->pointer, display);
  meta_wayland_keyboard_init (&seat->keyboard, display);

  seat->display = display;

  wl_global_create (display, &wl_seat_interface, META_WL_SEAT_VERSION, seat, bind_seat);

  return seat;
}

void
meta_wayland_seat_free (MetaWaylandSeat *seat)
{
  meta_wayland_pointer_release (&seat->pointer);
  meta_wayland_keyboard_release (&seat->keyboard);

  g_slice_free (MetaWaylandSeat, seat);
}

void
meta_wayland_seat_update (MetaWaylandSeat    *seat,
                          const ClutterEvent *event)
{
  switch (event->type)
    {
    case CLUTTER_MOTION:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_SCROLL:
      meta_wayland_pointer_update (&seat->pointer, event);
      break;

    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      meta_wayland_keyboard_update (&seat->keyboard, (const ClutterKeyEvent *) event);
      break;

    default:
      break;
    }
}

gboolean
meta_wayland_seat_handle_event (MetaWaylandSeat *seat,
                                const ClutterEvent *event)
{
  switch (event->type)
    {
    case CLUTTER_MOTION:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_SCROLL:
      return meta_wayland_pointer_handle_event (&seat->pointer, event);

    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      return meta_wayland_keyboard_handle_event (&seat->keyboard,
                                                 (const ClutterKeyEvent *) event);

    default:
      break;
    }

  return FALSE;
}

void
meta_wayland_seat_repick (MetaWaylandSeat *seat)
{
  meta_wayland_pointer_repick (&seat->pointer);
}

void
meta_wayland_seat_update_cursor_surface (MetaWaylandSeat *seat)
{
  meta_wayland_pointer_update_cursor_surface (&seat->pointer);
}
