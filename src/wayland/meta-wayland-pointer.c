/*
 * Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
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
 */

/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/* The file is based on src/input.c from Weston */

#include "config.h"

#include <clutter/clutter.h>
#include <clutter/evdev/clutter-evdev.h>
#include <cogl/cogl.h>
#include <cogl/cogl-wayland-server.h>
#include <linux/input.h>

#include "meta-wayland-pointer.h"
#include "meta-wayland-popup.h"
#include "meta-wayland-private.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-surface.h"
#include "meta-wayland-buffer.h"
#include "meta-wayland-surface-role-cursor.h"
#include "meta-xwayland.h"
#include "meta-cursor.h"
#include "meta-cursor-tracker-private.h"
#include "meta-surface-actor-wayland.h"
#include "meta/meta-cursor-tracker.h"
#include "backends/meta-backend-private.h"
#include "backends/meta-cursor-tracker-private.h"
#include "backends/meta-cursor-renderer.h"

#include "relative-pointer-unstable-v1-server-protocol.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-backend-native.h"
#endif

#include <string.h>

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int (10)

enum {
  FOCUS_SURFACE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (MetaWaylandPointer, meta_wayland_pointer,
               META_TYPE_WAYLAND_INPUT_DEVICE)

static void
meta_wayland_pointer_set_current (MetaWaylandPointer *pointer,
                                  MetaWaylandSurface *surface);

static void
meta_wayland_pointer_reset_grab (MetaWaylandPointer *pointer);

static void
meta_wayland_pointer_cancel_grab (MetaWaylandPointer *pointer);

static MetaWaylandPointerClient *
meta_wayland_pointer_client_new (void)
{
  MetaWaylandPointerClient *pointer_client;

  pointer_client = g_slice_new0 (MetaWaylandPointerClient);
  wl_list_init (&pointer_client->pointer_resources);
  wl_list_init (&pointer_client->swipe_gesture_resources);
  wl_list_init (&pointer_client->pinch_gesture_resources);
  wl_list_init (&pointer_client->relative_pointer_resources);

  return pointer_client;
}

static void
meta_wayland_pointer_client_free (MetaWaylandPointerClient *pointer_client)
{
  struct wl_resource *resource, *next;

  /* Since we make every wl_pointer resource defunct when we stop advertising
   * the pointer capability on the wl_seat, we need to make sure all the
   * resources in the pointer client instance gets removed.
   */
  wl_resource_for_each_safe (resource, next, &pointer_client->pointer_resources)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }
  wl_resource_for_each_safe (resource, next, &pointer_client->swipe_gesture_resources)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }
  wl_resource_for_each_safe (resource, next, &pointer_client->pinch_gesture_resources)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }
  wl_resource_for_each_safe (resource, next, &pointer_client->relative_pointer_resources)
    {
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_slice_free (MetaWaylandPointerClient, pointer_client);
}

static gboolean
meta_wayland_pointer_client_is_empty (MetaWaylandPointerClient *pointer_client)
{
  return (wl_list_empty (&pointer_client->pointer_resources) &&
          wl_list_empty (&pointer_client->swipe_gesture_resources) &&
          wl_list_empty (&pointer_client->pinch_gesture_resources) &&
          wl_list_empty (&pointer_client->relative_pointer_resources));
}

MetaWaylandPointerClient *
meta_wayland_pointer_get_pointer_client (MetaWaylandPointer *pointer,
                                         struct wl_client   *client)
{
  if (!pointer->pointer_clients)
    return NULL;
  return g_hash_table_lookup (pointer->pointer_clients, client);
}

static MetaWaylandPointerClient *
meta_wayland_pointer_ensure_pointer_client (MetaWaylandPointer *pointer,
                                            struct wl_client   *client)
{
  MetaWaylandPointerClient *pointer_client;

  pointer_client = meta_wayland_pointer_get_pointer_client (pointer, client);
  if (pointer_client)
    return pointer_client;

  pointer_client = meta_wayland_pointer_client_new ();
  g_hash_table_insert (pointer->pointer_clients, client, pointer_client);

  if (!pointer->focus_client &&
      pointer->focus_surface &&
      wl_resource_get_client (pointer->focus_surface->resource) == client)
    pointer->focus_client = pointer_client;

  return pointer_client;
}

static void
meta_wayland_pointer_cleanup_pointer_client (MetaWaylandPointer       *pointer,
                                             MetaWaylandPointerClient *pointer_client,
                                             struct wl_client         *client)
{
  if (meta_wayland_pointer_client_is_empty (pointer_client))
    {
      if (pointer->focus_client == pointer_client)
        pointer->focus_client = NULL;
      g_hash_table_remove (pointer->pointer_clients, client);
    }
}

