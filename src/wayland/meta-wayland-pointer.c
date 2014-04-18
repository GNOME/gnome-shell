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
#include <linux/input.h>

#include "meta-wayland-pointer.h"
#include "meta-wayland-private.h"
#include "meta-cursor.h"
#include "meta-cursor-tracker-private.h"
#include "meta-surface-actor-wayland.h"

#include <string.h>

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int (10)

static void meta_wayland_pointer_end_popup_grab (MetaWaylandPointer *pointer);

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
set_cursor_surface (MetaWaylandPointer *pointer,
                    MetaWaylandSurface *surface)
{
  if (pointer->cursor_surface == surface)
    return;

  if (pointer->cursor_surface)
    wl_list_remove (&pointer->cursor_surface_destroy_listener.link);

  pointer->cursor_surface = surface;

  if (pointer->cursor_surface)
    wl_resource_add_destroy_listener (pointer->cursor_surface->resource,
                                      &pointer->cursor_surface_destroy_listener);
}

static void
pointer_handle_cursor_surface_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandPointer *pointer = wl_container_of (listener, pointer, cursor_surface_destroy_listener);

  set_cursor_surface (pointer, NULL);
  meta_wayland_pointer_update_cursor_surface (pointer);
}

static void
pointer_handle_focus_surface_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandPointer *pointer = wl_container_of (listener, pointer, focus_surface_listener);

  meta_wayland_pointer_set_focus (pointer, NULL);
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
  struct wl_resource *resource;
  struct wl_list *l;

  l = &pointer->focus_resource_list;
  wl_resource_for_each(resource, l)
    {
      wl_fixed_t sx, sy;

      meta_wayland_pointer_get_relative_coordinates (pointer,
                                                     pointer->focus_surface,
                                                     &sx, &sy);
      wl_pointer_send_motion (resource, clutter_event_get_time (event), sx, sy);
    }
}

static void
default_grab_button (MetaWaylandPointerGrab *grab,
		     const ClutterEvent     *event)
{
  MetaWaylandPointer *pointer = grab->pointer;
  struct wl_resource *resource;
  struct wl_list *l;
  ClutterEventType event_type;

  event_type = clutter_event_type (event);

  l = &grab->pointer->focus_resource_list;
  wl_resource_for_each(resource, l)
    {
      struct wl_client *client = wl_resource_get_client (pointer->focus_surface->resource);
      struct wl_display *display = wl_client_get_display (client);
      uint32_t button;
      uint32_t serial;

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
      wl_pointer_send_button (resource, serial,
			      clutter_event_get_time (event), button,
			      event_type == CLUTTER_BUTTON_PRESS ? 1 : 0);
    }

  if (pointer->button_count == 0 && event_type == CLUTTER_BUTTON_RELEASE)
    meta_wayland_pointer_set_focus (pointer, pointer->current);
}

static const MetaWaylandPointerGrabInterface default_pointer_grab_interface = {
  default_grab_focus,
  default_grab_motion,
  default_grab_button
};

/*
 * The pointer constrain code is mostly a rip-off of the XRandR code from Xorg.
 * (from xserver/randr/rrcrtc.c, RRConstrainCursorHarder)
 *
 * Copyright © 2006 Keith Packard
 * Copyright 2010 Red Hat, Inc
 *
 */

static gboolean
check_all_screen_monitors(MetaMonitorInfo *monitors,
			  unsigned         n_monitors,
			  float            x,
			  float            y)
{
  unsigned int i;

  for (i = 0; i < n_monitors; i++)
    {
      MetaMonitorInfo *monitor = &monitors[i];
      int left, right, top, bottom;

      left = monitor->rect.x;
      right = left + monitor->rect.width;
      top = monitor->rect.y;
      bottom = left + monitor->rect.height;

      if ((x >= left) && (x < right) && (y >= top) && (y < bottom))
	return TRUE;
    }

  return FALSE;
}

