/*
 * Wayland Support
 *
 * Copyright (C) 2012,2013 Intel Corporation
 *               2013 Red Hat, Inc.
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

#include <config.h>

#include <clutter/clutter.h>
#include <clutter/wayland/clutter-wayland-compositor.h>
#include <clutter/wayland/clutter-wayland-surface.h>
#include <cogl/cogl-wayland-server.h>

#include <glib.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <wayland-server.h>
#include "gtk-shell-server-protocol.h"
#include "xdg-shell-server-protocol.h"

#include "meta-wayland-private.h"
#include "meta-xwayland-private.h"
#include "meta-wayland-stage.h"
#include "meta-window-actor-private.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-keyboard.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-data-device.h"
#include "meta-cursor-tracker-private.h"
#include "display-private.h"
#include "window-private.h"
#include <meta/types.h>
#include <meta/main.h>
#include "frame.h"
#include "meta-idle-monitor-private.h"
#include "meta-weston-launch.h"
#include "monitor-private.h"

static void
surface_process_damage (MetaWaylandSurface *surface,
                        cairo_region_t *region)
{
  g_assert (surface->window);

  if (surface->buffer_ref.buffer)
    {
      MetaWindowActor *window_actor =
        META_WINDOW_ACTOR (meta_window_get_compositor_private (surface->window));
      MetaRectangle rect;
      cairo_rectangle_int_t cairo_rect;

      meta_window_get_input_rect (surface->window, &rect);
      cairo_rect.x = 0;
      cairo_rect.y = 0;
      cairo_rect.width = rect.width;
      cairo_rect.height = rect.height;

      cairo_region_intersect_rectangle (region, &cairo_rect);

      if (window_actor)
        {
          int i, n_rectangles = cairo_region_num_rectangles (region);

          for (i = 0; i < n_rectangles; i++)
            {
              cairo_rectangle_int_t rectangle;

              cairo_region_get_rectangle (region, i, &rectangle);

              meta_window_actor_process_wayland_damage (window_actor,
                                                        rectangle.x,
                                                        rectangle.y,
                                                        rectangle.width,
                                                        rectangle.height);
            }
        }
    }
}

static void
meta_wayland_surface_destroy (struct wl_client *wayland_client,
                              struct wl_resource *wayland_resource)
{
  wl_resource_destroy (wayland_resource);
}

static void
meta_wayland_surface_attach (struct wl_client *wayland_client,
                             struct wl_resource *wayland_surface_resource,
                             struct wl_resource *wayland_buffer_resource,
                             gint32 dx, gint32 dy)
{
  MetaWaylandSurface *surface =
    wl_resource_get_user_data (wayland_surface_resource);
  MetaWaylandBuffer *buffer;

  /* X11 unmanaged window */
  if (!surface)
    return;

  if (wayland_buffer_resource)
    buffer = meta_wayland_buffer_from_resource (wayland_buffer_resource);
  else
    buffer = NULL;

  /* Attach without commit in between does not send wl_buffer.release */
  if (surface->pending.buffer)
    wl_list_remove (&surface->pending.buffer_destroy_listener.link);

  surface->pending.dx = dx;
  surface->pending.dy = dy;
  surface->pending.buffer = buffer;
  surface->pending.newly_attached = TRUE;

  if (buffer)
    wl_signal_add (&buffer->destroy_signal,
                   &surface->pending.buffer_destroy_listener);
}

static void
meta_wayland_surface_damage (struct wl_client *client,
                             struct wl_resource *surface_resource,
                             gint32 x,
                             gint32 y,
                             gint32 width,
                             gint32 height)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  cairo_rectangle_int_t rectangle = { x, y, width, height };

  /* X11 unmanaged window */
  if (!surface)
    return;

  cairo_region_union_rectangle (surface->pending.damage, &rectangle);
}

