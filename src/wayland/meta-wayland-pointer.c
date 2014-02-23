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
#include "xdg-shell-server-protocol.h"

#include <string.h>

static void meta_wayland_pointer_end_popup_grab (MetaWaylandPointer *pointer);

static void
pointer_handle_focus_surface_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandPointer *pointer = wl_container_of (listener, pointer, focus_surface_listener);

  wl_list_remove (&pointer->focus_surface_listener.link);
  pointer->focus_surface = NULL;

  if (pointer->focus_resource)
    {
      wl_list_remove (&pointer->focus_resource_listener.link);
      pointer->focus_resource = NULL;
    }
}

static void
pointer_handle_focus_resource_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandPointer *pointer = wl_container_of (listener, pointer, focus_resource_listener);

  wl_list_remove (&pointer->focus_resource_listener.link);
  pointer->focus_resource = NULL;
}

static void
default_grab_focus (MetaWaylandPointerGrab *grab,
                    MetaWaylandSurface     *surface,
		    const ClutterEvent     *event)
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
  struct wl_resource *resource;

  resource = grab->pointer->focus_resource;
  if (resource)
    {
      wl_fixed_t sx, sy;

      meta_wayland_pointer_get_relative_coordinates (grab->pointer,
						     grab->pointer->focus_surface,
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
  ClutterEventType event_type;

  event_type = clutter_event_type (event);

  resource = pointer->focus_resource;
  if (resource)
    {
      struct wl_client *client = wl_resource_get_client (resource);
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
meta_wayland_pointer_init (MetaWaylandPointer *pointer)
{
  ClutterDeviceManager *manager;
  ClutterInputDevice *device;
  ClutterPoint current;

  memset (pointer, 0, sizeof *pointer);
  wl_list_init (&pointer->resource_list);

  pointer->focus_surface_listener.notify = pointer_handle_focus_surface_destroy;
  pointer->focus_resource_listener.notify = pointer_handle_focus_resource_destroy;

  pointer->default_grab.interface = &default_pointer_grab_interface;
  pointer->default_grab.pointer = pointer;
  pointer->grab = &pointer->default_grab;

  manager = clutter_device_manager_get_default ();
  device = clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);

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

  clutter_input_device_get_coords (device, NULL, &current);
  pointer->x = wl_fixed_from_double (current.x);
  pointer->y = wl_fixed_from_double (current.y);
}

void
meta_wayland_pointer_release (MetaWaylandPointer *pointer)
{
  /* Do nothing. */
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
                                MetaWaylandSurface *surface)
{
  if (pointer->focus_surface == surface && pointer->focus_resource != NULL)
    return;

  if (pointer->focus_surface)
    {
      if (pointer->focus_resource)
        {
          if (pointer->focus_surface->resource)
            {
              struct wl_client *client = wl_resource_get_client (pointer->focus_resource);
              struct wl_display *display = wl_client_get_display (client);
              uint32_t serial = wl_display_next_serial (display);
              wl_pointer_send_leave (pointer->focus_resource, serial, pointer->focus_surface->resource);
            }

          wl_list_remove (&pointer->focus_resource_listener.link);
          pointer->focus_resource = NULL;
        }

      wl_list_remove (&pointer->focus_surface_listener.link);
      pointer->focus_surface = NULL;
    }

  if (surface != NULL)
    {
      pointer->focus_surface = surface;
      wl_resource_add_destroy_listener (pointer->focus_surface->resource, &pointer->focus_surface_listener);

      pointer->focus_resource = find_resource_for_surface (&pointer->resource_list, surface);
      if (pointer->focus_resource)
        {
          struct wl_client *client = wl_resource_get_client (pointer->focus_resource);
          struct wl_display *display = wl_client_get_display (client);
          uint32_t serial = wl_display_next_serial (display);

          meta_window_handle_enter (pointer->focus_surface->window,
                                    /* XXX -- can we reliably get a timestamp for setting focus? */
                                    clutter_get_current_event_time (),
                                    wl_fixed_to_int (pointer->x),
                                    wl_fixed_to_int (pointer->y));

          {
            wl_fixed_t sx, sy;

            meta_wayland_pointer_get_relative_coordinates (pointer, pointer->focus_surface, &sx, &sy);
            wl_pointer_send_enter (pointer->focus_resource, serial, pointer->focus_surface->resource, sx, sy);
          }

          wl_resource_add_destroy_listener (pointer->focus_resource, &pointer->focus_resource_listener);
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
    interface->focus (pointer->grab, pointer->current, NULL);
}

void
meta_wayland_pointer_end_grab (MetaWaylandPointer *pointer)
{
  const MetaWaylandPointerGrabInterface *interface;

  pointer->grab = &pointer->default_grab;
  interface = pointer->grab->interface;
  interface->focus (pointer->grab, pointer->current, NULL);
}

static void
modal_focus (MetaWaylandPointerGrab *grab,
	     MetaWaylandSurface     *surface,
	     const ClutterEvent     *event)
{
}

static void
modal_motion (MetaWaylandPointerGrab *grab,
	      const ClutterEvent     *event)
{
}

static void
modal_button (MetaWaylandPointerGrab *grab,
	      const ClutterEvent     *event)
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

  meta_wayland_pointer_set_focus (pointer, NULL);

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
		  MetaWaylandSurface     *surface,
		  const ClutterEvent     *event)
{
  MetaWaylandPopupGrab *popup_grab = (MetaWaylandPopupGrab*)grab;

  /* Popup grabs are in owner-events mode (ie, events for the same client
     are reported as normal) */
  if (surface && wl_resource_get_client (surface->resource) == popup_grab->grab_client)
    default_grab_focus (grab, surface, event);
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
  MetaWaylandPopupGrab *popup_grab = (MetaWaylandPopupGrab*)grab;
  MetaWaylandPointer *pointer = grab->pointer;

  if (pointer->focus_resource)
    {
      /* This is ensured by popup_grab_focus */
      g_assert (wl_resource_get_client (pointer->focus_resource) == popup_grab->grab_client);

      default_grab_button (grab, event);
    }
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
      MetaWaylandSurfaceExtension *xdg_popup = &popup->surface->xdg_popup;
      struct wl_client *client = wl_resource_get_client (xdg_popup->resource);
      struct wl_display *display = wl_client_get_display (client);
      uint32_t serial;

      serial = wl_display_next_serial (display);

      xdg_popup_send_popup_done (xdg_popup->resource, serial);
      wl_list_remove (&popup->surface_destroy_listener.link);
      wl_list_remove (&popup->link);
      g_slice_free (MetaWaylandPopup, popup);
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
      grab = g_slice_new0 (MetaWaylandPopupGrab);
      grab->generic.interface = &popup_grab_interface;
      grab->generic.pointer = pointer;
      grab->grab_client = wl_resource_get_client (surface->resource);
      wl_list_init (&grab->all_popups);

      meta_wayland_pointer_start_grab (pointer, (MetaWaylandPointerGrab*)grab);
    }
  else
    grab = (MetaWaylandPopupGrab*)pointer->grab;

  popup = g_slice_new0 (MetaWaylandPopup);
  popup->grab = grab;
  popup->surface = surface;
  popup->surface_destroy_listener.notify = on_popup_surface_destroy;
  wl_resource_add_destroy_listener (surface->xdg_popup.resource, &popup->surface_destroy_listener);

  wl_list_insert (&grab->all_popups, &popup->link);
      
  return TRUE;
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
        clutter_actor_transform_stage_point (actor,
                                             wl_fixed_to_double (pointer->x),
                                             wl_fixed_to_double (pointer->y),
                                             &xf, &yf);
    }

  *sx = wl_fixed_from_double (xf);
  *sy = wl_fixed_from_double (yf);
}
