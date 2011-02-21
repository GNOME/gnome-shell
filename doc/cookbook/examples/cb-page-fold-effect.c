#include <math.h>
#include "cb-page-fold-effect.h"

G_DEFINE_TYPE (CbPageFoldEffect, cb_page_fold_effect, CLUTTER_TYPE_DEFORM_EFFECT);

#define CB_PAGE_FOLD_EFFECT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                                                           CB_TYPE_PAGE_FOLD_EFFECT, \
                                                                           CbPageFoldEffectPrivate))

struct _CbPageFoldEffectPrivate
{
  gdouble angle;
  gdouble period;
};

enum {
  PROP_0,

  PROP_PERIOD,
  PROP_ANGLE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

/* ClutterDeformEffect implementation */
static void
cb_page_fold_effect_deform_vertex (ClutterDeformEffect *effect,
                                   gfloat               width,
                                   gfloat               height,
                                   CoglTextureVertex   *vertex)
{
  CbPageFoldEffectPrivate *priv = CB_PAGE_FOLD_EFFECT (effect)->priv;

  gfloat radians = (priv->angle * priv->period) / (180.0f / G_PI);

  /* rotate from the center of the actor on the y axis */
  gfloat adjusted_x = vertex->x - (width / 2);

  /* only rotate vertices to the right of the middle of the actor */
  if (adjusted_x >= 0.0)
    {
      vertex->x = (vertex->z * sin (radians))
                  + (adjusted_x * cos (radians))
                  + width / 2;

      /* NB add 1 to z to prevent "z fighting"; otherwise, when fully-folded
       * the image has "stripes" where vertices from the folded part
       * of the actor interfere with vertices from the unfolded part
       */
      vertex->z = (vertex->z * cos (radians))
                  + (adjusted_x * sin (radians))
                  + 1;
    }

  /* adjust depth of all vertices so they fit inside the actor while folding;
   * this has the effect of making the image smaller within the texture,
   * but does produce a cleaner fold animation
   */
  vertex->z -= width / 2;
}

/* GObject implementation */
static void
cb_page_fold_effect_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CbPageFoldEffect *effect = CB_PAGE_FOLD_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_PERIOD:
      cb_page_fold_effect_set_period (effect, g_value_get_double (value));
      break;

    case PROP_ANGLE:
      cb_page_fold_effect_set_angle (effect, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
cb_page_fold_effect_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CbPageFoldEffectPrivate *priv = CB_PAGE_FOLD_EFFECT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_PERIOD:
      g_value_set_double (value, priv->period);
      break;

    case PROP_ANGLE:
      g_value_set_double (value, priv->angle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

/* GObject class and instance init */
static void
cb_page_fold_effect_class_init (CbPageFoldEffectClass *klass)
{
  GParamSpec *pspec;
  ClutterDeformEffectClass *effect_class = CLUTTER_DEFORM_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  effect_class->deform_vertex = cb_page_fold_effect_deform_vertex;

  gobject_class->set_property = cb_page_fold_effect_set_property;
  gobject_class->get_property = cb_page_fold_effect_get_property;

  g_type_class_add_private (klass, sizeof (CbPageFoldEffectPrivate));

  /**
   * CbPageFoldEffect:period:
   *
   * The period of the page fold, between 0.0 (no fold) and
   * 1.0 (fully folded)
   */
  pspec = g_param_spec_double ("period",
                               "Period",
                               "The period of the page fold",
                               0.0, 1.0,
                               0.0,
                               G_PARAM_READWRITE);
  obj_props[PROP_PERIOD] = pspec;
  g_object_class_install_property (gobject_class, PROP_PERIOD, pspec);

  /**
   * CbPageFoldEffect:angle:
   *
   * The angle of the page fold, in degrees, between 0.0 and 180.0
   */
  pspec = g_param_spec_double ("angle",
                               "Angle",
                               "The angle of the page fold, in degrees",
                               0.0, 180.0,
                               0.0,
                               G_PARAM_READWRITE);
  obj_props[PROP_ANGLE] = pspec;
  g_object_class_install_property (gobject_class, PROP_ANGLE, pspec);
}

static void
cb_page_fold_effect_init (CbPageFoldEffect *self)
{
  CbPageFoldEffectPrivate *priv;

  priv = self->priv = CB_PAGE_FOLD_EFFECT_GET_PRIVATE (self);

  priv->period = 0.0;
  priv->angle = 0.0;
}

/* public API */
ClutterEffect *
cb_page_fold_effect_new (gdouble angle,
                         gdouble period)
{
  return g_object_new (CB_TYPE_PAGE_FOLD_EFFECT,
                       "angle", angle,
                       "period", period,
                       NULL);
}

/**
 * cb_page_fold_effect_set_period:
 * @effect: a #CbPageFoldEffect
 * @period: the period of the page fold, between 0.0 and 1.0
 *
 * Sets the period of the page fold, between 0.0 (no fold)
 * and 1.0 (fully folded)
 */
void
cb_page_fold_effect_set_period (CbPageFoldEffect *effect,
                                gdouble           period)
{
  g_return_if_fail (CB_IS_PAGE_FOLD_EFFECT (effect));
  g_return_if_fail (period >= 0.0 && period <= 1.0);

  effect->priv->period = period;

  clutter_deform_effect_invalidate (CLUTTER_DEFORM_EFFECT (effect));
}

/**
 * cb_page_fold_effect_get_period:
 * @effect: a #CbPageFoldEffect
 *
 * Retrieves the value set using cb_page_fold_effect_get_period()
 *
 * Return value: the period of the page fold
 */
gdouble
cb_page_fold_effect_get_period (CbPageFoldEffect *effect)
{
  g_return_val_if_fail (CB_IS_PAGE_FOLD_EFFECT (effect), 0.0);

  return effect->priv->period;
}

/**
 * cb_page_fold_effect_set_angle:
 * @effect: #CbPageFoldEffect
 * @angle: the angle of the page fold, in degrees
 *
 * Sets the angle of the page fold, in degrees; must be a value between
 * 0.0 and 180.0
 */
void
cb_page_fold_effect_set_angle (CbPageFoldEffect *effect,
                               gdouble           angle)
{
  g_return_if_fail (CB_IS_PAGE_FOLD_EFFECT (effect));
  g_return_if_fail (angle >= 0.0 && angle <= 180.0);

  effect->priv->angle = angle;

  clutter_deform_effect_invalidate (CLUTTER_DEFORM_EFFECT (effect));
}

/**
 * cb_page_fold_effect_get_angle:
 * @effect: a #CbPageFoldEffect:
 *
 * Retrieves the angle of the page fold, in degrees
 *
 * Return value: the angle of the page fold
 */
gdouble
cb_page_fold_effect_get_angle (CbPageFoldEffect *effect)
{
  g_return_val_if_fail (CB_IS_PAGE_FOLD_EFFECT (effect), 0.0);

  return effect->priv->angle;
}