static void
destroy_frame_callback (struct wl_resource *callback_resource)
{
  MetaWaylandFrameCallback *callback =
    wl_resource_get_user_data (callback_resource);

  wl_list_remove (&callback->link);
  g_slice_free (MetaWaylandFrameCallback, callback);
}

static void
meta_wayland_surface_frame (struct wl_client *client,
                            struct wl_resource *surface_resource,
                            guint32 callback_id)
{
  MetaWaylandFrameCallback *callback;
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  callback = g_slice_new0 (MetaWaylandFrameCallback);
  callback->compositor = surface->compositor;
  callback->resource = wl_resource_create (client,
					   &wl_callback_interface, 1,
					   callback_id);
  wl_resource_set_user_data (callback->resource, callback);
  wl_resource_set_destructor (callback->resource, destroy_frame_callback);

  wl_list_insert (surface->pending.frame_callback_list.prev, &callback->link);
}

static void
meta_wayland_surface_set_opaque_region (struct wl_client *client,
                                        struct wl_resource *surface_resource,
                                        struct wl_resource *region_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  g_clear_pointer (&surface->pending.opaque_region, cairo_region_destroy);
  if (region_resource)
    {
      MetaWaylandRegion *region = wl_resource_get_user_data (region_resource);
      surface->pending.opaque_region = cairo_region_copy (region->region);
    }
}

static void
meta_wayland_surface_set_input_region (struct wl_client *client,
                                       struct wl_resource *surface_resource,
                                       struct wl_resource *region_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  g_clear_pointer (&surface->pending.input_region, cairo_region_destroy);
  if (region_resource)
    {
      MetaWaylandRegion *region = wl_resource_get_user_data (region_resource);
      surface->pending.input_region = cairo_region_copy (region->region);
    }
}

static void
empty_region (cairo_region_t *region)
{
  cairo_rectangle_int_t rectangle = { 0, 0, 0, 0 };
  cairo_region_intersect_rectangle (region, &rectangle);
}

static gboolean
surface_wants_window (MetaWaylandSurface *surface)
{
  return (surface->xdg_surface.resource != NULL);
}

static void
surface_ensure_window (MetaWaylandSurface *surface)
{
  MetaDisplay *display = meta_get_display ();

  if (surface->window)
    return;

  if (!surface_wants_window (surface))
    return;

  surface->window = meta_window_new_for_wayland (display, surface);
}

static void
ensure_buffer_texture (MetaWaylandBuffer *buffer)
{
  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  CoglError *catch_error = NULL;
  CoglTexture *texture;

  texture = COGL_TEXTURE (cogl_wayland_texture_2d_new_from_buffer (ctx,
                                                                   buffer->resource,
                                                                   &catch_error));
  if (!texture)
    {
      cogl_error_free (catch_error);
      meta_warning ("Could not import pending buffer, ignoring commit\n");
      return;
    }

  buffer->texture = texture;
  buffer->width = cogl_texture_get_width (texture);
  buffer->height = cogl_texture_get_height (texture);
}