void
meta_wayland_pointer_unbind_pointer_client_resource (struct wl_resource *resource)
{
  MetaWaylandPointer *pointer = wl_resource_get_user_data (resource);
  MetaWaylandPointerClient *pointer_client;
  struct wl_client *client = wl_resource_get_client (resource);

  wl_list_remove (wl_resource_get_link (resource));

  pointer_client = meta_wayland_pointer_get_pointer_client (pointer, client);
  if (!pointer_client)
    {
      /* This happens if all pointer devices were unplugged and no new resources
       * were created by the client.
       *
       * If this is a resource that was previously made defunct, pointer_client
       * be non-NULL but it is harmless since the below cleanup call will be
       * prevented from removing the pointer client because of valid resources.
       */
      return;
    }

  meta_wayland_pointer_cleanup_pointer_client (pointer,
                                               pointer_client,
                                               client);
}

static void
sync_focus_surface (MetaWaylandPointer *pointer)
{
  MetaDisplay *display = meta_get_display ();

  switch (display->event_route)
    {
    case META_EVENT_ROUTE_WINDOW_OP:
    case META_EVENT_ROUTE_COMPOSITOR_GRAB:
    case META_EVENT_ROUTE_FRAME_BUTTON:
      /* The compositor has a grab, so remove our focus... */
      meta_wayland_pointer_set_focus (pointer, NULL);
      break;

    case META_EVENT_ROUTE_NORMAL:
    case META_EVENT_ROUTE_WAYLAND_POPUP:
      {
        const MetaWaylandPointerGrabInterface *interface = pointer->grab->interface;
        interface->focus (pointer->grab, pointer->current);
      }
      break;

    default:
      g_assert_not_reached ();
    }

}

static void
meta_wayland_pointer_send_frame (MetaWaylandPointer *pointer,
				 struct wl_resource *resource)
{
  if (wl_resource_get_version (resource) >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION)
    wl_pointer_send_frame (resource);
}

void
meta_wayland_pointer_broadcast_frame (MetaWaylandPointer *pointer)
{
  struct wl_resource *resource;

  if (!pointer->focus_client)
    return;

  wl_resource_for_each (resource, &pointer->focus_client->pointer_resources)
    {
      meta_wayland_pointer_send_frame (pointer, resource);
    }
}

void
meta_wayland_pointer_send_relative_motion (MetaWaylandPointer *pointer,
                                           const ClutterEvent *event)
{
  struct wl_resource *resource;
  double dx, dy;
  double dx_unaccel, dy_unaccel;
  uint64_t time_us;
  uint32_t time_us_hi;
  uint32_t time_us_lo;
  wl_fixed_t dxf, dyf;
  wl_fixed_t dx_unaccelf, dy_unaccelf;

  if (!pointer->focus_client)
    return;

  if (!meta_backend_get_relative_motion_deltas (meta_get_backend (),
                                                event,
                                                &dx, &dy,
                                                &dx_unaccel, &dy_unaccel))
    return;

#ifdef HAVE_NATIVE_BACKEND
  time_us = clutter_evdev_event_get_time_usec (event);
  if (time_us == 0)
#endif
    time_us = clutter_event_get_time (event) * 1000ULL;
  time_us_hi = (uint32_t) (time_us >> 32);
  time_us_lo = (uint32_t) time_us;
  dxf = wl_fixed_from_double (dx);
  dyf = wl_fixed_from_double (dy);
  dx_unaccelf = wl_fixed_from_double (dx_unaccel);
  dy_unaccelf = wl_fixed_from_double (dy_unaccel);

  wl_resource_for_each (resource,
                        &pointer->focus_client->relative_pointer_resources)
    {
      zwp_relative_pointer_v1_send_relative_motion (resource,
                                                    time_us_hi,
                                                    time_us_lo,
                                                    dxf,
                                                    dyf,
                                                    dx_unaccelf,
                                                    dy_unaccelf);
    }
}

void
meta_wayland_pointer_send_motion (MetaWaylandPointer *pointer,
                                  const ClutterEvent *event)
{
  struct wl_resource *resource;
  uint32_t time;
  float sx, sy;

  if (!pointer->focus_client)
    return;

  time = clutter_event_get_time (event);
  meta_wayland_surface_get_relative_coordinates (pointer->focus_surface,
                                                 event->motion.x,
                                                 event->motion.y,
                                                 &sx, &sy);

  wl_resource_for_each (resource, &pointer->focus_client->pointer_resources)
    {
      wl_pointer_send_motion (resource, time,
                              wl_fixed_from_double (sx),
                              wl_fixed_from_double (sy));
    }

  meta_wayland_pointer_send_relative_motion (pointer, event);

  meta_wayland_pointer_broadcast_frame (pointer);
}

