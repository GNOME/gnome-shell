/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
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
 *
 * Written by:
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#include "config.h"

#include "meta-wayland-pointer-constraints.h"

#include <glib.h>

#include "meta/meta-backend.h"
#include "meta-wayland-private.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-surface.h"
#include "meta-wayland-region.h"
#include "meta-xwayland.h"
#include "meta-pointer-lock-wayland.h"
#include "meta-pointer-confinement-wayland.h"
#include "window-private.h"
#include "backends/meta-backend-private.h"
#include "backends/native/meta-backend-native.h"
#include "backends/meta-pointer-constraint.h"

#include "pointer-constraints-unstable-v1-server-protocol.h"

static GQuark quark_pending_constraint_state = 0;
static GQuark quark_surface_pointer_constraints_data = 0;

struct _MetaWaylandPointerConstraint
{
  GObject parent;

  MetaWaylandSurface *surface;
  gboolean is_enabled;
  cairo_region_t *region;
  struct wl_resource *resource;
  MetaWaylandPointerGrab grab;
  MetaWaylandSeat *seat;
  enum zwp_pointer_constraints_v1_lifetime lifetime;
  gulong pointer_focus_surface_handler_id;

  gboolean hint_set;
  wl_fixed_t x_hint;
  wl_fixed_t y_hint;

  MetaPointerConstraint *constraint;
};

typedef struct _MetaWaylandSurfacePointerConstraintsData
{
  MetaWaylandSurface *surface;

  GList *pointer_constraints;

  MetaWindow *window;
  gulong window_associated_handler_id;

  gulong appears_changed_handler_id;
  gulong raised_handler_id;
} MetaWaylandSurfacePointerConstraintsData;

typedef struct
{
  MetaWaylandPointerConstraint *constraint;
  cairo_region_t *region;
  gulong applied_handler_id;
} MetaWaylandPendingConstraintState;

typedef struct
{
  GList *pending_constraint_states;
} MetaWaylandPendingConstraintStateContainer;

G_DEFINE_TYPE (MetaWaylandPointerConstraint, meta_wayland_pointer_constraint,
               G_TYPE_OBJECT);

static const struct zwp_locked_pointer_v1_interface locked_pointer_interface;
static const struct zwp_confined_pointer_v1_interface confined_pointer_interface;
static const MetaWaylandPointerGrabInterface locked_pointer_grab_interface;
static const MetaWaylandPointerGrabInterface confined_pointer_grab_interface;

static void
meta_wayland_pointer_constraint_destroy (MetaWaylandPointerConstraint *constraint);

static void
meta_wayland_pointer_constraint_maybe_enable (MetaWaylandPointerConstraint *constraint);

static void
meta_wayland_pointer_constraint_maybe_enable_for_window (MetaWindow *window);

static void
meta_wayland_pointer_constraint_maybe_remove_for_seat (MetaWaylandSeat *seat,
                                                       MetaWindow      *window);

static MetaWaylandSurfacePointerConstraintsData *
get_surface_constraints_data (MetaWaylandSurface *surface)
{
  return g_object_get_qdata (G_OBJECT (surface),
                             quark_surface_pointer_constraints_data);
}

static void
appears_focused_changed (MetaWindow *window,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  MetaWaylandCompositor *wayland_compositor;

  wayland_compositor = meta_wayland_compositor_get_default ();
  meta_wayland_pointer_constraint_maybe_remove_for_seat (wayland_compositor->seat,
                                                         window);

  meta_wayland_pointer_constraint_maybe_enable_for_window (window);
}

static void
window_raised (MetaWindow *window)
{
  meta_wayland_pointer_constraint_maybe_enable_for_window (window);
}

static void
connect_window (MetaWaylandSurfacePointerConstraintsData *data,
                MetaWindow                               *window)
{
  data->window = window;
  g_object_add_weak_pointer (G_OBJECT (data->window),
                             (gpointer *) &data->window);
  data->appears_changed_handler_id =
    g_signal_connect (data->window, "notify::appears-focused",
                      G_CALLBACK (appears_focused_changed), NULL);
  data->raised_handler_id =
    g_signal_connect (data->window, "raised",
                      G_CALLBACK (window_raised), NULL);
}