static void
meta_wayland_surface_commit (struct wl_client *client,
                             struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandCompositor *compositor;

  /* X11 unmanaged window */
  if (!surface)
    return;

  compositor = surface->compositor;

  /* wl_surface.attach */
  if (surface->pending.newly_attached &&
      surface->buffer_ref.buffer != surface->pending.buffer)
    {
      MetaWaylandBuffer *buffer = surface->pending.buffer;

      if (buffer)
        {
          /* Note: we set this before informing any window-actor since the
           * window actor will expect to find the new buffer within the
           * surface. */
          ensure_buffer_texture (buffer);
          meta_wayland_buffer_reference (&surface->buffer_ref, buffer);
        }
    }

  surface_ensure_window (surface);

  if (surface == compositor->seat->sprite)
    meta_wayland_seat_update_sprite (compositor->seat);
  else if (surface->window)
    {
      MetaWindow *window = surface->window;

      if (surface->pending.buffer)
	{
	  MetaWaylandBuffer *buffer = surface->pending.buffer;
	  MetaWindowActor *window_actor = META_WINDOW_ACTOR (meta_window_get_compositor_private (window));

	  meta_window_actor_attach_wayland_buffer (window_actor, buffer);
	}

      /* We resize X based surfaces according to X events */
      if (window->client_type == META_WINDOW_CLIENT_TYPE_WAYLAND)
	{
	  int new_width;
	  int new_height;

	  new_width = surface->buffer_ref.buffer->width;
	  new_height = surface->buffer_ref.buffer->height;
	  if (new_width != window->rect.width ||
	      new_height != window->rect.height ||
	      surface->pending.dx != 0 ||
	      surface->pending.dy != 0)
	    meta_window_move_resize_wayland (surface->window, new_width, new_height,
					     surface->pending.dx, surface->pending.dy);
	}

      meta_window_set_opaque_region (surface->window, surface->pending.opaque_region);
      meta_window_set_input_region (surface->window, surface->pending.input_region);
      surface_process_damage (surface, surface->pending.damage);

      meta_window_set_title (surface->window, surface->pending.title);
      g_clear_pointer (&surface->pending.title, g_free);

      meta_window_set_wm_class (surface->window, surface->pending.app_id, surface->pending.app_id);
      g_clear_pointer (&surface->pending.app_id, g_free);

      meta_window_set_gtk_dbus_properties (surface->window,
                                           surface->pending.gtk_application_id,
                                           surface->pending.gtk_unique_bus_name,
                                           surface->pending.gtk_app_menu_path,
                                           surface->pending.gtk_menubar_path,
                                           surface->pending.gtk_application_object_path,
                                           surface->pending.gtk_window_object_path);

      g_clear_pointer (&surface->pending.gtk_application_id, g_free);
      g_clear_pointer (&surface->pending.gtk_unique_bus_name, g_free);
      g_clear_pointer (&surface->pending.gtk_app_menu_path, g_free);
      g_clear_pointer (&surface->pending.gtk_menubar_path, g_free);
      g_clear_pointer (&surface->pending.gtk_application_object_path, g_free);
      g_clear_pointer (&surface->pending.gtk_window_object_path, g_free);
    }

  if (surface->pending.buffer)
    {
      wl_list_remove (&surface->pending.buffer_destroy_listener.link);
      surface->pending.buffer = NULL;
    }
  surface->pending.dx = 0;
  surface->pending.dy = 0;
  surface->pending.newly_attached = FALSE;
  g_clear_pointer (&surface->pending.opaque_region, cairo_region_destroy);
  g_clear_pointer (&surface->pending.input_region, cairo_region_destroy);
  empty_region (surface->pending.damage);

  /* wl_surface.frame */
  wl_list_insert_list (&compositor->frame_callbacks,
                       &surface->pending.frame_callback_list);
  wl_list_init (&surface->pending.frame_callback_list);
}

static void
meta_wayland_surface_set_buffer_transform (struct wl_client *client,
                                           struct wl_resource *resource,
                                           int32_t transform)
{
  g_warning ("TODO: support set_buffer_transform request");
}

static void
meta_wayland_surface_set_buffer_scale (struct wl_client *client,
                                       struct wl_resource *resource,
                                       int scale)
{
  if (scale != 1)
    g_warning ("TODO: support set_buffer_scale request");
}

const struct wl_surface_interface meta_wayland_surface_interface = {
  meta_wayland_surface_destroy,
  meta_wayland_surface_attach,
  meta_wayland_surface_damage,
  meta_wayland_surface_frame,
  meta_wayland_surface_set_opaque_region,
  meta_wayland_surface_set_input_region,
  meta_wayland_surface_commit,
  meta_wayland_surface_set_buffer_transform,
  meta_wayland_surface_set_buffer_scale
};