void
meta_wayland_pointer_send_button (MetaWaylandPointer *pointer,
                                  const ClutterEvent *event)
{
  struct wl_resource *resource;
  ClutterEventType event_type;

  event_type = clutter_event_type (event);

  if (pointer->focus_client &&
      !wl_list_empty (&pointer->focus_client->pointer_resources))
    {
      MetaWaylandInputDevice *input_device =
        META_WAYLAND_INPUT_DEVICE (pointer);
      uint32_t time;
      uint32_t button;
      uint32_t serial;

#ifdef HAVE_NATIVE_BACKEND
      MetaBackend *backend = meta_get_backend ();
      if (META_IS_BACKEND_NATIVE (backend))
        button = clutter_evdev_event_get_event_code (event);
      else
#endif
        {
          button = clutter_event_get_button (event);
          switch (button)
            {
            case 1:
              button = BTN_LEFT;
              break;

              /* The evdev input right and middle button numbers are swapped
                 relative to how Clutter numbers them */
            case 2:
              button = BTN_MIDDLE;
              break;

            case 3:
              button = BTN_RIGHT;
              break;

            default:
              button = button + (BTN_LEFT - 1) + 4;
              break;
            }
        }

      time = clutter_event_get_time (event);
      serial = meta_wayland_input_device_next_serial (input_device);

      wl_resource_for_each (resource, &pointer->focus_client->pointer_resources)
        {
          wl_pointer_send_button (resource, serial,
                                  time, button,
                                  event_type == CLUTTER_BUTTON_PRESS ? 1 : 0);
        }

      meta_wayland_pointer_broadcast_frame (pointer);
    }

  if (pointer->button_count == 0 && event_type == CLUTTER_BUTTON_RELEASE)
    sync_focus_surface (pointer);
}

static void
default_grab_focus (MetaWaylandPointerGrab *grab,
                    MetaWaylandSurface     *surface)
{
  MetaWaylandPointer *pointer = grab->pointer;
  MetaWaylandSeat *seat = meta_wayland_pointer_get_seat (pointer);
  MetaDisplay *display = meta_get_display ();

  if (pointer->button_count > 0)
    return;

  switch (display->event_route)
    {
    case META_EVENT_ROUTE_WINDOW_OP:
    case META_EVENT_ROUTE_COMPOSITOR_GRAB:
    case META_EVENT_ROUTE_FRAME_BUTTON:
      return;
      break;

    case META_EVENT_ROUTE_NORMAL:
    case META_EVENT_ROUTE_WAYLAND_POPUP:
      break;
    }

  if (meta_wayland_seat_has_pointer (seat))
    meta_wayland_pointer_set_focus (pointer, surface);
}

static void
default_grab_motion (MetaWaylandPointerGrab *grab,
		     const ClutterEvent     *event)
{
  MetaWaylandPointer *pointer = grab->pointer;

  meta_wayland_pointer_send_motion (pointer, event);
}

static void
default_grab_button (MetaWaylandPointerGrab *grab,
		     const ClutterEvent     *event)
{
  MetaWaylandPointer *pointer = grab->pointer;

  meta_wayland_pointer_send_button (pointer, event);
}

static const MetaWaylandPointerGrabInterface default_pointer_grab_interface = {
  default_grab_focus,
  default_grab_motion,
  default_grab_button
};

static void
meta_wayland_pointer_on_cursor_changed (MetaCursorTracker *cursor_tracker,
                                        MetaWaylandPointer *pointer)
{
  if (pointer->cursor_surface)
    meta_wayland_surface_update_outputs (pointer->cursor_surface);
}

void
meta_wayland_pointer_enable (MetaWaylandPointer *pointer)
{
  MetaBackend *backend = meta_get_backend ();
  MetaCursorTracker *cursor_tracker = meta_backend_get_cursor_tracker (backend);
  ClutterDeviceManager *manager;

  pointer->pointer_clients =
    g_hash_table_new_full (NULL, NULL, NULL,
                           (GDestroyNotify) meta_wayland_pointer_client_free);

  pointer->cursor_surface = NULL;

  manager = clutter_device_manager_get_default ();
  pointer->device = clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);

  g_signal_connect (cursor_tracker,
                    "cursor-changed",
                    G_CALLBACK (meta_wayland_pointer_on_cursor_changed),
                    pointer);
}

