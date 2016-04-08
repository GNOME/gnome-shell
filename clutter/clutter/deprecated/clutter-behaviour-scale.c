/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-behaviour-scale
 * @Title: ClutterBehaviourScale
 * @short_description: A behaviour controlling scale
 * @Deprecated: 1.6: Use clutter_actor_animate() with #ClutterActor:scale-x
 *   and #ClutterActor:scale-y instead.
 *
 * A #ClutterBehaviourScale interpolates actors size between two values.
 *
 * Deprecated: 1.6: Use the #ClutterActor:scale-x and #ClutterActor:scale-y
 *   properties, and clutter_actor_animate(), or #ClutterAnimator or
 *   #ClutterState instead.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-actor.h"

#include "clutter-alpha.h"
#include "clutter-behaviour.h"
#include "clutter-behaviour-scale.h"
#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-private.h"

struct _ClutterBehaviourScalePrivate
{
  gdouble x_scale_start;
  gdouble y_scale_start;

  gdouble x_scale_end;
  gdouble y_scale_end;
};

enum
{
  PROP_0,

  PROP_X_SCALE_START,
  PROP_Y_SCALE_START,
  PROP_X_SCALE_END,
  PROP_Y_SCALE_END,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (ClutterBehaviourScale,
                            clutter_behaviour_scale,
                            CLUTTER_TYPE_BEHAVIOUR)

typedef struct {
  gdouble scale_x;
  gdouble scale_y;
} ScaleFrameClosure;

static void
scale_frame_foreach (ClutterBehaviour *behaviour,
                     ClutterActor     *actor,
		     gpointer          data)
{
  ScaleFrameClosure *closure = data;

  clutter_actor_set_scale (actor, closure->scale_x, closure->scale_y);
}

static void
clutter_behaviour_scale_alpha_notify (ClutterBehaviour *behave,
                                      gdouble           alpha_value)
{
  ClutterBehaviourScalePrivate *priv;
  ScaleFrameClosure closure = { 0, };

  priv = CLUTTER_BEHAVIOUR_SCALE (behave)->priv;

  /* Fix the start/end values, avoids potential rounding errors on large
   * values.
  */
  if (alpha_value == 1.0)
    {
      closure.scale_x = priv->x_scale_end;
      closure.scale_y = priv->y_scale_end;
    }
  else if (alpha_value == 0)
    {
      closure.scale_x = priv->x_scale_start;
      closure.scale_y = priv->y_scale_start;
    }
  else
    {
      closure.scale_x = (priv->x_scale_end - priv->x_scale_start)
                      * alpha_value
                      + priv->x_scale_start;

      closure.scale_y = (priv->y_scale_end - priv->y_scale_start)
                      * alpha_value
                      + priv->y_scale_start;
    }

  clutter_behaviour_actors_foreach (behave,
                                    scale_frame_foreach,
                                    &closure);
}

static void
clutter_behaviour_scale_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterBehaviourScalePrivate *priv;

  priv = CLUTTER_BEHAVIOUR_SCALE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_X_SCALE_START:
      priv->x_scale_start = g_value_get_double (value);
      break;

    case PROP_X_SCALE_END:
      priv->x_scale_end = g_value_get_double (value);
      break;

    case PROP_Y_SCALE_START:
      priv->y_scale_start = g_value_get_double (value);
      break;

    case PROP_Y_SCALE_END:
      priv->y_scale_end = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_scale_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterBehaviourScalePrivate *priv;

  priv = CLUTTER_BEHAVIOUR_SCALE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_X_SCALE_START:
      g_value_set_double (value, priv->x_scale_start);
      break;

    case PROP_X_SCALE_END:
      g_value_set_double (value, priv->x_scale_end);
      break;

    case PROP_Y_SCALE_START:
      g_value_set_double (value, priv->y_scale_start);
      break;

    case PROP_Y_SCALE_END:
      g_value_set_double (value, priv->y_scale_end);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_scale_class_init (ClutterBehaviourScaleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behave_class = CLUTTER_BEHAVIOUR_CLASS (klass);
  GParamSpec *pspec = NULL;

  gobject_class->set_property = clutter_behaviour_scale_set_property;
  gobject_class->get_property = clutter_behaviour_scale_get_property;

  /**
   * ClutterBehaviourScale:x-scale-start:
   *
   * The initial scaling factor on the X axis for the actors.
   *
   * Since: 0.6
   *
   * Deprecated: 1.6
   */
  pspec = g_param_spec_double ("x-scale-start",
                               P_("X Start Scale"),
                               P_("Initial scale on the X axis"),
                               0.0, G_MAXDOUBLE,
                               1.0,
                               CLUTTER_PARAM_READWRITE);
  obj_props[PROP_X_SCALE_START] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_X_SCALE_START,
                                   pspec);
  /**
   * ClutterBehaviourScale:x-scale-end:
   *
   * The final scaling factor on the X axis for the actors.
   *
   * Since: 0.6
   *
   * Deprecated: 1.6
   */
  pspec = g_param_spec_double ("x-scale-end",
                               P_("X End Scale"),
                               P_("Final scale on the X axis"),
                               0.0, G_MAXDOUBLE,
                               1.0,
                               CLUTTER_PARAM_READWRITE);
  obj_props[PROP_X_SCALE_END] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_X_SCALE_END,
                                   pspec);
  /**
   * ClutterBehaviourScale:y-scale-start:
   *
   * The initial scaling factor on the Y axis for the actors.
   *
   * Since: 0.6
   *
   * Deprecated: 1.6
   */
  pspec = g_param_spec_double ("y-scale-start",
                               P_("Y Start Scale"),
                               P_("Initial scale on the Y axis"),
                               0.0, G_MAXDOUBLE,
                               1.0,
                               CLUTTER_PARAM_READWRITE);
  obj_props[PROP_Y_SCALE_START] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_Y_SCALE_START,
                                   pspec);
  /**
   * ClutterBehaviourScale:y-scale-end:
   *
   * The final scaling factor on the Y axis for the actors.
   *
   * Since: 0.6
   *
   * Deprecated: 1.6
   */
  pspec = g_param_spec_double ("y-scale-end",
                               P_("Y End Scale"),
                               P_("Final scale on the Y axis"),
                               0.0, G_MAXDOUBLE,
                               1.0,
                               CLUTTER_PARAM_READWRITE);
  obj_props[PROP_Y_SCALE_END] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_Y_SCALE_END,
                                   pspec);

  behave_class->alpha_notify = clutter_behaviour_scale_alpha_notify;
}