void
meta_wayland_surface_free (MetaWaylandSurface *surface)
{
  MetaWaylandCompositor *compositor = surface->compositor;
  MetaWaylandFrameCallback *cb, *next;

  compositor->surfaces = g_list_remove (compositor->surfaces, surface);

  meta_wayland_buffer_reference (&surface->buffer_ref, NULL);

  if (surface->pending.buffer)
    wl_list_remove (&surface->pending.buffer_destroy_listener.link);

  cairo_region_destroy (surface->pending.damage);

  wl_list_for_each_safe (cb, next,
                         &surface->pending.frame_callback_list, link)
    wl_resource_destroy (cb->resource);

  meta_wayland_compositor_repick (compositor);

  g_assert (surface != compositor->seat->keyboard.focus);
  if (surface == compositor->seat->pointer.focus)
    {
      meta_wayland_pointer_destroy_focus (&compositor->seat->pointer);

      g_assert (surface != compositor->seat->pointer.focus);
    }

  if (surface->resource)
    wl_resource_set_user_data (surface->resource, NULL);
  g_slice_free (MetaWaylandSurface, surface);
}

static void
meta_wayland_surface_resource_destroy_cb (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  /* There are four cases here:
     - An X11 unmanaged window -> surface is NULL, nothing to do
     - An X11 unmanaged window, but we got the wayland event first ->
       just clear the resource pointer
     - A wayland surface without window (destroyed before set_toplevel) ->
       need to free the surface itself
     - A wayland window -> need to unmanage
  */

  if (surface)
    {
      surface->resource = NULL;

      /* NB: If the surface corresponds to an X window then we will be
       * sure to free the MetaWindow according to some X event. */
      if (surface->window &&
	  surface->window->client_type == META_WINDOW_CLIENT_TYPE_WAYLAND)
	{
	  MetaDisplay *display = meta_get_display ();
	  guint32 timestamp = meta_display_get_current_time_roundtrip (display);

	  meta_window_unmanage (surface->window, timestamp);
	}
      else if (!surface->window)
	meta_wayland_surface_free (surface);
    }
}

static void
surface_handle_pending_buffer_destroy (struct wl_listener *listener,
                                       void *data)
{
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, pending.buffer_destroy_listener);

  surface->pending.buffer = NULL;
}

MetaWaylandSurface *
meta_wayland_surface_create (MetaWaylandCompositor *compositor,
			     struct wl_client      *wayland_client,
			     guint32                id,
			     guint32                version)
{
  MetaWaylandSurface *surface = g_slice_new0 (MetaWaylandSurface);

  surface->compositor = compositor;

  surface->resource = wl_resource_create (wayland_client,
					  &wl_surface_interface,
					  version, id);
  wl_resource_set_implementation (surface->resource, &meta_wayland_surface_interface, surface,
				  meta_wayland_surface_resource_destroy_cb);

  surface->pending.damage = cairo_region_create ();

  surface->pending.buffer_destroy_listener.notify =
    surface_handle_pending_buffer_destroy;
  wl_list_init (&surface->pending.frame_callback_list);

  return surface;
}

static void
destroy_surface_extension (struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *extension = wl_resource_get_user_data (resource);
  wl_list_remove (&extension->surface_destroy_listener.link);
  extension->surface_destroy_listener.notify = NULL;
  extension->resource = NULL;
}

static void
extension_handle_surface_destroy (struct wl_listener *listener,
				  void *data)
{
  MetaWaylandSurfaceExtension *extension = wl_container_of (listener, extension, surface_destroy_listener);
  wl_resource_destroy (extension->resource);
}

static int
get_resource_version (struct wl_resource *master_resource,
                      int                 max_version)
{
  return MIN (max_version, wl_resource_get_version (master_resource));
}

