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
#include "meta-wayland-stage.h"

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int (10)

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
transform_stage_point_fixed (MetaWaylandSurface *surface,
                             wl_fixed_t x,
                             wl_fixed_t y,
                             wl_fixed_t *sx,
                             wl_fixed_t *sy)
{
  float xf = 0.0f, yf = 0.0f;

  if (surface->window)
    {
      ClutterActor *actor =
        CLUTTER_ACTOR (meta_window_get_compositor_private (surface->window));

      if (actor)
        clutter_actor_transform_stage_point (actor,
                                             wl_fixed_to_double (x),
                                             wl_fixed_to_double (y),
                                             &xf, &yf);
    }

  *sx = wl_fixed_from_double (xf);
  *sy = wl_fixed_from_double (yf);
}

static void
pointer_unmap_sprite (MetaWaylandSeat *seat)
{
  if (seat->current_stage)
    {
      MetaWaylandStage *stage = META_WAYLAND_STAGE (seat->current_stage);
      meta_wayland_stage_set_invisible_cursor (stage);
    }

  if (seat->sprite)
    {
      wl_list_remove (&seat->sprite_destroy_listener.link);
      seat->sprite = NULL;
    }
}

void
meta_wayland_seat_update_sprite (MetaWaylandSeat *seat)
{
  if (seat->current_stage)
    {
      MetaWaylandStage *stage = META_WAYLAND_STAGE (seat->current_stage);
      ClutterBackend *backend = clutter_get_default_backend ();
      CoglContext *context = clutter_backend_get_cogl_context (backend);
      struct wl_resource *buffer = seat->sprite->buffer_ref.buffer->resource;
      CoglTexture2D *texture =
        cogl_wayland_texture_2d_new_from_buffer (context, buffer, NULL);

      meta_wayland_stage_set_cursor_from_texture (stage,
                                                  COGL_TEXTURE (texture),
                                                  seat->hotspot_x,
                                                  seat->hotspot_y);

      cogl_object_unref (texture);
    }
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

  surface = (surface_resource ?
             wl_resource_get_user_data (surface_resource) :
             NULL);

  if (seat->pointer.focus == NULL)
    return;
  if (wl_resource_get_client (seat->pointer.focus->resource) != client)
    return;
  if (seat->pointer.focus_serial - serial > G_MAXUINT32 / 2)
    return;

  seat->hotspot_x = x;
  seat->hotspot_y = y;

  if (seat->sprite != surface)
    {
      pointer_unmap_sprite (seat);

      if (!surface)
        return;

      wl_resource_add_destroy_listener (surface->resource,
                                        &seat->sprite_destroy_listener);

      seat->sprite = surface;

      if (seat->sprite->buffer_ref.buffer)
        meta_wayland_seat_update_sprite (seat);
    }
}

static const struct wl_pointer_interface
pointer_interface =
  {
    pointer_set_cursor
  };

static void
seat_get_pointer (struct wl_client *client,
                  struct wl_resource *resource,
                  uint32_t id)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (resource);
  struct wl_resource *cr;

  cr = wl_client_add_object (client, &wl_pointer_interface,
                             &pointer_interface, id, seat);
  wl_list_insert (&seat->pointer.resource_list, wl_resource_get_link (cr));
  wl_resource_set_destructor (cr, unbind_resource);

  if (seat->pointer.focus &&
      wl_resource_get_client (seat->pointer.focus->resource) == client)
    {
      MetaWaylandSurface *surface;
      wl_fixed_t sx, sy;

      surface = (MetaWaylandSurface *) seat->pointer.focus;
      transform_stage_point_fixed (surface,
                                   seat->pointer.x,
                                   seat->pointer.y,
                                   &sx, &sy);
      meta_wayland_pointer_set_focus (&seat->pointer,
                                  seat->pointer.focus,
                                  sx, sy);
    }
}