static void
constrain_all_screen_monitors (ClutterInputDevice *device,
			       MetaMonitorInfo    *monitors,
			       unsigned            n_monitors,
			       float              *x,
			       float              *y)
{
  ClutterPoint current;
  unsigned int i;

  clutter_input_device_get_coords (device, NULL, &current);

  /* if we're trying to escape, clamp to the CRTC we're coming from */
  for (i = 0; i < n_monitors; i++)
    {
      MetaMonitorInfo *monitor = &monitors[i];
      int left, right, top, bottom;
      float nx, ny;

      left = monitor->rect.x;
      right = left + monitor->rect.width;
      top = monitor->rect.y;
      bottom = left + monitor->rect.height;

      nx = current.x;
      ny = current.y;

      if ((nx >= left) && (nx < right) && (ny >= top) && (ny < bottom))
	{
	  if (*x < left)
	    *x = left;
	  if (*x >= right)
	    *x = right - 1;
	  if (*y < top)
	    *y = top;
	  if (*y >= bottom)
	    *y = bottom - 1;

	  return;
        }
    }
}

static void
pointer_constrain_callback (ClutterInputDevice *device,
			    guint32             time,
			    float              *new_x,
			    float              *new_y,
			    gpointer            user_data)
{
  MetaMonitorManager *monitor_manager;
  MetaMonitorInfo *monitors;
  unsigned int n_monitors;
  gboolean ret;

  monitor_manager = meta_monitor_manager_get ();
  monitors = meta_monitor_manager_get_monitor_infos (monitor_manager, &n_monitors);

  /* if we're moving inside a monitor, we're fine */
  ret = check_all_screen_monitors(monitors, n_monitors, *new_x, *new_y);
  if (ret == TRUE)
    return;

  /* if we're trying to escape, clamp to the CRTC we're coming from */
  constrain_all_screen_monitors(device, monitors, n_monitors, new_x, new_y);
}

void
meta_wayland_pointer_init (MetaWaylandPointer *pointer,
                           struct wl_display  *display)
{
  ClutterDeviceManager *manager;

  memset (pointer, 0, sizeof *pointer);

  pointer->display = display;

  wl_list_init (&pointer->resource_list);
  wl_list_init (&pointer->focus_resource_list);

  pointer->focus_surface_listener.notify = pointer_handle_focus_surface_destroy;

  pointer->cursor_surface = NULL;
  pointer->cursor_surface_destroy_listener.notify = pointer_handle_cursor_surface_destroy;
  pointer->hotspot_x = 16;
  pointer->hotspot_y = 16;

  pointer->default_grab.interface = &default_pointer_grab_interface;
  pointer->default_grab.pointer = pointer;
  pointer->grab = &pointer->default_grab;

  manager = clutter_device_manager_get_default ();
  pointer->device = clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);

#if defined(CLUTTER_WINDOWING_EGL)
  /* XXX -- the evdev backend can be used regardless of the
   * windowing backend. To do this properly we need a Clutter
   * API to check the input backend. */
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL))
    {
      clutter_evdev_set_pointer_constrain_callback (manager, pointer_constrain_callback,
                                                    pointer, NULL);
    }
#endif
}

