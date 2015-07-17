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

#include "config.h"

#include "meta-wayland-surface.h"

#include <clutter/clutter.h>
#include <clutter/wayland/clutter-wayland-compositor.h>
#include <clutter/wayland/clutter-wayland-surface.h>
#include <cogl/cogl-wayland-server.h>

#include <wayland-server.h>
#include "gtk-shell-server-protocol.h"
#include "xdg-shell-server-protocol.h"

#include "meta-wayland-private.h"
#include "meta-xwayland-private.h"
#include "meta-wayland-buffer.h"
#include "meta-wayland-region.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-keyboard.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-popup.h"
#include "meta-wayland-data-device.h"
#include "meta-wayland-outputs.h"

#include "meta-cursor-tracker-private.h"
#include "display-private.h"
#include "window-private.h"
#include "meta-window-wayland.h"

#include "compositor/region-utils.h"

#include "meta-surface-actor.h"
#include "meta-surface-actor-wayland.h"
#include "meta-xwayland-private.h"

typedef struct _MetaWaylandSurfaceRolePrivate
{
  MetaWaylandSurface *surface;
} MetaWaylandSurfaceRolePrivate;

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

GType meta_wayland_surface_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MetaWaylandSurface, meta_wayland_surface, G_TYPE_OBJECT);

GType meta_wayland_surface_role_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandSurfaceRole,
                            meta_wayland_surface_role,
                            G_TYPE_OBJECT);

struct _MetaWaylandSurfaceRoleSubsurface
{
  MetaWaylandSurfaceRole parent;
};

GType meta_wayland_surface_role_subsurface_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (MetaWaylandSurfaceRoleSubsurface,
               meta_wayland_surface_role_subsurface,
               META_TYPE_WAYLAND_SURFACE_ROLE);

struct _MetaWaylandSurfaceRoleXdgSurface
{
  MetaWaylandSurfaceRole parent;
};

GType meta_wayland_surface_role_xdg_surface_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (MetaWaylandSurfaceRoleXdgSurface,
               meta_wayland_surface_role_xdg_surface,
               META_TYPE_WAYLAND_SURFACE_ROLE);

struct _MetaWaylandSurfaceRoleXdgPopup
{
  MetaWaylandSurfaceRole parent;
};

GType meta_wayland_surface_role_xdg_popup_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (MetaWaylandSurfaceRoleXdgPopup,
               meta_wayland_surface_role_xdg_popup,
               META_TYPE_WAYLAND_SURFACE_ROLE);

struct _MetaWaylandSurfaceRoleWlShellSurface
{
  MetaWaylandSurfaceRole parent;
};

GType meta_wayland_surface_role_wl_shell_surface_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (MetaWaylandSurfaceRoleWlShellSurface,
               meta_wayland_surface_role_wl_shell_surface,
               META_TYPE_WAYLAND_SURFACE_ROLE);

struct _MetaWaylandSurfaceRoleDND
{
  MetaWaylandSurfaceRole parent;
};

GType meta_wayland_surface_role_dnd_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (MetaWaylandSurfaceRoleDND,
               meta_wayland_surface_role_dnd,
               META_TYPE_WAYLAND_SURFACE_ROLE);

static void
meta_wayland_surface_role_assigned (MetaWaylandSurfaceRole *surface_role);

static void
meta_wayland_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                                  MetaWaylandPendingState *pending);

static gboolean
meta_wayland_surface_role_is_on_output (MetaWaylandSurfaceRole *surface_role,
                                        MetaMonitorInfo *info);

gboolean
meta_wayland_surface_assign_role (MetaWaylandSurface *surface,
                                  GType               role_type)
{
  if (!surface->role)
    {
      MetaWaylandSurfaceRolePrivate *role_priv;

      surface->role = g_object_new (role_type, NULL);
      role_priv =
        meta_wayland_surface_role_get_instance_private (surface->role);
      role_priv->surface = surface;

      meta_wayland_surface_role_assigned (surface->role);

      return TRUE;
    }
  else if (G_OBJECT_TYPE (surface->role) != role_type)
    {
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

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
  unsigned int buffer_width;
  unsigned int buffer_height;
  cairo_rectangle_int_t surface_rect;
  cairo_region_t *scaled_region;
  int i, n_rectangles;

  if (!surface->buffer)
    return;

  /* Intersect the damage region with the surface region before scaling in
   * order to avoid integer overflow when scaling a damage region is too large
   * (for example INT32_MAX which mesa passes). */
  buffer_width = cogl_texture_get_width (surface->buffer->texture);
  buffer_height = cogl_texture_get_height (surface->buffer->texture);
  surface_rect = (cairo_rectangle_int_t) {
    .width = buffer_width / surface->scale,
    .height = buffer_height / surface->scale,
  };
  cairo_region_intersect_rectangle (region, &surface_rect);

  /* The damage region must be in the same coordinate space as the buffer,
   * i.e. scaled with surface->scale. */
  scaled_region = meta_region_scale (region, surface->scale);

  /* First update the buffer. */
  meta_wayland_buffer_process_damage (surface->buffer, scaled_region);

  /* Now damage the actor. The actor expects damage in the unscaled texture
   * coordinate space, i.e. same as the buffer. */
  /* XXX: Should this be a signal / callback on MetaWaylandBuffer instead? */
  n_rectangles = cairo_region_num_rectangles (scaled_region);
  for (i = 0; i < n_rectangles; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (scaled_region, i, &rect);

      meta_surface_actor_process_damage (surface->surface_actor,
                                         rect.x, rect.y,
                                         rect.width, rect.height);
    }

  cairo_region_destroy (scaled_region);
}

void
meta_wayland_surface_queue_pending_state_frame_callbacks (MetaWaylandSurface      *surface,
                                                          MetaWaylandPendingState *pending)
{
  wl_list_insert_list (&surface->compositor->frame_callbacks,
                       &pending->frame_callback_list);
  wl_list_init (&pending->frame_callback_list);
}

static void
dnd_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                    MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  meta_wayland_surface_queue_pending_state_frame_callbacks (surface, pending);

  meta_wayland_data_device_update_dnd_surface (&surface->compositor->seat->data_device);
}