void
meta_wayland_pointer_disable (MetaWaylandPointer *pointer)
{
  MetaBackend *backend = meta_get_backend ();
  MetaCursorTracker *cursor_tracker = meta_backend_get_cursor_tracker (backend);

  g_signal_handlers_disconnect_by_func (cursor_tracker,
                                        (gpointer) meta_wayland_pointer_on_cursor_changed,
                                        pointer);

  if (pointer->cursor_surface && pointer->cursor_surface_destroy_id)
    {
      g_signal_handler_disconnect (pointer->cursor_surface,
                                   pointer->cursor_surface_destroy_id);
    }

  meta_wayland_pointer_cancel_grab (pointer);
  meta_wayland_pointer_reset_grab (pointer);
  meta_wayland_pointer_set_focus (pointer, NULL);
  meta_wayland_pointer_set_current (pointer, NULL);

  g_clear_pointer (&pointer->pointer_clients, g_hash_table_unref);
  pointer->cursor_surface = NULL;
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
current_surface_destroyed (MetaWaylandSurface *surface,
                           MetaWaylandPointer *pointer)
{
  meta_wayland_pointer_set_current (pointer, NULL);
}

static void
meta_wayland_pointer_set_current (MetaWaylandPointer *pointer,
                                  MetaWaylandSurface *surface)
{
  if (pointer->current)
    {
      g_signal_handler_disconnect (pointer->current,
                                   pointer->current_surface_destroyed_handler_id);
      pointer->current = NULL;
    }

  if (surface)
    {
      pointer->current = surface;
      pointer->current_surface_destroyed_handler_id =
        g_signal_connect (surface, "destroy",
                          G_CALLBACK (current_surface_destroyed),
                          pointer);
    }
}

static void
repick_for_event (MetaWaylandPointer *pointer,
                  const ClutterEvent *for_event)
{
  ClutterActor *actor;
  MetaWaylandSurface *surface;

  if (for_event)
    actor = clutter_event_get_source (for_event);
  else
    actor = clutter_input_device_get_pointer_actor (pointer->device);

  if (META_IS_SURFACE_ACTOR_WAYLAND (actor))
    {
      MetaSurfaceActorWayland *actor_wayland =
        META_SURFACE_ACTOR_WAYLAND (actor);

      surface = meta_surface_actor_wayland_get_surface (actor_wayland);
    }
  else
    {
      surface = NULL;
    }

  meta_wayland_pointer_set_current (pointer, surface);

  sync_focus_surface (pointer);
  meta_wayland_pointer_update_cursor_surface (pointer);
}

void
meta_wayland_pointer_update (MetaWaylandPointer *pointer,
                             const ClutterEvent *event)
{
  repick_for_event (pointer, event);

  if (event->type == CLUTTER_MOTION ||
      event->type == CLUTTER_BUTTON_PRESS ||
      event->type == CLUTTER_BUTTON_RELEASE)
    {
      pointer->button_count = count_buttons (event);
    }
}

static void
notify_motion (MetaWaylandPointer *pointer,
               const ClutterEvent *event)
{
  pointer->grab->interface->motion (pointer->grab, event);
}

static void
handle_motion_event (MetaWaylandPointer *pointer,
                     const ClutterEvent *event)
{
  notify_motion (pointer, event);
}

static void
handle_button_event (MetaWaylandPointer *pointer,
                     const ClutterEvent *event)
{
  gboolean implicit_grab;

  implicit_grab = (event->type == CLUTTER_BUTTON_PRESS) && (pointer->button_count == 1);
  if (implicit_grab)
    {
      pointer->grab_button = clutter_event_get_button (event);
      pointer->grab_time = clutter_event_get_time (event);
      clutter_event_get_coords (event, &pointer->grab_x, &pointer->grab_y);
    }

  pointer->grab->interface->button (pointer->grab, event);

  if (implicit_grab)
    {
      MetaWaylandSeat *seat = meta_wayland_pointer_get_seat (pointer);

      pointer->grab_serial = wl_display_get_serial (seat->wl_display);
    }
}

static void
handle_scroll_event (MetaWaylandPointer *pointer,
                     const ClutterEvent *event)
{
  struct wl_resource *resource;
  wl_fixed_t x_value = 0, y_value = 0;
  int x_discrete = 0, y_discrete = 0;
  enum wl_pointer_axis_source source = -1;

  if (clutter_event_is_pointer_emulated (event))
    return;

  switch (event->scroll.scroll_source)
    {
    case CLUTTER_SCROLL_SOURCE_WHEEL:
      source = WL_POINTER_AXIS_SOURCE_WHEEL;
      break;
    case CLUTTER_SCROLL_SOURCE_FINGER:
      source = WL_POINTER_AXIS_SOURCE_FINGER;
      break;
    case CLUTTER_SCROLL_SOURCE_CONTINUOUS:
      source = WL_POINTER_AXIS_SOURCE_CONTINUOUS;
      break;
    default:
      source = WL_POINTER_AXIS_SOURCE_WHEEL;
      break;
    }

  switch (clutter_event_get_scroll_direction (event))
    {
    case CLUTTER_SCROLL_UP:
      y_value = -DEFAULT_AXIS_STEP_DISTANCE;
      y_discrete = -1;
      break;

    case CLUTTER_SCROLL_DOWN:
      y_value = DEFAULT_AXIS_STEP_DISTANCE;
      y_discrete = 1;
      break;

    case CLUTTER_SCROLL_LEFT:
      x_value = -DEFAULT_AXIS_STEP_DISTANCE;
      x_discrete = -1;
      break;

    case CLUTTER_SCROLL_RIGHT:
      x_value = DEFAULT_AXIS_STEP_DISTANCE;
      x_discrete = 1;
      break;

    case CLUTTER_SCROLL_SMOOTH:
      {
        double dx, dy;
        /* Clutter smooth scroll events are in discrete steps (1 step = 1.0 long
         * vector along one axis). To convert to smooth scroll events that are
         * in pointer motion event space, multiply the vector with the 10. */
        const double factor = 10.0;
        clutter_event_get_scroll_delta (event, &dx, &dy);
        x_value = wl_fixed_from_double (dx) * factor;
        y_value = wl_fixed_from_double (dy) * factor;
      }
      break;

    default:
      return;
    }

  if (pointer->focus_client)
    {
      wl_resource_for_each (resource, &pointer->focus_client->pointer_resources)
        {
          if (wl_resource_get_version (resource) >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION)
            wl_pointer_send_axis_source (resource, source);

          /* X axis */
          if (x_discrete != 0 &&
              wl_resource_get_version (resource) >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION)
            wl_pointer_send_axis_discrete (resource,
                                           WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                                           x_discrete);

          if (x_value)
            wl_pointer_send_axis (resource, clutter_event_get_time (event),
                                  WL_POINTER_AXIS_HORIZONTAL_SCROLL, x_value);

          if ((event->scroll.finish_flags & CLUTTER_SCROLL_FINISHED_HORIZONTAL) &&
              wl_resource_get_version (resource) >= WL_POINTER_AXIS_STOP_SINCE_VERSION)
            wl_pointer_send_axis_stop (resource,
                                       clutter_event_get_time (event),
                                       WL_POINTER_AXIS_HORIZONTAL_SCROLL);
          /* Y axis */
          if (y_discrete != 0 &&
              wl_resource_get_version (resource) >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION)
            wl_pointer_send_axis_discrete (resource,
                                           WL_POINTER_AXIS_VERTICAL_SCROLL,
                                           y_discrete);

          if (y_value)
            wl_pointer_send_axis (resource, clutter_event_get_time (event),
                                  WL_POINTER_AXIS_VERTICAL_SCROLL, y_value);

          if ((event->scroll.finish_flags & CLUTTER_SCROLL_FINISHED_VERTICAL) &&
              wl_resource_get_version (resource) >= WL_POINTER_AXIS_STOP_SINCE_VERSION)
            wl_pointer_send_axis_stop (resource,
                                       clutter_event_get_time (event),
                                       WL_POINTER_AXIS_VERTICAL_SCROLL);
        }

      meta_wayland_pointer_broadcast_frame (pointer);
    }
}

gboolean
meta_wayland_pointer_handle_event (MetaWaylandPointer *pointer,
                                   const ClutterEvent *event)
{
  switch (event->type)
    {
    case CLUTTER_MOTION:
      handle_motion_event (pointer, event);
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      handle_button_event (pointer, event);
      break;

    case CLUTTER_SCROLL:
      handle_scroll_event (pointer, event);
      break;

    case CLUTTER_TOUCHPAD_SWIPE:
      meta_wayland_pointer_gesture_swipe_handle_event (pointer, event);
      break;

    case CLUTTER_TOUCHPAD_PINCH:
      meta_wayland_pointer_gesture_pinch_handle_event (pointer, event);
      break;

    default:
      break;
    }

  return FALSE;
}

static void
meta_wayland_pointer_send_enter (MetaWaylandPointer *pointer,
                                 struct wl_resource *pointer_resource,
                                 uint32_t            serial,
                                 MetaWaylandSurface *surface)
{
  wl_fixed_t sx, sy;

  meta_wayland_pointer_get_relative_coordinates (pointer, surface, &sx, &sy);
  wl_pointer_send_enter (pointer_resource,
                         serial,
                         surface->resource,
                         sx, sy);
}

static void
meta_wayland_pointer_send_leave (MetaWaylandPointer *pointer,
                                 struct wl_resource *pointer_resource,
                                 uint32_t            serial,
                                 MetaWaylandSurface *surface)
{
  wl_pointer_send_leave (pointer_resource, serial, surface->resource);
}

static void
meta_wayland_pointer_broadcast_enter (MetaWaylandPointer *pointer,
                                      uint32_t            serial,
                                      MetaWaylandSurface *surface)
{
  struct wl_resource *pointer_resource;

  wl_resource_for_each (pointer_resource,
                        &pointer->focus_client->pointer_resources)
    meta_wayland_pointer_send_enter (pointer, pointer_resource,
                                     serial, surface);

  meta_wayland_pointer_broadcast_frame (pointer);
}

static void
meta_wayland_pointer_broadcast_leave (MetaWaylandPointer *pointer,
                                      uint32_t            serial,
                                      MetaWaylandSurface *surface)
{
  struct wl_resource *pointer_resource;

  wl_resource_for_each (pointer_resource,
                        &pointer->focus_client->pointer_resources)
    meta_wayland_pointer_send_leave (pointer, pointer_resource,
                                     serial, surface);

  meta_wayland_pointer_broadcast_frame (pointer);
}

static void
focus_surface_destroyed (MetaWaylandSurface *surface,
                         MetaWaylandPointer *pointer)
{
  meta_wayland_pointer_set_focus (pointer, NULL);
}

void
meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface)
{
  MetaWaylandInputDevice *input_device = META_WAYLAND_INPUT_DEVICE (pointer);

  if (pointer->focus_surface == surface)
    return;

  if (pointer->focus_surface != NULL)
    {
      uint32_t serial;

      serial = meta_wayland_input_device_next_serial (input_device);

      if (pointer->focus_client)
        {
          meta_wayland_pointer_broadcast_leave (pointer,
                                                serial,
                                                pointer->focus_surface);
          pointer->focus_client = NULL;
        }

      g_signal_handler_disconnect (pointer->focus_surface,
                                   pointer->focus_surface_destroyed_handler_id);
      pointer->focus_surface_destroyed_handler_id = 0;
      pointer->focus_surface = NULL;
    }

  if (surface != NULL)
    {
      struct wl_client *client = wl_resource_get_client (surface->resource);
      ClutterPoint pos;

      pointer->focus_surface = surface;

      pointer->focus_surface_destroyed_handler_id =
        g_signal_connect_after (pointer->focus_surface, "destroy",
                                G_CALLBACK (focus_surface_destroyed),
                                pointer);

      clutter_input_device_get_coords (pointer->device, NULL, &pos);

      if (pointer->focus_surface->window)
        meta_window_handle_enter (pointer->focus_surface->window,
                                  /* XXX -- can we reliably get a timestamp for setting focus? */
                                  clutter_get_current_event_time (),
                                  pos.x, pos.y);

      pointer->focus_client =
        meta_wayland_pointer_get_pointer_client (pointer, client);
      if (pointer->focus_client)
        {
          pointer->focus_serial =
            meta_wayland_input_device_next_serial (input_device);
          meta_wayland_pointer_broadcast_enter (pointer,
                                                pointer->focus_serial,
                                                pointer->focus_surface);
        }
    }

  meta_wayland_pointer_update_cursor_surface (pointer);

  g_signal_emit (pointer, signals[FOCUS_SURFACE_CHANGED], 0);
}

