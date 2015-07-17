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
#include <cogl/cogl.h>
#include <cogl/cogl-wayland-server.h>
#include <linux/input.h>

#include "meta-wayland-pointer.h"
#include "meta-wayland-popup.h"
#include "meta-wayland-private.h"
#include "meta-wayland-surface.h"
#include "meta-wayland-buffer.h"
#include "meta-cursor.h"
#include "meta-cursor-tracker-private.h"
#include "meta-surface-actor-wayland.h"
#include "meta/meta-cursor-tracker.h"
#include "backends/meta-backend-private.h"
#include "backends/meta-cursor-tracker-private.h"
#include "backends/meta-cursor-renderer.h"

#include <string.h>

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int (10)

struct _MetaWaylandSurfaceRoleCursor
{
  MetaWaylandSurfaceRole parent;

  int hot_x;
  int hot_y;
  MetaCursorSprite *cursor_sprite;
};

GType meta_wayland_surface_role_cursor_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (MetaWaylandSurfaceRoleCursor,
               meta_wayland_surface_role_cursor,
               META_TYPE_WAYLAND_SURFACE_ROLE);

static void
meta_wayland_pointer_update_cursor_surface (MetaWaylandPointer *pointer);

static MetaWaylandPointerClient *
meta_wayland_pointer_client_new (void)
{
  MetaWaylandPointerClient *pointer_client;

  pointer_client = g_slice_new0 (MetaWaylandPointerClient);
  wl_list_init (&pointer_client->pointer_resources);
  wl_list_init (&pointer_client->swipe_gesture_resources);
  wl_list_init (&pointer_client->pinch_gesture_resources);

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

  g_slice_free (MetaWaylandPointerClient, pointer_client);
}

static gboolean
meta_wayland_pointer_client_is_empty (MetaWaylandPointerClient *pointer_client)
{
  return (wl_list_empty (&pointer_client->pointer_resources) &&
          wl_list_empty (&pointer_client->swipe_gesture_resources) &&
          wl_list_empty (&pointer_client->pinch_gesture_resources));
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
pointer_handle_focus_surface_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandPointer *pointer = wl_container_of (listener, pointer, focus_surface_listener);

  meta_wayland_pointer_set_focus (pointer, NULL);
}

void
meta_wayland_pointer_send_motion (MetaWaylandPointer *pointer,
                                  const ClutterEvent *event)
{
  struct wl_resource *resource;
  uint32_t time;
  wl_fixed_t sx, sy;

  if (!pointer->focus_client)
    return;

  time = clutter_event_get_time (event);
  meta_wayland_pointer_get_relative_coordinates (pointer,
                                                 pointer->focus_surface,
                                                 &sx, &sy);

  wl_resource_for_each (resource, &pointer->focus_client->pointer_resources)
    {
      wl_pointer_send_motion (resource, time, sx, sy);
    }
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
      struct wl_client *client = wl_resource_get_client (pointer->focus_surface->resource);
      struct wl_display *display = wl_client_get_display (client);
      uint32_t time;
      uint32_t button;
      uint32_t serial;

      time = clutter_event_get_time (event);

      button = clutter_event_get_button (event);
      switch (button)
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
	  button = button + BTN_LEFT - 1;
	  break;
	}

      serial = wl_display_next_serial (display);

      wl_resource_for_each (resource, &pointer->focus_client->pointer_resources)
        {
          wl_pointer_send_button (resource, serial,
                                  time, button,
                                  event_type == CLUTTER_BUTTON_PRESS ? 1 : 0);
        }
    }

  if (pointer->button_count == 0 && event_type == CLUTTER_BUTTON_RELEASE)
    sync_focus_surface (pointer);
}

static void
default_grab_focus (MetaWaylandPointerGrab *grab,
                    MetaWaylandSurface     *surface)
{
  MetaWaylandPointer *pointer = grab->pointer;

  if (pointer->button_count > 0)
    return;

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
meta_wayland_pointer_init (MetaWaylandPointer *pointer,
                           struct wl_display  *display)
{
  MetaCursorTracker *cursor_tracker = meta_cursor_tracker_get_for_screen (NULL);
  ClutterDeviceManager *manager;

  memset (pointer, 0, sizeof *pointer);

  pointer->display = display;

  pointer->pointer_clients =
    g_hash_table_new_full (NULL, NULL, NULL,
                           (GDestroyNotify) meta_wayland_pointer_client_free);

  pointer->focus_surface_listener.notify = pointer_handle_focus_surface_destroy;

  pointer->cursor_surface = NULL;

  pointer->default_grab.interface = &default_pointer_grab_interface;
  pointer->default_grab.pointer = pointer;
  pointer->grab = &pointer->default_grab;

  manager = clutter_device_manager_get_default ();
  pointer->device = clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);

  g_signal_connect (cursor_tracker,
                    "cursor-changed",
                    G_CALLBACK (meta_wayland_pointer_on_cursor_changed),
                    pointer);
}

