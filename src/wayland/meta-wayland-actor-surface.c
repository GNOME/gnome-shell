/*
 * Copyright (C) 2012,2013 Intel Corporation
 * Copyright (C) 2013-2017 Red Hat, Inc.
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
 */

#include "config.h"

#include "wayland/meta-wayland-actor-surface.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "compositor/meta-surface-actor-wayland.h"
#include "compositor/region-utils.h"
#include "wayland/meta-wayland-surface.h"
#include "wayland/meta-window-wayland.h"

G_DEFINE_TYPE (MetaWaylandActorSurface,
               meta_wayland_actor_surface,
               META_TYPE_WAYLAND_SURFACE_ROLE)

static void
meta_wayland_actor_surface_assigned (MetaWaylandSurfaceRole *surface_role)
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
queue_surface_actor_frame_callbacks (MetaWaylandSurface      *surface,
                                     MetaWaylandPendingState *pending)
{
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  meta_surface_actor_wayland_add_frame_callbacks (surface_actor,
                                                  &pending->frame_callback_list);
  wl_list_init (&pending->frame_callback_list);
}

double
meta_wayland_actor_surface_calculate_scale (MetaWaylandActorSurface *actor_surface)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (actor_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *toplevel_window;
  int geometry_scale;

  toplevel_window = meta_wayland_surface_get_toplevel_window (surface);
  if (meta_is_stage_views_scaled ())
    {
      geometry_scale = 1;
    }
  else
    {
      if (!toplevel_window ||
          toplevel_window->client_type == META_WINDOW_CLIENT_TYPE_X11)
        geometry_scale = 1;
      else
        geometry_scale =
          meta_window_wayland_get_geometry_scale (toplevel_window);
    }

  return (double) geometry_scale / (double) surface->scale;
}

static void
meta_wayland_actor_surface_real_sync_actor_state (MetaWaylandActorSurface *actor_surface)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (actor_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActor *surface_actor;
  MetaShapedTexture *stex;
  double actor_scale;
  GList *l;

  surface_actor = surface->surface_actor;
  stex = meta_surface_actor_get_texture (surface_actor);

  actor_scale = meta_wayland_actor_surface_calculate_scale (actor_surface);
  clutter_actor_set_scale (CLUTTER_ACTOR (stex), actor_scale, actor_scale);

  if (surface->input_region)
    {
      cairo_region_t *scaled_input_region;
      int region_scale;

      /* Wayland surface coordinate space -> stage coordinate space */
      region_scale = (int) (surface->scale * actor_scale);
      scaled_input_region = meta_region_scale (surface->input_region,
                                               region_scale);
      meta_surface_actor_set_input_region (surface_actor, scaled_input_region);
      cairo_region_destroy (scaled_input_region);
    }
  else
    {
      meta_surface_actor_set_input_region (surface_actor, NULL);
    }

  if (surface->opaque_region)
    {
      cairo_region_t *scaled_opaque_region;

      /* Wayland surface coordinate space -> stage coordinate space */
      scaled_opaque_region = meta_region_scale (surface->opaque_region,
                                                surface->scale);
      meta_surface_actor_set_opaque_region (surface_actor,
                                            scaled_opaque_region);
      cairo_region_destroy (scaled_opaque_region);
    }
  else
    {
      meta_surface_actor_set_opaque_region (surface_actor, NULL);
    }

  for (l = surface->subsurfaces; l; l = l->next)
    {
      MetaWaylandSurface *subsurface_surface = l->data;
      MetaWaylandActorSurface *subsurface_actor_surface =
        META_WAYLAND_ACTOR_SURFACE (subsurface_surface->role);

      meta_wayland_actor_surface_sync_actor_state (subsurface_actor_surface);
    }
}

void
meta_wayland_actor_surface_sync_actor_state (MetaWaylandActorSurface *actor_surface)
{
  MetaWaylandActorSurfaceClass *actor_surface_class =
    META_WAYLAND_ACTOR_SURFACE_GET_CLASS (actor_surface);

  actor_surface_class->sync_actor_state (actor_surface);
}

static void
meta_wayland_actor_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                                   MetaWaylandPendingState *pending)
{
  MetaWaylandActorSurface *actor_surface =
    META_WAYLAND_ACTOR_SURFACE (surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *toplevel_surface;

  queue_surface_actor_frame_callbacks (surface, pending);

  toplevel_surface = meta_wayland_surface_get_toplevel (surface);
  if (!toplevel_surface || !toplevel_surface->window)
    return;

  meta_wayland_actor_surface_sync_actor_state (actor_surface);
}

static gboolean
meta_wayland_actor_surface_is_on_logical_monitor (MetaWaylandSurfaceRole *surface_role,
                                                  MetaLogicalMonitor     *logical_monitor)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  ClutterActor *actor = CLUTTER_ACTOR (surface->surface_actor);
  float x, y, width, height;
  cairo_rectangle_int_t actor_rect;
  cairo_region_t *region;
  MetaRectangle logical_monitor_layout;
  gboolean is_on_monitor;

  clutter_actor_get_transformed_position (actor, &x, &y);
  clutter_actor_get_transformed_size (actor, &width, &height);

  actor_rect.x = (int) roundf (x);
  actor_rect.y = (int) roundf (y);
  actor_rect.width = (int) roundf (x + width) - actor_rect.x;
  actor_rect.height = (int) roundf (y + height) - actor_rect.y;

  /* Calculate the scaled surface actor region. */
  region = cairo_region_create_rectangle (&actor_rect);

  logical_monitor_layout = meta_logical_monitor_get_layout (logical_monitor);

  cairo_region_intersect_rectangle (region,
				    &((cairo_rectangle_int_t) {
				      .x = logical_monitor_layout.x,
				      .y = logical_monitor_layout.y,
				      .width = logical_monitor_layout.width,
				      .height = logical_monitor_layout.height,
				    }));

  is_on_monitor = !cairo_region_is_empty (region);
  cairo_region_destroy (region);

  return is_on_monitor;
}

static void
meta_wayland_actor_surface_init (MetaWaylandActorSurface *role)
{
}

static void
meta_wayland_actor_surface_class_init (MetaWaylandActorSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = meta_wayland_actor_surface_assigned;
  surface_role_class->commit = meta_wayland_actor_surface_commit;
  surface_role_class->is_on_logical_monitor =
    meta_wayland_actor_surface_is_on_logical_monitor;

  klass->sync_actor_state = meta_wayland_actor_surface_real_sync_actor_state;
}
