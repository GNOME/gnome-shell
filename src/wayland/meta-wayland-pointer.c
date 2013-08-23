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

#include "meta-wayland-pointer.h"
#include "meta-wayland-private.h"

#include <string.h>

static MetaWaylandSeat *
meta_wayland_pointer_get_seat (MetaWaylandPointer *pointer)
{
  MetaWaylandSeat *seat = wl_container_of (pointer, seat, pointer);

  return seat;
}

static void
lose_pointer_focus (struct wl_listener *listener, void *data)
{
  MetaWaylandPointer *pointer =
    wl_container_of (listener, pointer, focus_listener);

  pointer->focus_resource = NULL;
  pointer->focus = NULL;
}

static void
default_grab_focus (MetaWaylandPointerGrab *grab,
                    MetaWaylandSurface *surface,
                    wl_fixed_t x,
                    wl_fixed_t y)
{
  MetaWaylandPointer *pointer = grab->pointer;

  if (pointer->button_count > 0)
    return;

  meta_wayland_pointer_set_focus (pointer, surface, x, y);
}

static void
default_grab_motion (MetaWaylandPointerGrab *grab,
                     uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
  struct wl_resource *resource;

  resource = grab->pointer->focus_resource;
  if (resource)
    wl_pointer_send_motion (resource, time, x, y);
}

static void
default_grab_button (MetaWaylandPointerGrab *grab,
                     uint32_t time, uint32_t button, uint32_t state_w)
{
  MetaWaylandPointer *pointer = grab->pointer;
  struct wl_resource *resource;
  uint32_t serial;
  enum wl_pointer_button_state state = state_w;

  resource = pointer->focus_resource;
  if (resource)
    {
      struct wl_client *client = wl_resource_get_client (resource);
      struct wl_display *display = wl_client_get_display (client);
      serial = wl_display_next_serial (display);
      wl_pointer_send_button (resource, serial, time, button, state_w);
    }

  if (pointer->button_count == 0 && state == WL_POINTER_BUTTON_STATE_RELEASED)
    meta_wayland_pointer_set_focus (pointer, pointer->current,
                                pointer->current_x, pointer->current_y);
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
			   gboolean            is_native)
{
  ClutterDeviceManager *manager;
  ClutterInputDevice *device;
  ClutterPoint current;

  memset (pointer, 0, sizeof *pointer);
  wl_list_init (&pointer->resource_list);
  pointer->focus_listener.notify = lose_pointer_focus;
  pointer->default_grab.interface = &default_pointer_grab_interface;
  pointer->default_grab.pointer = pointer;
  pointer->grab = &pointer->default_grab;
  wl_signal_init (&pointer->focus_signal);

  manager = clutter_device_manager_get_default ();
  device = clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);

  if (is_native)
    clutter_evdev_set_pointer_constrain_callback (manager, pointer_constrain_callback,
						  pointer, NULL);

  clutter_input_device_get_coords (device, NULL, &current);
  pointer->x = wl_fixed_from_double (current.x);
  pointer->y = wl_fixed_from_double (current.y);
}

void
meta_wayland_pointer_release (MetaWaylandPointer *pointer)
{
  /* XXX: What about pointer->resource_list? */
  if (pointer->focus_resource)
    wl_list_remove (&pointer->focus_listener.link);

  pointer->focus = NULL;
  pointer->focus_resource = NULL;
}

static struct wl_resource *
find_resource_for_surface (struct wl_list *list, MetaWaylandSurface *surface)
{
  struct wl_client *client;

  if (!surface)
    return NULL;

  g_assert (surface->resource);
  client = wl_resource_get_client (surface->resource);

  return wl_resource_find_for_client (list, client);
}