void
meta_wayland_pointer_release (MetaWaylandPointer *pointer)
{
  MetaCursorTracker *cursor_tracker = meta_cursor_tracker_get_for_screen (NULL);

  g_signal_handlers_disconnect_by_func (cursor_tracker,
                                        (gpointer) meta_wayland_pointer_on_cursor_changed,
                                        pointer);

  meta_wayland_pointer_set_focus (pointer, NULL);

  g_clear_pointer (&pointer->pointer_clients, g_hash_table_unref);
  pointer->display = NULL;
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
repick_for_event (MetaWaylandPointer *pointer,
                  const ClutterEvent *for_event)
{
  ClutterActor *actor;

  if (for_event)
    actor = clutter_event_get_source (for_event);
  else
    actor = clutter_input_device_get_pointer_actor (pointer->device);

  if (META_IS_SURFACE_ACTOR_WAYLAND (actor))
    pointer->current = meta_surface_actor_wayland_get_surface (META_SURFACE_ACTOR_WAYLAND (actor));
  else
    pointer->current = NULL;

  sync_focus_surface (pointer);
  meta_wayland_pointer_update_cursor_surface (pointer);
}

void
meta_wayland_pointer_update (MetaWaylandPointer *pointer,
                             const ClutterEvent *event)
{
  repick_for_event (pointer, event);

  pointer->button_count = count_buttons (event);
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
    pointer->grab_serial = wl_display_get_serial (pointer->display);
}

static void
handle_scroll_event (MetaWaylandPointer *pointer,
                     const ClutterEvent *event)
{
  struct wl_resource *resource;
  wl_fixed_t x_value = 0, y_value = 0;

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
          if (x_value)
            wl_pointer_send_axis (resource, clutter_event_get_time (event),
                                  WL_POINTER_AXIS_HORIZONTAL_SCROLL, x_value);
          if (y_value)
            wl_pointer_send_axis (resource, clutter_event_get_time (event),
                                  WL_POINTER_AXIS_VERTICAL_SCROLL, y_value);
        }
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
broadcast_focus (MetaWaylandPointer *pointer,
                 struct wl_resource *resource)
{
  wl_fixed_t sx, sy;

  meta_wayland_pointer_get_relative_coordinates (pointer, pointer->focus_surface, &sx, &sy);
  wl_pointer_send_enter (resource, pointer->focus_serial, pointer->focus_surface->resource, sx, sy);
}

void
meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface)
{
  if (pointer->display == NULL)
    return;

  if (pointer->focus_surface == surface)
    return;

  if (pointer->focus_surface != NULL)
    {
      struct wl_client *client =
        wl_resource_get_client (pointer->focus_surface->resource);
      struct wl_display *display = wl_client_get_display (client);
      uint32_t serial;
      struct wl_resource *resource;

      serial = wl_display_next_serial (display);

      if (pointer->focus_client)
        {
          wl_resource_for_each (resource,
                                &pointer->focus_client->pointer_resources)
            {
              wl_pointer_send_leave (resource, serial, pointer->focus_surface->resource);
            }

          pointer->focus_client = NULL;
        }

      wl_list_remove (&pointer->focus_surface_listener.link);
      pointer->focus_surface = NULL;
    }

  if (surface != NULL)
    {
      struct wl_client *client = wl_resource_get_client (surface->resource);
      struct wl_display *display = wl_client_get_display (client);
      struct wl_resource *resource;
      ClutterPoint pos;

      pointer->focus_surface = surface;
      wl_resource_add_destroy_listener (pointer->focus_surface->resource, &pointer->focus_surface_listener);

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
          pointer->focus_serial = wl_display_next_serial (display);

          wl_resource_for_each (resource,
                                &pointer->focus_client->pointer_resources)
            {
              broadcast_focus (pointer, resource);
            }
        }
    }

  meta_wayland_pointer_update_cursor_surface (pointer);
}

