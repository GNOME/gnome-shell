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
 */

#include "config.h"

#include "wayland/meta-wayland-subsurface.h"

#include "compositor/meta-surface-actor-wayland.h"
#include "wayland/meta-wayland.h"
#include "wayland/meta-wayland-surface.h"

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

struct _MetaWaylandSubsurface
{
  MetaWaylandActorSurface parent;
};

G_DEFINE_TYPE (MetaWaylandSubsurface,
               meta_wayland_subsurface,
               META_TYPE_WAYLAND_ACTOR_SURFACE)

void
meta_wayland_subsurface_parent_state_applied (MetaWaylandSubsurface *subsurface)
{
  MetaWaylandSurfaceRole *surface_role = META_WAYLAND_SURFACE_ROLE (subsurface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

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
      ClutterActor *surface_actor = CLUTTER_ACTOR (surface->surface_actor);

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

  if (meta_wayland_surface_is_effectively_synchronized (surface))
    meta_wayland_surface_apply_pending_state (surface, surface->sub.pending);

  meta_surface_actor_wayland_sync_subsurface_state (
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor));
}

static void
meta_wayland_subsurface_commit (MetaWaylandSurfaceRole  *surface_role,
                                MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  ClutterActor *actor = CLUTTER_ACTOR (surface->surface_actor);

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_subsurface_parent_class);
  surface_role_class->commit (surface_role, pending);

  if (surface->buffer_ref.buffer != NULL)
    clutter_actor_show (actor);
  else
    clutter_actor_hide (actor);
}

static MetaWaylandSurface *
meta_wayland_subsurface_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *parent = surface->sub.parent;

  if (parent)
    return meta_wayland_surface_get_toplevel (parent);
  else
    return NULL;
}

static void
meta_wayland_subsurface_init (MetaWaylandSubsurface *role)
{
}

static void
meta_wayland_subsurface_class_init (MetaWaylandSubsurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->commit = meta_wayland_subsurface_commit;
  surface_role_class->get_toplevel = meta_wayland_subsurface_get_toplevel;
}

static void
unparent_actor (MetaWaylandSurface *surface)
{
  ClutterActor *actor = CLUTTER_ACTOR (surface->surface_actor);
  ClutterActor *parent_actor;

  parent_actor = clutter_actor_get_parent (actor);
  clutter_actor_remove_child (parent_actor, actor);
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

  g_clear_object (&surface->sub.pending);
  surface->wl_subsurface = NULL;
}

static void
wl_subsurface_destroy (struct wl_client   *client,
                       struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
wl_subsurface_set_position (struct wl_client   *client,
                            struct wl_resource *resource,
                            int32_t             x,
                            int32_t             y)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->sub.pending_x = x;
  surface->sub.pending_y = y;
  surface->sub.pending_pos = TRUE;
}

static gboolean
is_valid_sibling (MetaWaylandSurface *surface,
                  MetaWaylandSurface *sibling)
{
  if (surface->sub.parent == sibling)
    return TRUE;
  if (surface->sub.parent == sibling->sub.parent)
    return TRUE;
  return FALSE;
}

static void
subsurface_handle_pending_sibling_destroyed (struct wl_listener *listener,
                                             void               *data)
{
  MetaWaylandSubsurfacePlacementOp *op =
    wl_container_of (listener, op, sibling_destroy_listener);

  op->sibling = NULL;
}

static void
queue_subsurface_placement (MetaWaylandSurface             *surface,
                            MetaWaylandSurface             *sibling,
                            MetaWaylandSubsurfacePlacement  placement)
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
wl_subsurface_place_above (struct wl_client   *client,
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
wl_subsurface_place_below (struct wl_client   *client,
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
wl_subsurface_set_sync (struct wl_client   *client,
                        struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->sub.synchronous = TRUE;
}

static void
wl_subsurface_set_desync (struct wl_client   *client,
                          struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  gboolean was_effectively_synchronized;

  was_effectively_synchronized =
    meta_wayland_surface_is_effectively_synchronized (surface);
  surface->sub.synchronous = FALSE;

  if (was_effectively_synchronized &&
      !meta_wayland_surface_is_effectively_synchronized (surface))
    meta_wayland_surface_apply_pending_state (surface, surface->sub.pending);
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
wl_subcompositor_destroy (struct wl_client   *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
surface_handle_parent_surface_destroyed (struct wl_listener *listener,
                                         void               *data)
{
  MetaWaylandSurface *surface = wl_container_of (listener,
                                                 surface,
                                                 sub.parent_destroy_listener);

  surface->sub.parent = NULL;
  unparent_actor (surface);
}

static void
wl_subcompositor_get_subsurface (struct wl_client   *client,
                                 struct wl_resource *resource,
                                 uint32_t            id,
                                 struct wl_resource *surface_resource,
                                 struct wl_resource *parent_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandSurface *parent = wl_resource_get_user_data (parent_resource);

  if (surface->wl_subsurface)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_subcompositor::get_subsurface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SUBSURFACE,
                                         NULL))
    {
      wl_resource_post_error (resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface->wl_subsurface =
    wl_resource_create (client,
                        &wl_subsurface_interface,
                        wl_resource_get_version (resource),
                        id);
  wl_resource_set_implementation (surface->wl_subsurface,
                                  &meta_wayland_wl_subsurface_interface,
                                  surface,
                                  wl_subsurface_destructor);

  surface->sub.pending = g_object_new (META_TYPE_WAYLAND_PENDING_STATE, NULL);
  surface->sub.synchronous = TRUE;
  surface->sub.parent = parent;
  surface->sub.parent_destroy_listener.notify =
    surface_handle_parent_surface_destroyed;
  wl_resource_add_destroy_listener (parent->resource,
                                    &surface->sub.parent_destroy_listener);
  parent->subsurfaces = g_list_append (parent->subsurfaces, surface);

  clutter_actor_add_child (CLUTTER_ACTOR (parent->surface_actor),
                           CLUTTER_ACTOR (surface->surface_actor));

  clutter_actor_set_reactive (CLUTTER_ACTOR (surface->surface_actor), TRUE);
}

static const struct wl_subcompositor_interface meta_wayland_subcompositor_interface = {
  wl_subcompositor_destroy,
  wl_subcompositor_get_subsurface,
};

static void
bind_subcompositor (struct wl_client *client,
                    void             *data,
                    uint32_t          version,
                    uint32_t          id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_subcompositor_interface,
                                 version, id);
  wl_resource_set_implementation (resource, &meta_wayland_subcompositor_interface,
                                  data, NULL);
}

void
meta_wayland_subsurfaces_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
                        &wl_subcompositor_interface,
                        META_WL_SUBCOMPOSITOR_VERSION,
                        compositor, bind_subcompositor) == NULL)
    g_error ("Failed to register a global wl-subcompositor object");
}
