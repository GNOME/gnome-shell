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

static void ensure_initial_state (MetaWaylandSurface *surface);
static void free_initial_state   (MetaWaylandSurfaceInitialState *surface);

static void
surface_process_damage (MetaWaylandSurface *surface,
                        cairo_region_t *region)
{
  if (surface->window &&
      surface->buffer_ref.buffer)
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
                             gint32 sx, gint32 sy)
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

  surface->pending.sx = sx;
  surface->pending.sy = sy;
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
  MetaWaylandRegion *region = wl_resource_get_user_data (region_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  g_clear_pointer (&surface->pending.opaque_region, cairo_region_destroy);
  surface->pending.opaque_region = cairo_region_copy (region->region);
}

static void
meta_wayland_surface_set_input_region (struct wl_client *client,
                                       struct wl_resource *surface_resource,
                                       struct wl_resource *region_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandRegion *region = wl_resource_get_user_data (region_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  g_clear_pointer (&surface->pending.input_region, cairo_region_destroy);
  surface->pending.input_region = cairo_region_copy (region->region);
}

static void
empty_region (cairo_region_t *region)
{
  cairo_rectangle_int_t rectangle = { 0, 0, 0, 0 };
  cairo_region_intersect_rectangle (region, &rectangle);
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
      /* Note: we set this before informing any window-actor since the
       * window actor will expect to find the new buffer within the
       * surface. */
      meta_wayland_buffer_reference (&surface->buffer_ref,
                                     surface->pending.buffer);

      if (surface->pending.buffer)
        {
          MetaWaylandBuffer *buffer = surface->pending.buffer;

          if (surface->window)
            {
              MetaWindow *window = surface->window;
              MetaWindowActor *window_actor =
                META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
              MetaRectangle rect;

              meta_window_get_input_rect (surface->window, &rect);

              if (window_actor)
                meta_window_actor_attach_wayland_buffer (window_actor, buffer);

              /* XXX: we resize X based surfaces according to X events */
              if (surface->xid == 0 &&
                  (buffer->width != rect.width || buffer->height != rect.height))
                meta_window_resize (surface->window, FALSE, buffer->width, buffer->height);
            }
          else if (surface == compositor->seat->sprite)
            meta_wayland_seat_update_sprite (compositor->seat);
        }
    }

  if (surface->pending.buffer)
    {
      wl_list_remove (&surface->pending.buffer_destroy_listener.link);
      surface->pending.buffer = NULL;
    }
  surface->pending.sx = 0;
  surface->pending.sy = 0;
  surface->pending.newly_attached = FALSE;

  if (surface->window)
    {
      meta_window_set_opaque_region (surface->window, surface->pending.opaque_region);
      g_clear_pointer (&surface->pending.opaque_region, cairo_region_destroy);

      meta_window_set_input_region (surface->window, surface->pending.input_region);
      g_clear_pointer (&surface->pending.input_region, cairo_region_destroy);
    }

  surface_process_damage (surface, surface->pending.damage);
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

  if (surface->initial_state)
    free_initial_state (surface->initial_state);

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
      g_assert (surface != compositor->seat->pointer.grab->focus);
    }

 if (compositor->implicit_grab_surface == surface)
   compositor->implicit_grab_surface = compositor->seat->pointer.current;

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
shell_surface_pong (struct wl_client *client,
                    struct wl_resource *resource,
                    guint32 serial)
{
}

typedef struct _MetaWaylandGrab
{
  MetaWaylandPointerGrab grab;
  MetaWaylandSurfaceExtension *shell_surface;
  struct wl_listener shell_surface_destroy_listener;
  MetaWaylandPointer *pointer;
} MetaWaylandGrab;

typedef struct _MetaWaylandMoveGrab
{
  MetaWaylandGrab base;
  wl_fixed_t dx, dy;
} MetaWaylandMoveGrab;

static void
destroy_shell_surface_grab_listener (struct wl_listener *listener,
                                     void *data)
{
  MetaWaylandGrab *grab = wl_container_of (listener, grab,
                                           shell_surface_destroy_listener);
  grab->shell_surface = NULL;

  /* XXX: Could we perhaps just stop the grab here so we don't have
   * to consider grab->shell_surface becoming NULL in grab interface
   * callbacks? */
}

typedef enum _GrabCursor
{
  GRAB_CURSOR_MOVE,
} GrabCursor;