void
meta_wayland_pointer_start_grab (MetaWaylandPointer *pointer,
                                 MetaWaylandPointerGrab *grab)
{
  const MetaWaylandPointerGrabInterface *interface;

  meta_wayland_pointer_cancel_grab (pointer);

  pointer->grab = grab;
  interface = pointer->grab->interface;
  grab->pointer = pointer;

  interface->focus (pointer->grab, pointer->current);
}

static void
meta_wayland_pointer_reset_grab (MetaWaylandPointer *pointer)
{
  pointer->grab = &pointer->default_grab;
}

void
meta_wayland_pointer_end_grab (MetaWaylandPointer *pointer)
{
  const MetaWaylandPointerGrabInterface *interface;

  pointer->grab = &pointer->default_grab;
  interface = pointer->grab->interface;
  interface->focus (pointer->grab, pointer->current);

  meta_wayland_pointer_update_cursor_surface (pointer);
}

static void
meta_wayland_pointer_cancel_grab (MetaWaylandPointer *pointer)
{
  if (pointer->grab->interface->cancel)
    pointer->grab->interface->cancel (pointer->grab);
}

void
meta_wayland_pointer_end_popup_grab (MetaWaylandPointer *pointer)
{
  MetaWaylandPopupGrab *popup_grab = (MetaWaylandPopupGrab*)pointer->grab;

  meta_wayland_popup_grab_destroy (popup_grab);
}