static void
calculate_surface_window_geometry (MetaWaylandSurface *surface,
                                   MetaRectangle      *total_geometry,
                                   float               parent_x,
                                   float               parent_y)
{
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);
  MetaRectangle subsurface_rect;
  MetaRectangle geom;
  GList *l;

  /* Unmapped surfaces don't count. */
  if (!CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (surface_actor)))
    return;

  if (!surface->buffer)
    return;

  meta_surface_actor_wayland_get_subsurface_rect (surface_actor,
                                                  &subsurface_rect);

  geom.x = parent_x + subsurface_rect.x;
  geom.y = parent_x + subsurface_rect.y;
  geom.width = subsurface_rect.width;
  geom.height = subsurface_rect.height;

  meta_rectangle_union (total_geometry, &geom, total_geometry);

  for (l = surface->subsurfaces; l != NULL; l = l->next)
    {
      MetaWaylandSurface *subsurface = l->data;
      calculate_surface_window_geometry (subsurface, total_geometry,
                                         subsurface_rect.x,
                                         subsurface_rect.y);
    }
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
queue_surface_actor_frame_callbacks (MetaWaylandSurface      *surface,
                                     MetaWaylandPendingState *pending)
{
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  meta_surface_actor_wayland_add_frame_callbacks (surface_actor,
                                                  &pending->frame_callback_list);
  wl_list_init (&pending->frame_callback_list);
}

static void
toplevel_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                         MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *window = surface->window;

  queue_surface_actor_frame_callbacks (surface, pending);

  if (META_IS_WAYLAND_SURFACE_ROLE_WL_SHELL_SURFACE (surface->role))
    {
      /* For wl_shell, it's equivalent to an unmap. Semantics
       * are poorly defined, so we can choose some that are
       * convenient for us. */
      if (surface->buffer && !window)
        {
          window = meta_window_wayland_new (meta_get_display (), surface);
          meta_wayland_surface_set_window (surface, window);
        }
      else if (surface->buffer == NULL && window)
        {
          destroy_window (surface);
          return;
        }
    }
  else if (META_IS_WAYLAND_SURFACE_ROLE_XDG_POPUP (surface->role))
    {
      /* Ignore commits if we couldn't grab the pointer */
      if (!window)
        return;
    }
  else
    {
      if (surface->buffer == NULL)
        {
          /* XDG surfaces can't commit NULL buffers */
          wl_resource_post_error (surface->resource,
                                  WL_DISPLAY_ERROR_INVALID_OBJECT,
                                  "Cannot commit a NULL buffer to an xdg_surface");
          return;
        }
    }

  g_assert (window != NULL);

  /* We resize X based surfaces according to X events */
  if (window->client_type == META_WINDOW_CLIENT_TYPE_WAYLAND)
    {
      MetaRectangle geom = { 0 };

      CoglTexture *texture = surface->buffer->texture;
      /* Update the buffer rect immediately. */
      window->buffer_rect.width = cogl_texture_get_width (texture);
      window->buffer_rect.height = cogl_texture_get_height (texture);

      if (pending->has_new_geometry)
        {
          /* If we have new geometry, use it. */
          geom = pending->new_geometry;
          surface->has_set_geometry = TRUE;
        }
      else if (!surface->has_set_geometry)
        {
          /* If the surface has never set any geometry, calculate
           * a default one unioning the surface and all subsurfaces together. */
          calculate_surface_window_geometry (surface, &geom, 0, 0);
        }
      else
        {
          /* Otherwise, keep the geometry the same. */

          /* XXX: We don't store the geometry in any consistent place
           * right now, so we can't re-fetch it. We should change
           * meta_window_wayland_move_resize. */

          /* XXX: This is the common case. Recognize it to prevent
           * a warning. */
          if (pending->dx == 0 && pending->dy == 0)
            return;

          g_warning ("XXX: Attach-initiated move without a new geometry. This is unimplemented right now.");
          return;
        }

      meta_window_wayland_move_resize (window,
                                       &surface->acked_configure_serial,
                                       geom, pending->dx, pending->dy);
      surface->acked_configure_serial.set = FALSE;
    }
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
  state->scale = 0;

  state->input_region = NULL;
  state->input_region_set = FALSE;
  state->opaque_region = NULL;
  state->opaque_region_set = FALSE;

  state->damage = cairo_region_create ();
  state->buffer_destroy_listener.notify = surface_handle_pending_buffer_destroy;
  wl_list_init (&state->frame_callback_list);

  state->has_new_geometry = FALSE;
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

  *to = *from;

  wl_list_init (&to->frame_callback_list);
  wl_list_insert_list (&to->frame_callback_list, &from->frame_callback_list);

  if (to->buffer)
    wl_signal_add (&to->buffer->destroy_signal, &to->buffer_destroy_listener);

  pending_state_init (from);
}

static void
subsurface_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                           MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  queue_surface_actor_frame_callbacks (surface, pending);

  if (surface->buffer != NULL)
    clutter_actor_show (CLUTTER_ACTOR (surface_actor));
  else
    clutter_actor_hide (CLUTTER_ACTOR (surface_actor));
}

/* A non-subsurface is always desynchronized.
 *
 * A subsurface is effectively synchronized if either its parent is
 * synchronized or itself is in synchronized mode. */
static gboolean
is_surface_effectively_synchronized (MetaWaylandSurface *surface)
{
  if (surface->wl_subsurface == NULL)
    {
      return FALSE;
    }
  else
    {
      if (surface->sub.synchronous)
        return TRUE;
      else
        return is_surface_effectively_synchronized (surface->sub.parent);
    }
}