static void
grab_pointer (MetaWaylandGrab *grab,
              const MetaWaylandPointerGrabInterface *interface,
              MetaWaylandSurfaceExtension *shell_surface,
              MetaWaylandPointer *pointer,
              GrabCursor cursor)
{
  /* TODO: popup_grab_end (pointer); */

  grab->grab.interface = interface;
  grab->shell_surface = shell_surface;
  grab->shell_surface_destroy_listener.notify =
    destroy_shell_surface_grab_listener;
  wl_resource_add_destroy_listener (shell_surface->resource,
                                    &grab->shell_surface_destroy_listener);

  grab->pointer = pointer;
  grab->grab.focus = shell_surface->surface;

  meta_wayland_pointer_start_grab (pointer, &grab->grab);

  /* TODO: send_grab_cursor (cursor); */

  /* XXX: In Weston there is a desktop shell protocol which has
   * a set_grab_surface request that's used to specify the surface
   * that's focused here.
   *
   * TODO: understand why.
   *
   * XXX: For now we just focus the surface directly associated with
   * the grab.
   */
  meta_wayland_pointer_set_focus (pointer,
                                  grab->shell_surface->surface,
                                  wl_fixed_from_int (0),
                                  wl_fixed_from_int (0));
}

static void
release_pointer (MetaWaylandGrab *grab)
{
  if (grab->shell_surface)
    wl_list_remove (&grab->shell_surface_destroy_listener.link);

  meta_wayland_pointer_end_grab (grab->pointer);
}

static void
noop_grab_focus (MetaWaylandPointerGrab *grab,
                 MetaWaylandSurface *surface,
                 wl_fixed_t x,
                 wl_fixed_t y)
{
  grab->focus = NULL;
}

static void
move_grab_motion (MetaWaylandPointerGrab *grab,
                  uint32_t time,
                  wl_fixed_t x,
                  wl_fixed_t y)
{
  MetaWaylandMoveGrab *move = (MetaWaylandMoveGrab *)grab;
  MetaWaylandPointer *pointer = move->base.pointer;
  MetaWaylandSurfaceExtension *shell_surface = move->base.shell_surface;

  if (!shell_surface)
    return;

  meta_window_move (shell_surface->surface->window,
                    TRUE,
                    wl_fixed_to_int (pointer->x + move->dx),
                    wl_fixed_to_int (pointer->y + move->dy));
}

static void
move_grab_button (MetaWaylandPointerGrab *pointer_grab,
                  uint32_t time,
                  uint32_t button,
                  uint32_t state_w)
{
  MetaWaylandGrab *grab =
    wl_container_of (pointer_grab, grab, grab);
  MetaWaylandMoveGrab *move = (MetaWaylandMoveGrab *)grab;
  MetaWaylandPointer *pointer = grab->pointer;
  enum wl_pointer_button_state state = state_w;

  if (pointer->button_count == 0 && state == WL_POINTER_BUTTON_STATE_RELEASED)
    {
      release_pointer (grab);
      g_slice_free (MetaWaylandMoveGrab, move);
    }
}

static const MetaWaylandPointerGrabInterface move_grab_interface = {
    noop_grab_focus,
    move_grab_motion,
    move_grab_button,
};

static void
start_surface_move (MetaWaylandSurfaceExtension *shell_surface,
                    MetaWaylandSeat *seat)
{
  MetaWaylandMoveGrab *move;
  MetaRectangle rect;

  g_return_if_fail (shell_surface != NULL);

  /* TODO: check if the surface is fullscreen when we support fullscreen */

  move = g_slice_new (MetaWaylandMoveGrab);

  meta_window_get_input_rect (shell_surface->surface->window,
                              &rect);

  move->dx = wl_fixed_from_int (rect.x) - seat->pointer.grab_x;
  move->dy = wl_fixed_from_int (rect.y) - seat->pointer.grab_y;

  grab_pointer (&move->base, &move_grab_interface, shell_surface,
                &seat->pointer, GRAB_CURSOR_MOVE);
}

static void
shell_surface_move (struct wl_client *client,
                    struct wl_resource *resource,
                    struct wl_resource *seat_resource,
                    guint32 serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurfaceExtension *shell_surface = wl_resource_get_user_data (resource);

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      seat->pointer.focus != shell_surface->surface)
    return;

  start_surface_move (shell_surface, seat);
}

static void
shell_surface_resize (struct wl_client *client,
                      struct wl_resource *resource,
                      struct wl_resource *seat,
                      guint32 serial,
                      guint32 edges)
{
  g_warning ("TODO: support shell_surface_resize request");
}

static void
ensure_surface_window (MetaWaylandSurface *surface)
{
  MetaDisplay *display = meta_get_display ();

  if (!surface->window)
    {
      int width, height;

      if (surface->buffer_ref.buffer)
        {
          MetaWaylandBuffer *buffer = surface->buffer_ref.buffer;
          width = buffer->width;
          height = buffer->width;
        }
      else
        {
          width = 0;
          height = 0;
        }

      surface->window =
        meta_window_new_for_wayland (display, width, height, surface);

      meta_window_calc_showing (surface->window);
    }
}

