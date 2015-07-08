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
#include "meta-wayland-data-device-private.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-private.h"
#include "meta-dnd-actor-private.h"

struct _MetaWaylandDataOffer
{
  struct wl_resource *resource;
  MetaWaylandDataSource *source;
  struct wl_listener source_destroy_listener;
};

typedef struct _MetaWaylandDataSourcePrivate
{
  struct wl_array mime_types;
  gboolean has_target;
} MetaWaylandDataSourcePrivate;

typedef struct _MetaWaylandDataSourceWayland
{
  MetaWaylandDataSource parent;

  struct wl_resource *resource;
} MetaWaylandDataSourceWayland;

GType meta_wayland_data_source_wayland_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandDataSource, meta_wayland_data_source,
                            G_TYPE_OBJECT);
G_DEFINE_TYPE (MetaWaylandDataSourceWayland, meta_wayland_data_source_wayland,
               META_TYPE_WAYLAND_DATA_SOURCE);

static MetaWaylandDataSource *
meta_wayland_data_source_wayland_new (struct wl_resource *resource);

static void
drag_grab_data_source_destroyed (gpointer data, GObject *where_the_object_was);

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
meta_wayland_data_source_target (MetaWaylandDataSource *source,
                                 const char *mime_type)
{
  META_WAYLAND_DATA_SOURCE_GET_CLASS (source)->target (source, mime_type);
}

void
meta_wayland_data_source_send (MetaWaylandDataSource *source,
                               const char *mime_type,
                               int fd)
{
  META_WAYLAND_DATA_SOURCE_GET_CLASS (source)->send (source, mime_type, fd);
}

gboolean
meta_wayland_data_source_has_target (MetaWaylandDataSource *source)
{
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);

  return priv->has_target;
}

void
meta_wayland_data_source_set_has_target (MetaWaylandDataSource *source,
                                         gboolean has_target)
{
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);

  priv->has_target = has_target;
}

struct wl_array *
meta_wayland_data_source_get_mime_types (const MetaWaylandDataSource *source)
{
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);

  return &priv->mime_types;
}

static void
meta_wayland_data_source_cancel (MetaWaylandDataSource *source)
{
  META_WAYLAND_DATA_SOURCE_GET_CLASS (source)->cancel (source);
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
    {
      meta_wayland_data_source_target (offer->source, mime_type);
      meta_wayland_data_source_set_has_target (offer->source,
                                               mime_type != NULL);
    }
}

static void
data_offer_receive (struct wl_client *client, struct wl_resource *resource,
                    const char *mime_type, int32_t fd)
{
  MetaWaylandDataOffer *offer = wl_resource_get_user_data (resource);

  if (offer->source)
    meta_wayland_data_source_send (offer->source, mime_type, fd);
  else
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
    g_object_remove_weak_pointer (G_OBJECT (offer->source),
                                  (gpointer *)&offer->source);

  g_slice_free (MetaWaylandDataOffer, offer);
}

static struct wl_resource *
meta_wayland_data_source_send_offer (MetaWaylandDataSource *source,
                                     struct wl_resource *target)
{
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);
  MetaWaylandDataOffer *offer = g_slice_new0 (MetaWaylandDataOffer);
  char **p;

  offer->source = source;
  g_object_add_weak_pointer (G_OBJECT (source), (gpointer *)&offer->source);
  offer->resource = wl_resource_create (wl_resource_get_client (target),
                                        &wl_data_offer_interface,
                                        wl_resource_get_version (target), 0);
  wl_resource_set_implementation (offer->resource,
                                  &data_offer_interface,
                                  offer,
                                  destroy_data_offer);

  wl_data_device_send_data_offer (target, offer->resource);

  wl_array_for_each (p, &priv->mime_types)
    wl_data_offer_send_offer (offer->resource, *p);

  return offer->resource;
}

static void
data_source_offer (struct wl_client *client,
                   struct wl_resource *resource, const char *type)
{
  MetaWaylandDataSource *source = wl_resource_get_user_data (resource);

