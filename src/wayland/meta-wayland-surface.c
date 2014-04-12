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

#include "meta-surface-actor.h"
#include "meta-surface-actor-wayland.h"

typedef enum
{
  META_WAYLAND_SUBSURFACE_PLACEMENT_ABOVE,
  META_WAYLAND_SUBSURFACE_PLACEMENT_BELOW
} MetaWaylandSubsurfacePlacement;

typedef struct
{
  MetaWaylandSubsurfacePlacement placement;
  MetaWaylandSurface *sibling;
  struct wl_listener sibling_destroy_listener;
} MetaWaylandSubsurfacePlacementOp;

static void
surface_set_buffer (MetaWaylandSurface *surface,
                    MetaWaylandBuffer  *buffer)
{
  if (surface->buffer == buffer)
    return;

  if (surface->buffer)
    {
      wl_list_remove (&surface->buffer_destroy_listener.link);
      meta_wayland_buffer_unref (surface->buffer);
    }

  surface->buffer = buffer;

  if (surface->buffer)
    {
      meta_wayland_buffer_ref (surface->buffer);
      wl_signal_add (&surface->buffer->destroy_signal, &surface->buffer_destroy_listener);
    }
}

static void
surface_handle_buffer_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandSurface *surface = wl_container_of (listener, surface, buffer_destroy_listener);

  surface_set_buffer (surface, NULL);
}

static void
surface_process_damage (MetaWaylandSurface *surface,
                        cairo_region_t *region)
{
  int i, n_rectangles = cairo_region_num_rectangles (region);

  for (i = 0; i < n_rectangles; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      meta_surface_actor_process_damage (surface->surface_actor,
                                         rect.x, rect.y, rect.width, rect.height);
    }
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
}

static void
cursor_surface_commit (MetaWaylandSurface      *surface,
                       MetaWaylandPendingState *pending)
{
  if (pending->newly_attached)
    meta_wayland_seat_update_cursor_surface (surface->compositor->seat);
}

static void
toplevel_surface_commit (MetaWaylandSurface             *surface,
                         MetaWaylandPendingState *pending)
{
  if (pending->newly_attached)
    {
      MetaWindow *window = surface->window;
      MetaWaylandBuffer *buffer = pending->buffer;

      meta_window_set_surface_mapped (window, buffer != NULL);
      /* We resize X based surfaces according to X events */
      if (buffer != NULL && window->client_type == META_WINDOW_CLIENT_TYPE_WAYLAND)
        {
          int new_width, new_height;

          new_width = cogl_texture_get_width (buffer->texture);
          new_height = cogl_texture_get_height (buffer->texture);

          if (new_width != window->rect.width ||
              new_height != window->rect.height ||
              pending->dx != 0 ||
              pending->dy != 0)
            meta_window_move_resize_wayland (window, new_width, new_height, pending->dx, pending->dy);
        }
    }

  if (pending->frame_extents_changed)
    meta_window_set_custom_frame_extents (surface->window, &pending->frame_extents);
}

static void
surface_handle_pending_buffer_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandPendingState *state = wl_container_of (listener, state, buffer_destroy_listener);

  state->buffer = NULL;
}

static void
pending_state_init (MetaWaylandPendingState *state)
{
  state->newly_attached = FALSE;
  state->buffer = NULL;
  state->dx = 0;
  state->dy = 0;

  state->input_region = NULL;
  state->opaque_region = NULL;

  state->damage = cairo_region_create ();
  state->buffer_destroy_listener.notify = surface_handle_pending_buffer_destroy;
  wl_list_init (&state->frame_callback_list);

  state->frame_extents_changed = FALSE;
}

static void
pending_state_destroy (MetaWaylandPendingState *state)
{
  MetaWaylandFrameCallback *cb, *next;

  g_clear_pointer (&state->damage, cairo_region_destroy);
  g_clear_pointer (&state->input_region, cairo_region_destroy);
  g_clear_pointer (&state->opaque_region, cairo_region_destroy);

  if (state->buffer)
    wl_list_remove (&state->buffer_destroy_listener.link);
  wl_list_for_each_safe (cb, next, &state->frame_callback_list, link)
    wl_resource_destroy (cb->resource);
}

static void
pending_state_reset (MetaWaylandPendingState *state)
{
  pending_state_destroy (state);
  pending_state_init (state);
}

static void
move_pending_state (MetaWaylandPendingState *from,
                    MetaWaylandPendingState *to)
{
  if (from->buffer)
    wl_list_remove (&from->buffer_destroy_listener.link);

  wl_list_insert_list (&to->frame_callback_list, &from->frame_callback_list);

  *to = *from;

  if (to->buffer)
    wl_signal_add (&to->buffer->destroy_signal, &to->buffer_destroy_listener);

  pending_state_init (from);
}