void
meta_wayland_pointer_release (MetaWaylandPointer *pointer)
{
  set_cursor_surface (pointer, NULL);
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
update_current_focus (MetaWaylandPointer *pointer,
                      MetaWaylandSurface *surface)
{
  pointer->current = surface;
  if (surface != pointer->focus_surface)
    {
      const MetaWaylandPointerGrabInterface *interface =
        pointer->grab->interface;
      interface->focus (pointer->grab, surface);
    }
}

static void
repick_for_event (MetaWaylandPointer *pointer,
                  const ClutterEvent *for_event)
{
  ClutterActor       *actor   = NULL;
  MetaWaylandSurface *surface = NULL;
  MetaDisplay        *display = meta_get_display ();

  if (meta_grab_op_should_block_wayland (display->grab_op))
    {
      update_current_focus (pointer, NULL);
      return;
    }

  if (for_event)
    {
      actor = clutter_event_get_source (for_event);
    }
  else
    {
      ClutterStage *stage = clutter_input_device_get_pointer_stage (pointer->device);

      if (stage)
        {
          ClutterPoint pos;

          clutter_input_device_get_coords (pointer->device, NULL, &pos);
          actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_REACTIVE,
                                                  pos.x, pos.y);
        }
    }

  if (META_IS_SURFACE_ACTOR_WAYLAND (actor))
    surface = meta_surface_actor_wayland_get_surface (META_SURFACE_ACTOR_WAYLAND (actor));

  update_current_focus (pointer, surface);
}

static void
notify_motion (MetaWaylandPointer *pointer,
               const ClutterEvent *event)
{
  repick_for_event (pointer, event);

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

  notify_motion (pointer, event);

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
  struct wl_list *l;
  wl_fixed_t x_value = 0, y_value = 0;

  notify_motion (pointer, event);

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

    default:
      break;
    }

  return FALSE;
}