static void
seat_get_keyboard (struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t id)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (resource);
  struct wl_resource *cr;

  cr = wl_client_add_object (client, &wl_keyboard_interface, NULL, id, seat);
  wl_list_insert (&seat->keyboard.resource_list, wl_resource_get_link (cr));
  wl_resource_set_destructor (cr, unbind_resource);

  wl_keyboard_send_keymap (cr,
                           WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                           seat->keyboard.xkb_info.keymap_fd,
                           seat->keyboard.xkb_info.keymap_size);

  if (seat->keyboard.focus &&
      wl_resource_get_client (seat->keyboard.focus->resource) == client)
    {
      meta_wayland_keyboard_set_focus (&seat->keyboard,
                                   seat->keyboard.focus);
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

  resource = wl_client_add_object (client,
                                   &wl_seat_interface,
                                   &seat_interface,
                                   id,
                                   data);
  wl_list_insert (&seat->base_resource_list, wl_resource_get_link (resource));
  wl_resource_set_destructor (resource, unbind_resource);

  wl_seat_send_capabilities (resource,
                             WL_SEAT_CAPABILITY_POINTER |
                             WL_SEAT_CAPABILITY_KEYBOARD);
}

static void
pointer_handle_sprite_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandSeat *seat =
    wl_container_of (listener, seat, sprite_destroy_listener);

  pointer_unmap_sprite (seat);
}

MetaWaylandSeat *
meta_wayland_seat_new (struct wl_display *display)
{
  MetaWaylandSeat *seat = g_new0 (MetaWaylandSeat, 1);

  wl_signal_init (&seat->destroy_signal);

  seat->selection_data_source = NULL;
  wl_list_init (&seat->base_resource_list);
  wl_signal_init (&seat->selection_signal);
  wl_list_init (&seat->drag_resource_list);
  wl_signal_init (&seat->drag_icon_signal);

  meta_wayland_pointer_init (&seat->pointer);

  meta_wayland_keyboard_init (&seat->keyboard, display);

  seat->display = display;

  seat->current_stage = 0;

  seat->sprite = NULL;
  seat->sprite_destroy_listener.notify = pointer_handle_sprite_destroy;
  seat->hotspot_x = 16;
  seat->hotspot_y = 16;

  wl_display_add_global (display, &wl_seat_interface, seat, bind_seat);

  return seat;
}

static void
notify_motion (MetaWaylandSeat *seat,
               const ClutterEvent *event)
{
  MetaWaylandPointer *pointer = &seat->pointer;
  float x, y;

  clutter_event_get_coords (event, &x, &y);
  pointer->x = wl_fixed_from_double (x);
  pointer->y = wl_fixed_from_double (y);

  meta_wayland_seat_repick (seat,
                        clutter_event_get_time (event),
                        clutter_event_get_source (event));

  pointer->grab->interface->motion (pointer->grab,
                                    clutter_event_get_time (event),
                                    pointer->grab->x,
                                    pointer->grab->y);
}

static void
handle_motion_event (MetaWaylandSeat *seat,
                     const ClutterMotionEvent *event)
{
  notify_motion (seat, (const ClutterEvent *) event);
}

static void
handle_button_event (MetaWaylandSeat *seat,
                     const ClutterButtonEvent *event)
{
  MetaWaylandPointer *pointer = &seat->pointer;
  gboolean state = event->type == CLUTTER_BUTTON_PRESS;
  uint32_t button;

  notify_motion (seat, (const ClutterEvent *) event);

  switch (event->button)
    {
      /* The evdev input right and middle button numbers are swapped
         relative to how Clutter numbers them */
    case 2:
      button = BTN_MIDDLE;
      break;

    case 3:
      button = BTN_RIGHT;
      break;

    default:
      button = event->button + BTN_LEFT - 1;
      break;
    }

  if (state)
    {
      if (pointer->button_count == 0)
        {
          MetaWaylandSurface *surface = pointer->current;

          pointer->grab_button = button;
          pointer->grab_time = event->time;
          pointer->grab_x = pointer->x;
          pointer->grab_y = pointer->y;

          if (button == BTN_LEFT &&
              surface &&
              surface->window &&
              surface->window->client_type == META_WINDOW_CLIENT_TYPE_WAYLAND)
            {
              meta_window_raise (surface->window);
            }
        }

      pointer->button_count++;
    }
  else
    pointer->button_count--;

  pointer->grab->interface->button (pointer->grab, event->time, button, state);

  if (pointer->button_count == 1)
    pointer->grab_serial = wl_display_get_serial (seat->display);
}