static void
apply_pending_state (MetaWaylandSurface      *surface,
                     MetaWaylandPendingState *pending);

static void
parent_surface_state_applied (gpointer data, gpointer user_data)
{
  MetaWaylandSurface *surface = data;

  if (surface->sub.pending_pos)
    {
      surface->sub.x = surface->sub.pending_x;
      surface->sub.y = surface->sub.pending_y;
      surface->sub.pending_pos = FALSE;
    }

  if (surface->sub.pending_placement_ops)
    {
      GSList *it;
      MetaWaylandSurface *parent = surface->sub.parent;
      ClutterActor *parent_actor =
        clutter_actor_get_parent (CLUTTER_ACTOR (parent->surface_actor));
      ClutterActor *surface_actor =
        surface_actor = CLUTTER_ACTOR (surface->surface_actor);

      for (it = surface->sub.pending_placement_ops; it; it = it->next)
        {
          MetaWaylandSubsurfacePlacementOp *op = it->data;
          ClutterActor *sibling_actor;

          if (!op->sibling)
            {
              g_slice_free (MetaWaylandSubsurfacePlacementOp, op);
              continue;
            }

          sibling_actor = CLUTTER_ACTOR (op->sibling->surface_actor);

          switch (op->placement)
            {
            case META_WAYLAND_SUBSURFACE_PLACEMENT_ABOVE:
              clutter_actor_set_child_above_sibling (parent_actor,
                                                     surface_actor,
                                                     sibling_actor);
              break;
            case META_WAYLAND_SUBSURFACE_PLACEMENT_BELOW:
              clutter_actor_set_child_below_sibling (parent_actor,
                                                     surface_actor,
                                                     sibling_actor);
              break;
            }

          wl_list_remove (&op->sibling_destroy_listener.link);
          g_slice_free (MetaWaylandSubsurfacePlacementOp, op);
        }

      g_slist_free (surface->sub.pending_placement_ops);
      surface->sub.pending_placement_ops = NULL;
    }

  if (is_surface_effectively_synchronized (surface))
    apply_pending_state (surface, &surface->sub.pending);

  meta_surface_actor_wayland_sync_subsurface_state (
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor));
}

static void
apply_pending_state (MetaWaylandSurface      *surface,
                     MetaWaylandPendingState *pending)
{
  if (pending->newly_attached)
    {
      if (!surface->buffer && surface->window)
        meta_window_queue (surface->window, META_QUEUE_CALC_SHOWING);

      surface_set_buffer (surface, pending->buffer);

      if (pending->buffer)
        {
          CoglTexture *texture = meta_wayland_buffer_ensure_texture (pending->buffer);
          meta_surface_actor_wayland_set_texture (META_SURFACE_ACTOR_WAYLAND (surface->surface_actor), texture);
        }
    }

  if (pending->scale > 0)
    surface->scale = pending->scale;

  if (!cairo_region_is_empty (pending->damage))
    surface_process_damage (surface, pending->damage);

  surface->offset_x += pending->dx;
  surface->offset_y += pending->dy;

  if (pending->opaque_region_set)
    {
      if (surface->opaque_region)
        cairo_region_destroy (surface->opaque_region);
      if (pending->opaque_region)
        surface->opaque_region = cairo_region_reference (pending->opaque_region);
      else
        surface->opaque_region = NULL;
    }

  if (pending->input_region_set)
    {
      if (surface->input_region)
        cairo_region_destroy (surface->input_region);
      if (pending->input_region)
        surface->input_region = cairo_region_reference (pending->input_region);
      else
        surface->input_region = NULL;
    }

  if (surface->role)
    {
      meta_wayland_surface_role_commit (surface->role, pending);
      g_assert (wl_list_empty (&pending->frame_callback_list));
    }
  else
    {
      /* Since there is no role assigned to the surface yet, keep frame
       * callbacks queued until a role is assigned and we know how
       * the surface will be drawn.
       */
      wl_list_insert_list (&surface->pending_frame_callback_list,
                           &pending->frame_callback_list);
      wl_list_init (&pending->frame_callback_list);
    }

  meta_surface_actor_wayland_sync_state (
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor));

  pending_state_reset (pending);

  g_list_foreach (surface->subsurfaces, parent_surface_state_applied, NULL);
}

static void
meta_wayland_surface_commit (MetaWaylandSurface *surface)
{
  /*
   * If this is a sub-surface and it is in effective synchronous mode, only
   * cache the pending surface state until either one of the following two
   * scenarios happens:
   *  1) Its parent surface gets its state applied.
   *  2) Its mode changes from synchronized to desynchronized and its parent
   *     surface is in effective desynchronized mode.
   */
  if (is_surface_effectively_synchronized (surface))
    move_pending_state (&surface->pending, &surface->sub.pending);
  else
    apply_pending_state (surface, &surface->pending);
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

  if (surface->pending.buffer)
    wl_list_remove (&surface->pending.buffer_destroy_listener.link);

  surface->pending.newly_attached = TRUE;
  surface->pending.buffer = buffer;
  surface->pending.dx = dx;
  surface->pending.dy = dy;

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
  callback->surface = surface;
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
      cairo_region_t *cr_region = meta_wayland_region_peek_cairo_region (region);
      surface->pending.opaque_region = cairo_region_copy (cr_region);
    }
  surface->pending.opaque_region_set = TRUE;
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
      cairo_region_t *cr_region = meta_wayland_region_peek_cairo_region (region);
      surface->pending.input_region = cairo_region_copy (cr_region);
    }
  surface->pending.input_region_set = TRUE;
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
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  if (scale > 0)
    surface->pending.scale = scale;
  else
    g_warning ("Trying to set invalid buffer_scale of %d\n", scale);
}

