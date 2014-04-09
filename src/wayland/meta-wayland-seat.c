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
set_cursor_surface (MetaWaylandSeat    *seat,
                    MetaWaylandSurface *surface)
{
  if (seat->cursor_surface == surface)
    return;

  if (seat->cursor_surface)
    wl_list_remove (&seat->cursor_surface_destroy_listener.link);

  seat->cursor_surface = surface;

  if (seat->cursor_surface)
    wl_resource_add_destroy_listener (seat->cursor_surface->resource,
                                      &seat->cursor_surface_destroy_listener);
}

void
meta_wayland_seat_update_cursor_surface (MetaWaylandSeat *seat)
{
  MetaCursorReference *cursor;

  if (seat->cursor_tracker == NULL)
    return;

  if (seat->cursor_surface && seat->cursor_surface->buffer)
    {
      struct wl_resource *buffer = seat->cursor_surface->buffer->resource;
      cursor = meta_cursor_reference_from_buffer (seat->cursor_tracker,
                                                  buffer,
                                                  seat->hotspot_x,
                                                  seat->hotspot_y);
    }
  else
    cursor = NULL;

  meta_cursor_tracker_set_window_cursor (seat->cursor_tracker, cursor);

  if (cursor)
    meta_cursor_reference_unref (cursor);
}

static void
pointer_set_cursor (struct wl_client *client,
                    struct wl_resource *resource,
                    uint32_t serial,
                    struct wl_resource *surface_resource,
                    int32_t x, int32_t y)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface;

  surface = (surface_resource ? wl_resource_get_user_data (surface_resource) : NULL);

  if (seat->pointer.focus_surface == NULL)
    return;
  if (wl_resource_get_client (seat->pointer.focus_surface->resource) != client)
    return;
  if (seat->pointer.focus_serial - serial > G_MAXUINT32 / 2)
    return;

  seat->hotspot_x = x;
  seat->hotspot_y = y;
  set_cursor_surface (seat, surface);
  meta_wayland_seat_update_cursor_surface (seat);
}

static void
pointer_release (struct wl_client *client,
                 struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct wl_pointer_interface pointer_interface = {
  pointer_set_cursor,
  pointer_release,
};

static void
seat_get_pointer (struct wl_client *client,
                  struct wl_resource *resource,
                  uint32_t id)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (resource);
  struct wl_resource *cr;

  cr = wl_resource_create (client, &wl_pointer_interface,
			   MIN (META_WL_POINTER_VERSION, wl_resource_get_version (resource)), id);
  wl_resource_set_implementation (cr, &pointer_interface, seat, unbind_resource);
  wl_list_insert (&seat->pointer.resource_list, wl_resource_get_link (cr));

  if (seat->pointer.focus_surface &&
      wl_resource_get_client (seat->pointer.focus_surface->resource) == client)
    meta_wayland_pointer_set_focus (&seat->pointer, seat->pointer.focus_surface);
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
  struct wl_resource *cr;

  cr = wl_resource_create (client, &wl_keyboard_interface,
			   MIN (META_WL_KEYBOARD_VERSION, wl_resource_get_version (resource)), id);
  wl_resource_set_implementation (cr, NULL, seat, unbind_resource);
  wl_list_insert (&seat->keyboard.resource_list, wl_resource_get_link (cr));

  wl_keyboard_send_keymap (cr,
                           WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                           seat->keyboard.xkb_info.keymap_fd,
                           seat->keyboard.xkb_info.keymap_size);

  if (seat->keyboard.focus_surface &&
      wl_resource_get_client (seat->keyboard.focus_surface->resource) == client)
    {
      meta_wayland_keyboard_set_focus (&seat->keyboard, seat->keyboard.focus_surface);
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

static const struct wl_seat_interface
seat_interface =
  {
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

static void
pointer_handle_cursor_surface_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandSeat *seat = wl_container_of (listener, seat, cursor_surface_destroy_listener);

  set_cursor_surface (seat, NULL);
  meta_wayland_seat_update_cursor_surface (seat);
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

  seat->cursor_surface = NULL;
  seat->cursor_surface_destroy_listener.notify = pointer_handle_cursor_surface_destroy;
  seat->hotspot_x = 16;
  seat->hotspot_y = 16;

  wl_global_create (display, &wl_seat_interface, META_WL_SEAT_VERSION, seat, bind_seat);

  return seat;
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
  wl_fixed_t x_value = 0, y_value = 0;

  notify_motion (seat, event);

  if (!seat->pointer.focus_resource)
    return;

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

  if (x_value)
    wl_pointer_send_axis (seat->pointer.focus_resource, clutter_event_get_time (event),
                          WL_POINTER_AXIS_HORIZONTAL_SCROLL, x_value);
  if (y_value)
    wl_pointer_send_axis (seat->pointer.focus_resource, clutter_event_get_time (event),
                          WL_POINTER_AXIS_VERTICAL_SCROLL, y_value);
}

static int
count_buttons (const ClutterEvent *event)
{
  static gint maskmap[5] =
    {
      CLUTTER_BUTTON1_MASK, CLUTTER_BUTTON2_MASK, CLUTTER_BUTTON3_MASK,
      CLUTTER_BUTTON4_MASK, CLUTTER_BUTTON5_MASK
    };
  ClutterModifierType mod_mask;
  int i, count;

  mod_mask = clutter_event_get_state (event);
  count = 0;
  for (i = 0; i < 5; i++)
    {
      if (mod_mask & maskmap[i])
	count++;
    }

  return count;
}

static void
meta_wayland_seat_update_pointer (MetaWaylandSeat    *seat,
                                  const ClutterEvent *event)
{
  float x, y;

  clutter_event_get_coords (event, &x, &y);
  seat->pointer.x = wl_fixed_from_double (x);
  seat->pointer.y = wl_fixed_from_double (y);

  seat->pointer.button_count = count_buttons (event);

  if (seat->cursor_tracker)
    {
      meta_cursor_tracker_update_position (seat->cursor_tracker,
					   wl_fixed_to_int (seat->pointer.x),
					   wl_fixed_to_int (seat->pointer.y));

      if (seat->pointer.current == NULL)
	meta_cursor_tracker_unset_window_cursor (seat->cursor_tracker);
    }
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
      meta_wayland_seat_update_pointer (seat, event);
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

  if (meta_grab_op_is_wayland (display->grab_op))
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
meta_wayland_seat_free (MetaWaylandSeat *seat)
{
  set_cursor_surface (seat, NULL);

  meta_wayland_pointer_release (&seat->pointer);
  meta_wayland_keyboard_release (&seat->keyboard);

  g_slice_free (MetaWaylandSeat, seat);
}