static void
clutter_behaviour_scale_init (ClutterBehaviourScale *self)
{
  ClutterBehaviourScalePrivate *priv;

  self->priv = priv = clutter_behaviour_scale_get_instance_private (self);

  priv->x_scale_start = priv->x_scale_end = 1.0;
  priv->y_scale_start = priv->y_scale_end = 1.0;
}

/**
 * clutter_behaviour_scale_new:
 * @alpha: (allow-none): a #ClutterAlpha instance, or %NULL
 * @x_scale_start: initial scale factor on the X axis
 * @y_scale_start: initial scale factor on the Y axis
 * @x_scale_end: final scale factor on the X axis
 * @y_scale_end: final scale factor on the Y axis
 *
 * Creates a new  #ClutterBehaviourScale instance.
 *
 * If @alpha is not %NULL, the #ClutterBehaviour will take ownership
 * of the #ClutterAlpha instance. In the case when @alpha is %NULL,
 * it can be set later with clutter_behaviour_set_alpha().
 *
 * Return value: (transfer full): the newly created #ClutterBehaviourScale
 *
 * Since: 0.2
 *
 * Deprecated: 1.6
 */
ClutterBehaviour *
clutter_behaviour_scale_new (ClutterAlpha   *alpha,
			     gdouble         x_scale_start,
			     gdouble         y_scale_start,
			     gdouble         x_scale_end,
			     gdouble         y_scale_end)
{
  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);

  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_SCALE,
                       "alpha", alpha,
                       "x-scale-start", x_scale_start,
                       "y-scale-start", y_scale_start,
                       "x-scale-end", x_scale_end,
                       "y-scale-end", y_scale_end,
                       NULL);
}

/**
 * clutter_behaviour_scale_set_bounds:
 * @scale: a #ClutterBehaviourScale
 * @x_scale_start: initial scale factor on the X axis
 * @y_scale_start: initial scale factor on the Y axis
 * @x_scale_end: final scale factor on the X axis
 * @y_scale_end: final scale factor on the Y axis
 *
 * Sets the bounds used by scale behaviour.
 *
 * Since: 0.6
 *
 * Deprecated: 1.6
 */
void
clutter_behaviour_scale_set_bounds (ClutterBehaviourScale *scale,
                                    gdouble                x_scale_start,
                                    gdouble                y_scale_start,
                                    gdouble                x_scale_end,
                                    gdouble                y_scale_end)
{
  ClutterBehaviourScalePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_SCALE (scale));

  priv = scale->priv;

  g_object_freeze_notify (G_OBJECT (scale));

  if (priv->x_scale_start != x_scale_start)
    {
      priv->x_scale_start = x_scale_start;
      g_object_notify_by_pspec (G_OBJECT (scale), obj_props[PROP_X_SCALE_START]);
    }

  if (priv->y_scale_start != y_scale_start)
    {
      priv->y_scale_start = y_scale_start;
      g_object_notify_by_pspec (G_OBJECT (scale), obj_props[PROP_Y_SCALE_START]);
    }

  if (priv->x_scale_end != x_scale_end)
    {
      priv->x_scale_end = x_scale_end;
      g_object_notify_by_pspec (G_OBJECT (scale), obj_props[PROP_X_SCALE_END]);
    }

  if (priv->y_scale_end != y_scale_end)
    {
      priv->y_scale_end = y_scale_end;
      g_object_notify_by_pspec (G_OBJECT (scale), obj_props[PROP_Y_SCALE_END]);
    }

  g_object_thaw_notify (G_OBJECT (scale));
}

/**
 * clutter_behaviour_scale_get_bounds:
 * @scale: a #ClutterBehaviourScale
 * @x_scale_start: (out): return location for the initial scale factor on the X
 *   axis, or %NULL
 * @y_scale_start: (out): return location for the initial scale factor on the Y
 *   axis, or %NULL
 * @x_scale_end: (out): return location for the final scale factor on the X axis,
 *   or %NULL
 * @y_scale_end: (out): return location for the final scale factor on the Y axis,
 *   or %NULL
 *
 * Retrieves the bounds used by scale behaviour.
 *
 * Since: 0.4
 *
 * Deprecated: 1.6
 */
void
clutter_behaviour_scale_get_bounds (ClutterBehaviourScale *scale,
                                    gdouble               *x_scale_start,
                                    gdouble               *y_scale_start,
                                    gdouble               *x_scale_end,
                                    gdouble               *y_scale_end)
{
  ClutterBehaviourScalePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_SCALE (scale));

  priv = scale->priv;

  if (x_scale_start)
    *x_scale_start = priv->x_scale_start;

  if (x_scale_end)
    *x_scale_end = priv->x_scale_end;

  if (y_scale_start)
    *y_scale_start = priv->y_scale_start;

  if (y_scale_end)
    *y_scale_end = priv->y_scale_end;
}