void
meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface,
                                wl_fixed_t sx, wl_fixed_t sy)
{
  MetaWaylandSeat *seat = meta_wayland_pointer_get_seat (pointer);
  MetaWaylandKeyboard *kbd = &seat->keyboard;
  struct wl_resource *resource, *kr;
  uint32_t serial;

  resource = pointer->focus_resource;
  if (resource && pointer->focus != surface)
    {
      struct wl_client *client = wl_resource_get_client (resource);
      struct wl_display *display = wl_client_get_display (client);
      serial = wl_display_next_serial (display);
      wl_pointer_send_leave (resource, serial, pointer->focus->resource);
      wl_list_remove (&pointer->focus_listener.link);
    }

  resource = find_resource_for_surface (&pointer->resource_list, surface);
  if (resource &&
      (pointer->focus != surface || pointer->focus_resource != resource))
    {
      struct wl_client *client = wl_resource_get_client (resource);
      struct wl_display *display = wl_client_get_display (client);
      serial = wl_display_next_serial (display);
      if (kbd)
        {
          kr = find_resource_for_surface (&kbd->resource_list, surface);
          if (kr)
            {
              wl_keyboard_send_modifiers (kr,
                                          serial,
                                          kbd->modifier_state.mods_depressed,
                                          kbd->modifier_state.mods_latched,
                                          kbd->modifier_state.mods_locked,
                                          kbd->modifier_state.group);
            }
        }
      wl_pointer_send_enter (resource, serial, surface->resource, sx, sy);
      wl_resource_add_destroy_listener (resource, &pointer->focus_listener);
      pointer->focus_serial = serial;
    }

  pointer->focus_resource = resource;
  pointer->focus = surface;
  pointer->default_grab.focus = surface;
  wl_signal_emit (&pointer->focus_signal, pointer);
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
    interface->focus (pointer->grab, pointer->current,
                      pointer->current_x, pointer->current_y);
}

void
meta_wayland_pointer_end_grab (MetaWaylandPointer *pointer)
{
  const MetaWaylandPointerGrabInterface *interface;

  pointer->grab = &pointer->default_grab;
  interface = pointer->grab->interface;
  interface->focus (pointer->grab, pointer->current,
                    pointer->current_x, pointer->current_y);
}

static void
current_surface_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandPointer *pointer =
    wl_container_of (listener, pointer, current_listener);

  pointer->current = NULL;
}

void
meta_wayland_pointer_set_current (MetaWaylandPointer *pointer,
                                  MetaWaylandSurface *surface)
{
  if (pointer->current)
    wl_list_remove (&pointer->current_listener.link);

  pointer->current = surface;

  if (!surface)
    return;

  wl_resource_add_destroy_listener (surface->resource,
                                    &pointer->current_listener);
  pointer->current_listener.notify = current_surface_destroy;
}

static void
modal_focus (MetaWaylandPointerGrab *grab,
	     MetaWaylandSurface     *surface,
	     wl_fixed_t              x,
	     wl_fixed_t              y)
{
}

static void
modal_motion (MetaWaylandPointerGrab *grab,
	      uint32_t                time,
	      wl_fixed_t              x,
	      wl_fixed_t              y)
{
}

static void
modal_button (MetaWaylandPointerGrab *grab,
	      uint32_t                time,
	      uint32_t                button,
	      uint32_t                state)
{
}

static MetaWaylandPointerGrabInterface modal_grab = {
  modal_focus,
  modal_motion,
  modal_button
};

gboolean
meta_wayland_pointer_begin_modal (MetaWaylandPointer *pointer)
{
  MetaWaylandPointerGrab *grab;

  if (pointer->grab != &pointer->default_grab)
    return FALSE;

  meta_wayland_pointer_set_focus (pointer, NULL,
				  wl_fixed_from_int (0),
				  wl_fixed_from_int (0));

  grab = g_slice_new0 (MetaWaylandPointerGrab);
  grab->interface = &modal_grab;
  meta_wayland_pointer_start_grab (pointer, grab);

  return TRUE;
}

void
meta_wayland_pointer_end_modal (MetaWaylandPointer *pointer)
{
  MetaWaylandPointerGrab *grab;

  grab = pointer->grab;

  g_assert (grab->interface == &modal_grab);

  meta_wayland_pointer_end_grab (pointer);
  g_slice_free (MetaWaylandPointerGrab, grab);
}

/* Called when the focused resource is destroyed */
void
meta_wayland_pointer_destroy_focus (MetaWaylandPointer *pointer)
{
  if (pointer->grab == &pointer->default_grab)
    {
      /* The surface was destroyed, but had the implicit pointer grab.
         Bypass the grab interface. */
      g_assert (pointer->button_count > 0);

      /* Note: we focus the NULL interface, not the current one, because
	 we have button down, and the clients would be confused if the
	 pointer enters the surface.
      */
      meta_wayland_pointer_set_focus (pointer, NULL, 0, 0);
    }
}
