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

#include "meta-surface-actor.h"

#include <clutter/clutter.h>
#include <meta/meta-shaped-texture.h>
#include "meta-wayland-private.h"
#include "meta-cullable.h"
#include "meta-shaped-texture-private.h"

struct _MetaSurfaceActorPrivate
{
  MetaShapedTexture *texture;

  /* The region that is visible, used to optimize out redraws */
  cairo_region_t   *unobscured_region;
};

static void cullable_iface_init (MetaCullableInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MetaSurfaceActor, meta_surface_actor, CLUTTER_TYPE_ACTOR,
                                  G_IMPLEMENT_INTERFACE (META_TYPE_CULLABLE, cullable_iface_init));

static gboolean
meta_surface_actor_get_paint_volume (ClutterActor       *actor,
                                     ClutterPaintVolume *volume)
{
  MetaSurfaceActor *self = META_SURFACE_ACTOR (actor);
  MetaSurfaceActorPrivate *priv = self->priv;

  if (!CLUTTER_ACTOR_CLASS (meta_surface_actor_parent_class)->get_paint_volume (actor, volume))
    return FALSE;

  if (priv->unobscured_region)
    {
      ClutterVertex origin;
      cairo_rectangle_int_t bounds, unobscured_bounds;

      /* I hate ClutterPaintVolume so much... */
      clutter_paint_volume_get_origin (volume, &origin);
      bounds.x = origin.x;
      bounds.y = origin.y;
      bounds.width = clutter_paint_volume_get_width (volume);
      bounds.height = clutter_paint_volume_get_height (volume);

      cairo_region_get_extents (priv->unobscured_region, &unobscured_bounds);
      gdk_rectangle_intersect (&bounds, &unobscured_bounds, &bounds);

      origin.x = bounds.x;
      origin.y = bounds.y;
      clutter_paint_volume_set_origin (volume, &origin);
      clutter_paint_volume_set_width (volume, bounds.width);
      clutter_paint_volume_set_height (volume, bounds.height);
    }

  return TRUE;
}

static void
meta_surface_actor_dispose (GObject *object)
{
  MetaSurfaceActor *self = META_SURFACE_ACTOR (object);
  MetaSurfaceActorPrivate *priv = self->priv;

  g_clear_pointer (&priv->unobscured_region, cairo_region_destroy);

  G_OBJECT_CLASS (meta_surface_actor_parent_class)->dispose (object);
}

static void
meta_surface_actor_class_init (MetaSurfaceActorClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  actor_class->get_paint_volume = meta_surface_actor_get_paint_volume;
  object_class->dispose = meta_surface_actor_dispose;

  g_type_class_add_private (klass, sizeof (MetaSurfaceActorPrivate));
}

static void
set_unobscured_region (MetaSurfaceActor *self,
                       cairo_region_t   *unobscured_region)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  if (priv->unobscured_region)
    cairo_region_destroy (priv->unobscured_region);

  if (unobscured_region)
    priv->unobscured_region = cairo_region_copy (unobscured_region);
  else
    priv->unobscured_region = NULL;
}

static void
meta_surface_actor_cull_out (MetaCullable   *cullable,
                             cairo_region_t *unobscured_region,
                             cairo_region_t *clip_region)
{
  MetaSurfaceActor *self = META_SURFACE_ACTOR (cullable);

  set_unobscured_region (self, unobscured_region);
  meta_cullable_cull_out_children (cullable, unobscured_region, clip_region);
}

static void
meta_surface_actor_reset_culling (MetaCullable *cullable)
{
  MetaSurfaceActor *self = META_SURFACE_ACTOR (cullable);

  set_unobscured_region (self, NULL);
  meta_cullable_reset_culling_children (cullable);
}

static void
cullable_iface_init (MetaCullableInterface *iface)
{
  iface->cull_out = meta_surface_actor_cull_out;
  iface->reset_culling = meta_surface_actor_reset_culling;
}

static void
meta_surface_actor_init (MetaSurfaceActor *self)
{
  MetaSurfaceActorPrivate *priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   META_TYPE_SURFACE_ACTOR,
                                                   MetaSurfaceActorPrivate);

  priv->texture = META_SHAPED_TEXTURE (meta_shaped_texture_new ());
  clutter_actor_add_child (CLUTTER_ACTOR (self), CLUTTER_ACTOR (priv->texture));
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

static cairo_region_t *
effective_unobscured_region (MetaSurfaceActor *self)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  return clutter_actor_has_mapped_clones (CLUTTER_ACTOR (self)) ? NULL : priv->unobscured_region;
}

gboolean
meta_surface_actor_redraw_area (MetaSurfaceActor *self,
                                int x, int y, int width, int height)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  return meta_shaped_texture_update_area (priv->texture,
                                          x, y, width, height,
                                          effective_unobscured_region (self));
}

gboolean
meta_surface_actor_is_obscured (MetaSurfaceActor *self)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  if (priv->unobscured_region)
    return cairo_region_is_empty (priv->unobscured_region);
  else
    return FALSE;
}

void
meta_surface_actor_set_input_region (MetaSurfaceActor *self,
                                     cairo_region_t   *region)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  meta_shaped_texture_set_input_shape_region (priv->texture, region);
}

void
meta_surface_actor_set_opaque_region (MetaSurfaceActor *self,
                                      cairo_region_t   *region)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  meta_shaped_texture_set_opaque_region (priv->texture, region);
}

void
meta_surface_actor_process_damage (MetaSurfaceActor *actor,
                                   int x, int y, int width, int height)
{
  META_SURFACE_ACTOR_GET_CLASS (actor)->process_damage (actor, x, y, width, height);
}

void
meta_surface_actor_pre_paint (MetaSurfaceActor *actor)
{
  META_SURFACE_ACTOR_GET_CLASS (actor)->pre_paint (actor);
}

gboolean
meta_surface_actor_is_argb32 (MetaSurfaceActor *actor)
{
  return META_SURFACE_ACTOR_GET_CLASS (actor)->is_argb32 (actor);
}

gboolean
meta_surface_actor_is_visible (MetaSurfaceActor *actor)
{
  return META_SURFACE_ACTOR_GET_CLASS (actor)->is_visible (actor);
}

void
meta_surface_actor_freeze (MetaSurfaceActor *actor)
{
  META_SURFACE_ACTOR_GET_CLASS (actor)->freeze (actor);
}

void
meta_surface_actor_thaw (MetaSurfaceActor *actor)
{
  META_SURFACE_ACTOR_GET_CLASS (actor)->thaw (actor);
}

gboolean
meta_surface_actor_is_frozen (MetaSurfaceActor *actor)
{
  return META_SURFACE_ACTOR_GET_CLASS (actor)->is_frozen (actor);
}

gboolean
meta_surface_actor_should_unredirect (MetaSurfaceActor *actor)
{
  return META_SURFACE_ACTOR_GET_CLASS (actor)->should_unredirect (actor);
}

void
meta_surface_actor_set_unredirected (MetaSurfaceActor *actor,
                                     gboolean          unredirected)
{
  META_SURFACE_ACTOR_GET_CLASS (actor)->set_unredirected (actor, unredirected);
}
