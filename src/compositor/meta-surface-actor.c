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

  cairo_region_t *input_region;

  /* Freeze/thaw accounting */
  guint needs_damage_all : 1;
  guint frozen : 1;
};

static void cullable_iface_init (MetaCullableInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MetaSurfaceActor, meta_surface_actor, CLUTTER_TYPE_ACTOR,
                                  G_IMPLEMENT_INTERFACE (META_TYPE_CULLABLE, cullable_iface_init));

enum {
  REPAINT_SCHEDULED,

  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

gboolean
meta_surface_actor_get_unobscured_bounds (MetaSurfaceActor      *self,
                                          cairo_rectangle_int_t *unobscured_bounds)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  return meta_shaped_texture_get_unobscured_bounds (priv->texture, unobscured_bounds);
}

static void
meta_surface_actor_pick (ClutterActor       *actor,
                         const ClutterColor *color)
{
  MetaSurfaceActor *self = META_SURFACE_ACTOR (actor);
  MetaSurfaceActorPrivate *priv = self->priv;

  if (!clutter_actor_should_pick_paint (actor))
    return;

  /* If there is no region then use the regular pick */
  if (priv->input_region == NULL)
    CLUTTER_ACTOR_CLASS (meta_surface_actor_parent_class)->pick (actor, color);
  else
    {
      int n_rects;
      float *rectangles;
      int i;
      CoglPipeline *pipeline;
      CoglContext *ctx;
      CoglFramebuffer *fb;
      CoglColor cogl_color;

      n_rects = cairo_region_num_rectangles (priv->input_region);
      rectangles = g_alloca (sizeof (float) * 4 * n_rects);

      for (i = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;
          int pos = i * 4;

          cairo_region_get_rectangle (priv->input_region, i, &rect);

          rectangles[pos + 0] = rect.x;
          rectangles[pos + 1] = rect.y;
          rectangles[pos + 2] = rect.x + rect.width;
          rectangles[pos + 3] = rect.y + rect.height;
        }

      ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
      fb = cogl_get_draw_framebuffer ();

      cogl_color_init_from_4ub (&cogl_color, color->red, color->green, color->blue, color->alpha);

      pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_color (pipeline, &cogl_color);
      cogl_framebuffer_draw_rectangles (fb, pipeline, rectangles, n_rects);
      cogl_object_unref (pipeline);
    }
}

static void
meta_surface_actor_dispose (GObject *object)
{
  MetaSurfaceActor *self = META_SURFACE_ACTOR (object);
  MetaSurfaceActorPrivate *priv = self->priv;

  g_clear_pointer (&priv->input_region, cairo_region_destroy);

  G_OBJECT_CLASS (meta_surface_actor_parent_class)->dispose (object);
}

static void
meta_surface_actor_class_init (MetaSurfaceActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  object_class->dispose = meta_surface_actor_dispose;
  actor_class->pick = meta_surface_actor_pick;

  signals[REPAINT_SCHEDULED] = g_signal_new ("repaint-scheduled",
                                             G_TYPE_FROM_CLASS (object_class),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, NULL, NULL,
                                             G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (MetaSurfaceActorPrivate));
}

static void
meta_surface_actor_cull_out (MetaCullable   *cullable,
                             cairo_region_t *unobscured_region,
                             cairo_region_t *clip_region)
{
  meta_cullable_cull_out_children (cullable, unobscured_region, clip_region);
}

static void
meta_surface_actor_reset_culling (MetaCullable *cullable)
{
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

void
meta_surface_actor_update_area (MetaSurfaceActor *self,
                                int x, int y, int width, int height)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  if (meta_shaped_texture_update_area (priv->texture, x, y, width, height))
    g_signal_emit (self, signals[REPAINT_SCHEDULED], 0);
}

gboolean
meta_surface_actor_is_obscured (MetaSurfaceActor *self)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  return meta_shaped_texture_is_obscured (priv->texture);
}

void
meta_surface_actor_set_input_region (MetaSurfaceActor *self,
                                     cairo_region_t   *region)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  if (priv->input_region)
    cairo_region_destroy (priv->input_region);

  if (region)
    priv->input_region = cairo_region_reference (region);
  else
    priv->input_region = NULL;
}

void
meta_surface_actor_set_opaque_region (MetaSurfaceActor *self,
                                      cairo_region_t   *region)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  meta_shaped_texture_set_opaque_region (priv->texture, region);
}

static gboolean
is_frozen (MetaSurfaceActor *self)
{
  MetaSurfaceActorPrivate *priv = self->priv;
  return priv->frozen;
}

void
meta_surface_actor_process_damage (MetaSurfaceActor *self,
                                   int x, int y, int width, int height)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  if (is_frozen (self))
    {
      /* The window is frozen due to an effect in progress: we ignore damage
       * here on the off chance that this will stop the corresponding
       * texture_from_pixmap from being update.
       *
       * needs_damage_all tracks that some unknown damage happened while the
       * window was frozen so that when the window becomes unfrozen we can
       * issue a full window update to cover any lost damage.
       *
       * It should be noted that this is an unreliable mechanism since it's
       * quite likely that drivers will aim to provide a zero-copy
       * implementation of the texture_from_pixmap extension and in those cases
       * any drawing done to the window is always immediately reflected in the
       * texture regardless of damage event handling.
       */
      priv->needs_damage_all = TRUE;
      return;
    }

  META_SURFACE_ACTOR_GET_CLASS (self)->process_damage (self, x, y, width, height);
}

void
meta_surface_actor_pre_paint (MetaSurfaceActor *self)
{
  META_SURFACE_ACTOR_GET_CLASS (self)->pre_paint (self);
}

gboolean
meta_surface_actor_is_argb32 (MetaSurfaceActor *self)
{
  return META_SURFACE_ACTOR_GET_CLASS (self)->is_argb32 (self);
}

gboolean
meta_surface_actor_is_visible (MetaSurfaceActor *self)
{
  return META_SURFACE_ACTOR_GET_CLASS (self)->is_visible (self);
}

void
meta_surface_actor_set_frozen (MetaSurfaceActor *self,
                               gboolean          frozen)
{
  MetaSurfaceActorPrivate *priv = self->priv;

  priv->frozen = frozen;

  if (!frozen && priv->needs_damage_all)
    {
      /* Since we ignore damage events while a window is frozen for certain effects
       * we may need to issue an update_area() covering the whole pixmap if we
       * don't know what real damage has happened. */

      meta_surface_actor_process_damage (self, 0, 0,
                                         clutter_actor_get_width (CLUTTER_ACTOR (priv->texture)),
                                         clutter_actor_get_height (CLUTTER_ACTOR (priv->texture)));
      priv->needs_damage_all = FALSE;
    }
}

gboolean
meta_surface_actor_should_unredirect (MetaSurfaceActor *self)
{
  return META_SURFACE_ACTOR_GET_CLASS (self)->should_unredirect (self);
}

void
meta_surface_actor_set_unredirected (MetaSurfaceActor *self,
                                     gboolean          unredirected)
{
  META_SURFACE_ACTOR_GET_CLASS (self)->set_unredirected (self, unredirected);
}

gboolean
meta_surface_actor_is_unredirected (MetaSurfaceActor *self)
{
  return META_SURFACE_ACTOR_GET_CLASS (self)->is_unredirected (self);
}

MetaWindow *
meta_surface_actor_get_window (MetaSurfaceActor *self)
{
  return META_SURFACE_ACTOR_GET_CLASS (self)->get_window (self);
}
