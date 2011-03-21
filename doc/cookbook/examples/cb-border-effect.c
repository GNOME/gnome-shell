#include "cb-border-effect.h"

G_DEFINE_TYPE (CbBorderEffect, cb_border_effect, CLUTTER_TYPE_EFFECT);

#define CB_BORDER_EFFECT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                                                        CB_TYPE_BORDER_EFFECT, \
                                                                        CbBorderEffectPrivate))

static const ClutterColor grey = { 0xaa, 0xaa, 0xaa, 0xff };

struct _CbBorderEffectPrivate
{
  CoglMaterial *border;
  ClutterColor  color;
  gfloat        width;
};

enum {
  PROP_0,

  PROP_COLOR,
  PROP_WIDTH,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

/* ClutterEffect implementation */
static void
cb_border_effect_post_paint (ClutterEffect *self)
{
  ClutterActor *actor;
  gfloat width;
  gfloat height;
  CbBorderEffectPrivate *priv;

  priv = CB_BORDER_EFFECT (self)->priv;

  /* get the associated actor's dimensions */
  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));
  clutter_actor_get_size (actor, &width, &height);

  /* draw Cogl rectangles on top */
  cogl_set_source (priv->border);
  cogl_path_new ();

  /* left rectangle */
  cogl_path_rectangle (0, 0, priv->width, height);

  /* top rectangle */
  cogl_path_rectangle (priv->width, 0, width, priv->width);

  /* right rectangle */
  cogl_path_rectangle (width - priv->width, priv->width, width, height);

  /* bottom rectangle */
  cogl_path_rectangle (priv->width,
                       height - priv->width,
                       width - priv->width,
                       height);

  cogl_path_fill ();
}

/* GObject implementation */
static void
cb_border_effect_dispose (GObject *gobject)
{
  CbBorderEffectPrivate *priv = CB_BORDER_EFFECT (gobject)->priv;

  if (priv->border != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->border);
      priv->border = COGL_INVALID_HANDLE;
    }

  G_OBJECT_CLASS (cb_border_effect_parent_class)->dispose (gobject);
}

static void
cb_border_effect_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CbBorderEffect *effect = CB_BORDER_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_COLOR:
      cb_border_effect_set_color (effect, clutter_value_get_color (value));
      break;

    case PROP_WIDTH:
      cb_border_effect_set_width (effect, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
cb_border_effect_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CbBorderEffectPrivate *priv = CB_BORDER_EFFECT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_COLOR:
      g_value_set_object (value, &(priv->color));
      break;

    case PROP_WIDTH:
      g_value_set_float (value, priv->width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

/* GObject class and instance init */
static void
cb_border_effect_class_init (CbBorderEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  effect_class->post_paint = cb_border_effect_post_paint;

  gobject_class->set_property = cb_border_effect_set_property;
  gobject_class->get_property = cb_border_effect_get_property;
  gobject_class->dispose = cb_border_effect_dispose;

  g_type_class_add_private (klass, sizeof (CbBorderEffectPrivate));

  /**
   * CbBorderEffect:width:
   *
   * The width of the border
   */
  pspec = g_param_spec_float ("width",
                              "Width",
                              "The width of the border (in pixels)",
                              1.0, 100.0,
                              10.0,
                              G_PARAM_READWRITE);
  obj_props[PROP_WIDTH] = pspec;
  g_object_class_install_property (gobject_class, PROP_WIDTH, pspec);

  /**
   * CbBorderEffect:color:
   *
   * The color of the border
   */
  pspec = clutter_param_spec_color ("color",
                                    "Color",
                                    "The border color",
                                    &grey,
                                    G_PARAM_READWRITE);
  obj_props[PROP_COLOR] = pspec;
  g_object_class_install_property (gobject_class, PROP_COLOR, pspec);
}

static void
cb_border_effect_init (CbBorderEffect *self)
{
  CbBorderEffectPrivate *priv;

  priv = self->priv = CB_BORDER_EFFECT_GET_PRIVATE (self);

  priv->border = cogl_material_new ();

  priv->color = grey;
}

/* called each time a property is set on the effect */
static void
cb_border_effect_update (CbBorderEffect *self)
{
  ClutterActor *actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));

  if (actor != NULL)
    clutter_actor_queue_redraw (actor);
}

/* public API */

/**
 * cb_border_effect_new:
 * @width: width of the border applied by the effect
 * @color: a #ClutterColor
 *
 * Creates a new #ClutterEffect with the given @width
 * and of the given @color.
 */
ClutterEffect *
cb_border_effect_new (gfloat              width,
                      const ClutterColor *color)
{
  return g_object_new (CB_TYPE_BORDER_EFFECT,
                       "width", width,
                       "color", color,
                       NULL);
}

/**
 * cb_border_effect_set_color:
 * @self: a #CbBorderEffect
 * @color: a #ClutterColor
 *
 * Sets the color of the border provided by the effect @self.
 */
void
cb_border_effect_set_color (CbBorderEffect     *self,
                            const ClutterColor *color)
{
  CbBorderEffectPrivate *priv;

  g_return_if_fail (CB_IS_BORDER_EFFECT (self));
  g_return_if_fail (color != NULL);

  priv = CB_BORDER_EFFECT_GET_PRIVATE (self);

  priv->color.red = color->red;
  priv->color.green = color->green;
  priv->color.blue = color->blue;
  priv->color.alpha = color->alpha;

  cogl_material_set_color4ub (priv->border,
                              color->red,
                              color->green,
                              color->blue,
                              color->alpha);

  cb_border_effect_update (self);
}

/**
 * cb_border_effect_get_color:
 * @self: a #CbBorderEffect
 * @color: return location for a #ClutterColor
 *
 * Retrieves the color of the border applied by the effect @self.
 */
void
cb_border_effect_get_color (CbBorderEffect *self,
                            ClutterColor   *color)
{
  CbBorderEffectPrivate *priv;

  g_return_if_fail (CB_IS_BORDER_EFFECT (self));

  priv = CB_BORDER_EFFECT_GET_PRIVATE (self);

  color->red = priv->color.red;
  color->green = priv->color.green;
  color->blue = priv->color.blue;
  color->alpha = priv->color.alpha;
}

/**
 * cb_border_effect_set_width:
 * @self: a #CbBorderEffect
 * @width: the width of the border
 *
 * Sets the width (in pixels) of the border applied by the effect @self.
 */
void
cb_border_effect_set_width (CbBorderEffect *self,
                            gfloat          width)
{
  CbBorderEffectPrivate *priv;

  g_return_if_fail (CB_IS_BORDER_EFFECT (self));

  priv = CB_BORDER_EFFECT_GET_PRIVATE (self);

  priv->width = width;

  cb_border_effect_update (self);
}

/**
 * cb_border_effect_get_width:
 * @self: a #CbBorderEffect
 *
 * Gets the width (in pixels) of the border applied by the effect @self.
 *
 * Return value: the border's width, or 0.0 if @self is not
 * a #CbBorderEffect
 */
gfloat
cb_border_effect_get_width (CbBorderEffect *self)
{
  CbBorderEffectPrivate *priv;

  g_return_val_if_fail (CB_IS_BORDER_EFFECT (self), 0.0);

  priv = CB_BORDER_EFFECT_GET_PRIVATE (self);

  return priv->width;
}