static const struct wl_surface_interface meta_wayland_wl_surface_interface = {
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
  if (surface->wl_subsurface)
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
sync_drag_dest_funcs (MetaWaylandSurface *surface)
{
  if (surface->window &&
      surface->window->client_type == META_WINDOW_CLIENT_TYPE_X11)
    surface->dnd.funcs = meta_xwayland_selection_get_drag_dest_funcs ();
  else
    surface->dnd.funcs = meta_wayland_data_device_get_drag_dest_funcs ();
}

static void
surface_entered_output (MetaWaylandSurface *surface,
                        MetaWaylandOutput *wayland_output)
{
  GList *iter;
  struct wl_resource *resource;

  for (iter = wayland_output->resources; iter != NULL; iter = iter->next)
    {
      resource = iter->data;

      if (wl_resource_get_client (resource) !=
          wl_resource_get_client (surface->resource))
        continue;

      wl_surface_send_enter (surface->resource, resource);
    }
}

static void
surface_left_output (MetaWaylandSurface *surface,
                     MetaWaylandOutput *wayland_output)
{
  GList *iter;
  struct wl_resource *resource;

  for (iter = wayland_output->resources; iter != NULL; iter = iter->next)
    {
      resource = iter->data;

      if (wl_resource_get_client (resource) !=
          wl_resource_get_client (surface->resource))
        continue;

      wl_surface_send_leave (surface->resource, resource);
    }
}

static void
set_surface_is_on_output (MetaWaylandSurface *surface,
                          MetaWaylandOutput *wayland_output,
                          gboolean is_on_output);

static void
surface_handle_output_destroy (MetaWaylandOutput *wayland_output,
                               GParamSpec *pspec,
                               MetaWaylandSurface *surface)
{
  set_surface_is_on_output (surface, wayland_output, FALSE);
}

static void
set_surface_is_on_output (MetaWaylandSurface *surface,
                          MetaWaylandOutput *wayland_output,
                          gboolean is_on_output)
{
  gboolean was_on_output = g_hash_table_contains (surface->outputs,
                                                  wayland_output);

  if (!was_on_output && is_on_output)
    {
      g_signal_connect (wayland_output, "output-destroyed",
                        G_CALLBACK (surface_handle_output_destroy),
                        surface);
      g_hash_table_add (surface->outputs, wayland_output);
      surface_entered_output (surface, wayland_output);
    }
  else if (was_on_output && !is_on_output)
    {
      g_hash_table_remove (surface->outputs, wayland_output);
      g_signal_handlers_disconnect_by_func  (
        wayland_output, (gpointer)surface_handle_output_destroy, surface);
      surface_left_output (surface, wayland_output);
    }
}

static gboolean
actor_surface_is_on_output (MetaWaylandSurfaceRole *surface_role,
                            MetaMonitorInfo        *monitor)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActorWayland *actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  return meta_surface_actor_wayland_is_on_monitor (actor, monitor);
}

static void
update_surface_output_state (gpointer key, gpointer value, gpointer user_data)
{
  MetaWaylandOutput *wayland_output = value;
  MetaWaylandSurface *surface = user_data;
  MetaMonitorInfo *monitor;
  gboolean is_on_output;

  g_assert (surface->role);

  monitor = wayland_output->monitor_info;
  if (!monitor)
    {
      set_surface_is_on_output (surface, wayland_output, FALSE);
      return;
    }

  is_on_output = meta_wayland_surface_role_is_on_output (surface->role, monitor);
  set_surface_is_on_output (surface, wayland_output, is_on_output);
}

void
meta_wayland_surface_update_outputs (MetaWaylandSurface *surface)
{
  if (!surface->compositor)
    return;

  g_hash_table_foreach (surface->compositor->outputs,
                        update_surface_output_state,
                        surface);
}

void
meta_wayland_surface_set_window (MetaWaylandSurface *surface,
                                 MetaWindow         *window)
{
  surface->window = window;
  sync_reactive (surface);
  sync_drag_dest_funcs (surface);
}

static void
wl_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandCompositor *compositor = surface->compositor;
  MetaWaylandFrameCallback *cb, *next;

  g_clear_object (&surface->role);

  /* If we still have a window at the time of destruction, that means that
   * the client is disconnecting, as the resources are destroyed in a random
   * order. Simply destroy the window in this case. */
  if (surface->window)
    destroy_window (surface);

  surface_set_buffer (surface, NULL);
  pending_state_destroy (&surface->pending);

  if (surface->opaque_region)
    cairo_region_destroy (surface->opaque_region);
  if (surface->input_region)
    cairo_region_destroy (surface->input_region);

  meta_surface_actor_wayland_surface_destroyed (
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor));

  g_object_unref (surface->surface_actor);

  meta_wayland_compositor_destroy_frame_callbacks (compositor, surface);

  g_hash_table_unref (surface->outputs);

  wl_list_for_each_safe (cb, next, &surface->pending_frame_callback_list, link)
    wl_resource_destroy (cb->resource);

  if (surface->resource)
    wl_resource_set_user_data (surface->resource, NULL);

  if (surface->xdg_surface)
    wl_resource_destroy (surface->xdg_surface);
  if (surface->xdg_popup)
    wl_resource_destroy (surface->xdg_popup);
  if (surface->wl_subsurface)
    wl_resource_destroy (surface->wl_subsurface);
  if (surface->wl_shell_surface)
    wl_resource_destroy (surface->wl_shell_surface);
  if (surface->gtk_surface)
    wl_resource_destroy (surface->gtk_surface);

  g_object_unref (surface);

  meta_wayland_compositor_repick (compositor);
}

