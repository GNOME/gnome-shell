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
#include "meta-surface-actor-wayland.h"

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int (10)

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
    {
      meta_wayland_keyboard_set_focus (keyboard, keyboard->focus_surface);
      meta_wayland_data_device_set_keyboard_focus (seat);
    }
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

  meta_wayland_pointer_init (&seat->pointer);
  meta_wayland_keyboard_init (&seat->keyboard, display);

  seat->display = display;

  seat->current_stage = 0;

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

static void
notify_motion (MetaWaylandSeat    *seat,
               const ClutterEvent *event)
{
  MetaWaylandPointer *pointer = &seat->pointer;

  meta_wayland_seat_repick (seat, event);

  pointer->grab->interface->motion (pointer->grab, event);
}

static void
handle_motion_event (MetaWaylandSeat    *seat,
                     const ClutterEvent *event)
{
  notify_motion (seat, event);
}

static void
handle_button_event (MetaWaylandSeat    *seat,
                     const ClutterEvent *event)
{
  MetaWaylandPointer *pointer = &seat->pointer;
  gboolean implicit_grab;

  notify_motion (seat, event);

  implicit_grab = (event->type == CLUTTER_BUTTON_PRESS) && (pointer->button_count == 1);
  if (implicit_grab)
    {
      pointer->grab_button = clutter_event_get_button (event);
      pointer->grab_time = clutter_event_get_time (event);
      pointer->grab_x = pointer->x;
      pointer->grab_y = pointer->y;
    }

  pointer->grab->interface->button (pointer->grab, event);

  if (implicit_grab)
    pointer->grab_serial = wl_display_get_serial (seat->display);
}

static void
handle_scroll_event (MetaWaylandSeat    *seat,
                     const ClutterEvent *event)
{
  MetaWaylandPointer *pointer = &seat->pointer;
  struct wl_resource *resource;
  struct wl_list *l;
  wl_fixed_t x_value = 0, y_value = 0;

  notify_motion (seat, event);

  if (clutter_event_is_pointer_emulated (event))
    return;

  switch (clutter_event_get_scroll_direction (event))
    {
    case CLUTTER_SCROLL_UP:
      y_value = -DEFAULT_AXIS_STEP_DISTANCE;
      break;

    case CLUTTER_SCROLL_DOWN:
      y_value = DEFAULT_AXIS_STEP_DISTANCE;
      break;

    case CLUTTER_SCROLL_LEFT:
      x_value = -DEFAULT_AXIS_STEP_DISTANCE;
      break;

    case CLUTTER_SCROLL_RIGHT:
      x_value = DEFAULT_AXIS_STEP_DISTANCE;
      break;

    case CLUTTER_SCROLL_SMOOTH:
      {
        double dx, dy;
        clutter_event_get_scroll_delta (event, &dx, &dy);
        x_value = wl_fixed_from_double (dx);
        y_value = wl_fixed_from_double (dy);
      }
      break;

    default:
      return;
    }

  l = &pointer->focus_resource_list;
  wl_resource_for_each (resource, l)
    {
      if (x_value)
        wl_pointer_send_axis (resource, clutter_event_get_time (event),
                              WL_POINTER_AXIS_HORIZONTAL_SCROLL, x_value);
      if (y_value)
        wl_pointer_send_axis (resource, clutter_event_get_time (event),
                              WL_POINTER_AXIS_VERTICAL_SCROLL, y_value);
    }
}

gboolean
meta_wayland_seat_handle_event (MetaWaylandSeat *seat,
                                const ClutterEvent *event)
{
  switch (event->type)
    {
    case CLUTTER_MOTION:
      handle_motion_event (seat, event);
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      handle_button_event (seat, event);
      break;

    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      return meta_wayland_keyboard_handle_event (&seat->keyboard,
                                                 (const ClutterKeyEvent *) event);

    case CLUTTER_SCROLL:
      handle_scroll_event (seat, event);
      break;

    default:
      break;
    }

  return FALSE;
}

/* The actor argument can be NULL in which case a Clutter pick will be
   performed to determine the right actor. An actor should only be
   passed if the repick is being performed due to an event in which
   case Clutter will have already performed a pick so we can avoid
   redundantly doing another one */
void
meta_wayland_seat_repick (MetaWaylandSeat    *seat,
			  const ClutterEvent *for_event)
{
  ClutterActor       *actor   = NULL;
  MetaWaylandPointer *pointer = &seat->pointer;
  MetaWaylandSurface *surface = NULL;
  MetaDisplay        *display = meta_get_display ();

  if (meta_grab_op_should_block_wayland (display->grab_op))
    {
      meta_wayland_pointer_update_current_focus (pointer, NULL);
      return;
    }

  if (for_event)
    {
      actor = clutter_event_get_source (for_event);
    }
  else if (seat->current_stage)
    {
      ClutterStage *stage = CLUTTER_STAGE (seat->current_stage);
      actor = clutter_stage_get_actor_at_pos (stage,
                                              CLUTTER_PICK_REACTIVE,
                                              wl_fixed_to_double (pointer->x),
                                              wl_fixed_to_double (pointer->y));
    }

  if (actor)
    seat->current_stage = clutter_actor_get_stage (actor);
  else
    seat->current_stage = NULL;

  if (META_IS_SURFACE_ACTOR_WAYLAND (actor))
    surface = meta_surface_actor_wayland_get_surface (META_SURFACE_ACTOR_WAYLAND (actor));

  meta_wayland_pointer_update_current_focus (pointer, surface);
}

void
meta_wayland_seat_update_cursor_surface (MetaWaylandSeat *seat)
{
  meta_wayland_pointer_update_cursor_surface (&seat->pointer);
}