static gboolean
create_surface_extension (MetaWaylandSurfaceExtension *extension,
                          struct wl_client            *client,
                          struct wl_resource          *master_resource,
                          struct wl_resource          *surface_resource,
                          guint32                      id,
                          int                          max_version,
                          const struct wl_interface   *interface,
                          const void                  *implementation)
{
  struct wl_resource *resource;

  if (extension->resource != NULL)
    return FALSE;

  resource = wl_resource_create (client, interface, get_resource_version (master_resource, max_version), id);
  wl_resource_set_implementation (resource, implementation, extension, destroy_surface_extension);

  extension->resource = resource;
  extension->surface_destroy_listener.notify = extension_handle_surface_destroy;
  wl_resource_add_destroy_listener (surface_resource, &extension->surface_destroy_listener);
  return TRUE;
}

static void
xdg_surface_destroy (struct wl_client *client,
                     struct wl_resource *resource)
{
  destroy_surface_extension (resource);
}

static void
xdg_surface_set_transient_for (struct wl_client *client,
                               struct wl_resource *resource,
                               struct wl_resource *parent)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);
  MetaWaylandSurfaceExtension *parent_xdg_surface = wl_resource_get_user_data (parent);
  MetaWaylandSurface *parent_surface = wl_container_of (parent_xdg_surface, surface, xdg_surface);

  if (surface->window && parent_surface->window)
    meta_window_set_transient_for (surface->window, parent_surface->window);
}

static void
xdg_surface_set_title (struct wl_client *client,
                       struct wl_resource *resource,
                       const char *title)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  g_clear_pointer (&surface->pending.title, g_free);
  surface->pending.title = g_strdup (title);
}

static void
xdg_surface_set_app_id (struct wl_client *client,
                        struct wl_resource *resource,
                        const char *app_id)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  g_clear_pointer (&surface->pending.app_id, g_free);
  surface->pending.app_id = g_strdup (app_id);
}

static void
xdg_surface_pong (struct wl_client *client,
                  struct wl_resource *resource,
                  guint32 serial)
{
}

static gboolean
begin_grab_op_on_surface (MetaWaylandSurface *surface,
                          MetaWaylandSeat    *seat,
                          MetaGrabOp          grab_op)
{
  MetaWindow *window = surface->window;

  if (!window)
    return FALSE;

  if (grab_op == META_GRAB_OP_NONE)
    return FALSE;

  return meta_display_begin_grab_op (window->display,
                                     window->screen,
                                     window,
                                     grab_op,
                                     TRUE, /* pointer_already_grabbed */
                                     FALSE, /* frame_action */
                                     1, /* button. XXX? */
                                     0, /* modmask */
                                     meta_display_get_current_time_roundtrip (window->display),
                                     wl_fixed_to_int (seat->pointer.grab_x),
                                     wl_fixed_to_int (seat->pointer.grab_y));
}

static void
xdg_surface_move (struct wl_client *client,
                  struct wl_resource *resource,
                  struct wl_resource *seat_resource,
                  guint32 serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      seat->pointer.focus != surface)
    return;

  begin_grab_op_on_surface (surface, seat, META_GRAB_OP_MOVING);
}

static MetaGrabOp
grab_op_for_edge (int edge)
{
  switch (edge)
    {
    case XDG_SURFACE_RESIZE_EDGE_TOP_LEFT:
      return META_GRAB_OP_RESIZING_NW;
    case XDG_SURFACE_RESIZE_EDGE_TOP:
      return META_GRAB_OP_RESIZING_N;
    case XDG_SURFACE_RESIZE_EDGE_TOP_RIGHT:
      return META_GRAB_OP_RESIZING_NE;
    case XDG_SURFACE_RESIZE_EDGE_RIGHT:
      return META_GRAB_OP_RESIZING_E;
    case XDG_SURFACE_RESIZE_EDGE_BOTTOM_RIGHT:
      return META_GRAB_OP_RESIZING_SE;
    case XDG_SURFACE_RESIZE_EDGE_BOTTOM:
      return META_GRAB_OP_RESIZING_S;
    case XDG_SURFACE_RESIZE_EDGE_BOTTOM_LEFT:
      return META_GRAB_OP_RESIZING_SW;
    case XDG_SURFACE_RESIZE_EDGE_LEFT:
      return META_GRAB_OP_RESIZING_W;
    default:
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }
}