static void
subsurface_surface_commit (MetaWaylandSurface      *surface,
                           MetaWaylandPendingState *pending)
{
  if (pending->newly_attached)
    {
      MetaSurfaceActor *surface_actor = surface->surface_actor;
      MetaWaylandBuffer *buffer = pending->buffer;
      float x, y;

      if (buffer != NULL)
        clutter_actor_show (CLUTTER_ACTOR (surface_actor));
      else
        clutter_actor_hide (CLUTTER_ACTOR (surface_actor));

      clutter_actor_get_position (CLUTTER_ACTOR (surface_actor), &x, &y);
      x += pending->dx;
      y += pending->dy;
      clutter_actor_set_position (CLUTTER_ACTOR (surface_actor), x, y);
    }
}

static void
subsurface_parent_surface_committed (MetaWaylandSurface *surface);

static void
parent_surface_committed (gpointer data, gpointer user_data)
{
  subsurface_parent_surface_committed (data);
}

static void
commit_pending_state (MetaWaylandSurface      *surface,
                      MetaWaylandPendingState *pending)
{
  MetaWaylandCompositor *compositor = surface->compositor;

  /* If this surface is a subsurface in in synchronous mode, commit
   * has a special-case and should not apply the pending state immediately.
   *
   * Instead, we move it to another pending state, which will be
   * actually committed when the parent commits.
   */
  if (surface->sub.synchronous)
    {
      move_pending_state (pending, &surface->sub.pending);
      return;
    }

  if (pending->newly_attached)
    {
      surface_set_buffer (surface, pending->buffer);

      if (pending->buffer)
        {
          ensure_buffer_texture (pending->buffer);
          meta_surface_actor_wayland_set_buffer (META_SURFACE_ACTOR_WAYLAND (surface->surface_actor), pending->buffer);
        }
    }

  surface_process_damage (surface, pending->damage);

  if (pending->opaque_region)
    meta_surface_actor_set_opaque_region (surface->surface_actor, pending->opaque_region);
  if (pending->input_region)
    meta_surface_actor_set_input_region (surface->surface_actor, pending->input_region);

  if (surface == compositor->seat->pointer.cursor_surface)
    cursor_surface_commit (surface, pending);
  else if (surface->window)
    toplevel_surface_commit (surface, pending);
  else if (surface->subsurface.resource)
    subsurface_surface_commit (surface, pending);

  g_list_foreach (surface->subsurfaces, parent_surface_committed, NULL);

  /* wl_surface.frame */
  wl_list_insert_list (&compositor->frame_callbacks, &pending->frame_callback_list);
  wl_list_init (&pending->frame_callback_list);

  pending_state_reset (pending);
}

static void
meta_wayland_surface_commit (MetaWaylandSurface *surface)
{
  commit_pending_state (surface, &surface->pending);
}