  if (!meta_wayland_data_source_add_mime_type (source, type))
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

struct _MetaWaylandDragGrab {
  MetaWaylandPointerGrab  generic;

  MetaWaylandSeat        *seat;
  struct wl_client       *drag_client;

  MetaWaylandSurface     *drag_focus;
  struct wl_resource     *drag_focus_data_device;
  struct wl_listener      drag_focus_listener;

  MetaWaylandSurface     *drag_surface;
  struct wl_listener      drag_icon_listener;

  MetaWaylandDataSource  *drag_data_source;

  ClutterActor           *feedback_actor;

  MetaWaylandSurface     *drag_origin;
  struct wl_listener      drag_origin_listener;

  int                     drag_start_x, drag_start_y;
};

static void
destroy_drag_focus (struct wl_listener *listener, void *data)
{
  MetaWaylandDragGrab *grab = wl_container_of (listener, grab, drag_focus_listener);

  grab->drag_focus_data_device = NULL;
}

void
meta_wayland_drag_grab_set_focus (MetaWaylandDragGrab *drag_grab,
                                  MetaWaylandSurface  *surface)
{
  MetaWaylandSeat *seat = drag_grab->seat;
  struct wl_client *client;
  struct wl_resource *data_device_resource, *offer = NULL;

  if (drag_grab->drag_focus == surface)
    return;

  if (drag_grab->drag_focus)
    {
      meta_wayland_surface_drag_dest_focus_out (drag_grab->drag_focus);
      drag_grab->drag_focus = NULL;
    }

  if (!surface)
    return;

  if (!drag_grab->drag_data_source &&
      wl_resource_get_client (surface->resource) != drag_grab->drag_client)
    return;

  client = wl_resource_get_client (surface->resource);

  data_device_resource = wl_resource_find_for_client (&seat->data_device.resource_list, client);

  if (drag_grab->drag_data_source && data_device_resource)
    offer = meta_wayland_data_source_send_offer (drag_grab->drag_data_source,
                                                 data_device_resource);

  drag_grab->drag_focus = surface;
  drag_grab->drag_focus_data_device = data_device_resource;

  meta_wayland_surface_drag_dest_focus_in (drag_grab->drag_focus,
                                           offer ? wl_resource_get_user_data (offer) : NULL);
}

MetaWaylandSurface *
meta_wayland_drag_grab_get_focus (MetaWaylandDragGrab *drag_grab)
{
  return drag_grab->drag_focus;
}

static void
drag_grab_focus (MetaWaylandPointerGrab *grab,
                 MetaWaylandSurface     *surface)
{
  MetaWaylandDragGrab *drag_grab = (MetaWaylandDragGrab*) grab;

  meta_wayland_drag_grab_set_focus (drag_grab, surface);
}

static void
drag_grab_motion (MetaWaylandPointerGrab *grab,
		  const ClutterEvent     *event)
{
  MetaWaylandDragGrab *drag_grab = (MetaWaylandDragGrab*) grab;

  if (drag_grab->drag_focus)
    meta_wayland_surface_drag_dest_motion (drag_grab->drag_focus, event);

  if (drag_grab->drag_surface)
    meta_feedback_actor_update (META_FEEDBACK_ACTOR (drag_grab->feedback_actor),
                                event);
}

static void
data_device_end_drag_grab (MetaWaylandDragGrab *drag_grab)
{
  meta_wayland_drag_grab_set_focus (drag_grab, NULL);

  if (drag_grab->drag_origin)
    {
      drag_grab->drag_origin = NULL;
      wl_list_remove (&drag_grab->drag_origin_listener.link);
    }

  if (drag_grab->drag_surface)
    {
      drag_grab->drag_surface = NULL;
      wl_list_remove (&drag_grab->drag_icon_listener.link);
    }

  if (drag_grab->drag_data_source)
    g_object_weak_unref (G_OBJECT (drag_grab->drag_data_source),
                         drag_grab_data_source_destroyed,
                         drag_grab);

  if (drag_grab->feedback_actor)
    {
      clutter_actor_remove_all_children (drag_grab->feedback_actor);
      clutter_actor_destroy (drag_grab->feedback_actor);
    }

  drag_grab->seat->data_device.current_grab = NULL;

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

  if (drag_grab->generic.pointer->grab_button == clutter_event_get_button (event) &&
      event_type == CLUTTER_BUTTON_RELEASE)
    {
      gboolean success = FALSE;

      if (meta_wayland_data_source_has_target (drag_grab->drag_data_source))
        {
          meta_wayland_surface_drag_dest_drop (drag_grab->drag_focus);
          success = TRUE;
        }

      /* Finish drag and let actor self-destruct */
      meta_dnd_actor_drag_finish (META_DND_ACTOR (drag_grab->feedback_actor),
                                  success);
      drag_grab->feedback_actor = NULL;
    }

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
destroy_data_device_origin (struct wl_listener *listener, void *data)
{
  MetaWaylandDragGrab *drag_grab =
    wl_container_of (listener, drag_grab, drag_origin_listener);

  drag_grab->drag_origin = NULL;
  data_device_end_drag_grab (drag_grab);
  meta_wayland_data_device_set_dnd_source (&drag_grab->seat->data_device, NULL);
}

static void
drag_grab_data_source_destroyed (gpointer data, GObject *where_the_object_was)
{
  MetaWaylandDragGrab *drag_grab = data;

  drag_grab->drag_data_source = NULL;
  data_device_end_drag_grab (drag_grab);
  meta_wayland_data_device_set_dnd_source (&drag_grab->seat->data_device, NULL);
}

static void
destroy_data_device_icon (struct wl_listener *listener, void *data)
{
  MetaWaylandDragGrab *drag_grab =
    wl_container_of (listener, drag_grab, drag_icon_listener);

  drag_grab->drag_surface = NULL;

  if (drag_grab->feedback_actor)
    clutter_actor_remove_all_children (drag_grab->feedback_actor);
}

void
meta_wayland_data_device_start_drag (MetaWaylandDataDevice                 *data_device,
                                     struct wl_client                      *client,
                                     const MetaWaylandPointerGrabInterface *funcs,
                                     MetaWaylandSurface                    *surface,
                                     MetaWaylandDataSource                 *source,
                                     MetaWaylandSurface                    *icon_surface)
{
  MetaWaylandSeat *seat = wl_container_of (data_device, seat, data_device);
  MetaWaylandDragGrab *drag_grab;
  ClutterPoint pos, stage_pos;

  data_device->current_grab = drag_grab = g_slice_new0 (MetaWaylandDragGrab);

  drag_grab->generic.interface = funcs;
  drag_grab->generic.pointer = &seat->pointer;

  drag_grab->drag_client = client;
  drag_grab->seat = seat;

  drag_grab->drag_origin = surface;
  drag_grab->drag_origin_listener.notify = destroy_data_device_origin;
  wl_resource_add_destroy_listener (surface->resource,
                                    &drag_grab->drag_origin_listener);

  clutter_input_device_get_coords (seat->pointer.device, NULL, &pos);
  clutter_actor_transform_stage_point (CLUTTER_ACTOR (meta_surface_actor_get_texture (surface->surface_actor)),
                                       pos.x, pos.y, &stage_pos.x, &stage_pos.y);
  drag_grab->drag_start_x = stage_pos.x;
  drag_grab->drag_start_y = stage_pos.y;

  g_object_weak_ref (G_OBJECT (source),
                     drag_grab_data_source_destroyed,
                     drag_grab);

  drag_grab->drag_data_source = source;
  meta_wayland_data_device_set_dnd_source (data_device,
                                           drag_grab->drag_data_source);

  if (icon_surface)
    {
      drag_grab->drag_surface = icon_surface;

      drag_grab->drag_icon_listener.notify = destroy_data_device_icon;
      wl_resource_add_destroy_listener (icon_surface->resource,
                                        &drag_grab->drag_icon_listener);

      drag_grab->feedback_actor = meta_dnd_actor_new (CLUTTER_ACTOR (drag_grab->drag_origin->surface_actor),
                                                      drag_grab->drag_start_x,
                                                      drag_grab->drag_start_y);
      meta_feedback_actor_set_anchor (META_FEEDBACK_ACTOR (drag_grab->feedback_actor),
                                      -drag_grab->drag_surface->offset_x,
                                      -drag_grab->drag_surface->offset_y);
      clutter_actor_add_child (drag_grab->feedback_actor,
                               CLUTTER_ACTOR (drag_grab->drag_surface->surface_actor));

      meta_feedback_actor_set_position (META_FEEDBACK_ACTOR (drag_grab->feedback_actor),
                                        pos.x, pos.y);
    }

  meta_wayland_pointer_start_grab (&seat->pointer, (MetaWaylandPointerGrab*) drag_grab);
}

void
meta_wayland_data_device_end_drag (MetaWaylandDataDevice *data_device)
{
  if (data_device->current_grab)
    data_device_end_drag_grab (data_device->current_grab);
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
  MetaWaylandSurface *surface = NULL, *icon_surface = NULL;
  MetaWaylandDataSource *drag_source = NULL;

  if (origin_resource)
    surface = wl_resource_get_user_data (origin_resource);

  if (!surface)
    return;

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      !seat->pointer.focus_surface ||
      seat->pointer.focus_surface != surface)
    return;

  /* FIXME: Check that the data source type array isn't empty. */

  if (data_device->current_grab ||
      seat->pointer.grab != &seat->pointer.default_grab)
    return;

  if (icon_resource)
    icon_surface = wl_resource_get_user_data (icon_resource);
  if (source_resource)
    drag_source = wl_resource_get_user_data (source_resource);

  if (icon_resource &&
      !meta_wayland_surface_assign_role (icon_surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_DND))
    {
      wl_resource_post_error (resource, WL_DATA_DEVICE_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (icon_resource));
      return;
    }

  meta_wayland_pointer_set_focus (&seat->pointer, NULL);
  meta_wayland_data_device_start_drag (data_device, client,
                                       &drag_grab_interface,
                                       surface, drag_source, icon_surface);
}

static void
selection_data_source_destroyed (gpointer data, GObject *object_was_here)
{
  MetaWaylandDataDevice *data_device = data;
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
meta_wayland_source_send (MetaWaylandDataSource *source,
                          const gchar           *mime_type,
                          gint                   fd)
{
  MetaWaylandDataSourceWayland *source_wayland =
    META_WAYLAND_DATA_SOURCE_WAYLAND (source);

  wl_data_source_send_send (source_wayland->resource, mime_type, fd);
  close (fd);
}

static void
meta_wayland_source_target (MetaWaylandDataSource *source,
                            const gchar           *mime_type)
{
  MetaWaylandDataSourceWayland *source_wayland =
    META_WAYLAND_DATA_SOURCE_WAYLAND (source);

  wl_data_source_send_target (source_wayland->resource, mime_type);
}

static void
meta_wayland_source_cancel (MetaWaylandDataSource *source)
{
  MetaWaylandDataSourceWayland *source_wayland =
    META_WAYLAND_DATA_SOURCE_WAYLAND (source);

  wl_data_source_send_cancelled (source_wayland->resource);
}

static void
meta_wayland_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (meta_wayland_data_source_parent_class)->finalize (object);
}

static void
meta_wayland_data_source_wayland_init (MetaWaylandDataSourceWayland *source_wayland)
{
}

static void
meta_wayland_data_source_wayland_class_init (MetaWaylandDataSourceWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaWaylandDataSourceClass *data_source_class =
    META_WAYLAND_DATA_SOURCE_CLASS (klass);

  object_class->finalize = meta_wayland_source_finalize;

  data_source_class->send = meta_wayland_source_send;
  data_source_class->target = meta_wayland_source_target;
  data_source_class->cancel = meta_wayland_source_cancel;
}

static void
meta_wayland_data_source_finalize (GObject *object)
{
  MetaWaylandDataSource *source = META_WAYLAND_DATA_SOURCE (object);
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);
  char **pos;

