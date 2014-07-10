/*
 * Copyright © 2011 Kristian Høgsberg
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

/* The file is based on src/data-device.c from Weston */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>

#include "meta-wayland-data-device.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-private.h"

typedef struct
{
  struct wl_resource *resource;
  MetaWaylandDataSource *source;
  struct wl_listener source_destroy_listener;
} MetaWaylandDataOffer;

struct _MetaWaylandDataSource
{
  struct wl_resource *resource;
  struct wl_array mime_types;
};

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
data_offer_accept (struct wl_client *client,
                   struct wl_resource *resource,
                   guint32 serial,
                   const char *mime_type)
{
  MetaWaylandDataOffer *offer = wl_resource_get_user_data (resource);

  /* FIXME: Check that client is currently focused by the input
   * device that is currently dragging this data source.  Should
   * this be a wl_data_device request? */

  if (offer->source)
    wl_data_source_send_target (offer->source->resource, mime_type);
}

static void
data_offer_receive (struct wl_client *client, struct wl_resource *resource,
                    const char *mime_type, int32_t fd)
{
  MetaWaylandDataOffer *offer = wl_resource_get_user_data (resource);

  if (offer->source)
    wl_data_source_send_send (offer->source->resource, mime_type, fd);

  close (fd);
}

static void
data_offer_destroy (struct wl_client *client, struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct wl_data_offer_interface data_offer_interface = {
  data_offer_accept,
  data_offer_receive,
  data_offer_destroy,
};

static void
destroy_data_offer (struct wl_resource *resource)
{
  MetaWaylandDataOffer *offer = wl_resource_get_user_data (resource);

  if (offer->source)
    wl_list_remove (&offer->source_destroy_listener.link);

  g_slice_free (MetaWaylandDataOffer, offer);
}

static void
destroy_offer_data_source (struct wl_listener *listener, void *data)
{
  MetaWaylandDataOffer *offer;

  offer = wl_container_of (listener, offer, source_destroy_listener);

  offer->source = NULL;
}

static struct wl_resource *
meta_wayland_data_source_send_offer (MetaWaylandDataSource *source,
                                     struct wl_resource *target)
{
  MetaWaylandDataOffer *offer = g_slice_new0 (MetaWaylandDataOffer);
  char **p;

  offer->source = source;
  offer->source_destroy_listener.notify = destroy_offer_data_source;

  offer->resource = wl_resource_create (wl_resource_get_client (target),
					&wl_data_offer_interface,
					MIN (META_WL_DATA_OFFER_VERSION,
					     wl_resource_get_version (target)), 0);
  wl_resource_set_implementation (offer->resource, &data_offer_interface,
				  offer, destroy_data_offer);
  wl_resource_add_destroy_listener (source->resource,
                                    &offer->source_destroy_listener);

  wl_data_device_send_data_offer (target, offer->resource);

  wl_array_for_each (p, &source->mime_types)
    wl_data_offer_send_offer (offer->resource, *p);

  return offer->resource;
}

static void
data_source_offer (struct wl_client *client,
                   struct wl_resource *resource, const char *type)
{
  MetaWaylandDataSource *source = wl_resource_get_user_data (resource);
  char **p;

  p = wl_array_add (&source->mime_types, sizeof *p);
  if (p)
    *p = strdup (type);
  if (!p || !*p)
    wl_resource_post_no_memory (resource);
}

static void
data_source_destroy (struct wl_client *client, struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static struct wl_data_source_interface data_source_interface = {
  data_source_offer,
  data_source_destroy
};

typedef struct {
  MetaWaylandPointerGrab  generic;

  MetaWaylandSeat        *seat;
  struct wl_client       *drag_client;

  MetaWaylandSurface     *drag_focus;
  struct wl_resource     *drag_focus_data_device;
  struct wl_listener      drag_focus_listener;

  MetaWaylandSurface     *drag_surface;
  struct wl_listener      drag_icon_listener;

  MetaWaylandDataSource  *drag_data_source;
  struct wl_listener      drag_data_source_listener;
} MetaWaylandDragGrab;

static void
destroy_drag_focus (struct wl_listener *listener, void *data)
{
  MetaWaylandDragGrab *grab = wl_container_of (listener, grab, drag_focus_listener);

  grab->drag_focus_data_device = NULL;
}

static void
drag_grab_focus (MetaWaylandPointerGrab *grab,
                 MetaWaylandSurface     *surface)
{
  MetaWaylandDragGrab *drag_grab = (MetaWaylandDragGrab*) grab;
  MetaWaylandSeat *seat = drag_grab->seat;
  struct wl_client *client;
  struct wl_resource *data_device_resource, *offer = NULL;
  struct wl_display *display;
  guint32 serial;
  wl_fixed_t sx, sy;

  if (drag_grab->drag_focus == surface)
    return;

  if (drag_grab->drag_focus_data_device)
    {
      wl_data_device_send_leave (drag_grab->drag_focus_data_device);
      wl_list_remove (&drag_grab->drag_focus_listener.link);
      drag_grab->drag_focus_data_device = NULL;
      drag_grab->drag_focus = NULL;
    }

  if (!surface)
    return;

  if (!drag_grab->drag_data_source &&
      wl_resource_get_client (surface->resource) != drag_grab->drag_client)
    return;

  client = wl_resource_get_client (surface->resource);

  data_device_resource = wl_resource_find_for_client (&seat->data_device.resource_list, client);
  if (!data_device_resource)
    return;

  display = wl_client_get_display (client);
  serial = wl_display_next_serial (display);

  if (drag_grab->drag_data_source)
    offer = meta_wayland_data_source_send_offer (drag_grab->drag_data_source,
                                                 data_device_resource);

  meta_wayland_pointer_get_relative_coordinates (grab->pointer, surface, &sx, &sy);
  wl_data_device_send_enter (data_device_resource, serial, surface->resource,
                             sx, sy, offer);

  drag_grab->drag_focus = surface;

  drag_grab->drag_focus_data_device = data_device_resource;
  drag_grab->drag_focus_listener.notify = destroy_drag_focus;
  wl_resource_add_destroy_listener (data_device_resource, &drag_grab->drag_focus_listener);
}

static void
drag_grab_motion (MetaWaylandPointerGrab *grab,
		  const ClutterEvent     *event)
{
  MetaWaylandDragGrab *drag_grab = (MetaWaylandDragGrab*) grab;
  wl_fixed_t sx, sy;

  if (drag_grab->drag_focus_data_device)
    {
      meta_wayland_pointer_get_relative_coordinates (grab->pointer,
						     drag_grab->drag_focus,
						     &sx, &sy);
      wl_data_device_send_motion (drag_grab->drag_focus_data_device,
				  clutter_event_get_time (event),
				  sx, sy);
    }
}

static void
data_device_end_drag_grab (MetaWaylandDragGrab *drag_grab)
{
  if (drag_grab->drag_surface)
    {
      drag_grab->drag_surface = NULL;
      wl_list_remove (&drag_grab->drag_icon_listener.link);
    }

  if (drag_grab->drag_data_source)
    wl_list_remove (&drag_grab->drag_data_source_listener.link);

  drag_grab_focus (&drag_grab->generic, NULL);

  meta_wayland_pointer_end_grab (drag_grab->generic.pointer);
  g_slice_free (MetaWaylandDragGrab, drag_grab);
}

static void
drag_grab_button (MetaWaylandPointerGrab *grab,
		  const ClutterEvent     *event)
{
  MetaWaylandDragGrab *drag_grab = (MetaWaylandDragGrab*) grab;
  MetaWaylandSeat *seat = drag_grab->seat;
  ClutterEventType event_type = clutter_event_type (event);

  if (drag_grab->drag_focus_data_device &&
      drag_grab->generic.pointer->grab_button == clutter_event_get_button (event) &&
      event_type == CLUTTER_BUTTON_RELEASE)
    wl_data_device_send_drop (drag_grab->drag_focus_data_device);

  if (seat->pointer.button_count == 0 &&
      event_type == CLUTTER_BUTTON_RELEASE)
    data_device_end_drag_grab (drag_grab);
}

static const MetaWaylandPointerGrabInterface drag_grab_interface = {
  drag_grab_focus,
  drag_grab_motion,
  drag_grab_button,
};

static void
destroy_data_device_source (struct wl_listener *listener, void *data)
{
  MetaWaylandDragGrab *drag_grab =
    wl_container_of (listener, drag_grab, drag_data_source_listener);

  drag_grab->drag_data_source = NULL;
  data_device_end_drag_grab (drag_grab);
}

static void
destroy_data_device_icon (struct wl_listener *listener, void *data)
{
  MetaWaylandDragGrab *drag_grab =
    wl_container_of (listener, drag_grab, drag_data_source_listener);

  drag_grab->drag_surface = NULL;
}

static void
data_device_start_drag (struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *source_resource,
                        struct wl_resource *origin_resource,
                        struct wl_resource *icon_resource, guint32 serial)
{
  MetaWaylandDataDevice *data_device = wl_resource_get_user_data (resource);
  MetaWaylandSeat *seat = wl_container_of (data_device, seat, data_device);
  MetaWaylandDragGrab *drag_grab;

  if ((seat->pointer.button_count == 0 ||
       seat->pointer.grab_serial != serial ||
       !seat->pointer.focus_surface ||
       seat->pointer.focus_surface != wl_resource_get_user_data (origin_resource)))
    return;

  /* FIXME: Check that the data source type array isn't empty. */

  if (seat->pointer.grab != &seat->pointer.default_grab)
    return;

  drag_grab = g_slice_new0 (MetaWaylandDragGrab);

  drag_grab->generic.interface = &drag_grab_interface;
  drag_grab->generic.pointer = &seat->pointer;

  drag_grab->drag_client = client;
  drag_grab->seat = seat;

  if (source_resource)
    {
      drag_grab->drag_data_source = wl_resource_get_user_data (source_resource);
      drag_grab->drag_data_source_listener.notify = destroy_data_device_source;
      wl_resource_add_destroy_listener (source_resource,
                                        &drag_grab->drag_data_source_listener);
    }

  if (icon_resource)
    {
      drag_grab->drag_surface = wl_resource_get_user_data (icon_resource);
      drag_grab->drag_icon_listener.notify = destroy_data_device_icon;
      wl_resource_add_destroy_listener (icon_resource,
                                        &drag_grab->drag_icon_listener);
    }

  meta_wayland_pointer_set_focus (&seat->pointer, NULL);
  meta_wayland_pointer_start_grab (&seat->pointer, (MetaWaylandPointerGrab*)drag_grab);
}

static void
destroy_selection_data_source (struct wl_listener *listener, void *data)
{
  MetaWaylandDataDevice *data_device = wl_container_of (listener, data_device, selection_data_source_listener);
  MetaWaylandSeat *seat = wl_container_of (data_device, seat, data_device);
  struct wl_resource *data_device_resource;
  struct wl_client *focus_client = NULL;

  data_device->selection_data_source = NULL;

  focus_client = meta_wayland_keyboard_get_focus_client (&seat->keyboard);
  if (focus_client)
    {
      data_device_resource = wl_resource_find_for_client (&data_device->resource_list, focus_client);
      if (data_device_resource)
        wl_data_device_send_selection (data_device_resource, NULL);
    }
}

static void
meta_wayland_data_device_set_selection (MetaWaylandDataDevice *data_device,
                                        MetaWaylandDataSource *source,
                                        guint32 serial)
{
  MetaWaylandSeat *seat = wl_container_of (data_device, seat, data_device);
  struct wl_resource *data_device_resource, *offer;
  struct wl_client *focus_client;

  if (data_device->selection_data_source &&
      data_device->selection_serial - serial < UINT32_MAX / 2)
    return;

  if (data_device->selection_data_source)
    {
      wl_data_source_send_cancelled (data_device->selection_data_source->resource);
      wl_list_remove (&data_device->selection_data_source_listener.link);
      data_device->selection_data_source = NULL;
    }

  data_device->selection_data_source = source;
  data_device->selection_serial = serial;

  focus_client = meta_wayland_keyboard_get_focus_client (&seat->keyboard);
  if (focus_client)
    {
      data_device_resource = wl_resource_find_for_client (&data_device->resource_list, focus_client);
      if (data_device_resource)
        {
          if (data_device->selection_data_source)
            {
              offer = meta_wayland_data_source_send_offer (data_device->selection_data_source, data_device_resource);
              wl_data_device_send_selection (data_device_resource, offer);
            }
          else
            {
              wl_data_device_send_selection (data_device_resource, NULL);
            }
        }
    }

  if (source)
    {
      data_device->selection_data_source_listener.notify = destroy_selection_data_source;
      wl_resource_add_destroy_listener (source->resource, &data_device->selection_data_source_listener);
    }
}

static void
data_device_set_selection (struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *source_resource,
                           guint32 serial)
{
  MetaWaylandDataDevice *data_device = wl_resource_get_user_data (resource);
  MetaWaylandDataSource *source;

  if (!source_resource)
    return;

  source = wl_resource_get_user_data (source_resource);

  /* FIXME: Store serial and check against incoming serial here. */
  meta_wayland_data_device_set_selection (data_device, source, serial);
}

static const struct wl_data_device_interface data_device_interface = {
  data_device_start_drag,
  data_device_set_selection,
};

static void
destroy_data_source (struct wl_resource *resource)
{
  MetaWaylandDataSource *source = wl_resource_get_user_data (resource);
  char **p;

  wl_array_for_each (p, &source->mime_types) free (*p);

  wl_array_release (&source->mime_types);

  g_slice_free (MetaWaylandDataSource, source);
}

static void
create_data_source (struct wl_client *client,
                    struct wl_resource *resource, guint32 id)
{
  MetaWaylandDataSource *source = g_slice_new0 (MetaWaylandDataSource);

  source->resource = wl_resource_create (client, &wl_data_source_interface,
					 MIN (META_WL_DATA_SOURCE_VERSION,
					      wl_resource_get_version (resource)), id);
  wl_resource_set_implementation (source->resource, &data_source_interface,
				  source, destroy_data_source);

  wl_array_init (&source->mime_types);
}

static void
get_data_device (struct wl_client *client,
                 struct wl_resource *manager_resource,
                 guint32 id, struct wl_resource *seat_resource)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  struct wl_resource *cr;

  cr = wl_resource_create (client, &wl_data_device_interface,
                           MIN (META_WL_DATA_DEVICE_VERSION, wl_resource_get_version (manager_resource)), id);
  wl_resource_set_implementation (cr, &data_device_interface, &seat->data_device, unbind_resource);
  wl_list_insert (&seat->data_device.resource_list, wl_resource_get_link (cr));
}

static const struct wl_data_device_manager_interface manager_interface = {
  create_data_source,
  get_data_device
};

static void
bind_manager (struct wl_client *client,
              void *data, guint32 version, guint32 id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_data_device_manager_interface,
				 MIN (version, META_WL_DATA_DEVICE_MANAGER_VERSION), id);
  wl_resource_set_implementation (resource, &manager_interface, NULL, NULL);
}

void
meta_wayland_data_device_manager_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
			&wl_data_device_manager_interface,
			META_WL_DATA_DEVICE_MANAGER_VERSION,
			NULL, bind_manager) == NULL)
    g_error ("Could not create data_device");
}

void
meta_wayland_data_device_init (MetaWaylandDataDevice *data_device)
{
  wl_list_init (&data_device->resource_list);
}

void
meta_wayland_data_device_set_keyboard_focus (MetaWaylandDataDevice *data_device)
{
  MetaWaylandSeat *seat = wl_container_of (data_device, seat, data_device);
  struct wl_client *focus_client;
  struct wl_resource *data_device_resource, *offer;
  MetaWaylandDataSource *source;

  focus_client = meta_wayland_keyboard_get_focus_client (&seat->keyboard);
  if (!focus_client)
    return;

  data_device_resource = wl_resource_find_for_client (&data_device->resource_list, focus_client);
  if (!data_device_resource)
    return;

  source = data_device->selection_data_source;
  if (source)
    {
      offer = meta_wayland_data_source_send_offer (source, data_device_resource);
      wl_data_device_send_selection (data_device_resource, offer);
    }
}