static void
wl_surface_destroy (struct wl_client *client,
                    struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
wl_surface_attach (struct wl_client *client,
                   struct wl_resource *surface_resource,
                   struct wl_resource *buffer_resource,
                   gint32 dx, gint32 dy)
{
  MetaWaylandSurface *surface =
    wl_resource_get_user_data (surface_resource);
  MetaWaylandBuffer *buffer;

  /* X11 unmanaged window */
  if (!surface)
    return;

  if (buffer_resource)
    buffer = meta_wayland_buffer_from_resource (buffer_resource);
  else
    buffer = NULL;

  if (surface->buffer == buffer)
    return;

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
wl_surface_damage (struct wl_client *client,
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
wl_surface_frame (struct wl_client *client,
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
  callback->resource = wl_resource_create (client, &wl_callback_interface, META_WL_CALLBACK_VERSION, callback_id);
  wl_resource_set_implementation (callback->resource, NULL, callback, destroy_frame_callback);

  wl_list_insert (surface->pending.frame_callback_list.prev, &callback->link);
}

static void
wl_surface_set_opaque_region (struct wl_client *client,
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
wl_surface_set_input_region (struct wl_client *client,
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
wl_surface_commit (struct wl_client *client,
                   struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  meta_wayland_surface_commit (surface);
}

static void
wl_surface_set_buffer_transform (struct wl_client *client,
                                 struct wl_resource *resource,
                                 int32_t transform)
{
  g_warning ("TODO: support set_buffer_transform request");
}

static void
wl_surface_set_buffer_scale (struct wl_client *client,
                             struct wl_resource *resource,
                             int scale)
{
  if (scale != 1)
    g_warning ("TODO: support set_buffer_scale request");
}

const struct wl_surface_interface meta_wayland_wl_surface_interface = {
  wl_surface_destroy,
  wl_surface_attach,
  wl_surface_damage,
  wl_surface_frame,
  wl_surface_set_opaque_region,
  wl_surface_set_input_region,
  wl_surface_commit,
  wl_surface_set_buffer_transform,
  wl_surface_set_buffer_scale
};

static gboolean
surface_should_be_reactive (MetaWaylandSurface *surface)
{
  /* If we have a toplevel window, we should be reactive */
  if (surface->window)
    return TRUE;

  /* If we're a subsurface, we should be reactive */
  if (surface->subsurface.resource)
    return TRUE;

  return FALSE;
}

static void
sync_reactive (MetaWaylandSurface *surface)
{
  clutter_actor_set_reactive (CLUTTER_ACTOR (surface->surface_actor),
                              surface_should_be_reactive (surface));
}

static void
surface_state_changed (MetaWaylandSurface *surface)
{
  if (surface->window)
    meta_wayland_surface_configure_notify (surface,
                                           surface->window->rect.width,
                                           surface->window->rect.height);
}

static void
window_appears_focused_changed (MetaWindow *window,
                                GParamSpec *pspec,
                                gpointer    user_data)
{
  MetaWaylandSurface *surface = user_data;

  surface_state_changed (surface);
}

void
meta_wayland_surface_set_window (MetaWaylandSurface *surface,
                                 MetaWindow         *window)
{
  if (surface->window)
    {
      g_signal_handlers_disconnect_by_func (surface->window, window_appears_focused_changed, surface);
    }

  surface->window = window;

  if (surface->window)
    {
      g_signal_connect (surface->window, "notify::appears-focused",
                        G_CALLBACK (window_appears_focused_changed), surface);
    }

  sync_reactive (surface);
}

static void
destroy_window (MetaWaylandSurface *surface)
{
  if (surface->window)
    {
      MetaDisplay *display = meta_get_display ();
      guint32 timestamp = meta_display_get_current_time_roundtrip (display);

      meta_window_unmanage (surface->window, timestamp);
    }

  g_assert (surface->window == NULL);
}

static void
wl_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandCompositor *compositor = surface->compositor;

  /* If we still have a window at the time of destruction, that means that
   * the client is disconnecting, as the resources are destroyed in a random
   * order. Simply destroy the window in this case. */
  if (surface->window)
    destroy_window (surface);

  surface_set_buffer (surface, NULL);
  pending_state_destroy (&surface->pending);

  g_object_unref (surface->surface_actor);

  if (surface->resource)
    wl_resource_set_user_data (surface->resource, NULL);
  g_slice_free (MetaWaylandSurface, surface);

  meta_wayland_compositor_repick (compositor);
}

MetaWaylandSurface *
meta_wayland_surface_create (MetaWaylandCompositor *compositor,
                             struct wl_client      *client,
                             struct wl_resource    *compositor_resource,
                             guint32                id)
{
  MetaWaylandSurface *surface = g_slice_new0 (MetaWaylandSurface);

  surface->compositor = compositor;

  surface->resource = wl_resource_create (client, &wl_surface_interface,
                                          MIN (META_WL_SURFACE_VERSION, wl_resource_get_version (compositor_resource)), id);
  wl_resource_set_implementation (surface->resource, &meta_wayland_wl_surface_interface, surface, wl_surface_destructor);

  surface->buffer_destroy_listener.notify = surface_handle_buffer_destroy;
  surface->surface_actor = g_object_ref_sink (meta_surface_actor_wayland_new (surface));

  pending_state_init (&surface->pending);
  return surface;
}

static void
destroy_surface_extension (MetaWaylandSurfaceExtension *extension)
{
  extension->resource = NULL;
}

static gboolean
create_surface_extension (MetaWaylandSurfaceExtension *extension,
                          int                          max_version,
                          const struct wl_interface   *interface,
                          const void                  *implementation,
                          wl_resource_destroy_func_t   destructor,
                          MetaWaylandSurface          *surface,
                          struct wl_resource          *master_resource,
                          guint32                      id)
{
  struct wl_client *client;

  if (extension->resource != NULL)
    return FALSE;

  client = wl_resource_get_client (surface->resource);
  extension->resource = wl_resource_create (client, interface,
                                            MIN (max_version, wl_resource_get_version (master_resource)), id);
  wl_resource_set_implementation (extension->resource, implementation, surface, destructor);

  return TRUE;
}

static void
xdg_shell_use_unstable_version (struct wl_client *client,
                                struct wl_resource *resource,
                                int32_t version)
{
  if (version != XDG_SHELL_VERSION_CURRENT)
    g_warning ("Bad xdg_shell version: %d", version);
}

static void
xdg_shell_pong (struct wl_client *client,
                struct wl_resource *resource,
                uint32_t serial)
{
  MetaDisplay *display = meta_get_display ();

  meta_display_pong_for_serial (display, serial);
}

static void
xdg_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  destroy_window (surface);
  destroy_surface_extension (&surface->xdg_surface);
}

static void
xdg_surface_destroy (struct wl_client *client,
                     struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_surface_set_parent (struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *parent_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWindow *transient_for = NULL;

  if (parent_resource)
    {
      MetaWaylandSurface *parent_surface = wl_resource_get_user_data (parent_resource);
      transient_for = parent_surface->window;
    }

  meta_window_set_transient_for (surface->window, transient_for);
}

static void
xdg_surface_set_margin (struct wl_client *client,
                        struct wl_resource *resource,
                        int32_t left_margin,
                        int32_t right_margin,
                        int32_t top_margin,
                        int32_t bottom_margin)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->pending.frame_extents_changed = TRUE;
  surface->pending.frame_extents.left = left_margin;
  surface->pending.frame_extents.right = right_margin;
  surface->pending.frame_extents.top = top_margin;
  surface->pending.frame_extents.bottom = bottom_margin;
}

static void
xdg_surface_set_title (struct wl_client *client,
                       struct wl_resource *resource,
                       const char *title)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_set_title (surface->window, title);
}

static void
xdg_surface_set_app_id (struct wl_client *client,
                        struct wl_resource *resource,
                        const char *app_id)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_set_wm_class (surface->window, app_id, app_id);
}

static gboolean
begin_grab_op_on_surface (MetaWaylandSurface *surface,
                          MetaWaylandSeat    *seat,
                          MetaGrabOp          grab_op)
{
  MetaWindow *window = surface->window;

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
                                     seat->pointer.grab_x,
                                     seat->pointer.grab_y);
}