static void
window_associated (MetaWaylandSurfaceRole                   *surface_role,
                   MetaWaylandSurfacePointerConstraintsData *data)
{
  MetaWaylandSurface *surface = data->surface;

  connect_window (data, surface->window);
  g_signal_handler_disconnect (surface, data->window_associated_handler_id);
  data->window_associated_handler_id = 0;

  meta_wayland_pointer_constraint_maybe_enable_for_window (surface->window);
}

static MetaWaylandSurfacePointerConstraintsData *
surface_constraint_data_new (MetaWaylandSurface *surface)
{
  MetaWaylandSurfacePointerConstraintsData *data;

  data = g_new0 (MetaWaylandSurfacePointerConstraintsData, 1);

  data->surface = surface;

  if (surface->window)
    {
      connect_window (data, surface->window);
    }
  else if (meta_xwayland_is_xwayland_surface (surface))
    {
      data->window_associated_handler_id =
        g_signal_connect (surface->role, "window-associated",
                          G_CALLBACK (window_associated),
                          data);
    }
  else
    {
      /* TODO: Support constraints on non-toplevel windows, such as subsurfaces.
       */
      g_warn_if_reached ();
    }

  return data;
}
static void
surface_constraint_data_free (MetaWaylandSurfacePointerConstraintsData *data)
{
  if (data->window)
    {
      g_signal_handler_disconnect (data->window,
                                   data->appears_changed_handler_id);
      g_signal_handler_disconnect (data->window,
                                   data->raised_handler_id);
      g_object_remove_weak_pointer (G_OBJECT (data->window),
                                    (gpointer *) &data->window);
    }
  else
    {
      g_signal_handler_disconnect (data->surface->role,
                                   data->window_associated_handler_id);
    }

  g_list_free_full (data->pointer_constraints,
                    (GDestroyNotify) meta_wayland_pointer_constraint_destroy);
  g_free (data);
}

static void
constrained_surface_destroyed (MetaWaylandSurface                       *surface,
                               MetaWaylandSurfacePointerConstraintsData *data)
{
  surface_constraint_data_free (data);
}

static MetaWaylandSurfacePointerConstraintsData *
ensure_surface_constraints_data (MetaWaylandSurface *surface)
{
  MetaWaylandSurfacePointerConstraintsData *data;

  data = get_surface_constraints_data (surface);
  if (!data)
    {
      data = surface_constraint_data_new (surface);
      g_object_set_qdata (G_OBJECT (surface),
                          quark_surface_pointer_constraints_data,
                          data);
      g_signal_connect (surface, "destroy",
                        G_CALLBACK (constrained_surface_destroyed), data);
    }

  return data;
}

static void
surface_add_pointer_constraint (MetaWaylandSurface           *surface,
                                MetaWaylandPointerConstraint *constraint)
{
  MetaWaylandSurfacePointerConstraintsData *data;

  data = ensure_surface_constraints_data (surface);
  data->pointer_constraints = g_list_append (data->pointer_constraints,
                                             constraint);
}

static void
surface_remove_pointer_constraints (MetaWaylandSurface           *surface,
                                    MetaWaylandPointerConstraint *constraint)
{
  MetaWaylandSurfacePointerConstraintsData *data;

  data = get_surface_constraints_data (surface);
  data->pointer_constraints =
    g_list_remove (data->pointer_constraints, constraint);

  if (!data->pointer_constraints)
    {
      g_object_set_qdata (G_OBJECT (surface),
                          quark_surface_pointer_constraints_data,
                          NULL);
    }
}

static void
pointer_focus_surface_changed (MetaWaylandPointer           *pointer,
                               MetaWaylandPointerConstraint *constraint)
{
  meta_wayland_pointer_constraint_maybe_enable (constraint);
}