MetaWaylandPopup *
meta_wayland_pointer_start_popup_grab (MetaWaylandPointer      *pointer,
                                       MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandPopupGrab *grab;

  if (pointer->grab != &pointer->default_grab &&
      !meta_wayland_pointer_grab_is_popup_grab (pointer->grab))
    return NULL;

  if (pointer->grab == &pointer->default_grab)
    grab = meta_wayland_popup_grab_create (pointer, popup_surface);
  else
    grab = (MetaWaylandPopupGrab*)pointer->grab;

  return meta_wayland_popup_create (popup_surface, grab);
}

void
meta_wayland_pointer_repick (MetaWaylandPointer *pointer)
{
  repick_for_event (pointer, NULL);
}

void
meta_wayland_pointer_get_relative_coordinates (MetaWaylandPointer *pointer,
					       MetaWaylandSurface *surface,
					       wl_fixed_t         *sx,
					       wl_fixed_t         *sy)
{
  float xf = 0.0f, yf = 0.0f;
  ClutterPoint pos;

  clutter_input_device_get_coords (pointer->device, NULL, &pos);
  meta_wayland_surface_get_relative_coordinates (surface, pos.x, pos.y, &xf, &yf);

  *sx = wl_fixed_from_double (xf);
  *sy = wl_fixed_from_double (yf);
}

