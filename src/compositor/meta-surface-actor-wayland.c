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

struct _MetaSurfaceActorWaylandPrivate
{
  MetaWaylandSurface *surface;
  MetaWaylandBuffer *buffer;
  struct wl_listener buffer_destroy_listener;
};
typedef struct _MetaSurfaceActorWaylandPrivate MetaSurfaceActorWaylandPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaSurfaceActorWayland, meta_surface_actor_wayland, META_TYPE_SURFACE_ACTOR)

static void
meta_surface_actor_handle_buffer_destroy (struct wl_listener *listener, void *data)
{
  MetaSurfaceActorWaylandPrivate *priv = wl_container_of (listener, priv, buffer_destroy_listener);

  /* If the buffer is destroyed while we're attached to it,
   * we want to unset priv->buffer so we don't access freed
   * memory. Keep the texture set however so the user doesn't
   * see the window disappear. */
  priv->buffer = NULL;
}

static void
meta_surface_actor_wayland_process_damage (MetaSurfaceActor *actor,
                                           int x, int y, int width, int height)
{
  MetaSurfaceActorWayland *self = META_SURFACE_ACTOR_WAYLAND (actor);
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);

  if (priv->buffer)
    {
      struct wl_resource *resource = priv->buffer->resource;
      struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get (resource);

      if (shm_buffer)
        {
          CoglTexture2D *texture = COGL_TEXTURE_2D (priv->buffer->texture);
          cogl_wayland_texture_set_region_from_shm_buffer (texture, x, y, width, height, shm_buffer, x, y, 0, NULL);
        }

      meta_surface_actor_update_area (META_SURFACE_ACTOR (self), x, y, width, height);
    }
}

static void
meta_surface_actor_wayland_pre_paint (MetaSurfaceActor *actor)
{
}

static gboolean
meta_surface_actor_wayland_is_argb32 (MetaSurfaceActor *actor)
{
  /* XXX -- look at the SHM buffer format. */
  return TRUE;
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

static MetaWindow *
meta_surface_actor_wayland_get_window (MetaSurfaceActor *actor)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (META_SURFACE_ACTOR_WAYLAND (actor));

  return priv->surface->window;
}

static void
meta_surface_actor_wayland_class_init (MetaSurfaceActorWaylandClass *klass)
{
  MetaSurfaceActorClass *surface_actor_class = META_SURFACE_ACTOR_CLASS (klass);

  surface_actor_class->process_damage = meta_surface_actor_wayland_process_damage;
  surface_actor_class->pre_paint = meta_surface_actor_wayland_pre_paint;
  surface_actor_class->is_argb32 = meta_surface_actor_wayland_is_argb32;
  surface_actor_class->is_visible = meta_surface_actor_wayland_is_visible;

  surface_actor_class->should_unredirect = meta_surface_actor_wayland_should_unredirect;
  surface_actor_class->set_unredirected = meta_surface_actor_wayland_set_unredirected;
  surface_actor_class->is_unredirected = meta_surface_actor_wayland_is_unredirected;

  surface_actor_class->get_window = meta_surface_actor_wayland_get_window;
}

static void
meta_surface_actor_wayland_init (MetaSurfaceActorWayland *self)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);

  priv->buffer_destroy_listener.notify = meta_surface_actor_handle_buffer_destroy;
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
meta_surface_actor_wayland_set_buffer (MetaSurfaceActorWayland *self,
                                       MetaWaylandBuffer       *buffer)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);
  MetaShapedTexture *stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));

  if (priv->buffer)
    wl_list_remove (&priv->buffer_destroy_listener.link);

  priv->buffer = buffer;

  if (priv->buffer)
    {
      wl_signal_add (&priv->buffer->destroy_signal, &priv->buffer_destroy_listener);
      meta_shaped_texture_set_texture (stex, priv->buffer->texture);
    }
  else
    meta_shaped_texture_set_texture (stex, NULL);
}

MetaWaylandSurface *
meta_surface_actor_wayland_get_surface (MetaSurfaceActorWayland *self)
{
  MetaSurfaceActorWaylandPrivate *priv = meta_surface_actor_wayland_get_instance_private (self);
  return priv->surface;
}