static void
xdg_surface_move (struct wl_client *client,
                  struct wl_resource *resource,
                  struct wl_resource *seat_resource,
                  guint32 serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      seat->pointer.focus_surface != surface)
    return;

  begin_grab_op_on_surface (surface, seat, META_GRAB_OP_MOVING);
}

static MetaGrabOp
grab_op_for_xdg_surface_resize_edge (int edge)
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
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      seat->pointer.focus_surface != surface)
    return;

  begin_grab_op_on_surface (surface, seat, grab_op_for_xdg_surface_resize_edge (edges));
}

static void
xdg_surface_ack_configure (struct wl_client *client,
                           struct wl_resource *resource,
                           uint32_t serial)
{
  /* Do nothing for now. In the future, we'd imagine that
   * we'd ignore attaches when we have a state pending that
   * we haven't had the client ACK'd, to prevent a race
   * condition when we have an in-flight attach when the
   * client gets the new state. */
}

static void
xdg_surface_set_maximized (struct wl_client *client,
                           struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  meta_window_maximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
xdg_surface_unset_maximized (struct wl_client *client,
                             struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  meta_window_unmaximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
xdg_surface_set_fullscreen (struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *output_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  meta_window_make_fullscreen (surface->window);
}

static void
xdg_surface_unset_fullscreen (struct wl_client *client,
                              struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  meta_window_unmake_fullscreen (surface->window);
}

static void
xdg_surface_set_minimized (struct wl_client *client,
                           struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  meta_window_minimize (surface->window);
}

static const struct xdg_surface_interface meta_wayland_xdg_surface_interface = {
  xdg_surface_destroy,
  xdg_surface_set_parent,
  xdg_surface_set_margin,
  xdg_surface_set_title,
  xdg_surface_set_app_id,
  xdg_surface_move,
  xdg_surface_resize,
  xdg_surface_ack_configure,
  xdg_surface_set_maximized,
  xdg_surface_unset_maximized,
  xdg_surface_set_fullscreen,
  xdg_surface_unset_fullscreen,
  xdg_surface_set_minimized,
};

static void
xdg_shell_get_xdg_surface (struct wl_client *client,
                           struct wl_resource *resource,
                           guint32 id,
                           struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWindow *window;

  if (!create_surface_extension (&surface->xdg_surface,
                                 META_XDG_SURFACE_VERSION,
                                 &xdg_surface_interface,
                                 &meta_wayland_xdg_surface_interface,
                                 xdg_surface_destructor,
                                 surface, resource, id))
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "xdg_shell::get_xdg_surface already requested");
      return;
    }

  window = meta_window_wayland_new (meta_get_display (), surface);
  meta_wayland_surface_set_window (surface, window);
}

static void
xdg_popup_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  destroy_window (surface);
  destroy_surface_extension (&surface->xdg_popup);
}

static void
xdg_popup_destroy (struct wl_client *client,
                   struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct xdg_popup_interface meta_wayland_xdg_popup_interface = {
  xdg_popup_destroy,
};

static void
xdg_shell_get_xdg_popup (struct wl_client *client,
                         struct wl_resource *resource,
                         uint32_t id,
                         struct wl_resource *surface_resource,
                         struct wl_resource *parent_resource,
                         struct wl_resource *seat_resource,
                         uint32_t serial,
                         int32_t x,
                         int32_t y,
                         uint32_t flags)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWindow *window;

  if (parent_surf == NULL || parent_surf->window == NULL)
    return;

  if (!create_surface_extension (&surface->xdg_popup,
                                 META_XDG_POPUP_VERSION,
                                 &xdg_popup_interface,
                                 &meta_wayland_xdg_popup_interface,
                                 xdg_popup_destructor,
                                 surface, resource, id))
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "xdg_shell::get_xdg_popup already requested");
      return;
    }

  window = meta_window_wayland_new (meta_get_display (), surface);
  window->rect.x = parent_surf->window->rect.x + x;
  window->rect.y = parent_surf->window->rect.y + y;
  window->showing_for_first_time = FALSE;
  window->placed = TRUE;
  meta_window_set_transient_for (window, parent_surf->window);
  meta_window_set_type (window, META_WINDOW_DROPDOWN_MENU);

  meta_wayland_surface_set_window (surface, window);

  meta_wayland_pointer_start_popup_grab (&seat->pointer, surface);
}

static const struct xdg_shell_interface meta_wayland_xdg_shell_interface = {
  xdg_shell_use_unstable_version,
  xdg_shell_get_xdg_surface,
  xdg_shell_get_xdg_popup,
  xdg_shell_pong,
};

