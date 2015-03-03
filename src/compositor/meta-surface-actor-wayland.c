/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013 Red Hat
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "meta-surface-actor-wayland.h"

#include <cogl/cogl-wayland-server.h>
#include "meta-shaped-texture-private.h"

#include "wayland/meta-wayland-private.h"

#include "compositor/region-utils.h"

struct _MetaSurfaceActorWaylandPrivate
{
  MetaWaylandSurface *surface;
};
typedef struct _MetaSurfaceActorWaylandPrivate MetaSurfaceActorWaylandPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaSurfaceActorWayland, meta_surface_actor_wayland, META_TYPE_SURFACE_ACTOR)

static void
meta_surface_actor_wayland_process_damage (MetaSurfaceActor *actor,
                                           int x, int y, int width, int height)
{
}

static void
meta_surface_actor_wayland_pre_paint (MetaSurfaceActor *actor)
{
}

static gboolean
meta_surface_actor_wayland_is_visible (MetaSurfaceActor *actor)
{
  /* TODO: ensure that the buffer isn't NULL, implement
   * wayland mapping semantics */
  return TRUE;
}

static gboolean
meta_surface_actor_wayland_should_unredirect (MetaSurfaceActor *actor)
{
  return FALSE;
}

static void
meta_surface_actor_wayland_set_unredirected (MetaSurfaceActor *actor,
                                             gboolean          unredirected)
{
  /* Do nothing. In the future, we'll use KMS to set this
   * up as a hardware overlay or something. */
}

static gboolean
meta_surface_actor_wayland_is_unredirected (MetaSurfaceActor *actor)
{
  return FALSE;
}

static int
get_output_scale (int winsys_id)
{
  MetaMonitorManager *monitor_manager = meta_monitor_manager_get ();
  MetaOutput *outputs;
  guint n_outputs, i;
  int output_scale = 1;

  outputs = meta_monitor_manager_get_outputs (monitor_manager, &n_outputs);

  for (i = 0; i < n_outputs; i++)
    {
      if (outputs[i].winsys_id == winsys_id)
        {
          output_scale = outputs[i].scale;
          break;
        }
    }

  return output_scale;
}

double
meta_surface_actor_wayland_get_scale (MetaSurfaceActorWayland *actor)
{
   MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (actor);
   MetaWaylandSurface *surface = priv->surface;
   MetaWindow *window = surface->window;
   int output_scale = 1;

   while (surface)
    {
      if (surface->window)
        {
          window = surface->window;
          break;
        }
      surface = surface->sub.parent;
    }

   /* XXX: We do not handle x11 clients yet */
   if (window && window->client_type != META_WINDOW_CLIENT_TYPE_X11)
     output_scale = get_output_scale (window->monitor->winsys_id);

   return (double)output_scale / (double)priv->surface->scale;
}

void
meta_surface_actor_wayland_sync_state (MetaSurfaceActorWayland *self)
{
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaShapedTexture *stex =
    meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  double texture_scale;

  /* Given the surface's window type and what output the surface actor has the
   * largest region, scale the actor with the determined scale. */
  texture_scale = meta_surface_actor_wayland_get_scale (self);

  /* Actor scale. */
  clutter_actor_set_scale (CLUTTER_ACTOR (stex), texture_scale, texture_scale);

  /* Input region */
  if (surface->input_region)
    {
      cairo_region_t *scaled_input_region;
      int region_scale;

      /* The input region from the Wayland surface is in the Wayland surface
       * coordinate space, while the surface actor input region is in the
       * physical pixel coordinate space. */
      region_scale = (int)(surface->scale * texture_scale);
      scaled_input_region = meta_region_scale (surface->input_region,
                                               region_scale);
      meta_surface_actor_set_input_region (META_SURFACE_ACTOR (self),
                                           scaled_input_region);
      cairo_region_destroy (scaled_input_region);
    }

  /* Opaque region */
  if (surface->opaque_region)
    {
      cairo_region_t *scaled_opaque_region;

      /* The opaque region from the Wayland surface is in Wayland surface
       * coordinate space, while the surface actor opaque region is in the
       * same coordinate space as the unscaled buffer texture. */
      scaled_opaque_region = meta_region_scale (surface->opaque_region,
                                                surface->scale);
      meta_surface_actor_set_opaque_region (META_SURFACE_ACTOR (self),
                                            scaled_opaque_region);
      cairo_region_destroy (scaled_opaque_region);
    }
}

