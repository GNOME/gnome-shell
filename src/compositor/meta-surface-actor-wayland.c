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

#include <math.h>
#include <cogl/cogl-wayland-server.h>
#include "meta-shaped-texture-private.h"

#include "backends/meta-logical-monitor.h"
#include "wayland/meta-wayland-buffer.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-window-wayland.h"

#include "backends/meta-backend-private.h"
#include "compositor/region-utils.h"

enum {
  PAINTING,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _MetaSurfaceActorWaylandPrivate
{
  MetaWaylandSurface *surface;
  struct wl_list frame_callback_list;
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

double
meta_surface_actor_wayland_get_scale (MetaSurfaceActorWayland *self)
{
   MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
   MetaWindow *window;
   int geometry_scale = 1;

   g_assert (surface);

   window = meta_wayland_surface_get_toplevel_window (surface);

   if (!meta_is_stage_views_scaled ())
     {
       /* XXX: We do not handle x11 clients yet */
       if (window && window->client_type != META_WINDOW_CLIENT_TYPE_X11)
         geometry_scale = meta_window_wayland_get_geometry_scale (window);
     }

   return (double) geometry_scale / (double) surface->scale;
}

static void
logical_to_actor_position (MetaSurfaceActorWayland *self,
                           int                     *x,
                           int                     *y)
{
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaWindow *toplevel_window;
  int geometry_scale = 1;

  g_assert (surface);

  toplevel_window = meta_wayland_surface_get_toplevel_window (surface);
  if (toplevel_window)
    geometry_scale = meta_window_wayland_get_geometry_scale (toplevel_window);

  *x = *x * geometry_scale;
  *y = *y * geometry_scale;
}

/* Convert the current actor state to the corresponding subsurface rectangle
 * in logical pixel coordinate space. */
void
meta_surface_actor_wayland_get_subsurface_rect (MetaSurfaceActorWayland *self,
                                                MetaRectangle           *rect)
{
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaWaylandBuffer *buffer = meta_wayland_surface_get_buffer (surface);
  CoglTexture *texture;
  MetaWindow *toplevel_window;
  int geometry_scale;
  float x, y;

  g_assert (surface);

  texture = buffer->texture;
  toplevel_window = meta_wayland_surface_get_toplevel_window (surface);
  geometry_scale = meta_window_wayland_get_geometry_scale (toplevel_window);

  clutter_actor_get_position (CLUTTER_ACTOR (self), &x, &y);
  *rect = (MetaRectangle) {
    .x = x / geometry_scale,
    .y = y / geometry_scale,
    .width = cogl_texture_get_width (texture) / surface->scale,
    .height = cogl_texture_get_height (texture) / surface->scale,
  };
}

void
meta_surface_actor_wayland_sync_subsurface_state (MetaSurfaceActorWayland *self)
{
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaWindow *window;
  int x = surface->offset_x + surface->sub.x;
  int y = surface->offset_y + surface->sub.y;

  g_assert (surface);

  window = meta_wayland_surface_get_toplevel_window (surface);
  if (window && window->client_type == META_WINDOW_CLIENT_TYPE_X11)
    {
      /* Bail directly if this is part of a Xwayland window and warn
       * if there happen to be offsets anyway since that is not supposed
       * to happen. */
      g_warn_if_fail (x == 0 && y == 0);
      return;
    }

  logical_to_actor_position (self, &x, &y);
  clutter_actor_set_position (CLUTTER_ACTOR (self), x, y);
}

void
meta_surface_actor_wayland_sync_state (MetaSurfaceActorWayland *self)
{
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaShapedTexture *stex =
    meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  double texture_scale;

  g_assert (surface);

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
  else
    {
      meta_surface_actor_set_input_region (META_SURFACE_ACTOR (self), NULL);
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
  else
    {
      meta_surface_actor_set_opaque_region (META_SURFACE_ACTOR (self), NULL);
    }

  meta_surface_actor_wayland_sync_subsurface_state (self);
}

void
meta_surface_actor_wayland_sync_state_recursive (MetaSurfaceActorWayland *self)
{
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaWindow *window;
  GList *iter;

  g_assert (surface);

  window = meta_wayland_surface_get_toplevel_window (surface);
  meta_surface_actor_wayland_sync_state (self);

  if (window && window->client_type != META_WINDOW_CLIENT_TYPE_X11)
    {
      for (iter = surface->subsurfaces; iter != NULL; iter = iter->next)
        {
          MetaWaylandSurface *subsurf = iter->data;

          meta_surface_actor_wayland_sync_state_recursive (
            META_SURFACE_ACTOR_WAYLAND (subsurf->surface_actor));
        }
    }
}

gboolean
meta_surface_actor_wayland_is_on_monitor (MetaSurfaceActorWayland *self,
                                          MetaLogicalMonitor      *logical_monitor)
{
  float x, y, width, height;
  cairo_rectangle_int_t actor_rect;
  cairo_region_t *region;
  gboolean is_on_monitor;

  clutter_actor_get_transformed_position (CLUTTER_ACTOR (self), &x, &y);
  clutter_actor_get_transformed_size (CLUTTER_ACTOR (self), &width, &height);

  actor_rect.x = (int)roundf (x);
  actor_rect.y = (int)roundf (y);
  actor_rect.width = (int)roundf (x + width) - actor_rect.x;
  actor_rect.height = (int)roundf (y + height) - actor_rect.y;

  /* Calculate the scaled surface actor region. */
  region = cairo_region_create_rectangle (&actor_rect);

  cairo_region_intersect_rectangle (region,
				    &((cairo_rectangle_int_t) {
				      .x = logical_monitor->rect.x,
				      .y = logical_monitor->rect.y,
				      .width = logical_monitor->rect.width,
				      .height = logical_monitor->rect.height,
				    }));

  is_on_monitor = !cairo_region_is_empty (region);
  cairo_region_destroy (region);

  return is_on_monitor;
}

void
meta_surface_actor_wayland_add_frame_callbacks (MetaSurfaceActorWayland *self,
                                                struct wl_list *frame_callbacks)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);

  wl_list_insert_list (&priv->frame_callback_list, frame_callbacks);
}

static MetaWindow *
meta_surface_actor_wayland_get_window (MetaSurfaceActor *actor)
{
  MetaSurfaceActorWayland *self = META_SURFACE_ACTOR_WAYLAND (actor);
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);