static MetaWaylandPointerConstraint *
meta_wayland_pointer_constraint_new (MetaWaylandSurface                      *surface,
                                     MetaWaylandSeat                         *seat,
                                     MetaWaylandRegion                       *region,
                                     enum zwp_pointer_constraints_v1_lifetime lifetime,
                                     struct wl_resource                      *resource,
                                     const MetaWaylandPointerGrabInterface   *grab_interface)
{
  MetaWaylandPointerConstraint *constraint;

  constraint = g_object_new (META_TYPE_WAYLAND_POINTER_CONSTRAINT, NULL);
  if (!constraint)
    return NULL;

  constraint->surface = surface;
  constraint->seat = seat;
  constraint->lifetime = lifetime;
  constraint->resource = resource;
  constraint->grab.interface = grab_interface;

  if (region)
    {
      constraint->region =
        cairo_region_copy (meta_wayland_region_peek_cairo_region (region));
    }
  else
    {
      constraint->region = NULL;
    }

  constraint->pointer_focus_surface_handler_id =
    g_signal_connect (seat->pointer, "focus-surface-changed",
                      G_CALLBACK (pointer_focus_surface_changed),
                      constraint);

  return constraint;
}

static gboolean
meta_wayland_pointer_constraint_is_enabled (MetaWaylandPointerConstraint *constraint)
{
  return constraint->is_enabled;
}

static void
meta_wayland_pointer_constraint_notify_activated (MetaWaylandPointerConstraint *constraint)
{
  struct wl_resource *resource = constraint->resource;

  if (wl_resource_instance_of (resource,
                               &zwp_locked_pointer_v1_interface,
                               &locked_pointer_interface))
    {
      zwp_locked_pointer_v1_send_locked (resource);
    }
  else if (wl_resource_instance_of (resource,
                                    &zwp_confined_pointer_v1_interface,
                                    &confined_pointer_interface))
    {
      zwp_confined_pointer_v1_send_confined (resource);
    }
}

static void
meta_wayland_pointer_constraint_notify_deactivated (MetaWaylandPointerConstraint *constraint)
{
  struct wl_resource *resource = constraint->resource;

  if (wl_resource_instance_of (resource,
                               &zwp_locked_pointer_v1_interface,
                               &locked_pointer_interface))
    zwp_locked_pointer_v1_send_unlocked (resource);
  else if (wl_resource_instance_of (resource,
                                    &zwp_confined_pointer_v1_interface,
                                    &confined_pointer_interface))
    zwp_confined_pointer_v1_send_unconfined (resource);
}

static MetaPointerConstraint *
meta_wayland_pointer_constraint_create_pointer_constraint (MetaWaylandPointerConstraint *constraint)
{
  struct wl_resource *resource = constraint->resource;

  if (wl_resource_instance_of (resource,
                               &zwp_locked_pointer_v1_interface,
                               &locked_pointer_interface))
    {
      return meta_pointer_lock_wayland_new ();
    }
  else if (wl_resource_instance_of (resource,
                                    &zwp_confined_pointer_v1_interface,
                                    &confined_pointer_interface))
    {
      return meta_pointer_confinement_wayland_new (constraint);
    }
  g_assert_not_reached ();
  return NULL;
}

static void
meta_wayland_pointer_constraint_enable (MetaWaylandPointerConstraint *constraint)
{
  MetaBackend *backend = meta_get_backend ();

  g_assert (!constraint->is_enabled);

  constraint->is_enabled = TRUE;
  meta_wayland_pointer_constraint_notify_activated (constraint);
  meta_wayland_pointer_start_grab (constraint->seat->pointer,
                                   &constraint->grab);

  constraint->constraint =
    meta_wayland_pointer_constraint_create_pointer_constraint (constraint);
  meta_backend_set_client_pointer_constraint (backend, constraint->constraint);
  g_object_add_weak_pointer (G_OBJECT (constraint->constraint),
                             (gpointer *) &constraint->constraint);
  g_object_unref (constraint->constraint);
}

static void
meta_wayland_pointer_constraint_disable (MetaWaylandPointerConstraint *constraint)
{
  constraint->is_enabled = FALSE;
  meta_wayland_pointer_constraint_notify_deactivated (constraint);
  meta_wayland_pointer_end_grab (constraint->grab.pointer);
  meta_backend_set_client_pointer_constraint (meta_get_backend (), NULL);
}

