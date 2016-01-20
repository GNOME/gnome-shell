/*
 * Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
 * Copyright (C) 2015 Red Hat, Inc.
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

#include "config.h"

#include "meta-wayland-popup.h"

#include "meta-wayland-pointer.h"
#include "meta-wayland-private.h"
#include "meta-wayland-surface.h"

struct _MetaWaylandPopupGrab
{
  MetaWaylandPointerGrab  generic;

  struct wl_client       *grab_client;
  struct wl_list          all_popups;
};

struct _MetaWaylandPopup
{
  MetaWaylandPopupGrab *grab;
  MetaWaylandSurface   *surface;
  struct wl_listener    surface_destroy_listener;
  struct wl_signal      destroy_signal;

  struct wl_list        link;
};

static void
meta_wayland_popup_grab_begin (MetaWaylandPopupGrab *grab,
                               MetaWaylandSurface   *surface);

static void
meta_wayland_popup_grab_end (MetaWaylandPopupGrab *grab);

static void
popup_grab_focus (MetaWaylandPointerGrab *grab,
		  MetaWaylandSurface     *surface)
{
  MetaWaylandPopupGrab *popup_grab = (MetaWaylandPopupGrab*)grab;

  /* Popup grabs are in owner-events mode (ie, events for the same client
     are reported as normal) */
  if (surface &&
      wl_resource_get_client (surface->resource) == popup_grab->grab_client)
    meta_wayland_pointer_set_focus (grab->pointer, surface);
  else
    meta_wayland_pointer_set_focus (grab->pointer, NULL);
}

static void
popup_grab_motion (MetaWaylandPointerGrab *grab,
		   const ClutterEvent     *event)
{
  meta_wayland_pointer_send_motion (grab->pointer, event);
}

static void
popup_grab_button (MetaWaylandPointerGrab *grab,
		   const ClutterEvent     *event)
{
  MetaWaylandPointer *pointer = grab->pointer;

  if (pointer->focus_surface)
    meta_wayland_pointer_send_button (grab->pointer, event);
  else if (clutter_event_type (event) == CLUTTER_BUTTON_RELEASE &&
	   pointer->button_count == 0)
    meta_wayland_pointer_end_popup_grab (grab->pointer);
}

static MetaWaylandPointerGrabInterface popup_grab_interface = {
  popup_grab_focus,
  popup_grab_motion,
  popup_grab_button
};

MetaWaylandPopupGrab *
meta_wayland_popup_grab_create (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface)
{
  struct wl_client *client = wl_resource_get_client (surface->resource);
  MetaWaylandPopupGrab *grab;

  grab = g_slice_new0 (MetaWaylandPopupGrab);
  grab->generic.interface = &popup_grab_interface;
  grab->generic.pointer = pointer;
  grab->grab_client = client;
  wl_list_init (&grab->all_popups);

  meta_wayland_popup_grab_begin (grab, surface);

  return grab;
}

void
meta_wayland_popup_grab_destroy (MetaWaylandPopupGrab *grab)
{
  meta_wayland_popup_grab_end (grab);
  g_slice_free (MetaWaylandPopupGrab, grab);
}

static void
meta_wayland_popup_grab_begin (MetaWaylandPopupGrab *grab,
                               MetaWaylandSurface   *surface)
{
  MetaWaylandPointer *pointer = grab->generic.pointer;
  MetaWindow *window = surface->window;

  meta_wayland_pointer_start_grab (pointer, (MetaWaylandPointerGrab*)grab);
  meta_display_begin_grab_op (window->display,
                              window->screen,
                              window,
                              META_GRAB_OP_WAYLAND_POPUP,
                              FALSE, /* pointer_already_grabbed */
                              FALSE, /* frame_action */
                              1, /* button. XXX? */
                              0, /* modmask */
                              meta_display_get_current_time_roundtrip (
                                window->display),
                              pointer->grab_x,
                              pointer->grab_y);
}

void
meta_wayland_popup_grab_end (MetaWaylandPopupGrab *grab)
{
  MetaWaylandPopup *popup, *tmp;

  g_assert (grab->generic.interface == &popup_grab_interface);

  wl_list_for_each_safe (popup, tmp, &grab->all_popups, link)
    {
      meta_wayland_surface_popup_done (popup->surface);
      meta_wayland_popup_destroy (popup);
    }

  {
    MetaDisplay *display = meta_get_display ();
    meta_display_end_grab_op (display,
                              meta_display_get_current_time_roundtrip (display));
  }

  meta_wayland_pointer_end_grab (grab->generic.pointer);
}

MetaWaylandSurface *
meta_wayland_popup_grab_get_top_popup (MetaWaylandPopupGrab *grab)
{
  MetaWaylandPopup *popup;

  g_assert (!wl_list_empty (&grab->all_popups));
  popup = wl_container_of (grab->all_popups.next, popup, link);

  return popup->surface;
}

gboolean
meta_wayland_pointer_grab_is_popup_grab (MetaWaylandPointerGrab *grab)
{
  return grab->interface == &popup_grab_interface;
}

void
meta_wayland_popup_destroy (MetaWaylandPopup *popup)
{
  wl_signal_emit (&popup->destroy_signal, popup);

  wl_list_remove (&popup->surface_destroy_listener.link);
  wl_list_remove (&popup->link);
  g_slice_free (MetaWaylandPopup, popup);
}

void
meta_wayland_popup_dismiss (MetaWaylandPopup *popup)
{
  MetaWaylandPopupGrab *popup_grab = popup->grab;

  meta_wayland_popup_destroy (popup);

  if (wl_list_empty (&popup_grab->all_popups))
    meta_wayland_pointer_end_popup_grab (popup_grab->generic.pointer);
}

MetaWaylandSurface *
meta_wayland_popup_get_top_popup (MetaWaylandPopup *popup)
{
  return meta_wayland_popup_grab_get_top_popup (popup->grab);
}

struct wl_signal *
meta_wayland_popup_get_destroy_signal (MetaWaylandPopup *popup)
{
  return &popup->destroy_signal;
}

static void
on_popup_surface_destroy (struct wl_listener *listener,
			  void               *data)
{
  MetaWaylandPopup *popup =
    wl_container_of (listener, popup, surface_destroy_listener);

  meta_wayland_popup_dismiss (popup);
}

MetaWaylandPopup *
meta_wayland_popup_create (MetaWaylandSurface   *surface,
                           MetaWaylandPopupGrab *grab)
{
  MetaWaylandPopup *popup;

  /* Don't allow creating popups if the grab has a different client. */
  if (grab->grab_client != wl_resource_get_client (surface->resource))
    return NULL;

  popup = g_slice_new0 (MetaWaylandPopup);
  popup->grab = grab;
  popup->surface = surface;
  popup->surface_destroy_listener.notify = on_popup_surface_destroy;
  wl_signal_init (&popup->destroy_signal);

  if (surface->xdg_popup)
    {
      wl_resource_add_destroy_listener (surface->xdg_popup,
                                        &popup->surface_destroy_listener);
    }
  else if (surface->wl_shell_surface)
    {
      wl_resource_add_destroy_listener (surface->wl_shell_surface,
                                        &popup->surface_destroy_listener);
    }

  wl_list_insert (&grab->all_popups, &popup->link);

  return popup;
}