void
meta_wayland_pointer_start_grab (MetaWaylandPointer *pointer,
                                 MetaWaylandPointerGrab *grab)
{
  const MetaWaylandPointerGrabInterface *interface;

  pointer->grab = grab;
  interface = pointer->grab->interface;
  grab->pointer = pointer;

  if (pointer->current)
    interface->focus (pointer->grab, pointer->current);
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

void
meta_wayland_pointer_end_popup_grab (MetaWaylandPointer *pointer)
{
  MetaWaylandPopupGrab *popup_grab = (MetaWaylandPopupGrab*)pointer->grab;

  meta_wayland_popup_grab_end (popup_grab);
  meta_wayland_popup_grab_destroy (popup_grab);
}

MetaWaylandPopup *
meta_wayland_pointer_start_popup_grab (MetaWaylandPointer *pointer,
				       MetaWaylandSurface *surface)
{
  MetaWaylandPopupGrab *grab;

  if (pointer->grab != &pointer->default_grab &&
      !meta_wayland_pointer_grab_is_popup_grab (pointer->grab))
    return NULL;

  if (pointer->grab == &pointer->default_grab)
    {
      struct wl_client *client = wl_resource_get_client (surface->resource);

      grab = meta_wayland_popup_grab_create (pointer, client);
      meta_wayland_popup_grab_begin (grab, surface);
    }
  else
    grab = (MetaWaylandPopupGrab*)pointer->grab;

  return meta_wayland_popup_create (surface, grab);
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
  clutter_actor_transform_stage_point (CLUTTER_ACTOR (meta_surface_actor_get_texture (surface->surface_actor)),
                                       pos.x, pos.y, &xf, &yf);

  *sx = wl_fixed_from_double (xf) / surface->scale;
  *sy = wl_fixed_from_double (yf) / surface->scale;
}

static void
meta_wayland_pointer_update_cursor_surface (MetaWaylandPointer *pointer)
{
  MetaCursorTracker *cursor_tracker = meta_cursor_tracker_get_for_screen (NULL);

  if (pointer->current)
    {
      MetaCursorSprite *cursor_sprite = NULL;

      if (pointer->cursor_surface)
        {
          MetaWaylandSurfaceRoleCursor *cursor_role =
            META_WAYLAND_SURFACE_ROLE_CURSOR (pointer->cursor_surface->role);

          cursor_sprite = cursor_role->cursor_sprite;
        }

      meta_cursor_tracker_set_window_cursor (cursor_tracker, cursor_sprite);
    }
  else
    {
      meta_cursor_tracker_unset_window_cursor (cursor_tracker);
    }
}

static void
update_cursor_sprite_texture (MetaWaylandSurface *surface)
{
  MetaCursorRenderer *cursor_renderer =
    meta_backend_get_cursor_renderer (meta_get_backend ());
  MetaCursorTracker *cursor_tracker = meta_cursor_tracker_get_for_screen (NULL);
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (surface->role);
  MetaCursorSprite *cursor_sprite = cursor_role->cursor_sprite;
  ClutterBackend *clutter_backend = clutter_get_default_backend ();
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  CoglTexture *texture;

  if (surface->buffer)
    {
      struct wl_resource *buffer;

      buffer = surface->buffer->resource;
      texture = cogl_wayland_texture_2d_new_from_buffer (cogl_context,
                                                         buffer,
                                                         NULL);

      meta_cursor_sprite_set_texture (cursor_sprite,
                                      texture,
                                      cursor_role->hot_x * surface->scale,
                                      cursor_role->hot_y * surface->scale);
      meta_cursor_renderer_realize_cursor_from_wl_buffer (cursor_renderer,
                                                          cursor_sprite,
                                                          buffer);
      cogl_object_unref (texture);
    }
  else
    {
      meta_cursor_sprite_set_texture (cursor_sprite, NULL, 0, 0);
    }

  if (cursor_sprite == meta_cursor_tracker_get_displayed_cursor (cursor_tracker))
    meta_cursor_renderer_force_update (cursor_renderer);
}

static void
cursor_sprite_prepare_at (MetaCursorSprite *cursor_sprite,
                          int x,
                          int y,
                          MetaWaylandSurfaceRoleCursor *cursor_role)
{
  MetaWaylandSurfaceRole *role = META_WAYLAND_SURFACE_ROLE (cursor_role);
  MetaWaylandSurface *surface = meta_wayland_surface_role_get_surface (role);
  MetaDisplay *display = meta_get_display ();
  MetaScreen *screen = display->screen;
  const MetaMonitorInfo *monitor;

  monitor = meta_screen_get_monitor_for_point (screen, x, y);
  meta_cursor_sprite_set_texture_scale (cursor_sprite,
                                        (float)monitor->scale / surface->scale);
  meta_wayland_surface_update_outputs (surface);
}

static void
meta_wayland_pointer_set_cursor_surface (MetaWaylandPointer *pointer,
                                         MetaWaylandSurface *cursor_surface)
{
  MetaWaylandSurface *prev_cursor_surface;

  prev_cursor_surface = pointer->cursor_surface;
  pointer->cursor_surface = cursor_surface;

  if (prev_cursor_surface != cursor_surface)
    {
      if (prev_cursor_surface)
        meta_wayland_surface_update_outputs (prev_cursor_surface);
      meta_wayland_pointer_update_cursor_surface (pointer);
    }
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
                                         META_TYPE_WAYLAND_SURFACE_ROLE_CURSOR))
    {
      wl_resource_post_error (resource, WL_POINTER_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface_resource));
      return;
    }

  if (surface)
    {
      MetaWaylandSurfaceRoleCursor *cursor_role;

      cursor_role = META_WAYLAND_SURFACE_ROLE_CURSOR (surface->role);
      if (!cursor_role->cursor_sprite)
        {
          cursor_role->cursor_sprite = meta_cursor_sprite_new ();
          g_signal_connect_object (cursor_role->cursor_sprite,
                                   "prepare-at",
                                   G_CALLBACK (cursor_sprite_prepare_at),
                                   cursor_role,
                                   0);
        }

      cursor_role->hot_x = hot_x;
      cursor_role->hot_y = hot_y;

      update_cursor_sprite_texture (surface);
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
  struct wl_resource *cr;
  MetaWaylandPointerClient *pointer_client;

  cr = wl_resource_create (client, &wl_pointer_interface, wl_resource_get_version (seat_resource), id);
  wl_resource_set_implementation (cr, &pointer_interface, pointer,
                                  meta_wayland_pointer_unbind_pointer_client_resource);

  pointer_client = meta_wayland_pointer_ensure_pointer_client (pointer, client);

  wl_list_insert (&pointer_client->pointer_resources,
                  wl_resource_get_link (cr));

  if (pointer->focus_client == pointer_client)
    broadcast_focus (pointer, cr);
}