  wl_array_for_each (pos, &priv->mime_types)
    g_free (*pos);
  wl_array_release (&priv->mime_types);

  G_OBJECT_CLASS (meta_wayland_data_source_parent_class)->finalize (object);
}

static void
meta_wayland_data_source_init (MetaWaylandDataSource *source)
{
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);

  wl_array_init (&priv->mime_types);
}

static void
meta_wayland_data_source_class_init (MetaWaylandDataSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_data_source_finalize;
}

static void
meta_wayland_drag_dest_focus_in (MetaWaylandDataDevice *data_device,
                                 MetaWaylandSurface    *surface,
                                 MetaWaylandDataOffer  *offer)
{
  MetaWaylandDragGrab *grab = data_device->current_grab;
  struct wl_display *display;
  struct wl_client *client;
  wl_fixed_t sx, sy;

  if (!grab->drag_focus_data_device)
    return;

  client = wl_resource_get_client (surface->resource);
  display = wl_client_get_display (client);

  grab->drag_focus_listener.notify = destroy_drag_focus;
  wl_resource_add_destroy_listener (grab->drag_focus_data_device,
                                    &grab->drag_focus_listener);

  meta_wayland_pointer_get_relative_coordinates (grab->generic.pointer,
                                                 surface, &sx, &sy);
  wl_data_device_send_enter (grab->drag_focus_data_device,
                             wl_display_next_serial (display),
                             surface->resource, sx, sy, offer->resource);
}