void
meta_surface_actor_wayland_sync_state_recursive (MetaSurfaceActorWayland *self)
{
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  GList *iter;

  meta_surface_actor_wayland_sync_state (self);

  for (iter = surface->subsurfaces; iter != NULL; iter = iter->next)
    {
      MetaWaylandSurface *subsurf = iter->data;

      meta_surface_actor_wayland_sync_state_recursive (
        META_SURFACE_ACTOR_WAYLAND (subsurf->surface_actor));
    }
}

static MetaWindow *
meta_surface_actor_wayland_get_window (MetaSurfaceActor *actor)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (META_SURFACE_ACTOR_WAYLAND (actor));

  return priv->surface->window;
}

static void
meta_surface_actor_wayland_get_preferred_width  (ClutterActor *self,
                                                 gfloat        for_height,
                                                 gfloat       *min_width_p,
                                                 gfloat       *natural_width_p)
{
  MetaShapedTexture *stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  double scale = meta_surface_actor_wayland_get_scale (META_SURFACE_ACTOR_WAYLAND (self));

  clutter_actor_get_preferred_width (CLUTTER_ACTOR (stex), for_height, min_width_p, natural_width_p);

  if (min_width_p)
     *min_width_p *= scale;

  if (natural_width_p)
    *natural_width_p *= scale;
}

static void
meta_surface_actor_wayland_get_preferred_height  (ClutterActor *self,
                                                  gfloat        for_width,
                                                  gfloat       *min_height_p,
                                                  gfloat       *natural_height_p)
{
  MetaShapedTexture *stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  double scale = meta_surface_actor_wayland_get_scale (META_SURFACE_ACTOR_WAYLAND (self));

  clutter_actor_get_preferred_height (CLUTTER_ACTOR (stex), for_width, min_height_p, natural_height_p);

  if (min_height_p)
     *min_height_p *= scale;

  if (natural_height_p)
    *natural_height_p *= scale;
}

static void
meta_surface_actor_wayland_dispose (GObject *object)
{
  MetaSurfaceActorWayland *self = META_SURFACE_ACTOR_WAYLAND (object);

  meta_surface_actor_wayland_set_texture (self, NULL);

  G_OBJECT_CLASS (meta_surface_actor_wayland_parent_class)->dispose (object);
}

static void
meta_surface_actor_wayland_class_init (MetaSurfaceActorWaylandClass *klass)
{
  MetaSurfaceActorClass *surface_actor_class = META_SURFACE_ACTOR_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  actor_class->get_preferred_width = meta_surface_actor_wayland_get_preferred_width;
  actor_class->get_preferred_height = meta_surface_actor_wayland_get_preferred_height;

  surface_actor_class->process_damage = meta_surface_actor_wayland_process_damage;
  surface_actor_class->pre_paint = meta_surface_actor_wayland_pre_paint;
  surface_actor_class->is_visible = meta_surface_actor_wayland_is_visible;

  surface_actor_class->should_unredirect = meta_surface_actor_wayland_should_unredirect;
  surface_actor_class->set_unredirected = meta_surface_actor_wayland_set_unredirected;
  surface_actor_class->is_unredirected = meta_surface_actor_wayland_is_unredirected;

  surface_actor_class->get_window = meta_surface_actor_wayland_get_window;

  object_class->dispose = meta_surface_actor_wayland_dispose;
}

static void
meta_surface_actor_wayland_init (MetaSurfaceActorWayland *self)
{
}

MetaSurfaceActor *
meta_surface_actor_wayland_new (MetaWaylandSurface *surface)
{
  MetaSurfaceActorWayland *self = g_object_new (META_TYPE_SURFACE_ACTOR_WAYLAND, NULL);
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);

  g_assert (meta_is_wayland_compositor ());

  priv->surface = surface;

  return META_SURFACE_ACTOR (self);
}

void
meta_surface_actor_wayland_set_texture (MetaSurfaceActorWayland *self,
                                        CoglTexture *texture)
{
  MetaShapedTexture *stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  meta_shaped_texture_set_texture (stex, texture);
}

MetaWaylandSurface *
meta_surface_actor_wayland_get_surface (MetaSurfaceActorWayland *self)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);
  return priv->surface;
}