MetaWaylandSurface *
meta_wayland_surface_create (MetaWaylandCompositor *compositor,
                             struct wl_client      *client,
                             struct wl_resource    *compositor_resource,
                             guint32                id)
{
  MetaWaylandSurface *surface = g_object_new (META_TYPE_WAYLAND_SURFACE, NULL);

  surface->compositor = compositor;
  surface->scale = 1;

  surface->resource = wl_resource_create (client, &wl_surface_interface, wl_resource_get_version (compositor_resource), id);
  wl_resource_set_implementation (surface->resource, &meta_wayland_wl_surface_interface, surface, wl_surface_destructor);

  surface->buffer_destroy_listener.notify = surface_handle_buffer_destroy;
  surface->surface_actor = g_object_ref_sink (meta_surface_actor_wayland_new (surface));

  wl_list_init (&surface->pending_frame_callback_list);

  sync_drag_dest_funcs (surface);

  surface->outputs = g_hash_table_new (NULL, NULL);

  pending_state_init (&surface->pending);
  return surface;
}

static void
xdg_shell_destroy (struct wl_client *client,
                   struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_shell_use_unstable_version (struct wl_client *client,
                                struct wl_resource *resource,
                                int32_t version)
{
  if (version != XDG_SHELL_VERSION_CURRENT)
    wl_resource_post_error (resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                            "bad xdg-shell version: %d\n", version);
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

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);
  destroy_window (surface);
  surface->xdg_surface = NULL;
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

static void
xdg_surface_show_window_menu (struct wl_client *client,
                              struct wl_resource *resource,
                              struct wl_resource *seat_resource,
                              uint32_t serial,
                              int32_t x,
                              int32_t y)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, NULL, NULL))
    return;

  meta_window_show_menu (surface->window, META_WINDOW_MENU_WM,
                         surface->window->buffer_rect.x + x,
                         surface->window->buffer_rect.y + y);
}

static gboolean
begin_grab_op_on_surface (MetaWaylandSurface *surface,
                          MetaWaylandSeat    *seat,
                          MetaGrabOp          grab_op,
                          gfloat              x,
                          gfloat              y)
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
                                     x, y);
}

static void
xdg_surface_move (struct wl_client *client,
                  struct wl_resource *resource,
                  struct wl_resource *seat_resource,
                  guint32 serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  gfloat x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, &x, &y))
    return;

  begin_grab_op_on_surface (surface, seat, META_GRAB_OP_MOVING, x, y);
}

static MetaGrabOp
grab_op_for_xdg_surface_resize_edge (int edge)
{
  MetaGrabOp op = META_GRAB_OP_WINDOW_BASE;

  if (edge & XDG_SURFACE_RESIZE_EDGE_TOP)
    op |= META_GRAB_OP_WINDOW_DIR_NORTH;
  if (edge & XDG_SURFACE_RESIZE_EDGE_BOTTOM)
    op |= META_GRAB_OP_WINDOW_DIR_SOUTH;
  if (edge & XDG_SURFACE_RESIZE_EDGE_LEFT)
    op |= META_GRAB_OP_WINDOW_DIR_WEST;
  if (edge & XDG_SURFACE_RESIZE_EDGE_RIGHT)
    op |= META_GRAB_OP_WINDOW_DIR_EAST;

  if (op == META_GRAB_OP_WINDOW_BASE)
    {
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }

  return op;
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
  gfloat x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, &x, &y))
    return;

  begin_grab_op_on_surface (surface, seat, grab_op_for_xdg_surface_resize_edge (edges), x, y);
}

static void
xdg_surface_ack_configure (struct wl_client *client,
                           struct wl_resource *resource,
                           uint32_t serial)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->acked_configure_serial.set = TRUE;
  surface->acked_configure_serial.value = serial;
}

static void
xdg_surface_set_window_geometry (struct wl_client *client,
                                 struct wl_resource *resource,
                                 int32_t x, int32_t y, int32_t width, int32_t height)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->pending.has_new_geometry = TRUE;
  surface->pending.new_geometry.x = x;
  surface->pending.new_geometry.y = y;
  surface->pending.new_geometry.width = width;
  surface->pending.new_geometry.height = height;
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
  xdg_surface_set_title,
  xdg_surface_set_app_id,
  xdg_surface_show_window_menu,
  xdg_surface_move,
  xdg_surface_resize,
  xdg_surface_ack_configure,
  xdg_surface_set_window_geometry,
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

  if (surface->xdg_surface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "xdg_shell::get_xdg_surface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_XDG_SURFACE))
    {
      wl_resource_post_error (resource, XDG_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface->xdg_surface = wl_resource_create (client, &xdg_surface_interface, wl_resource_get_version (resource), id);
  wl_resource_set_implementation (surface->xdg_surface, &meta_wayland_xdg_surface_interface, surface, xdg_surface_destructor);

  surface->xdg_shell_resource = resource;

  window = meta_window_wayland_new (meta_get_display (), surface);
  meta_wayland_surface_set_window (surface, window);
}

static void
xdg_popup_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);
  if (surface->popup.parent)
    {
      wl_list_remove (&surface->popup.parent_destroy_listener.link);
      surface->popup.parent = NULL;
    }

  if (surface->popup.popup)
    meta_wayland_popup_dismiss (surface->popup.popup);

  surface->xdg_popup = NULL;
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
handle_popup_parent_destroyed (struct wl_listener *listener, void *data)
{
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, popup.parent_destroy_listener);

  wl_resource_post_error (surface->xdg_popup,
                          XDG_POPUP_ERROR_NOT_THE_TOPMOST_POPUP,
                          "destroyed popup not top most popup");
  surface->popup.parent = NULL;

  destroy_window (surface);
}

static void
handle_popup_destroyed (struct wl_listener *listener, void *data)
{
  MetaWaylandPopup *popup = data;
  MetaWaylandSurface *top_popup;
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, popup.destroy_listener);

  top_popup = meta_wayland_popup_get_top_popup (popup);
  if (surface != top_popup)
    {
      wl_resource_post_error (surface->xdg_popup,
                              XDG_POPUP_ERROR_NOT_THE_TOPMOST_POPUP,
                              "destroyed popup not top most popup");
    }

  surface->popup.popup = NULL;

  destroy_window (surface);
}