void
meta_wayland_pointer_update_cursor_surface (MetaWaylandPointer *pointer)
{
  MetaBackend *backend = meta_get_backend ();
  MetaCursorTracker *cursor_tracker = meta_backend_get_cursor_tracker (backend);

  if (pointer->current)
    {
      MetaCursorSprite *cursor_sprite = NULL;

      if (pointer->cursor_surface)
        {
          MetaWaylandSurfaceRoleCursor *cursor_role =
            META_WAYLAND_SURFACE_ROLE_CURSOR (pointer->cursor_surface->role);

          cursor_sprite = meta_wayland_surface_role_cursor_get_sprite (cursor_role);
        }

      meta_cursor_tracker_set_window_cursor (cursor_tracker, cursor_sprite);
    }
  else
    {
      meta_cursor_tracker_unset_window_cursor (cursor_tracker);
    }
}

static void
ensure_update_cursor_surface (MetaWaylandPointer *pointer,
                              MetaWaylandSurface *surface)
{
  if (pointer->cursor_surface != surface)
    return;

  pointer->cursor_surface = NULL;
  meta_wayland_pointer_update_cursor_surface (pointer);
}

static void
meta_wayland_pointer_set_cursor_surface (MetaWaylandPointer *pointer,
                                         MetaWaylandSurface *cursor_surface)
{
  MetaWaylandSurface *prev_cursor_surface;

  prev_cursor_surface = pointer->cursor_surface;

  if (prev_cursor_surface == cursor_surface)
    return;

  pointer->cursor_surface = cursor_surface;

  if (prev_cursor_surface)
    {
      meta_wayland_surface_update_outputs (prev_cursor_surface);
      g_signal_handler_disconnect (prev_cursor_surface,
                                   pointer->cursor_surface_destroy_id);
    }

  if (cursor_surface)
    {
      pointer->cursor_surface_destroy_id =
        g_signal_connect_swapped (cursor_surface, "destroy",
                                  G_CALLBACK (ensure_update_cursor_surface),
                                  pointer);
    }

  meta_wayland_pointer_update_cursor_surface (pointer);
}

static void
pointer_set_cursor (struct wl_client *client,
                    struct wl_resource *resource,
                    uint32_t serial,
                    struct wl_resource *surface_resource,
                    int32_t hot_x, int32_t hot_y)
{
  MetaWaylandPointer *pointer = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface;

  surface = (surface_resource ? wl_resource_get_user_data (surface_resource) : NULL);

  if (pointer->focus_surface == NULL)
    return;
  if (wl_resource_get_client (pointer->focus_surface->resource) != client)
    return;
  if (pointer->focus_serial - serial > G_MAXUINT32 / 2)
    return;

  if (surface &&
      !meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_CURSOR,
                                         NULL))
    {
      wl_resource_post_error (resource, WL_POINTER_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface_resource));
      return;
    }

  if (surface)
    {
      MetaCursorRenderer *cursor_renderer =
        meta_backend_get_cursor_renderer (meta_get_backend ());
      MetaWaylandSurfaceRoleCursor *cursor_role;

      cursor_role = META_WAYLAND_SURFACE_ROLE_CURSOR (surface->role);
      meta_wayland_surface_role_cursor_set_renderer (cursor_role,
                                                     cursor_renderer);
      meta_wayland_surface_role_cursor_set_hotspot (cursor_role,
                                                    hot_x, hot_y);
    }

  meta_wayland_pointer_set_cursor_surface (pointer, surface);
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

void
meta_wayland_pointer_create_new_resource (MetaWaylandPointer *pointer,
                                          struct wl_client   *client,
                                          struct wl_resource *seat_resource,
                                          uint32_t id)
{
  struct wl_resource *resource;
  MetaWaylandPointerClient *pointer_client;

  resource = wl_resource_create (client, &wl_pointer_interface,
                                 wl_resource_get_version (seat_resource), id);
  wl_resource_set_implementation (resource, &pointer_interface, pointer,
                                  meta_wayland_pointer_unbind_pointer_client_resource);

  pointer_client = meta_wayland_pointer_ensure_pointer_client (pointer, client);

  wl_list_insert (&pointer_client->pointer_resources,
                  wl_resource_get_link (resource));

  if (pointer->focus_client == pointer_client)
    {
      meta_wayland_pointer_send_enter (pointer, resource,
                                       pointer->focus_serial,
                                       pointer->focus_surface);
      meta_wayland_pointer_send_frame (pointer, resource);
    }
}