typedef struct {
  struct wl_resource *resource;
  struct wl_listener client_destroy_listener;
} XdgShell;

static void
xdg_shell_handle_client_destroy (struct wl_listener *listener, void *data)
{
  XdgShell *xdg_shell = wl_container_of (listener, xdg_shell, client_destroy_listener);
  g_slice_free (XdgShell, xdg_shell);
}

static struct wl_resource *
get_xdg_shell_for_client (struct wl_client *client)
{
  struct wl_listener *listener;
  XdgShell *xdg_shell;

  listener = wl_client_get_destroy_listener (client, xdg_shell_handle_client_destroy);

  /* No xdg_shell has been bound for this client */
  if (listener == NULL)
    return NULL;

  xdg_shell = wl_container_of (listener, xdg_shell, client_destroy_listener);
  return xdg_shell->resource;
}

static void
bind_xdg_shell (struct wl_client *client,
                void *data,
                guint32 version,
                guint32 id)
{
  struct wl_resource *resource;
  XdgShell *xdg_shell;

  if (version != 1)
    {
      g_warning ("using xdg-shell without stable version 1\n");
      return;
    }

  xdg_shell = g_slice_new (XdgShell);

  resource = wl_resource_create (client, &xdg_shell_interface, 1, id);
  wl_resource_set_implementation (resource, &meta_wayland_xdg_shell_interface, data, NULL);
  xdg_shell->resource = wl_resource_create (client, &xdg_shell_interface, 1, id);
  wl_resource_set_implementation (xdg_shell->resource, &meta_wayland_xdg_shell_interface, data, NULL);

  xdg_shell->client_destroy_listener.notify = xdg_shell_handle_client_destroy;
  wl_client_add_destroy_listener (client, &xdg_shell->client_destroy_listener);
}

static void
wl_shell_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  destroy_surface_extension (&surface->wl_shell_surface);
}

static void
wl_shell_surface_pong (struct wl_client *client,
                       struct wl_resource *resource,
                       uint32_t serial)
{
  MetaDisplay *display = meta_get_display ();

  meta_display_pong_for_serial (display, serial);
}

static void
wl_shell_surface_move (struct wl_client *client,
                       struct wl_resource *resource,
                       struct wl_resource *seat_resource,
                       uint32_t serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      seat->pointer.focus_surface != surface)
    return;

  begin_grab_op_on_surface (surface, seat, META_GRAB_OP_MOVING);
}

static MetaGrabOp
grab_op_for_wl_shell_surface_resize_edge (int edge)
{
  switch (edge)
    {
    case WL_SHELL_SURFACE_RESIZE_TOP_LEFT:
      return META_GRAB_OP_RESIZING_NW;
    case WL_SHELL_SURFACE_RESIZE_TOP:
      return META_GRAB_OP_RESIZING_N;
    case WL_SHELL_SURFACE_RESIZE_TOP_RIGHT:
      return META_GRAB_OP_RESIZING_NE;
    case WL_SHELL_SURFACE_RESIZE_RIGHT:
      return META_GRAB_OP_RESIZING_E;
    case WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT:
      return META_GRAB_OP_RESIZING_SE;
    case WL_SHELL_SURFACE_RESIZE_BOTTOM:
      return META_GRAB_OP_RESIZING_S;
    case WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT:
      return META_GRAB_OP_RESIZING_SW;
    case WL_SHELL_SURFACE_RESIZE_LEFT:
      return META_GRAB_OP_RESIZING_W;
    default:
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }
}

static void
wl_shell_surface_resize (struct wl_client *client,
                         struct wl_resource *resource,
                         struct wl_resource *seat_resource,
                         uint32_t serial,
                         uint32_t edges)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (seat->pointer.button_count == 0 ||
      seat->pointer.grab_serial != serial ||
      seat->pointer.focus_surface != surface)
    return;

  begin_grab_op_on_surface (surface, seat, grab_op_for_wl_shell_surface_resize_edge (edges));
}

typedef enum {
  SURFACE_STATE_TOPLEVEL,
  SURFACE_STATE_FULLSCREEN,
  SURFACE_STATE_MAXIMIZED,
} SurfaceState;

static void
wl_shell_surface_set_state (MetaWaylandSurface *surface,
                            SurfaceState        state)
{
  if (state == SURFACE_STATE_FULLSCREEN)
    meta_window_make_fullscreen (surface->window);
  else
    meta_window_unmake_fullscreen (surface->window);

  if (state == SURFACE_STATE_MAXIMIZED)
    meta_window_maximize (surface->window, META_MAXIMIZE_BOTH);
  else
    meta_window_unmaximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
wl_shell_surface_set_toplevel (struct wl_client *client,
                               struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface, SURFACE_STATE_TOPLEVEL);
}

static void
wl_shell_surface_set_transient (struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *parent_resource,
                                int32_t x,
                                int32_t y,
                                uint32_t flags)
{
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface, SURFACE_STATE_TOPLEVEL);

  meta_window_set_transient_for (surface->window, parent_surf->window);
  meta_window_move (surface->window, FALSE,
                    parent_surf->window->rect.x + x,
                    parent_surf->window->rect.y + y);
  surface->window->placed = TRUE;
}