static void
xdg_shell_get_xdg_popup (struct wl_client *client,
                         struct wl_resource *resource,
                         uint32_t id,
                         struct wl_resource *surface_resource,
                         struct wl_resource *parent_resource,
                         struct wl_resource *seat_resource,
                         uint32_t serial,
                         int32_t x,
                         int32_t y)
{
  struct wl_resource *popup_resource;
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSurface *top_popup;
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWindow *window;
  MetaDisplay *display = meta_get_display ();
  MetaWaylandPopup *popup;

  if (surface->xdg_popup != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "xdg_shell::get_xdg_popup already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_XDG_POPUP))
    {
      wl_resource_post_error (resource, XDG_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  if (parent_surf == NULL ||
      parent_surf->window == NULL ||
      (parent_surf->xdg_popup == NULL && parent_surf->xdg_surface == NULL))
    {
      wl_resource_post_error (resource,
                              XDG_POPUP_ERROR_INVALID_PARENT,
                              "invalid parent surface");
      return;
    }

  top_popup = meta_wayland_pointer_get_top_popup (&seat->pointer);
  if ((top_popup == NULL && parent_surf->xdg_surface == NULL) ||
      (top_popup != NULL && parent_surf != top_popup))
    {
      wl_resource_post_error (resource,
                              XDG_POPUP_ERROR_NOT_THE_TOPMOST_POPUP,
                              "parent not top most surface");
      return;
    }

  popup_resource = wl_resource_create (client, &xdg_popup_interface,
                                       wl_resource_get_version (resource), id);
  wl_resource_set_implementation (popup_resource,
                                  &meta_wayland_xdg_popup_interface,
                                  surface,
                                  xdg_popup_destructor);

  if (!meta_wayland_pointer_can_popup (&seat->pointer, serial))
    {
      xdg_popup_send_popup_done (popup_resource);
      return;
    }

  surface->xdg_popup = popup_resource;
  surface->xdg_shell_resource = resource;

  surface->popup.parent = parent_surf;
  surface->popup.parent_destroy_listener.notify = handle_popup_parent_destroyed;
  wl_resource_add_destroy_listener (parent_surf->resource,
                                    &surface->popup.parent_destroy_listener);

  window = meta_window_wayland_new (display, surface);
  meta_window_wayland_place_relative_to (window, parent_surf->window, x, y);
  window->showing_for_first_time = FALSE;

  meta_wayland_surface_set_window (surface, window);

  meta_window_focus (window, meta_display_get_current_time (display));
  popup = meta_wayland_pointer_start_popup_grab (&seat->pointer, surface);
  if (popup == NULL)
    {
      destroy_window (surface);
      return;
    }

  surface->popup.destroy_listener.notify = handle_popup_destroyed;
  surface->popup.popup = popup;
  wl_signal_add (meta_wayland_popup_get_destroy_signal (popup),
                 &surface->popup.destroy_listener);
}

static const struct xdg_shell_interface meta_wayland_xdg_shell_interface = {
  xdg_shell_destroy,
  xdg_shell_use_unstable_version,
  xdg_shell_get_xdg_surface,
  xdg_shell_get_xdg_popup,
  xdg_shell_pong,
};

static void
bind_xdg_shell (struct wl_client *client,
                void *data,
                guint32 version,
                guint32 id)
{
  struct wl_resource *resource;

  if (version != META_XDG_SHELL_VERSION)
    {
      g_warning ("using xdg-shell without stable version %d\n", META_XDG_SHELL_VERSION);
      return;
    }

  resource = wl_resource_create (client, &xdg_shell_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_xdg_shell_interface, data, NULL);
}

static void
wl_shell_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);
  surface->wl_shell_surface = NULL;
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
  gfloat x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, &x, &y))
    return;

  begin_grab_op_on_surface (surface, seat, META_GRAB_OP_MOVING, x, y);
}

static MetaGrabOp
grab_op_for_wl_shell_surface_resize_edge (int edge)
{
  MetaGrabOp op = META_GRAB_OP_WINDOW_BASE;

  if (edge & WL_SHELL_SURFACE_RESIZE_TOP)
    op |= META_GRAB_OP_WINDOW_DIR_NORTH;
  if (edge & WL_SHELL_SURFACE_RESIZE_BOTTOM)
    op |= META_GRAB_OP_WINDOW_DIR_SOUTH;
  if (edge & WL_SHELL_SURFACE_RESIZE_LEFT)
    op |= META_GRAB_OP_WINDOW_DIR_WEST;
  if (edge & WL_SHELL_SURFACE_RESIZE_RIGHT)
    op |= META_GRAB_OP_WINDOW_DIR_EAST;

  if (op == META_GRAB_OP_WINDOW_BASE)
    {
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }

  return op;
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
  gfloat x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, &x, &y))
    return;

  begin_grab_op_on_surface (surface, seat, grab_op_for_wl_shell_surface_resize_edge (edges), x, y);
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
  meta_window_wayland_place_relative_to (surface->window,
                                         parent_surf->window,
                                         x, y);
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
handle_wl_shell_popup_parent_destroyed (struct wl_listener *listener,
                                        void *data)
{
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, popup.parent_destroy_listener);

  wl_list_remove (&surface->popup.parent_destroy_listener.link);
  surface->popup.parent = NULL;
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

  if (!meta_wayland_pointer_can_popup (&seat->pointer, serial))
    {
      wl_shell_surface_send_popup_done (resource);
      return;
    }

  meta_window_set_transient_for (surface->window, parent_surf->window);
  meta_window_wayland_place_relative_to (surface->window,
                                         parent_surf->window,
                                         x, y);

  if (!surface->popup.parent)
    {
      surface->popup.parent = parent_surf;
      surface->popup.parent_destroy_listener.notify =
        handle_wl_shell_popup_parent_destroyed;
      wl_resource_add_destroy_listener (parent_surf->resource,
                                        &surface->popup.parent_destroy_listener);
    }

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

  if (surface->wl_shell_surface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_shell_surface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_WL_SHELL_SURFACE))
    {
      wl_resource_post_error (resource, WL_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface->wl_shell_surface = wl_resource_create (client, &wl_shell_surface_interface, wl_resource_get_version (resource), id);
  wl_resource_set_implementation (surface->wl_shell_surface, &meta_wayland_wl_shell_surface_interface, surface, wl_shell_surface_destructor);

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

  resource = wl_resource_create (client, &wl_shell_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_wl_shell_interface, data, NULL);
}

static void
gtk_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->gtk_surface = NULL;
}