  if (!surface)
    return NULL;

  return surface->window;
}

static void
meta_surface_actor_wayland_get_preferred_width  (ClutterActor *actor,
                                                 gfloat        for_height,
                                                 gfloat       *min_width_p,
                                                 gfloat       *natural_width_p)
{
  MetaSurfaceActorWayland *self = META_SURFACE_ACTOR_WAYLAND (actor);
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaShapedTexture *stex;
  double scale;

  if (surface)
    scale = meta_surface_actor_wayland_get_scale (self);
  else
    scale = 1.0;

  stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  clutter_actor_get_preferred_width (CLUTTER_ACTOR (stex),
                                     for_height,
                                     min_width_p,
                                     natural_width_p);

  if (min_width_p)
     *min_width_p *= scale;

  if (natural_width_p)
    *natural_width_p *= scale;
}

static void
meta_surface_actor_wayland_get_preferred_height  (ClutterActor *actor,
                                                  gfloat        for_width,
                                                  gfloat       *min_height_p,
                                                  gfloat       *natural_height_p)
{
  MetaSurfaceActorWayland *self = META_SURFACE_ACTOR_WAYLAND (actor);
  MetaWaylandSurface *surface = meta_surface_actor_wayland_get_surface (self);
  MetaShapedTexture *stex;
  double scale;

  if (surface)
    scale = meta_surface_actor_wayland_get_scale (self);
  else
    scale = 1.0;

  stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  clutter_actor_get_preferred_height (CLUTTER_ACTOR (stex),
                                      for_width,
                                      min_height_p,
                                      natural_height_p);

  if (min_height_p)
     *min_height_p *= scale;

  if (natural_height_p)
    *natural_height_p *= scale;
}

static void
meta_surface_actor_wayland_paint (ClutterActor *actor)
{
  MetaSurfaceActorWayland *self = META_SURFACE_ACTOR_WAYLAND (actor);
  MetaSurfaceActorWaylandPrivate *priv =
    meta_surface_actor_wayland_get_instance_private (self);

  if (priv->surface)
    {
      MetaWaylandCompositor *compositor = priv->surface->compositor;

      wl_list_insert_list (&compositor->frame_callbacks, &priv->frame_callback_list);
      wl_list_init (&priv->frame_callback_list);
    }

  g_signal_emit (actor, signals[PAINTING], 0);

  CLUTTER_ACTOR_CLASS (meta_surface_actor_wayland_parent_class)->paint (actor);
}

static void
meta_surface_actor_wayland_dispose (GObject *object)
{
  MetaSurfaceActorWayland *self = META_SURFACE_ACTOR_WAYLAND (object);
  MetaSurfaceActorWaylandPrivate *priv =
    meta_surface_actor_wayland_get_instance_private (self);
  MetaWaylandFrameCallback *cb, *next;
  MetaShapedTexture *stex =
    meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));

  meta_shaped_texture_set_texture (stex, NULL);
  if (priv->surface)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->surface),
                                    (gpointer *) &priv->surface);
      priv->surface = NULL;
    }

  wl_list_for_each_safe (cb, next, &priv->frame_callback_list, link)
    wl_resource_destroy (cb->resource);

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
  actor_class->paint = meta_surface_actor_wayland_paint;

  surface_actor_class->process_damage = meta_surface_actor_wayland_process_damage;
  surface_actor_class->pre_paint = meta_surface_actor_wayland_pre_paint;
  surface_actor_class->is_visible = meta_surface_actor_wayland_is_visible;

  surface_actor_class->should_unredirect = meta_surface_actor_wayland_should_unredirect;
  surface_actor_class->set_unredirected = meta_surface_actor_wayland_set_unredirected;
  surface_actor_class->is_unredirected = meta_surface_actor_wayland_is_unredirected;

  surface_actor_class->get_window = meta_surface_actor_wayland_get_window;

  object_class->dispose = meta_surface_actor_wayland_dispose;

  signals[PAINTING] = g_signal_new ("painting",
                                    G_TYPE_FROM_CLASS (object_class),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL, NULL,
                                    G_TYPE_NONE, 0);
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

  wl_list_init (&priv->frame_callback_list);
  priv->surface = surface;
  g_object_add_weak_pointer (G_OBJECT (priv->surface),
                             (gpointer *) &priv->surface);

  return META_SURFACE_ACTOR (self);
}

MetaWaylandSurface *
meta_surface_actor_wayland_get_surface (MetaSurfaceActorWayland *self)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);
  return priv->surface;
}