static gboolean
pointer_can_grab_surface (MetaWaylandPointer *pointer,
                          MetaWaylandSurface *surface)
{
  GList *l;

  if (pointer->focus_surface == surface)
    return TRUE;

  for (l = surface->subsurfaces; l; l = l->next)
    {
      MetaWaylandSurface *subsurface = l->data;

      if (pointer_can_grab_surface (pointer, subsurface))
        return TRUE;
    }

  return FALSE;
}

gboolean
meta_wayland_pointer_can_grab_surface (MetaWaylandPointer *pointer,
                                       MetaWaylandSurface *surface,
                                       uint32_t            serial)
{
  return (pointer->grab_serial == serial &&
          pointer_can_grab_surface (pointer, surface));
}

gboolean
meta_wayland_pointer_can_popup (MetaWaylandPointer *pointer, uint32_t serial)
{
  return pointer->grab_serial == serial;
}

MetaWaylandSurface *
meta_wayland_pointer_get_top_popup (MetaWaylandPointer *pointer)
{
  MetaWaylandPopupGrab *grab;

  if (!meta_wayland_pointer_grab_is_popup_grab (pointer->grab))
    return NULL;

  grab = (MetaWaylandPopupGrab*)pointer->grab;
  return meta_wayland_popup_grab_get_top_popup(grab);
}

static void
relative_pointer_destroy (struct wl_client *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_relative_pointer_v1_interface relative_pointer_interface = {
  relative_pointer_destroy
};

static void
relative_pointer_manager_destroy (struct wl_client *client,
                                  struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
relative_pointer_manager_get_relative_pointer (struct wl_client   *client,
                                               struct wl_resource *manager_resource,
                                               uint32_t            id,
                                               struct wl_resource *pointer_resource)
{
  MetaWaylandPointer *pointer = wl_resource_get_user_data (pointer_resource);
  struct wl_resource *resource;
  MetaWaylandPointerClient *pointer_client;

  resource = wl_resource_create (client, &zwp_relative_pointer_v1_interface,
                                 wl_resource_get_version (manager_resource),
                                 id);
  if (!resource)
    {
      wl_client_post_no_memory (client);
      return;
    }

  wl_resource_set_implementation (resource, &relative_pointer_interface,
                                  pointer,
                                  meta_wayland_pointer_unbind_pointer_client_resource);

  pointer_client = meta_wayland_pointer_ensure_pointer_client (pointer, client);

  wl_list_insert (&pointer_client->relative_pointer_resources,
                  wl_resource_get_link (resource));
}

static const struct zwp_relative_pointer_manager_v1_interface relative_pointer_manager = {
  relative_pointer_manager_destroy,
  relative_pointer_manager_get_relative_pointer,
};

static void
bind_relative_pointer_manager (struct wl_client *client,
                               void             *data,
                               uint32_t          version,
                               uint32_t          id)
{
  MetaWaylandCompositor *compositor = data;
  struct wl_resource *resource;

  resource = wl_resource_create (client,
                                 &zwp_relative_pointer_manager_v1_interface,
                                 1, id);

  if (version != 1)
    wl_resource_post_error (resource,
                            WL_DISPLAY_ERROR_INVALID_OBJECT,
                            "bound invalid version %u of "
                            "wp_relative_pointer_manager",
                            version);

  wl_resource_set_implementation (resource, &relative_pointer_manager,
                                  compositor,
                                  NULL);
}

void
meta_wayland_relative_pointer_init (MetaWaylandCompositor *compositor)
{
  /* Relative pointer events are currently only supported by the native backend
   * so lets just advertise the extension when the native backend is used.
   */
#ifdef HAVE_NATIVE_BACKEND
  if (!META_IS_BACKEND_NATIVE (meta_get_backend ()))
    return;
#else
  return;
#endif

  if (!wl_global_create (compositor->wayland_display,
                         &zwp_relative_pointer_manager_v1_interface, 1,
                         compositor, bind_relative_pointer_manager))
    g_error ("Could not create relative pointer manager global");
}

MetaWaylandSeat *
meta_wayland_pointer_get_seat (MetaWaylandPointer *pointer)
{
  MetaWaylandInputDevice *input_device = META_WAYLAND_INPUT_DEVICE (pointer);

  return meta_wayland_input_device_get_seat (input_device);
}

static void
meta_wayland_pointer_init (MetaWaylandPointer *pointer)
{
  pointer->default_grab.interface = &default_pointer_grab_interface;
  pointer->default_grab.pointer = pointer;
  pointer->grab = &pointer->default_grab;
}

static void
meta_wayland_pointer_class_init (MetaWaylandPointerClass *klass)
{
  signals[FOCUS_SURFACE_CHANGED] = g_signal_new ("focus-surface-changed",
                                                 G_TYPE_FROM_CLASS (klass),
                                                 G_SIGNAL_RUN_LAST,
                                                 0,
                                                 NULL, NULL, NULL,
                                                 G_TYPE_NONE, 0);
}