static void
wl_shell_surface_set_fullscreen (struct wl_client *client,
                                 struct wl_resource *resource,
                                 uint32_t method,
                                 uint32_t framerate,
                                 struct wl_resource *output)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface, SURFACE_STATE_FULLSCREEN);
}

static void
wl_shell_surface_set_popup (struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *seat_resource,
                            uint32_t serial,
                            struct wl_resource *parent_resource,
                            int32_t x,
                            int32_t y,
                            uint32_t flags)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);

  wl_shell_surface_set_state (surface, SURFACE_STATE_TOPLEVEL);

  meta_window_set_transient_for (surface->window, parent_surf->window);
  meta_window_move (surface->window, FALSE,
                    parent_surf->window->rect.x + x,
                    parent_surf->window->rect.y + y);
  surface->window->placed = TRUE;

  meta_wayland_pointer_start_popup_grab (&seat->pointer, surface);
}

static void
wl_shell_surface_set_maximized (struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *output)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface, SURFACE_STATE_MAXIMIZED);
}

static void
wl_shell_surface_set_title (struct wl_client *client,
                            struct wl_resource *resource,
                            const char *title)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_set_title (surface->window, title);
}

static void
wl_shell_surface_set_class (struct wl_client *client,
                            struct wl_resource *resource,
                            const char *class_)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_set_wm_class (surface->window, class_, class_);
}

static const struct wl_shell_surface_interface meta_wayland_wl_shell_surface_interface = {
  wl_shell_surface_pong,
  wl_shell_surface_move,
  wl_shell_surface_resize,
  wl_shell_surface_set_toplevel,
  wl_shell_surface_set_transient,
  wl_shell_surface_set_fullscreen,
  wl_shell_surface_set_popup,
  wl_shell_surface_set_maximized,
  wl_shell_surface_set_title,
  wl_shell_surface_set_class,
};

static void
wl_shell_get_shell_surface (struct wl_client *client,
                            struct wl_resource *resource,
                            uint32_t id,
                            struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWindow *window;

  if (!create_surface_extension (&surface->wl_shell_surface,
                                 META_WL_SHELL_SURFACE_VERSION,
                                 &wl_shell_surface_interface,
                                 &meta_wayland_wl_shell_surface_interface,
                                 wl_shell_surface_destructor,
                                 surface, resource, id))
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_shell_surface already requested");
      return;
    }

  window = meta_window_wayland_new (meta_get_display (), surface);
  meta_wayland_surface_set_window (surface, window);
}

static const struct wl_shell_interface meta_wayland_wl_shell_interface = {
  wl_shell_get_shell_surface,
};

static void
bind_wl_shell (struct wl_client *client,
               void             *data,
               uint32_t          version,
               uint32_t          id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_shell_interface,
                                 MIN (META_WL_SHELL_VERSION, version), id);
  wl_resource_set_implementation (resource, &meta_wayland_wl_shell_interface, data, NULL);
}

static void
gtk_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  destroy_surface_extension (&surface->gtk_surface);
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
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  /* Broken client, let it die instead of us */
  if (!surface->window)
    {
      meta_warning ("meta-wayland-surface: set_dbus_properties called with invalid window!\n");
      return;
    }

  meta_window_set_gtk_dbus_properties (surface->window,
                                       application_id,
                                       unique_bus_name,
                                       app_menu_path,
                                       menubar_path,
                                       application_object_path,
                                       window_object_path);
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

  if (!create_surface_extension (&surface->gtk_surface,
                                 META_GTK_SURFACE_VERSION,
                                 &gtk_surface_interface,
                                 &meta_wayland_gtk_surface_interface,
                                 gtk_surface_destructor,
                                 surface, resource, id))
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "gtk_shell::get_gtk_surface already requested");
      return;
    }
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

static void
subsurface_parent_surface_committed (MetaWaylandSurface *surface)
{
  if (surface->sub.pending_pos)
    {
      clutter_actor_set_position (CLUTTER_ACTOR (surface->surface_actor),
                                  surface->sub.pending_x,
                                  surface->sub.pending_y);
      surface->sub.pending_pos = FALSE;
    }

  if (surface->sub.pending_placement_ops)
    {
      GSList *it;
      for (it = surface->sub.pending_placement_ops; it; it = it->next)
        {
          MetaWaylandSubsurfacePlacementOp *op = it->data;
          ClutterActor *surface_actor;
          ClutterActor *parent_actor;
          ClutterActor *sibling_actor;

          if (!op->sibling)
            {
              g_slice_free (MetaWaylandSubsurfacePlacementOp, op);
              continue;
            }

          surface_actor = CLUTTER_ACTOR (surface->surface_actor);
          parent_actor = clutter_actor_get_parent (CLUTTER_ACTOR (surface->sub.parent));
          sibling_actor = CLUTTER_ACTOR (op->sibling->surface_actor);

          switch (op->placement)
            {
            case META_WAYLAND_SUBSURFACE_PLACEMENT_ABOVE:
              clutter_actor_set_child_above_sibling (parent_actor, surface_actor, sibling_actor);
              break;
            case META_WAYLAND_SUBSURFACE_PLACEMENT_BELOW:
              clutter_actor_set_child_below_sibling (parent_actor, surface_actor, sibling_actor);
              break;
            }

          wl_list_remove (&op->sibling_destroy_listener.link);
          g_slice_free (MetaWaylandSubsurfacePlacementOp, op);
        }

      g_slist_free (surface->sub.pending_placement_ops);
      surface->sub.pending_placement_ops = NULL;
    }

  if (surface->sub.synchronous)
    commit_pending_state (surface, &surface->sub.pending);
}