static void
meta_wayland_drag_dest_focus_out (MetaWaylandDataDevice *data_device,
                                  MetaWaylandSurface    *surface)
{
  MetaWaylandDragGrab *grab = data_device->current_grab;

  if (grab->drag_focus_data_device)
    wl_data_device_send_leave (grab->drag_focus_data_device);

  wl_list_remove (&grab->drag_focus_listener.link);
  grab->drag_focus_data_device = NULL;
}

static void
meta_wayland_drag_dest_motion (MetaWaylandDataDevice *data_device,
                               MetaWaylandSurface    *surface,
                               const ClutterEvent    *event)
{
  MetaWaylandDragGrab *grab = data_device->current_grab;
  wl_fixed_t sx, sy;

  meta_wayland_pointer_get_relative_coordinates (grab->generic.pointer,
                                                 grab->drag_focus,
                                                 &sx, &sy);
  wl_data_device_send_motion (grab->drag_focus_data_device,
                              clutter_event_get_time (event),
                              sx, sy);
}

static void
meta_wayland_drag_dest_drop (MetaWaylandDataDevice *data_device,
                             MetaWaylandSurface    *surface)
{
  MetaWaylandDragGrab *grab = data_device->current_grab;

  wl_data_device_send_drop (grab->drag_focus_data_device);
}