static void
xdg_surface_resize (struct wl_client *client,
                    struct wl_resource *resource,
                    struct wl_resource *seat_resource,
                    guint32 serial,
                    guint32 edges)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      seat->pointer.focus != surface)
    return;

  begin_grab_op_on_surface (surface, seat, grab_op_for_edge (edges));
}

static void
xdg_surface_set_output (struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *output)
{
  g_warning ("TODO: support xdg_surface.set_output");
}

static void
xdg_surface_set_fullscreen (struct wl_client *client,
                            struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  if (surface->window)
    meta_window_make_fullscreen (surface->window);
}

static void
xdg_surface_unset_fullscreen (struct wl_client *client,
                              struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  if (surface->window)
    meta_window_unmake_fullscreen (surface->window);
}

static void
xdg_surface_set_maximized (struct wl_client *client,
                           struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  if (surface->window)
    meta_window_maximize (surface->window, META_MAXIMIZE_HORIZONTAL | META_MAXIMIZE_VERTICAL);
}

static void
xdg_surface_unset_maximized (struct wl_client *client,
                             struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  if (surface->window)
    meta_window_unmaximize (surface->window, META_MAXIMIZE_HORIZONTAL | META_MAXIMIZE_VERTICAL);
}

static void
xdg_surface_set_minimized (struct wl_client *client,
                           struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (xdg_surface, surface, xdg_surface);

  if (surface->window)
    meta_window_minimize (surface->window);
}

static const struct xdg_surface_interface meta_wayland_xdg_surface_interface = {
  xdg_surface_destroy,
  xdg_surface_set_transient_for,
  xdg_surface_set_title,
  xdg_surface_set_app_id,
  xdg_surface_pong,
  xdg_surface_move,
  xdg_surface_resize,
  xdg_surface_set_output,
  xdg_surface_set_fullscreen,
  xdg_surface_unset_fullscreen,
  xdg_surface_set_maximized,
  xdg_surface_unset_maximized,
  xdg_surface_set_minimized,
};

static void
use_unstable_version (struct wl_client *client,
                      struct wl_resource *resource,
                      int32_t version)
{
  if (version != META_XDG_SHELL_VERSION)
    g_warning ("Bad xdg_shell version: %d", version);
}

static void
get_xdg_surface (struct wl_client *client,
                 struct wl_resource *resource,
                 guint32 id,
                 struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  if (!create_surface_extension (&surface->xdg_surface, client, surface_resource, resource, id,
                                 META_XDG_SURFACE_VERSION,
                                 &xdg_surface_interface,
                                 &meta_wayland_xdg_surface_interface))
    wl_resource_post_error (surface_resource,
                            WL_DISPLAY_ERROR_INVALID_OBJECT,
                            "xdg_shell::get_xdg_surface already requested");
}

static void
get_xdg_popup (struct wl_client *client,
               struct wl_resource *resource,
               uint32_t id,
               struct wl_resource *surface,
               struct wl_resource *parent,
               struct wl_resource *seat,
               uint32_t serial,
               int32_t x,
               int32_t y,
               uint32_t flags)
{
  g_warning ("TODO: support xdg_shell.get_xdg_popup");
}

static const struct xdg_shell_interface meta_wayland_xdg_shell_interface = {
  use_unstable_version,
  get_xdg_surface,
  get_xdg_popup,
};

static void
bind_xdg_shell (struct wl_client *client,
                void *data,
                guint32 version,
                guint32 id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &xdg_shell_interface,
				 MIN (META_XDG_SHELL_VERSION, version), id);
  wl_resource_set_implementation (resource, &meta_wayland_xdg_shell_interface, data, NULL);
}

