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

#include "compositor/meta-surface-actor-wayland.h"
#include "wayland/meta-wayland-surface.h"

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

static void
meta_wayland_actor_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                                   MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *toplevel_surface;

  queue_surface_actor_frame_callbacks (surface, pending);

  toplevel_surface = meta_wayland_surface_get_toplevel (surface);
  if (!toplevel_surface || !toplevel_surface->window)
    return;

  meta_surface_actor_wayland_sync_state (
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor));
}

static gboolean
meta_wayland_actor_surface_is_on_logical_monitor (MetaWaylandSurfaceRole *surface_role,
                                                  MetaLogicalMonitor     *logical_monitor)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActorWayland *actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  return meta_surface_actor_wayland_is_on_monitor (actor, logical_monitor);
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
}