static void
gtk_surface_set_dbus_properties (struct wl_client   *client,
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

static void
gtk_surface_set_modal (struct wl_client   *client,
                       struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (surface->is_modal)
    return;

  surface->is_modal = TRUE;
  meta_window_set_type (surface->window, META_WINDOW_MODAL_DIALOG);
}

static void
gtk_surface_unset_modal (struct wl_client   *client,
                         struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (!surface->is_modal)
    return;

  surface->is_modal = FALSE;
  meta_window_set_type (surface->window, META_WINDOW_NORMAL);
}

static const struct gtk_surface_interface meta_wayland_gtk_surface_interface = {
  gtk_surface_set_dbus_properties,
  gtk_surface_set_modal,
  gtk_surface_unset_modal,
};

static void
get_gtk_surface (struct wl_client *client,
                 struct wl_resource *resource,
                 guint32 id,
                 struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  if (surface->gtk_surface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "gtk_shell::get_gtk_surface already requested");
      return;
    }

  surface->gtk_surface = wl_resource_create (client, &gtk_surface_interface, wl_resource_get_version (resource), id);
  wl_resource_set_implementation (surface->gtk_surface, &meta_wayland_gtk_surface_interface, surface, gtk_surface_destructor);
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
  uint32_t capabilities = 0;

  resource = wl_resource_create (client, &gtk_shell_interface, version, id);

  if (version != META_GTK_SHELL_VERSION)
    {
      wl_resource_post_error (resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "Incompatible gtk-shell version "
                              "(supported version: %d)",
                              META_GTK_SHELL_VERSION);
      return;
    }

  wl_resource_set_implementation (resource, &meta_wayland_gtk_shell_interface, data, NULL);

  if (!meta_prefs_get_show_fallback_app_menu ())
    capabilities = GTK_SHELL_CAPABILITY_GLOBAL_APP_MENU;

  gtk_shell_send_capabilities (resource, capabilities);
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

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);
  if (surface->sub.parent)
    {
      wl_list_remove (&surface->sub.parent_destroy_listener.link);
      surface->sub.parent->subsurfaces =
        g_list_remove (surface->sub.parent->subsurfaces, surface);
      unparent_actor (surface);
      surface->sub.parent = NULL;
    }

  pending_state_destroy (&surface->sub.pending);
  surface->wl_subsurface = NULL;
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
  gboolean was_effectively_synchronized;

  was_effectively_synchronized = is_surface_effectively_synchronized (surface);
  surface->sub.synchronous = FALSE;
  if (was_effectively_synchronized &&
      !is_surface_effectively_synchronized (surface))
    apply_pending_state (surface, &surface->sub.pending);
}

static const struct wl_subsurface_interface meta_wayland_wl_subsurface_interface = {
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

  if (surface->wl_subsurface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_subcompositor::get_subsurface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_SUBSURFACE))
    {
      /* FIXME: There is no subcompositor "role" error yet, so lets just use something
       * similar until there is.
       */
      wl_resource_post_error (resource, WL_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface->wl_subsurface = wl_resource_create (client, &wl_subsurface_interface, wl_resource_get_version (resource), id);
  wl_resource_set_implementation (surface->wl_subsurface, &meta_wayland_wl_subsurface_interface, surface, wl_subsurface_destructor);

  pending_state_init (&surface->sub.pending);
  surface->sub.synchronous = TRUE;
  surface->sub.parent = parent;
  surface->sub.parent_destroy_listener.notify = surface_handle_parent_surface_destroyed;
  wl_resource_add_destroy_listener (parent->resource, &surface->sub.parent_destroy_listener);
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

  resource = wl_resource_create (client, &wl_subcompositor_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_subcompositor_interface, data, NULL);
}

void
meta_wayland_shell_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
                        &xdg_shell_interface,
                        META_XDG_SHELL_VERSION,
                        compositor, bind_xdg_shell) == NULL)
    g_error ("Failed to register a global xdg-shell object");

  if (wl_global_create (compositor->wayland_display,
                        &wl_shell_interface,
                        META_WL_SHELL_VERSION,
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
                                       int                 new_height,
                                       MetaWaylandSerial  *sent_serial)
{
  if (surface->xdg_surface)
    {
      struct wl_client *client = wl_resource_get_client (surface->xdg_surface);
      struct wl_display *display = wl_client_get_display (client);
      uint32_t serial = wl_display_next_serial (display);
      struct wl_array states;

      wl_array_init (&states);
      fill_states (&states, surface->window);

      xdg_surface_send_configure (surface->xdg_surface, new_width, new_height, &states, serial);

      wl_array_release (&states);

      if (sent_serial)
        {
          sent_serial->set = TRUE;
          sent_serial->value = serial;
        }
    }
  else if (surface->xdg_popup)
    {
      /* This can happen if the popup window loses or receives focus.
       * Just ignore it. */
    }
  else if (surface->wl_shell_surface)
    wl_shell_surface_send_configure (surface->wl_shell_surface,
                                     0, new_width, new_height);
  else
    g_assert_not_reached ();
}

void
meta_wayland_surface_ping (MetaWaylandSurface *surface,
                           guint32             serial)
{
  if (surface->xdg_shell_resource)
    xdg_shell_send_ping (surface->xdg_shell_resource, serial);
  else if (surface->wl_shell_surface)
    wl_shell_surface_send_ping (surface->wl_shell_surface, serial);
}

void
meta_wayland_surface_delete (MetaWaylandSurface *surface)
{
  if (surface->xdg_surface)
    xdg_surface_send_close (surface->xdg_surface);
}

void
meta_wayland_surface_popup_done (MetaWaylandSurface *surface)
{
  if (surface->xdg_popup)
    xdg_popup_send_popup_done (surface->xdg_popup);
  else if (surface->wl_shell_surface)
    wl_shell_surface_send_popup_done (surface->wl_shell_surface);
}

void
meta_wayland_surface_drag_dest_focus_in (MetaWaylandSurface   *surface,
                                         MetaWaylandDataOffer *offer)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->focus_in (data_device, surface, offer);
}