void
meta_wayland_pointer_constraint_destroy (MetaWaylandPointerConstraint *constraint)
{
  if (meta_wayland_pointer_constraint_is_enabled (constraint))
    meta_wayland_pointer_constraint_disable (constraint);

  wl_resource_set_user_data (constraint->resource, NULL);
  g_clear_pointer (&constraint->region, cairo_region_destroy);
  g_object_unref (constraint);
}

static gboolean
is_within_constraint_region (MetaWaylandPointerConstraint *constraint,
                             wl_fixed_t                    sx,
                             wl_fixed_t                    sy)
{
  cairo_region_t *region;
  gboolean is_within;

  region = meta_wayland_pointer_constraint_calculate_effective_region (constraint);
  is_within = cairo_region_contains_point (region,
                                           wl_fixed_to_int (sx),
                                           wl_fixed_to_int (sy));
  cairo_region_destroy (region);

  return is_within;
}

static void
meta_wayland_pointer_constraint_maybe_enable (MetaWaylandPointerConstraint *constraint)
{
  MetaWindow *window;
  wl_fixed_t sx, sy;

  if (constraint->is_enabled)
    return;

  if (constraint->seat->pointer->focus_surface != constraint->surface)
    return;

  window = constraint->surface->window;
  if (!window)
    {
      /*
       * Locks from Xwayland may come before we have had the opportunity to
       * associate the X11 Window with the wl_surface.
       */
      g_warn_if_fail (meta_xwayland_is_xwayland_surface (constraint->surface));
      return;
    }

  if (window->unmanaging)
    return;

  if (meta_xwayland_is_xwayland_surface (constraint->surface))
    {
      MetaDisplay *display = meta_get_display ();

      /*
       * We need to handle Xwayland surfaces differently in order to allow
       * Xwayland to be able to lock the pointer. For example, we cannot require
       * the locked window to "appear focused" because the surface Xwayland
       * locks might not be able to appear focused (for example it may be a
       * override redirect window).
       *
       * Since we don't have any way to know what focused window an override
       * redirect is associated with, nor have a way to know if the override
       * redirect window even shares the same connection as a focused window,
       * we simply can only really restrict it to enable the lock if any
       * Xwayland window appears focused.
       */

      if (display->focus_window &&
          display->focus_window->client_type != META_WINDOW_CLIENT_TYPE_X11)
        return;
    }
  else
    {
      if (!meta_window_appears_focused (window))
        return;
    }

  meta_wayland_pointer_get_relative_coordinates (constraint->seat->pointer,
                                                 constraint->surface,
                                                 &sx, &sy);
  if (!is_within_constraint_region (constraint, sx, sy))
    return;

  meta_wayland_pointer_constraint_enable (constraint);
}

static void
meta_wayland_pointer_constraint_remove (MetaWaylandPointerConstraint *constraint)
{
  MetaWaylandSurface *surface = constraint->surface;

  surface_remove_pointer_constraints (surface, constraint);
  meta_wayland_pointer_constraint_destroy (constraint);
}

