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

typedef struct _MetaSurfaceActorWaylandPrivate
{
  MetaWaylandSurface *surface;
  struct wl_list frame_callback_list;
} MetaSurfaceActorWaylandPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaSurfaceActorWayland,
                            meta_surface_actor_wayland,
                            META_TYPE_SURFACE_ACTOR)

static void
meta_surface_actor_wayland_process_damage (MetaSurfaceActor *actor,
                                           int               x,
                                           int               y,
                                           int               width,
                                           int               height)
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
  MetaShapedTexture *stex;
  double scale;

  stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  clutter_actor_get_scale (CLUTTER_ACTOR (stex), &scale, NULL);
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
  MetaShapedTexture *stex;
  double scale;

  stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  clutter_actor_get_scale (CLUTTER_ACTOR (stex), NULL, &scale);
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