void
meta_wayland_surface_drag_dest_motion (MetaWaylandSurface *surface,
                                       const ClutterEvent *event)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->motion (data_device, surface, event);
}

void
meta_wayland_surface_drag_dest_focus_out (MetaWaylandSurface *surface)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->focus_out (data_device, surface);
}

void
meta_wayland_surface_drag_dest_drop (MetaWaylandSurface *surface)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->drop (data_device, surface);
}

MetaWindow *
meta_wayland_surface_get_toplevel_window (MetaWaylandSurface *surface)
{
  while (surface)
    {
      if (surface->window)
        {
          if (surface->popup.parent)
            surface = surface->popup.parent;
          else
            return surface->window;
        }
      else
        surface = surface->sub.parent;
    }

  return NULL;
}

static void
meta_wayland_surface_init (MetaWaylandSurface *surface)
{
}

static void
meta_wayland_surface_class_init (MetaWaylandSurfaceClass *klass)
{
}

static void
meta_wayland_surface_role_init (MetaWaylandSurfaceRole *role)
{
}

static void
meta_wayland_surface_role_class_init (MetaWaylandSurfaceRoleClass *klass)
{
}

static void
meta_wayland_surface_role_assigned (MetaWaylandSurfaceRole *surface_role)
{
  META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role)->assigned (surface_role);
}

static void
meta_wayland_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                                  MetaWaylandPendingState *pending)
{
  META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role)->commit (surface_role,
                                                              pending);
}

static gboolean
meta_wayland_surface_role_is_on_output (MetaWaylandSurfaceRole *surface_role,
                                        MetaMonitorInfo        *monitor)
{
  MetaWaylandSurfaceRoleClass *klass;

  klass = META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role);
  if (klass->is_on_output)
    return klass->is_on_output (surface_role, monitor);
  else
    return FALSE;
}

MetaWaylandSurface *
meta_wayland_surface_role_get_surface (MetaWaylandSurfaceRole *role)
{
  MetaWaylandSurfaceRolePrivate *priv =
    meta_wayland_surface_role_get_instance_private (role);

  return priv->surface;
}

void
meta_wayland_surface_queue_pending_frame_callbacks (MetaWaylandSurface *surface)
{
  wl_list_insert_list (&surface->compositor->frame_callbacks,
                       &surface->pending_frame_callback_list);
  wl_list_init (&surface->pending_frame_callback_list);
}

static void
default_role_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  meta_wayland_surface_queue_pending_frame_callbacks (surface);
}

static void
actor_surface_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  meta_surface_actor_wayland_add_frame_callbacks (surface_actor,
                                                  &surface->pending_frame_callback_list);
  wl_list_init (&surface->pending_frame_callback_list);
}

static void
meta_wayland_surface_role_dnd_init (MetaWaylandSurfaceRoleDND *role)
{
}

static void
meta_wayland_surface_role_dnd_class_init (MetaWaylandSurfaceRoleDNDClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = default_role_assigned;
  surface_role_class->commit = dnd_surface_commit;
}

static void
meta_wayland_surface_role_xdg_surface_init (MetaWaylandSurfaceRoleXdgSurface *role)
{
}

static void
meta_wayland_surface_role_xdg_surface_class_init (MetaWaylandSurfaceRoleXdgSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = actor_surface_assigned;
  surface_role_class->commit = toplevel_surface_commit;
  surface_role_class->is_on_output = actor_surface_is_on_output;
}

static void
meta_wayland_surface_role_xdg_popup_init (MetaWaylandSurfaceRoleXdgPopup *role)
{
}

static void
meta_wayland_surface_role_xdg_popup_class_init (MetaWaylandSurfaceRoleXdgPopupClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = actor_surface_assigned;
  surface_role_class->commit = toplevel_surface_commit;
  surface_role_class->is_on_output = actor_surface_is_on_output;
}

static void
meta_wayland_surface_role_wl_shell_surface_init (MetaWaylandSurfaceRoleWlShellSurface *role)
{
}

static void
meta_wayland_surface_role_wl_shell_surface_class_init (MetaWaylandSurfaceRoleWlShellSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = actor_surface_assigned;
  surface_role_class->commit = toplevel_surface_commit;
  surface_role_class->is_on_output = actor_surface_is_on_output;
}

static void
meta_wayland_surface_role_subsurface_init (MetaWaylandSurfaceRoleSubsurface *role)
{
}

static void
meta_wayland_surface_role_subsurface_class_init (MetaWaylandSurfaceRoleSubsurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = actor_surface_assigned;
  surface_role_class->commit = subsurface_surface_commit;
  surface_role_class->is_on_output = actor_surface_is_on_output;
}