static const MetaWaylandDragDestFuncs meta_wayland_drag_dest_funcs = {
  meta_wayland_drag_dest_focus_in,
  meta_wayland_drag_dest_focus_out,
  meta_wayland_drag_dest_motion,
  meta_wayland_drag_dest_drop
};

const MetaWaylandDragDestFuncs *
meta_wayland_data_device_get_drag_dest_funcs (void)
{
  return &meta_wayland_drag_dest_funcs;
}

void
meta_wayland_data_device_set_dnd_source (MetaWaylandDataDevice *data_device,
                                         MetaWaylandDataSource *source)
{
  if (data_device->dnd_data_source == source)
    return;

  if (data_device->dnd_data_source)
    g_object_remove_weak_pointer (G_OBJECT (source),
                                  (gpointer *)&data_device->dnd_data_source);

  data_device->dnd_data_source = source;
  g_object_add_weak_pointer (G_OBJECT (source),
                             (gpointer *)&data_device->dnd_data_source);

  wl_signal_emit (&data_device->dnd_ownership_signal, source);
}

void
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
      meta_wayland_data_source_cancel (data_device->selection_data_source);
      g_object_weak_unref (G_OBJECT (data_device->selection_data_source),
                           selection_data_source_destroyed,
                           data_device);
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
      g_object_weak_ref (G_OBJECT (source),
                         selection_data_source_destroyed,
                         data_device);
    }

  wl_signal_emit (&data_device->selection_ownership_signal, source);
}

