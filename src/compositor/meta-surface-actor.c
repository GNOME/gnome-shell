/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:meta-surface-actor
 * @title: MetaSurfaceActor
 * @short_description: An actor representing a surface in the scene graph
 *
 * A surface can be either a shaped texture, or a group of shaped texture,
 * used to draw the content of a window.
 */

#include <config.h>
#include <clutter/clutter.h>
#include <cogl/cogl-wayland-server.h>
#include <cogl/cogl-texture-pixmap-x11.h>
#include <meta/meta-shaped-texture.h>
#include "meta-surface-actor.h"

#include "meta-shaped-texture-private.h"

struct _MetaSurfaceActorPrivate
{
  MetaShapedTexture *texture;
  MetaWaylandBuffer *buffer;
};

G_DEFINE_TYPE (MetaSurfaceActor, meta_surface_actor, CLUTTER_TYPE_ACTOR);

static void
meta_surface_actor_class_init (MetaSurfaceActorClass *klass)
{
  g_type_class_add_private (klass, sizeof (MetaSurfaceActorPrivate));
}

static void
meta_surface_actor_init (MetaSurfaceActor *self)
{
  MetaSurfaceActorPrivate *priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   META_TYPE_SURFACE_ACTOR,
                                                   MetaSurfaceActorPrivate);

  priv->texture = NULL;
}

MetaSurfaceActor *
meta_surface_actor_new (void)
{
  MetaSurfaceActor *self = g_object_new (META_TYPE_SURFACE_ACTOR, NULL);
  MetaShapedTexture *stex;

  stex = META_SHAPED_TEXTURE (meta_shaped_texture_new ());
  self->priv->texture = stex;

  clutter_actor_add_child (CLUTTER_ACTOR (self), CLUTTER_ACTOR (stex));

  return self;
}

cairo_surface_t *
meta_surface_actor_get_image (MetaSurfaceActor      *self,
                              cairo_rectangle_int_t *clip)
{
  return meta_shaped_texture_get_image (self->priv->texture, clip);
}

MetaShapedTexture *
meta_surface_actor_get_texture (MetaSurfaceActor *self)
{
  return self->priv->texture;
}

void
meta_surface_actor_set_clip_region (MetaSurfaceActor *self,
                                    cairo_region_t   *clip_region)
{
  meta_shaped_texture_set_clip_region (self->priv->texture, clip_region);
}

static void
update_area (MetaSurfaceActor *self,
             int x, int y, int width, int height)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  if (meta_is_wayland_compositor ())
    {
      struct wl_resource *resource = priv->buffer->resource;
      struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get (resource);

      if (shm_buffer)
        {
          CoglTexture2D *texture = COGL_TEXTURE_2D (priv->buffer->texture);
          cogl_wayland_texture_2d_update_area (texture, shm_buffer, x, y, width, height);
        }
    }
  else
    {
      CoglTexturePixmapX11 *texture = COGL_TEXTURE_PIXMAP_X11 (meta_shaped_texture_get_texture (priv->texture));
      cogl_texture_pixmap_x11_update_area (texture, x, y, width, height);
    }
}

gboolean
meta_surface_actor_damage_all (MetaSurfaceActor *self,
                               cairo_region_t   *unobscured_region)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  CoglTexture *texture = meta_shaped_texture_get_texture (priv->texture);

  update_area (self, 0, 0, cogl_texture_get_width (texture), cogl_texture_get_height (texture));
  return meta_shaped_texture_update_area (self->priv->texture,
                                          0, 0,
                                          cogl_texture_get_width (texture),
                                          cogl_texture_get_height (texture),
                                          unobscured_region);
}

gboolean
meta_surface_actor_damage_area (MetaSurfaceActor *self,
                                int               x,
                                int               y,
                                int               width,
                                int               height,
                                cairo_region_t   *unobscured_region)
{
  update_area (self, x, y, width, height);
  return meta_shaped_texture_update_area (self->priv->texture,
                                          x, y, width, height,
                                          unobscured_region);
}

void
meta_surface_actor_attach_wayland_buffer (MetaSurfaceActor *self,
                                          MetaWaylandBuffer *buffer)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  priv->buffer = buffer;
  meta_shaped_texture_set_texture (self->priv->texture, buffer->texture);
}

void
meta_surface_actor_set_texture (MetaSurfaceActor *self,
                                CoglTexture      *texture)
{
  meta_shaped_texture_set_texture (self->priv->texture, texture);
}