static void
handle_scroll_event (MetaWaylandSeat *seat,
                     const ClutterScrollEvent *event)
{
  enum wl_pointer_axis axis;
  wl_fixed_t value;

  notify_motion (seat, (const ClutterEvent *) event);

  switch (event->direction)
    {
    case CLUTTER_SCROLL_UP:
      axis = WL_POINTER_AXIS_VERTICAL_SCROLL;
      value = -DEFAULT_AXIS_STEP_DISTANCE;
      break;

    case CLUTTER_SCROLL_DOWN:
      axis = WL_POINTER_AXIS_VERTICAL_SCROLL;
      value = DEFAULT_AXIS_STEP_DISTANCE;
      break;

    case CLUTTER_SCROLL_LEFT:
      axis = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
      value = -DEFAULT_AXIS_STEP_DISTANCE;
      break;

    case CLUTTER_SCROLL_RIGHT:
      axis = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
      value = DEFAULT_AXIS_STEP_DISTANCE;
      break;

    default:
      return;
    }

  if (seat->pointer.focus_resource)
    wl_pointer_send_axis (seat->pointer.focus_resource,
                          event->time,
                          axis,
                          value);
}

void
meta_wayland_seat_handle_event (MetaWaylandSeat *seat,
                                const ClutterEvent *event)
{
  switch (event->type)
    {
    case CLUTTER_MOTION:
      handle_motion_event (seat,
                           (const ClutterMotionEvent *) event);
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      handle_button_event (seat,
                           (const ClutterButtonEvent *) event);
      break;

    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      meta_wayland_keyboard_handle_event (&seat->keyboard,
                                          (const ClutterKeyEvent *) event);
      break;

    case CLUTTER_SCROLL:
      handle_scroll_event (seat, (const ClutterScrollEvent *) event);
      break;

    default:
      break;
    }
}

static void
update_pointer_position_for_actor (MetaWaylandPointer *pointer,
                                   ClutterActor *actor)
{
  float ax, ay;

  clutter_actor_transform_stage_point (actor,
                                       wl_fixed_to_double (pointer->x),
                                       wl_fixed_to_double (pointer->y),
                                       &ax, &ay);
  pointer->current_x = wl_fixed_from_double (ax);
  pointer->current_y = wl_fixed_from_double (ay);
}

/* The actor argument can be NULL in which case a Clutter pick will be
   performed to determine the right actor. An actor should only be
   passed if the repick is being performed due to an event in which
   case Clutter will have already performed a pick so we can avoid
   redundantly doing another one */
void
meta_wayland_seat_repick (MetaWaylandSeat *seat,
                          uint32_t time,
                          ClutterActor *actor)
{
  MetaWaylandPointer *pointer = &seat->pointer;
  MetaWaylandSurface *surface = NULL;

  if (actor == NULL && seat->current_stage)
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

  if (META_IS_WINDOW_ACTOR (actor))
    {
      MetaWindow *window =
        meta_window_actor_get_meta_window (META_WINDOW_ACTOR (actor));

      update_pointer_position_for_actor (pointer, actor);

      surface = window->surface;
    }
  else if (META_IS_SHAPED_TEXTURE (actor))
    {
      MetaShapedTexture *shaped_texture = META_SHAPED_TEXTURE (actor);

      update_pointer_position_for_actor (pointer, actor);

      surface = meta_shaped_texture_get_wayland_surface (shaped_texture);
    }

  if (surface != pointer->current)
    {
      const MetaWaylandPointerGrabInterface *interface =
        pointer->grab->interface;
      interface->focus (pointer->grab,
                        surface,
                        pointer->current_x, pointer->current_y);
      pointer->current = surface;
    }

  if (pointer->grab->focus)
    transform_stage_point_fixed (pointer->grab->focus,
                                 pointer->x,
                                 pointer->y,
                                 &pointer->grab->x,
                                 &pointer->grab->y);
}

void
meta_wayland_seat_free (MetaWaylandSeat *seat)
{
  pointer_unmap_sprite (seat);

  meta_wayland_pointer_release (&seat->pointer);
  meta_wayland_keyboard_release (&seat->keyboard);

  wl_signal_emit (&seat->destroy_signal, seat);

  g_slice_free (MetaWaylandSeat, seat);
}