void
meta_wayland_pointer_update (MetaWaylandPointer *pointer,
                             const ClutterEvent *event)
{
  pointer->button_count = count_buttons (event);

  if (pointer->cursor_tracker)
    {
      ClutterPoint pos;

      clutter_input_device_get_coords (pointer->device, NULL, &pos);
      meta_cursor_tracker_update_position (pointer->cursor_tracker, pos.x, pos.y);

      if (pointer->current == NULL)
	meta_cursor_tracker_unset_window_cursor (pointer->cursor_tracker);
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

void
meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface)
{
  if (pointer->focus_surface == surface && !wl_list_empty (&pointer->focus_resource_list))
    return;

  if (pointer->focus_surface)
    {
      struct wl_resource *resource;
      struct wl_list *l;

      l = &pointer->focus_resource_list;
      if (!wl_list_empty (l))
        {
          struct wl_client *client = wl_resource_get_client (pointer->focus_surface->resource);
          struct wl_display *display = wl_client_get_display (client);
          uint32_t serial = wl_display_next_serial (display);

          wl_resource_for_each (resource, l)
            {
              wl_pointer_send_leave (resource, serial, pointer->focus_surface->resource);
            }

          move_resources (&pointer->resource_list, &pointer->focus_resource_list);
        }

      wl_list_remove (&pointer->focus_surface_listener.link);
      pointer->focus_surface = NULL;
    }

  if (surface != NULL)
    {
      struct wl_resource *resource;
      struct wl_list *l;
      ClutterPoint pos;

      pointer->focus_surface = surface;
      wl_resource_add_destroy_listener (pointer->focus_surface->resource, &pointer->focus_surface_listener);

      clutter_input_device_get_coords (pointer->device, NULL, &pos);

      meta_window_handle_enter (pointer->focus_surface->window,
                                /* XXX -- can we reliably get a timestamp for setting focus? */
                                clutter_get_current_event_time (),
                                pos.x, pos.y);

      move_resources_for_client (&pointer->focus_resource_list,
                                 &pointer->resource_list,
                                 wl_resource_get_client (pointer->focus_surface->resource));

      l = &pointer->focus_resource_list;
      if (!wl_list_empty (l))
        {
          struct wl_client *client = wl_resource_get_client (pointer->focus_surface->resource);
          struct wl_display *display = wl_client_get_display (client);
          uint32_t serial = wl_display_next_serial (display);

          wl_resource_for_each (resource, l)
            {
              wl_fixed_t sx, sy;

              meta_wayland_pointer_get_relative_coordinates (pointer, pointer->focus_surface, &sx, &sy);
              wl_pointer_send_enter (resource, serial, pointer->focus_surface->resource, sx, sy);
            }

          pointer->focus_serial = serial;
        }
    }
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
}

typedef struct {
  MetaWaylandPointerGrab  generic;

  struct wl_client       *grab_client;
  struct wl_list          all_popups;
} MetaWaylandPopupGrab;

typedef struct {
  MetaWaylandPopupGrab *grab;
  MetaWaylandSurface   *surface;
  struct wl_listener    surface_destroy_listener;

  struct wl_list        link;
} MetaWaylandPopup;

static void
popup_grab_focus (MetaWaylandPointerGrab *grab,
		  MetaWaylandSurface     *surface)
{
  MetaWaylandPopupGrab *popup_grab = (MetaWaylandPopupGrab*)grab;

  /* Popup grabs are in owner-events mode (ie, events for the same client
     are reported as normal) */
  if (surface && wl_resource_get_client (surface->resource) == popup_grab->grab_client)
    meta_wayland_pointer_set_focus (grab->pointer, surface);
  else
    meta_wayland_pointer_set_focus (grab->pointer, NULL);
}

static void
popup_grab_motion (MetaWaylandPointerGrab *grab,
		   const ClutterEvent     *event)
{
  default_grab_motion (grab, event);
}

static void
popup_grab_button (MetaWaylandPointerGrab *grab,
		   const ClutterEvent     *event)
{
  MetaWaylandPointer *pointer = grab->pointer;

  if (pointer->focus_surface)
    default_grab_button (grab, event);
  else if (clutter_event_type (event) == CLUTTER_BUTTON_RELEASE &&
	   pointer->button_count == 0)
    meta_wayland_pointer_end_popup_grab (grab->pointer);
}

static MetaWaylandPointerGrabInterface popup_grab_interface = {
  popup_grab_focus,
  popup_grab_motion,
  popup_grab_button
};

static void
meta_wayland_pointer_end_popup_grab (MetaWaylandPointer *pointer)
{
  MetaWaylandPopupGrab *popup_grab;
  MetaWaylandPopup *popup, *tmp;

  popup_grab = (MetaWaylandPopupGrab*)pointer->grab;

  g_assert (popup_grab->generic.interface == &popup_grab_interface);

  wl_list_for_each_safe (popup, tmp, &popup_grab->all_popups, link)
    {
      meta_wayland_surface_popup_done (popup->surface);
      wl_list_remove (&popup->surface_destroy_listener.link);
      wl_list_remove (&popup->link);
      g_slice_free (MetaWaylandPopup, popup);
    }

  {
    MetaDisplay *display = meta_get_display ();
    meta_display_end_grab_op (display,
                              meta_display_get_current_time_roundtrip (display));
  }

  meta_wayland_pointer_end_grab (pointer);
  g_slice_free (MetaWaylandPopupGrab, popup_grab);
}

static void
on_popup_surface_destroy (struct wl_listener *listener,
			  void               *data)
{
  MetaWaylandPopup *popup =
    wl_container_of (listener, popup, surface_destroy_listener);
  MetaWaylandPopupGrab *popup_grab = popup->grab;

  wl_list_remove (&popup->link);
  g_slice_free (MetaWaylandPopup, popup);

  if (wl_list_empty (&popup_grab->all_popups))
    meta_wayland_pointer_end_popup_grab (popup_grab->generic.pointer);
}

gboolean
meta_wayland_pointer_start_popup_grab (MetaWaylandPointer *pointer,
				       MetaWaylandSurface *surface)
{
  MetaWaylandPopupGrab *grab;
  MetaWaylandPopup *popup;

  if (pointer->grab != &pointer->default_grab)
    {
      if (pointer->grab->interface != &popup_grab_interface)
        return FALSE;

      grab = (MetaWaylandPopupGrab*)pointer->grab;

      if (wl_resource_get_client (surface->resource) != grab->grab_client)
        return FALSE;
    }

  if (pointer->grab == &pointer->default_grab)
    {
      MetaWindow *window = surface->window;

      grab = g_slice_new0 (MetaWaylandPopupGrab);
      grab->generic.interface = &popup_grab_interface;
      grab->generic.pointer = pointer;
      grab->grab_client = wl_resource_get_client (surface->resource);
      wl_list_init (&grab->all_popups);

      meta_wayland_pointer_start_grab (pointer, (MetaWaylandPointerGrab*)grab);

      meta_display_begin_grab_op (window->display,
                                  window->screen,
                                  window,
                                  META_GRAB_OP_WAYLAND_CLIENT,
                                  FALSE, /* pointer_already_grabbed */
                                  FALSE, /* frame_action */
                                  1, /* button. XXX? */
                                  0, /* modmask */
                                  meta_display_get_current_time_roundtrip (window->display),
                                  pointer->grab_x,
                                  pointer->grab_y);
    }
  else
    grab = (MetaWaylandPopupGrab*)pointer->grab;

  popup = g_slice_new0 (MetaWaylandPopup);
  popup->grab = grab;
  popup->surface = surface;
  popup->surface_destroy_listener.notify = on_popup_surface_destroy;
  if (surface->xdg_popup.resource)
    wl_resource_add_destroy_listener (surface->xdg_popup.resource, &popup->surface_destroy_listener);
  else if (surface->wl_shell_surface.resource)
    wl_resource_add_destroy_listener (surface->wl_shell_surface.resource, &popup->surface_destroy_listener);

  wl_list_insert (&grab->all_popups, &popup->link);
  return TRUE;
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

  if (surface->window)
    {
      ClutterActor *actor =
        CLUTTER_ACTOR (meta_window_get_compositor_private (surface->window));

      if (actor)
        {
          ClutterPoint pos;
          clutter_input_device_get_coords (pointer->device, NULL, &pos);

          clutter_actor_transform_stage_point (actor, pos.x, pos.y, &xf, &yf);
        }
    }

  *sx = wl_fixed_from_double (xf);
  *sy = wl_fixed_from_double (yf);
}

void
meta_wayland_pointer_update_cursor_surface (MetaWaylandPointer *pointer)
{
  MetaCursorReference *cursor;

  if (pointer->cursor_tracker == NULL)
    return;

  if (pointer->cursor_surface && pointer->cursor_surface->buffer)
    {
      struct wl_resource *buffer = pointer->cursor_surface->buffer->resource;
      cursor = meta_cursor_reference_from_buffer (pointer->cursor_tracker,
                                                  buffer,
                                                  pointer->hotspot_x,
                                                  pointer->hotspot_y);
    }
  else
    cursor = NULL;

  meta_cursor_tracker_set_window_cursor (pointer->cursor_tracker, cursor);

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
  MetaWaylandPointer *pointer = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface;

  surface = (surface_resource ? wl_resource_get_user_data (surface_resource) : NULL);

  if (pointer->focus_surface == NULL)
    return;
  if (wl_resource_get_client (pointer->focus_surface->resource) != client)
    return;
  if (pointer->focus_serial - serial > G_MAXUINT32 / 2)
    return;

  pointer->hotspot_x = x;
  pointer->hotspot_y = y;
  set_cursor_surface (pointer, surface);
  meta_wayland_pointer_update_cursor_surface (pointer);
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

  cr = wl_resource_create (client, &wl_pointer_interface,
			   MIN (META_WL_POINTER_VERSION, wl_resource_get_version (seat_resource)), id);
  wl_resource_set_implementation (cr, &pointer_interface, pointer, unbind_resource);
  wl_list_insert (&pointer->resource_list, wl_resource_get_link (cr));

  if (pointer->focus_surface && wl_resource_get_client (pointer->focus_surface->resource) == client)
    meta_wayland_pointer_set_focus (pointer, pointer->focus_surface);
}
