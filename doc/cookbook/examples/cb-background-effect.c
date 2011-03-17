#include "cb-background-effect.h"

G_DEFINE_TYPE (CbBackgroundEffect, cb_background_effect, CLUTTER_TYPE_EFFECT);

#define CB_BACKGROUND_EFFECT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                                                            CB_TYPE_BACKGROUND_EFFECT, \
                                                                            CbBackgroundEffectPrivate))

struct _CbBackgroundEffectPrivate
{
  CoglMaterial *background;
  CoglColor    *color;
};

/* ClutterEffect implementation */

/* note that if pre_paint() returns FALSE
 * any post_paint() defined for the effect will not be called
 */
static gboolean
cb_background_effect_pre_paint (ClutterEffect *self)
{
  ClutterActor *actor;
  gfloat width;
  gfloat height;
  CbBackgroundEffectPrivate *priv;

  priv = CB_BACKGROUND_EFFECT (self)->priv;

  /* get the associated actor's dimensions */
  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));
  clutter_actor_get_size (actor, &width, &height);

  /* draw a grey Cogl rectangle in the background */
  cogl_set_source (priv->background);

  cogl_rectangle (0, 0, width, height);

  return TRUE;
}

/* GObject implementation */
static void
cb_background_effect_dispose (GObject *gobject)
{
  CbBackgroundEffectPrivate *priv = CB_BACKGROUND_EFFECT (gobject)->priv;

  if (priv->background != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->background);
      priv->background = COGL_INVALID_HANDLE;
    }

  if (priv->color != NULL)
    {
      cogl_color_free (priv->color);
      priv->color = NULL;
    }

  G_OBJECT_CLASS (cb_background_effect_parent_class)->dispose (gobject);
}

static void
cb_background_effect_class_init (CbBackgroundEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  effect_class->pre_paint = cb_background_effect_pre_paint;
  gobject_class->dispose = cb_background_effect_dispose;

  g_type_class_add_private (klass, sizeof (CbBackgroundEffectPrivate));
}

static void
cb_background_effect_init (CbBackgroundEffect *self)
{
  CbBackgroundEffectPrivate *priv;

  priv = self->priv = CB_BACKGROUND_EFFECT_GET_PRIVATE (self);

  priv->background = cogl_material_new ();

  /* grey color for filling the background material */
  priv->color = cogl_color_new ();
  cogl_color_init_from_4ub (priv->color, 122, 122, 122, 255);

  cogl_material_set_color (priv->background, priv->color);
}

/* public API */

/**
 * cb_background_effect_new:
 *
 * Creates a new #ClutterEffect which adds a grey background
 * when applied to a rectangular actor.
 */
ClutterEffect *
cb_background_effect_new ()
{
  return g_object_new (CB_TYPE_BACKGROUND_EFFECT,
                       NULL);
}