static void
unparent_actor (MetaWaylandSurface *surface)
{
  ClutterActor *parent_actor;
  parent_actor = clutter_actor_get_parent (CLUTTER_ACTOR (surface->surface_actor));
  clutter_actor_remove_child (parent_actor, CLUTTER_ACTOR (surface->surface_actor));
}

static void
wl_subsurface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (surface->sub.parent)
    {
      wl_list_remove (&surface->sub.parent_destroy_listener.link);
      surface->sub.parent->subsurfaces =
        g_list_remove (surface->sub.parent->subsurfaces, surface);
      unparent_actor (surface);
      surface->sub.parent = NULL;
    }

  pending_state_destroy (&surface->sub.pending);
  destroy_surface_extension (&surface->subsurface);
}

static void
wl_subsurface_destroy (struct wl_client *client,
                       struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
wl_subsurface_set_position (struct wl_client *client,
                            struct wl_resource *resource,
                            int32_t x,
                            int32_t y)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->sub.pending_x = x;
  surface->sub.pending_y = y;
  surface->sub.pending_pos = TRUE;
}

static gboolean
is_valid_sibling (MetaWaylandSurface *surface, MetaWaylandSurface *sibling)
{
  if (surface->sub.parent == sibling)
    return TRUE;
  if (surface->sub.parent == sibling->sub.parent)
    return TRUE;
  return FALSE;
}

static void
subsurface_handle_pending_sibling_destroyed (struct wl_listener *listener, void *data)
{
  MetaWaylandSubsurfacePlacementOp *op =
    wl_container_of (listener, op, sibling_destroy_listener);

  op->sibling = NULL;
}

static void
queue_subsurface_placement (MetaWaylandSurface *surface,
                            MetaWaylandSurface *sibling,
                            MetaWaylandSubsurfacePlacement placement)
{
  MetaWaylandSubsurfacePlacementOp *op =
    g_slice_new (MetaWaylandSubsurfacePlacementOp);

  op->placement = placement;
  op->sibling = sibling;
  op->sibling_destroy_listener.notify =
    subsurface_handle_pending_sibling_destroyed;
  wl_resource_add_destroy_listener (sibling->resource,
                                    &op->sibling_destroy_listener);

  surface->sub.pending_placement_ops =
    g_slist_append (surface->sub.pending_placement_ops, op);
}

static void
wl_subsurface_place_above (struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *sibling_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *sibling = wl_resource_get_user_data (sibling_resource);

  if (!is_valid_sibling (surface, sibling))
    {
      wl_resource_post_error (resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
                              "wl_subsurface::place_above: wl_surface@%d is "
                              "not a valid parent or sibling",
                              wl_resource_get_id (sibling->resource));
      return;
    }

  queue_subsurface_placement (surface,
                              sibling,
                              META_WAYLAND_SUBSURFACE_PLACEMENT_ABOVE);
}

static void
wl_subsurface_place_below (struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *sibling_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *sibling = wl_resource_get_user_data (sibling_resource);

  if (!is_valid_sibling (surface, sibling))
    {
      wl_resource_post_error (resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
                              "wl_subsurface::place_below: wl_surface@%d is "
                              "not a valid parent or sibling",
                              wl_resource_get_id (sibling->resource));
      return;
    }

  queue_subsurface_placement (surface,
                              sibling,
                              META_WAYLAND_SUBSURFACE_PLACEMENT_BELOW);
}

static void
wl_subsurface_set_sync (struct wl_client *client,
                        struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->sub.synchronous = TRUE;
}

static void
wl_subsurface_set_desync (struct wl_client *client,
                          struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (surface->sub.synchronous)
    subsurface_parent_surface_committed (surface);

  surface->sub.synchronous = FALSE;
}

static const struct wl_subsurface_interface meta_wayland_subsurface_interface = {
  wl_subsurface_destroy,
  wl_subsurface_set_position,
  wl_subsurface_place_above,
  wl_subsurface_place_below,
  wl_subsurface_set_sync,
  wl_subsurface_set_desync,
};