gboolean
meta_wayland_pointer_can_grab_surface (MetaWaylandPointer *pointer,
                                       MetaWaylandSurface *surface,
                                       uint32_t            serial)
{
  return (pointer->button_count > 0 &&
          pointer->grab_serial == serial &&
          pointer->focus_surface == surface);
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
cursor_surface_role_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  meta_wayland_surface_queue_pending_frame_callbacks (surface);
}

static void
cursor_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                            MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  meta_wayland_surface_queue_pending_state_frame_callbacks (surface, pending);

  if (pending->newly_attached)
    update_cursor_sprite_texture (surface);
}

static gboolean
cursor_surface_role_is_on_output (MetaWaylandSurfaceRole *role,
                                  MetaMonitorInfo        *monitor)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (role);
  MetaWaylandPointer *pointer = &surface->compositor->seat->pointer;
  MetaCursorTracker *cursor_tracker = meta_cursor_tracker_get_for_screen (NULL);
  MetaCursorRenderer *cursor_renderer =
    meta_backend_get_cursor_renderer (meta_get_backend ());
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (surface->role);
  MetaCursorSprite *displayed_cursor_sprite;
  MetaRectangle rect;

  if (surface != pointer->cursor_surface)
    return FALSE;

  displayed_cursor_sprite =
    meta_cursor_tracker_get_displayed_cursor (cursor_tracker);
  if (!displayed_cursor_sprite)
    return FALSE;

  if (cursor_role->cursor_sprite != displayed_cursor_sprite)
    return FALSE;

  rect = meta_cursor_renderer_calculate_rect (cursor_renderer,
                                              cursor_role->cursor_sprite);
  return meta_rectangle_overlap (&rect, &monitor->rect);
}

static void
cursor_surface_role_dispose (GObject *object)
{
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (object);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (META_WAYLAND_SURFACE_ROLE (object));
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandPointer *pointer = &compositor->seat->pointer;
  MetaCursorTracker *cursor_tracker = meta_cursor_tracker_get_for_screen (NULL);

  g_signal_handlers_disconnect_by_func (cursor_tracker,
                                        (gpointer) cursor_sprite_prepare_at,
                                        cursor_role);

  if (pointer->cursor_surface == surface)
    pointer->cursor_surface = NULL;
  meta_wayland_pointer_update_cursor_surface (pointer);

  g_clear_object (&cursor_role->cursor_sprite);

  G_OBJECT_CLASS (meta_wayland_surface_role_cursor_parent_class)->dispose (object);
}

static void
meta_wayland_surface_role_cursor_init (MetaWaylandSurfaceRoleCursor *role)
{
}

static void
meta_wayland_surface_role_cursor_class_init (MetaWaylandSurfaceRoleCursorClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  surface_role_class->assigned = cursor_surface_role_assigned;
  surface_role_class->commit = cursor_surface_role_commit;
  surface_role_class->is_on_output = cursor_surface_role_is_on_output;

  object_class->dispose = cursor_surface_role_dispose;
}