static void
data_device_set_selection (struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *source_resource,
                           guint32 serial)
{
  MetaWaylandDataDevice *data_device = wl_resource_get_user_data (resource);
  MetaWaylandDataSource *source;

  if (source_resource)
    source = wl_resource_get_user_data (source_resource);
  else
    source = NULL;

  /* FIXME: Store serial and check against incoming serial here. */
  meta_wayland_data_device_set_selection (data_device, source, serial);
}

static void
data_device_release(struct wl_client *client, struct wl_resource *resource)
{
  wl_resource_destroy(resource);
}

static const struct wl_data_device_interface data_device_interface = {
  data_device_start_drag,
  data_device_set_selection,
  data_device_release,
};

static void
destroy_data_source (struct wl_resource *resource)
{
  MetaWaylandDataSourceWayland *source = wl_resource_get_user_data (resource);

  source->resource = NULL;
  g_object_unref (source);
}

static void
create_data_source (struct wl_client *client,
                    struct wl_resource *resource, guint32 id)
{
  struct wl_resource *source_resource;

  source_resource = wl_resource_create (client, &wl_data_source_interface,
                                        wl_resource_get_version (resource), id);
  meta_wayland_data_source_wayland_new (source_resource);
}

static void
get_data_device (struct wl_client *client,
                 struct wl_resource *manager_resource,
                 guint32 id, struct wl_resource *seat_resource)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  struct wl_resource *cr;

  cr = wl_resource_create (client, &wl_data_device_interface, wl_resource_get_version (manager_resource), id);
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
  resource = wl_resource_create (client, &wl_data_device_manager_interface, version, id);
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
  wl_signal_init (&data_device->selection_ownership_signal);
  wl_signal_init (&data_device->dnd_ownership_signal);
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
  else
    wl_data_device_send_selection (data_device_resource, NULL);
}

gboolean
meta_wayland_data_device_is_dnd_surface (MetaWaylandDataDevice *data_device,
                                         MetaWaylandSurface    *surface)
{
  return data_device->current_grab &&
    data_device->current_grab->drag_surface == surface;
}

void
meta_wayland_data_device_update_dnd_surface (MetaWaylandDataDevice *data_device)
{
  MetaWaylandDragGrab *drag_grab;

  if (!data_device->current_grab)
    return;

  drag_grab = data_device->current_grab;

  if (!drag_grab->feedback_actor || !drag_grab->drag_surface)
    return;

  meta_feedback_actor_set_anchor (META_FEEDBACK_ACTOR (drag_grab->feedback_actor),
                                  -drag_grab->drag_surface->offset_x,
                                  -drag_grab->drag_surface->offset_y);
}

gboolean
meta_wayland_data_source_has_mime_type (const MetaWaylandDataSource *source,
                                        const gchar                 *mime_type)
{
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);
  gchar **p;

  wl_array_for_each (p, &priv->mime_types)
    {
      if (g_strcmp0 (mime_type, *p) == 0)
        return TRUE;
    }

  return FALSE;
}

static MetaWaylandDataSource *
meta_wayland_data_source_wayland_new (struct wl_resource *resource)
{
  MetaWaylandDataSourceWayland *source_wayland =
   g_object_new (META_TYPE_WAYLAND_DATA_SOURCE_WAYLAND, NULL);

  source_wayland->resource = resource;
  wl_resource_set_implementation (resource, &data_source_interface,
                                  source_wayland, destroy_data_source);

  return META_WAYLAND_DATA_SOURCE (source_wayland);
}

gboolean
meta_wayland_data_source_add_mime_type (MetaWaylandDataSource *source,
                                        const gchar           *mime_type)
{
  MetaWaylandDataSourcePrivate *priv =
    meta_wayland_data_source_get_instance_private (source);
  gchar **pos;

  pos = wl_array_add (&priv->mime_types, sizeof (*pos));

  if (pos)
    {
      *pos = g_strdup (mime_type);
      return *pos != NULL;
    }

  return FALSE;
}