static void
wl_subcompositor_destroy (struct wl_client *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
surface_handle_parent_surface_destroyed (struct wl_listener *listener,
                                         void *data)
{
  MetaWaylandSurface *surface = wl_container_of (listener,
                                                 surface,
                                                 sub.parent_destroy_listener);

  surface->sub.parent = NULL;
  unparent_actor (surface);
}

static void
wl_subcompositor_get_subsurface (struct wl_client *client,
                                 struct wl_resource *resource,
                                 guint32 id,
                                 struct wl_resource *surface_resource,
                                 struct wl_resource *parent_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandSurface *parent = wl_resource_get_user_data (parent_resource);

  if (!create_surface_extension (&surface->subsurface,
                                 META_GTK_SURFACE_VERSION,
                                 &wl_subsurface_interface,
                                 &meta_wayland_subsurface_interface,
                                 wl_subsurface_destructor,
                                 surface, resource, id))
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_subcompositor::get_subsurface already requested");
      return;
    }

  pending_state_init (&surface->sub.pending);
  surface->sub.parent = parent;
  surface->sub.parent_destroy_listener.notify =
    surface_handle_parent_surface_destroyed;
  wl_resource_add_destroy_listener (parent->resource,
                                    &surface->sub.parent_destroy_listener);
  parent->subsurfaces = g_list_append (parent->subsurfaces, surface);

  clutter_actor_add_child (CLUTTER_ACTOR (parent->surface_actor),
                           CLUTTER_ACTOR (surface->surface_actor));

  sync_reactive (surface);
}

static const struct wl_subcompositor_interface meta_wayland_subcompositor_interface = {
  wl_subcompositor_destroy,
  wl_subcompositor_get_subsurface,
};

static void
bind_subcompositor (struct wl_client *client,
                    void             *data,
                    guint32           version,
                    guint32           id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_subcompositor_interface,
				 MIN (META_WL_SUBCOMPOSITOR_VERSION, version), id);
  wl_resource_set_implementation (resource, &meta_wayland_subcompositor_interface, data, NULL);
}

void
meta_wayland_shell_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
			&xdg_shell_interface, 1,
			compositor, bind_xdg_shell) == NULL)
    g_error ("Failed to register a global xdg-shell object");

  if (wl_global_create (compositor->wayland_display,
			&wl_shell_interface, 1,
			compositor, bind_wl_shell) == NULL)
    g_error ("Failed to register a global wl-shell object");

  if (wl_global_create (compositor->wayland_display,
			&gtk_shell_interface,
			META_GTK_SHELL_VERSION,
			compositor, bind_gtk_shell) == NULL)
    g_error ("Failed to register a global gtk-shell object");

  if (wl_global_create (compositor->wayland_display,
                        &wl_subcompositor_interface,
                        META_WL_SUBCOMPOSITOR_VERSION,
                        compositor, bind_subcompositor) == NULL)
    g_error ("Failed to register a global wl-subcompositor object");
}

static void
fill_states (struct wl_array *states, MetaWindow *window)
{
  uint32_t *s;

  if (META_WINDOW_MAXIMIZED (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_MAXIMIZED;
    }
  if (meta_window_is_fullscreen (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_FULLSCREEN;
    }
  if (meta_grab_op_is_resizing (window->display->grab_op))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_RESIZING;
    }
  if (meta_window_appears_focused (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_ACTIVATED;
    }
}

void
meta_wayland_surface_configure_notify (MetaWaylandSurface *surface,
				       int                 new_width,
				       int                 new_height)
{
  if (surface->xdg_surface.resource)
    {
      struct wl_client *client = wl_resource_get_client (surface->xdg_surface.resource);
      struct wl_display *display = wl_client_get_display (client);
      uint32_t serial = wl_display_next_serial (display);
      struct wl_array states;

      wl_array_init (&states);
      fill_states (&states, surface->window);

      xdg_surface_send_configure (surface->xdg_surface.resource, new_width, new_height, &states, serial);

      wl_array_release (&states);
    }
  else if (surface->wl_shell_surface.resource)
    wl_shell_surface_send_configure (surface->wl_shell_surface.resource,
                                     0, new_width, new_height);
}

void
meta_wayland_surface_ping (MetaWaylandSurface *surface,
                           guint32             serial)
{
  if (surface->xdg_surface.resource)
    {
      struct wl_client *client = wl_resource_get_client (surface->resource);
      struct wl_resource *xdg_shell = get_xdg_shell_for_client (client);

      if (xdg_shell == NULL)
        {
          g_warning ("Trying to ping a surface without an xdg_shell bound. How does this happen?");
          return;
        }

      xdg_shell_send_ping (xdg_shell, serial);
    }
  else if (surface->wl_shell_surface.resource)
    {
      wl_shell_surface_send_ping (surface->wl_shell_surface.resource, serial);
    }
}

void
meta_wayland_surface_delete (MetaWaylandSurface *surface)
{
  if (surface->xdg_surface.resource)
    xdg_surface_send_close (surface->xdg_surface.resource);
}

void
meta_wayland_surface_popup_done (MetaWaylandSurface *surface)
{
  struct wl_client *client = wl_resource_get_client (surface->resource);
  struct wl_display *display = wl_client_get_display (client);
  uint32_t serial = wl_display_next_serial (display);

  if (surface->xdg_popup.resource)
    xdg_popup_send_popup_done (surface->xdg_popup.resource, serial);
  else if (surface->wl_shell_surface.resource)
    wl_shell_surface_send_popup_done (surface->wl_shell_surface.resource);
}