static void
set_dbus_properties (struct wl_client   *client,
		     struct wl_resource *resource,
		     const char         *application_id,
		     const char         *app_menu_path,
		     const char         *menubar_path,
		     const char         *window_object_path,
		     const char         *application_object_path,
		     const char         *unique_bus_name)
{
  MetaWaylandSurfaceExtension *gtk_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_container_of (gtk_surface, surface, gtk_surface);

  g_clear_pointer (&surface->pending.gtk_application_id, g_free);
  surface->pending.gtk_application_id = g_strdup (application_id);
  g_clear_pointer (&surface->pending.gtk_unique_bus_name, g_free);
  surface->pending.gtk_unique_bus_name = g_strdup (unique_bus_name);
  g_clear_pointer (&surface->pending.gtk_app_menu_path, g_free);
  surface->pending.gtk_app_menu_path = g_strdup (app_menu_path);
  g_clear_pointer (&surface->pending.gtk_menubar_path, g_free);
  surface->pending.gtk_menubar_path = g_strdup (menubar_path);
  g_clear_pointer (&surface->pending.gtk_application_object_path, g_free);
  surface->pending.gtk_application_object_path = g_strdup (application_object_path);
  g_clear_pointer (&surface->pending.gtk_window_object_path, g_free);
  surface->pending.gtk_window_object_path = g_strdup (window_object_path);
}

static const struct gtk_surface_interface meta_wayland_gtk_surface_interface = {
  set_dbus_properties
};

static void
get_gtk_surface (struct wl_client *client,
		 struct wl_resource *resource,
		 guint32 id,
		 struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  if (!create_surface_extension (&surface->gtk_surface, client, surface_resource, resource, id,
                                 META_GTK_SURFACE_VERSION,
                                 &gtk_surface_interface,
                                 &meta_wayland_gtk_surface_interface))
    wl_resource_post_error (surface_resource,
                            WL_DISPLAY_ERROR_INVALID_OBJECT,
                            "gtk_shell::get_gtk_surface already requested");
}

static const struct gtk_shell_interface meta_wayland_gtk_shell_interface = {
  get_gtk_surface
};

static void
bind_gtk_shell (struct wl_client *client,
		void             *data,
		guint32           version,
		guint32           id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &gtk_shell_interface,
				 MIN (META_GTK_SHELL_VERSION, version), id);
  wl_resource_set_implementation (resource, &meta_wayland_gtk_shell_interface, data, NULL);

  /* FIXME: ask the plugin */
  gtk_shell_send_capabilities (resource, GTK_SHELL_CAPABILITY_GLOBAL_APP_MENU);
}

void
meta_wayland_init_shell (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
			&xdg_shell_interface,
			META_XDG_SHELL_VERSION,
			compositor, bind_xdg_shell) == NULL)
    g_error ("Failed to register a global xdg-shell object");

  if (wl_global_create (compositor->wayland_display,
			&gtk_shell_interface,
			META_GTK_SHELL_VERSION,
			compositor, bind_gtk_shell) == NULL)
    g_error ("Failed to register a global gtk-shell object");
}

void
meta_wayland_surface_configure_notify (MetaWaylandSurface *surface,
				       int                 new_width,
				       int                 new_height,
				       int                 edges)
{
  if (surface->xdg_surface.resource)
    xdg_surface_send_configure (surface->xdg_surface.resource,
                                edges, new_width, new_height,
                                0, 0 /* XXX: support this */);
}

void
meta_wayland_surface_focused_set (MetaWaylandSurface *surface)
{
  if (surface->xdg_surface.resource)
    xdg_surface_send_focused_set (surface->xdg_surface.resource);
}

void
meta_wayland_surface_focused_unset (MetaWaylandSurface *surface)
{
  if (surface->xdg_surface.resource)
    xdg_surface_send_focused_unset (surface->xdg_surface.resource);
}