static void
shell_surface_set_toplevel (struct wl_client *client,
                            struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *shell_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = shell_surface->surface;
  MetaWaylandCompositor *compositor = surface->compositor;

  /* NB: Surfaces from xwayland become managed based on X events. */
  if (client == compositor->xwayland_client)
    return;

  ensure_surface_window (surface);

  meta_window_unmake_fullscreen (surface->window);
}

static void
shell_surface_set_transient (struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *parent,
                             int x,
                             int y,
                             guint32 flags)
{
  MetaWaylandSurfaceExtension *shell_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = shell_surface->surface;
  MetaWaylandCompositor *compositor = surface->compositor;

  /* NB: Surfaces from xwayland become managed based on X events. */
  if (client == compositor->xwayland_client)
    return;

  ensure_surface_window (surface);
}

static void
shell_surface_set_fullscreen (struct wl_client *client,
                              struct wl_resource *resource,
                              guint32 method,
                              guint32 framerate,
                              struct wl_resource *output)
{
  MetaWaylandSurfaceExtension *shell_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = shell_surface->surface;
  MetaWaylandCompositor *compositor = surface->compositor;

  /* NB: Surfaces from xwayland become managed based on X events. */
  if (client == compositor->xwayland_client)
    return;

  ensure_surface_window (surface);

  meta_window_make_fullscreen (surface->window);
}

static void
shell_surface_set_popup (struct wl_client *client,
                         struct wl_resource *resource,
                         struct wl_resource *seat,
                         guint32 serial,
                         struct wl_resource *parent,
                         gint32 x,
                         gint32 y,
                         guint32 flags)
{
}

static void
shell_surface_set_maximized (struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *output)
{
  g_warning ("TODO: support shell_surface_set_maximized request");
}

static void
shell_surface_set_title (struct wl_client *client,
                         struct wl_resource *resource,
                         const char *title)
{
  MetaWaylandSurfaceExtension *extension = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = extension->surface;

  if (surface->window)
    meta_window_set_title (surface->window, title);
  else
    {
      ensure_initial_state (surface);

      g_free (surface->initial_state->title);
      surface->initial_state->title = g_strdup (title);
    }
}

static void
shell_surface_set_class (struct wl_client *client,
                         struct wl_resource *resource,
                         const char *class_)
{
  MetaWaylandSurfaceExtension *extension = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = extension->surface;

  if (surface->window)
    meta_window_set_wm_class (surface->window, class_, class_);
  else
    {
      ensure_initial_state (surface);

      g_free (surface->initial_state->wm_class);
      surface->initial_state->wm_class = g_strdup (class_);
    }
}

static const struct wl_shell_surface_interface meta_wayland_shell_surface_interface =
{
  shell_surface_pong,
  shell_surface_move,
  shell_surface_resize,
  shell_surface_set_toplevel,
  shell_surface_set_transient,
  shell_surface_set_fullscreen,
  shell_surface_set_popup,
  shell_surface_set_maximized,
  shell_surface_set_title,
  shell_surface_set_class
};

static void
extension_handle_surface_destroy (struct wl_listener *listener,
				  void *data)
{
  MetaWaylandSurfaceExtension *extension =
    wl_container_of (listener, extension, surface_destroy_listener);

  extension->surface = NULL;
  wl_resource_destroy (extension->resource);
}

static void
destroy_surface_extension (struct wl_resource *resource)
{
  MetaWaylandSurfaceExtension *extension = wl_resource_get_user_data (resource);

  /* In case cleaning up a dead client destroys extension first */
  if (extension->surface)
    {
      wl_list_remove (&extension->surface_destroy_listener.link);
    }

  g_free (extension);
}

static void
create_surface_extension (struct wl_client          *client,
			  struct wl_resource        *master_resource,
			  guint32                    id,
			  int                        max_version,
			  MetaWaylandSurface        *surface,
			  const struct wl_interface *interface,
			  const void                *implementation)
{
  MetaWaylandSurfaceExtension *extension;

  extension = g_new0 (MetaWaylandSurfaceExtension, 1);

  extension->resource = wl_resource_create (client, interface,
					    MIN (max_version, wl_resource_get_version (master_resource)), id);
  wl_resource_set_implementation (extension->resource, implementation,
				  extension, destroy_surface_extension);

  extension->surface = surface;
  extension->surface_destroy_listener.notify = extension_handle_surface_destroy;
  wl_resource_add_destroy_listener (surface->resource,
                                    &extension->surface_destroy_listener);
}