void
meta_wayland_pointer_constraint_maybe_remove_for_seat (MetaWaylandSeat *seat,
                                                       MetaWindow      *window)
{
  MetaWaylandPointer *pointer = seat->pointer;
  MetaWaylandPointerConstraint *constraint;

  if ((pointer->grab->interface != &confined_pointer_grab_interface &&
       pointer->grab->interface != &locked_pointer_grab_interface))
    return;

  constraint = wl_container_of (pointer->grab, constraint, grab);

  if (constraint->surface != window->surface)
    return;

  if (meta_window_appears_focused (window) &&
      pointer->focus_surface == window->surface)
    return;

  switch (constraint->lifetime)
    {
    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT:
      meta_wayland_pointer_constraint_remove (constraint);
      break;

    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT:
      meta_wayland_pointer_constraint_disable (constraint);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
meta_wayland_pointer_constraint_maybe_enable_for_window (MetaWindow *window)
{
  MetaWaylandSurface *surface = window->surface;
  MetaWaylandSurfacePointerConstraintsData *surface_data;
  GList *l;

  if (!surface)
    {
      g_warn_if_fail (window->client_type == META_WINDOW_CLIENT_TYPE_X11);
      return;
    }

  surface_data = get_surface_constraints_data (surface);
  if (!surface_data)
    return;

  for (l = surface_data->pointer_constraints; l; l = l->next)
    {
      MetaWaylandPointerConstraint *constraint = l->data;

      meta_wayland_pointer_constraint_maybe_enable (constraint);
    }
}

MetaWaylandSeat *
meta_wayland_pointer_constraint_get_seat (MetaWaylandPointerConstraint *constraint)
{
  return constraint->seat;
}

cairo_region_t *
meta_wayland_pointer_constraint_calculate_effective_region (MetaWaylandPointerConstraint *constraint)
{
  cairo_region_t *region;

  region = meta_wayland_surface_calculate_input_region (constraint->surface);
  if (constraint->region)
    cairo_region_intersect (region, constraint->region);

  return region;
}

MetaWaylandSurface *
meta_wayland_pointer_constraint_get_surface (MetaWaylandPointerConstraint *constraint)
{
  return constraint->surface;
}

static void
pointer_constraint_resource_destroyed (struct wl_resource *resource)
{
  MetaWaylandPointerConstraint *constraint =
    wl_resource_get_user_data (resource);

  if (!constraint)
    return;

  meta_wayland_pointer_constraint_remove (constraint);
}

static void
pending_constraint_state_free (MetaWaylandPendingConstraintState *constraint_pending)
{
  g_clear_pointer (&constraint_pending->region, cairo_region_destroy);
  if (constraint_pending->constraint)
    g_object_remove_weak_pointer (G_OBJECT (constraint_pending->constraint),
                                  (gpointer *) &constraint_pending->constraint);
}

static MetaWaylandPendingConstraintStateContainer *
get_pending_constraint_state_container (MetaWaylandPendingState *pending)
{
  return g_object_get_qdata (G_OBJECT (pending),
                             quark_pending_constraint_state);
}

static MetaWaylandPendingConstraintState *
get_pending_constraint_state (MetaWaylandPointerConstraint *constraint)
{
  MetaWaylandPendingState *pending = constraint->surface->pending;
  MetaWaylandPendingConstraintStateContainer *container;
  GList *l;

  container = get_pending_constraint_state_container (pending);
  for (l = container->pending_constraint_states; l; l = l->next)
    {
      MetaWaylandPendingConstraintState *constraint_pending = l->data;

      if (constraint_pending->constraint == constraint)
        return constraint_pending;
    }

  return NULL;
}

static void
pending_constraint_state_container_free (MetaWaylandPendingConstraintStateContainer *container)
{
  g_list_free_full (container->pending_constraint_states,
                    (GDestroyNotify) pending_constraint_state_free);
  g_free (container);
}

static MetaWaylandPendingConstraintStateContainer *
ensure_pending_constraint_state_container (MetaWaylandPendingState *pending)
{
  MetaWaylandPendingConstraintStateContainer *container;

  container = get_pending_constraint_state_container (pending);
  if (!container)
    {
      container = g_new0 (MetaWaylandPendingConstraintStateContainer, 1);
      g_object_set_qdata_full (G_OBJECT (pending),
                               quark_pending_constraint_state,
                               container,
                               (GDestroyNotify) pending_constraint_state_container_free);

    }

  return container;
}

static void
remove_pending_constraint_state (MetaWaylandPointerConstraint *constraint,
                                 MetaWaylandPendingState      *pending)
{
  MetaWaylandPendingConstraintStateContainer *container;
  GList *l;

  container = get_pending_constraint_state_container (pending);
  for (l = container->pending_constraint_states; l; l = l->next)
    {
      MetaWaylandPendingConstraintState *constraint_pending = l->data;
      if (constraint_pending->constraint != constraint)
        continue;

      pending_constraint_state_free (l->data);
      container->pending_constraint_states =
        g_list_remove_link (container->pending_constraint_states, l);
      break;
    }
}

static void
pending_constraint_state_applied (MetaWaylandPendingState           *pending,
                                  MetaWaylandPendingConstraintState *constraint_pending)
{
  MetaWaylandPointerConstraint *constraint = constraint_pending->constraint;

  if (!constraint)
    return;

  g_clear_pointer (&constraint->region, cairo_region_destroy);
  if (constraint_pending->region)
    {
      constraint->region = constraint_pending->region;
      constraint_pending->region = NULL;
    }
  else
    {
      constraint->region = NULL;
    }

  g_signal_handler_disconnect (pending,
                               constraint_pending->applied_handler_id);
  remove_pending_constraint_state (constraint, pending);

  /* The pointer is potentially warped by the actor paint signal callback if
   * the new region proved it necessary.
   */
}

static MetaWaylandPendingConstraintState *
ensure_pending_constraint_state (MetaWaylandPointerConstraint *constraint)
{
  MetaWaylandPendingState *pending = constraint->surface->pending;
  MetaWaylandPendingConstraintStateContainer *container;
  MetaWaylandPendingConstraintState *constraint_pending;

  container = ensure_pending_constraint_state_container (pending);
  constraint_pending = get_pending_constraint_state (constraint);
  if (!constraint_pending)
    {
      constraint_pending = g_new0 (MetaWaylandPendingConstraintState, 1);
      constraint_pending->constraint = constraint;
      constraint_pending->applied_handler_id =
        g_signal_connect (pending, "applied",
                          G_CALLBACK (pending_constraint_state_applied),
                          constraint_pending);
      g_object_add_weak_pointer (G_OBJECT (constraint),
                                 (gpointer *) &constraint_pending->constraint);

      container->pending_constraint_states =
        g_list_append (container->pending_constraint_states,
                       constraint_pending);
    }

  return constraint_pending;
}

static void
meta_wayland_pointer_constraint_set_pending_region (MetaWaylandPointerConstraint *constraint,
                                                    MetaWaylandRegion            *region)
{
  MetaWaylandPendingConstraintState *constraint_pending;

  constraint_pending = ensure_pending_constraint_state (constraint);

  g_clear_pointer (&constraint_pending->region, cairo_region_destroy);
  if (region)
    {
      constraint_pending->region =
        cairo_region_copy (meta_wayland_region_peek_cairo_region (region));
    }
}

static MetaWaylandPointerConstraint *
get_pointer_constraint_for_seat (MetaWaylandSurface *surface,
                                 MetaWaylandSeat    *seat)
{
  MetaWaylandSurfacePointerConstraintsData *surface_data;
  GList *l;

  surface_data = get_surface_constraints_data (surface);
  if (!surface_data)
    return NULL;

  for (l = surface_data->pointer_constraints; l; l = l->next)
    {
      MetaWaylandPointerConstraint *constraint = l->data;

      if (seat == constraint->seat)
        return constraint;
    }

  return NULL;
}

static void
init_pointer_constraint (struct wl_resource                      *resource,
                         uint32_t                                 id,
                         MetaWaylandSurface                      *surface,
                         MetaWaylandSeat                         *seat,
                         MetaWaylandRegion                       *region,
                         enum zwp_pointer_constraints_v1_lifetime lifetime,
                         const struct wl_interface               *interface,
                         const void                              *implementation,
                         const MetaWaylandPointerGrabInterface   *grab_interface)
{
  struct wl_client *client = wl_resource_get_client (resource);
  struct wl_resource *cr;
  MetaWaylandPointerConstraint *constraint;

  if (get_pointer_constraint_for_seat (surface, seat))
    {
      wl_resource_post_error (resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "the pointer as already requested to be "
                              "locked or confined on that surface");
      return;
    }

  cr = wl_resource_create (client, interface,
                           wl_resource_get_version (resource),
                           id);
  if (cr == NULL)
    {
      wl_client_post_no_memory (client);
      return;
    }

  constraint = meta_wayland_pointer_constraint_new (surface, seat,
                                                    region,
                                                    lifetime,
                                                    cr, grab_interface);
  if (constraint == NULL)
    {
      wl_client_post_no_memory (client);
      return;
    }

  surface_add_pointer_constraint (surface, constraint);

  wl_resource_set_implementation (cr, implementation, constraint,
                                  pointer_constraint_resource_destroyed);

  meta_wayland_pointer_constraint_maybe_enable (constraint);
}

static void
locked_pointer_destroy (struct wl_client   *client,
                        struct wl_resource *resource)
{
  MetaWaylandPointerConstraint *constraint =
    wl_resource_get_user_data (resource);
  gboolean warp_pointer = FALSE;
  int warp_x, warp_y;

  if (constraint && constraint->is_enabled && constraint->hint_set &&
      is_within_constraint_region (constraint,
                                   constraint->x_hint,
                                   constraint->y_hint))
    {
      float sx, sy;
      float x, y;

      sx = (float)wl_fixed_to_double (constraint->x_hint);
      sy = (float)wl_fixed_to_double (constraint->y_hint);
      meta_wayland_surface_get_absolute_coordinates (constraint->surface,
                                                     sx, sy,
                                                     &x, &y);
      warp_pointer = TRUE;
      warp_x = (int) x;
      warp_y = (int) y;
    }
  wl_resource_destroy (resource);

  if (warp_pointer)
    meta_backend_warp_pointer (meta_get_backend (), warp_x, warp_y);
}

static void
locked_pointer_set_cursor_position_hint (struct wl_client   *client,
                                         struct wl_resource *resource,
                                         wl_fixed_t          surface_x,
                                         wl_fixed_t          surface_y)
{
  MetaWaylandPointerConstraint *constraint =
    wl_resource_get_user_data (resource);

  /* Ignore a set cursor hint that was already sent after the constraint
   * was cancelled. */
  if (!constraint || !constraint->resource || constraint->resource != resource)
    return;

  constraint->hint_set = TRUE;
  constraint->x_hint = surface_x;
  constraint->y_hint = surface_y;
}

static void
locked_pointer_set_region (struct wl_client   *client,
                           struct wl_resource *resource,
                           struct wl_resource *region_resource)
{
  MetaWaylandPointerConstraint *constraint =
    wl_resource_get_user_data (resource);
  MetaWaylandRegion *region =
    region_resource ? wl_resource_get_user_data (region_resource) : NULL;

  if (!constraint)
    return;

  meta_wayland_pointer_constraint_set_pending_region (constraint, region);
}

static const struct zwp_locked_pointer_v1_interface locked_pointer_interface = {
  locked_pointer_destroy,
  locked_pointer_set_cursor_position_hint,
  locked_pointer_set_region,
};

static void
locked_pointer_grab_pointer_focus (MetaWaylandPointerGrab *grab,
                                   MetaWaylandSurface     *surface)
{
}

static void
locked_pointer_grab_pointer_motion (MetaWaylandPointerGrab *grab,
                                    const ClutterEvent     *event)
{
  meta_wayland_pointer_send_relative_motion (grab->pointer, event);
  meta_wayland_pointer_broadcast_frame (grab->pointer);
}

static void
locked_pointer_grab_pointer_button (MetaWaylandPointerGrab *grab,
                                    const ClutterEvent     *event)
{
  meta_wayland_pointer_send_button (grab->pointer, event);
}

static const MetaWaylandPointerGrabInterface locked_pointer_grab_interface = {
  locked_pointer_grab_pointer_focus,
  locked_pointer_grab_pointer_motion,
  locked_pointer_grab_pointer_button,
};

static void
pointer_constraints_destroy (struct wl_client   *client,
                             struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
pointer_constraints_lock_pointer (struct wl_client   *client,
                                  struct wl_resource *resource,
                                  uint32_t            id,
                                  struct wl_resource *surface_resource,
                                  struct wl_resource *pointer_resource,
                                  struct wl_resource *region_resource,
                                  uint32_t            lifetime)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandPointer *pointer = wl_resource_get_user_data (pointer_resource);
  MetaWaylandSeat *seat = meta_wayland_pointer_get_seat (pointer);
  MetaWaylandRegion *region =
    region_resource ? wl_resource_get_user_data (region_resource) : NULL;

  init_pointer_constraint (resource, id, surface, seat, region, lifetime,
                           &zwp_locked_pointer_v1_interface,
                           &locked_pointer_interface,
                           &locked_pointer_grab_interface);
}

static void
confined_pointer_grab_pointer_focus (MetaWaylandPointerGrab *grab,
                                     MetaWaylandSurface *surface)
{
}

static void
confined_pointer_grab_pointer_motion (MetaWaylandPointerGrab *grab,
                                      const ClutterEvent     *event)
{
  MetaWaylandPointerConstraint *constraint =
    wl_container_of (grab, constraint, grab);
  MetaWaylandPointer *pointer = grab->pointer;

  g_assert (pointer->focus_surface);
  g_assert (pointer->focus_surface == constraint->surface);

  meta_wayland_pointer_send_motion (pointer, event);
}

static void
confined_pointer_grab_pointer_button (MetaWaylandPointerGrab *grab,
                                      const ClutterEvent     *event)
{
  meta_wayland_pointer_send_button (grab->pointer, event);
}

static const MetaWaylandPointerGrabInterface confined_pointer_grab_interface = {
  confined_pointer_grab_pointer_focus,
  confined_pointer_grab_pointer_motion,
  confined_pointer_grab_pointer_button,
};

static void
confined_pointer_destroy (struct wl_client   *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
confined_pointer_set_region (struct wl_client   *client,
                             struct wl_resource *resource,
                             struct wl_resource *region_resource)
{
  MetaWaylandPointerConstraint *constraint =
    wl_resource_get_user_data (resource);
  MetaWaylandRegion *region =
    region_resource ? wl_resource_get_user_data (region_resource) : NULL;

  if (!constraint)
    return;

  meta_wayland_pointer_constraint_set_pending_region (constraint, region);
}

static const struct zwp_confined_pointer_v1_interface confined_pointer_interface = {
  confined_pointer_destroy,
  confined_pointer_set_region,
};

static void
pointer_constraints_confine_pointer (struct wl_client   *client,
                                     struct wl_resource *resource,
                                     uint32_t            id,
                                     struct wl_resource *surface_resource,
                                     struct wl_resource *pointer_resource,
                                     struct wl_resource *region_resource,
                                     uint32_t            lifetime)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandPointer *pointer = wl_resource_get_user_data (pointer_resource);
  MetaWaylandSeat *seat = meta_wayland_pointer_get_seat (pointer);
  MetaWaylandRegion *region =
    region_resource ? wl_resource_get_user_data (region_resource) : NULL;

  init_pointer_constraint (resource, id, surface, seat, region, lifetime,
                           &zwp_confined_pointer_v1_interface,
                           &confined_pointer_interface,
                           &confined_pointer_grab_interface);

}

static const struct zwp_pointer_constraints_v1_interface pointer_constraints = {
  pointer_constraints_destroy,
  pointer_constraints_lock_pointer,
  pointer_constraints_confine_pointer,
};

static void
bind_pointer_constraints (struct wl_client *client,
                          void             *data,
                          uint32_t          version,
                          uint32_t          id)
{
  MetaWaylandCompositor *compositor = data;
  struct wl_resource *resource;

  resource = wl_resource_create (client,
                                 &zwp_pointer_constraints_v1_interface,
                                 1, id);

  wl_resource_set_implementation (resource,
                                  &pointer_constraints,
                                  compositor,
                                  NULL);
}

static void
meta_wayland_pointer_constraint_finalize (GObject *object)
{
  MetaWaylandPointerConstraint *constraint =
    META_WAYLAND_POINTER_CONSTRAINT (object);

  g_signal_handler_disconnect (constraint->seat->pointer,
                               constraint->pointer_focus_surface_handler_id);

  G_OBJECT_CLASS (meta_wayland_pointer_constraint_parent_class)->finalize (object);
}

void
meta_wayland_pointer_constraints_init (MetaWaylandCompositor *compositor)
{
  if (!wl_global_create (compositor->wayland_display,
                         &zwp_pointer_constraints_v1_interface, 1,
                         compositor, bind_pointer_constraints))
    g_error ("Could not create wp_pointer_constraints global");
}

static void
meta_wayland_pointer_constraint_init (MetaWaylandPointerConstraint *constraint)
{
}

static void
meta_wayland_pointer_constraint_class_init (MetaWaylandPointerConstraintClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_wayland_pointer_constraint_finalize;

  quark_pending_constraint_state =
    g_quark_from_static_string ("-meta-wayland-pointer-constraint-pending_state");
  quark_surface_pointer_constraints_data =
    g_quark_from_static_string ("-meta-wayland-surface-constraints-data");
}