static void
get_shell_surface (struct wl_client *client,
                   struct wl_resource *resource,
                   guint32 id,
                   struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  if (surface->has_shell_surface)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_shell_surface already requested");
      return;
    }

  create_surface_extension (client, resource, id, META_WL_SHELL_SURFACE_VERSION, surface,
			    &wl_shell_surface_interface,
			    &meta_wayland_shell_surface_interface);
  surface->has_shell_surface = TRUE;
}

static const struct wl_shell_interface meta_wayland_shell_interface =
{
  get_shell_surface
};

static void
bind_shell (struct wl_client *client,
            void *data,
            guint32 version,
            guint32 id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_shell_interface,
				 MIN (META_WL_SHELL_VERSION, version), id);
  wl_resource_set_implementation (resource, &meta_wayland_shell_interface, data, NULL);
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
  MetaWaylandSurfaceExtension *extension = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = extension->surface;

  if (surface == NULL)
    {
      wl_resource_post_error (resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "object is not associated with a toplevel surface");
      return;
    }

  if (surface->window)
    {
      meta_window_set_gtk_dbus_properties (surface->window,
					   application_id,
					   unique_bus_name,
					   app_menu_path,
					   menubar_path,
					   application_object_path,
					   window_object_path);
    }
  else
    {
      MetaWaylandSurfaceInitialState *initial;

      ensure_initial_state (surface);
      initial = surface->initial_state;

      g_free (initial->gtk_application_id);
      initial->gtk_application_id = g_strdup (application_id);

      g_free (initial->gtk_unique_bus_name);
      initial->gtk_unique_bus_name = g_strdup (unique_bus_name);

      g_free (initial->gtk_app_menu_path);
      initial->gtk_app_menu_path = g_strdup (app_menu_path);

      g_free (initial->gtk_menubar_path);
      initial->gtk_menubar_path = g_strdup (menubar_path);

      g_free (initial->gtk_application_object_path);
      initial->gtk_application_object_path = g_strdup (application_object_path);

      g_free (initial->gtk_window_object_path);
      initial->gtk_window_object_path = g_strdup (window_object_path);
    }
}

static const struct gtk_surface_interface meta_wayland_gtk_surface_interface =
{
  set_dbus_properties
};

static void
get_gtk_surface (struct wl_client *client,
		 struct wl_resource *resource,
		 guint32 id,
		 struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  if (surface->has_gtk_surface)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_gtk_surface already requested");
      return;
    }

  create_surface_extension (client, resource, id, META_GTK_SURFACE_VERSION, surface,
			    &gtk_surface_interface,
			    &meta_wayland_gtk_surface_interface);
  surface->has_gtk_surface = TRUE;
}

static const struct gtk_shell_interface meta_wayland_gtk_shell_interface =
{
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
			&wl_shell_interface,
			META_WL_SHELL_VERSION,
			compositor, bind_shell) == NULL)
    g_error ("Failed to register a global shell object");

  if (wl_global_create (compositor->wayland_display,
			&gtk_shell_interface,
			META_GTK_SHELL_VERSION,
			compositor, bind_gtk_shell) == NULL)
    g_error ("Failed to register a global gtk-shell object");
}

void
meta_wayland_surface_set_initial_state (MetaWaylandSurface *surface,
					MetaWindow         *window)
{
  MetaWaylandSurfaceInitialState *initial = surface->initial_state;

  if (initial == NULL)
    return;

  if (initial->title)
    meta_window_set_title (window, initial->title);

  if (initial->wm_class)
    meta_window_set_wm_class (window, initial->wm_class, initial->wm_class);

  meta_window_set_gtk_dbus_properties (window,
				       initial->gtk_application_id,
				       initial->gtk_unique_bus_name,
				       initial->gtk_app_menu_path,
				       initial->gtk_menubar_path,
				       initial->gtk_application_object_path,
				       initial->gtk_window_object_path);

  free_initial_state (initial);
  surface->initial_state = NULL;
}

static void
ensure_initial_state (MetaWaylandSurface *surface)
{
  if (surface->initial_state)
    return;

  surface->initial_state = g_slice_new0 (MetaWaylandSurfaceInitialState);
}

static void
free_initial_state (MetaWaylandSurfaceInitialState *initial)
{
  g_free (initial->title);
  g_free (initial->wm_class);

  g_free (initial->gtk_application_id);
  g_free (initial->gtk_unique_bus_name);
  g_free (initial->gtk_app_menu_path);
  g_free (initial->gtk_menubar_path);
  g_free (initial->gtk_application_object_path);
  g_free (initial->gtk_window_object_path);

  g_slice_free (MetaWaylandSurfaceInitialState, initial);
}
