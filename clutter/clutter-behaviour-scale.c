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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-behaviour-scale
 * @short_description: A behaviour controlling scale
 *
 * A #ClutterBehaviourScale interpolates actors size between two values.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-fixed.h"
#include "clutter-behaviour-scale.h"
#include "clutter-private.h"
#include "clutter-debug.h"

#include <math.h>

G_DEFINE_TYPE (ClutterBehaviourScale,
               clutter_behaviour_scale,
	       CLUTTER_TYPE_BEHAVIOUR);

struct _ClutterBehaviourScalePrivate
{
  ClutterFixed x_scale_start;
  ClutterFixed y_scale_start;
  ClutterFixed x_scale_end;
  ClutterFixed y_scale_end;
};

#define CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE(obj)        \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),      \
               CLUTTER_TYPE_BEHAVIOUR_SCALE,            \
               ClutterBehaviourScalePrivate))

enum
{
  PROP_0,

  PROP_X_SCALE_START,
  PROP_Y_SCALE_START,
  PROP_X_SCALE_END,
  PROP_Y_SCALE_END,
};

typedef struct {
  ClutterFixed scale_x;
  ClutterFixed scale_y;
} ScaleFrameClosure;

static void
scale_frame_foreach (ClutterBehaviour *behaviour,
                     ClutterActor     *actor,
		     gpointer          data)
{
  ScaleFrameClosure *closure = data;

  clutter_actor_set_scalex (actor, closure->scale_x, closure->scale_y);
}

static void
clutter_behaviour_scale_alpha_notify (ClutterBehaviour *behave,
                                      guint32           alpha_value)
{
  ClutterBehaviourScalePrivate *priv;
  ClutterFixed scale_x, scale_y;
  ScaleFrameClosure closure = { 0, };

  priv = CLUTTER_BEHAVIOUR_SCALE (behave)->priv;

  /* Fix the start/end values, avoids potential rounding errors on large
   * values. 
  */
  if (alpha_value == CLUTTER_ALPHA_MAX_ALPHA)
    {
      scale_x = priv->x_scale_end;
      scale_y = priv->y_scale_end;
    }
  else if (alpha_value == 0)
    {
      scale_x = priv->x_scale_start;
      scale_y = priv->y_scale_start;
    }
  else
    {
      ClutterFixed factor;

      factor = (float)(alpha_value) / CLUTTER_ALPHA_MAX_ALPHA;

      scale_x =
        CLUTTER_FIXED_MUL (factor, (priv->x_scale_end - priv->x_scale_start));
      scale_x += priv->x_scale_start;
      
      scale_y =
        CLUTTER_FIXED_MUL (factor, (priv->y_scale_end - priv->y_scale_start));
      scale_y += priv->y_scale_start;
    }

  closure.scale_x = scale_x;
  closure.scale_y = scale_y;

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
      priv->x_scale_start = CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value));
      break;
    case PROP_X_SCALE_END:
      priv->x_scale_end = CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value));
      break;
    case PROP_Y_SCALE_START:
      priv->y_scale_start = CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value));
      break;
    case PROP_Y_SCALE_END:
      priv->y_scale_end = CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value));
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
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->x_scale_start));
      break;
    case PROP_X_SCALE_END:
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->x_scale_end));
      break;
    case PROP_Y_SCALE_START:
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->y_scale_start));
      break;
    case PROP_Y_SCALE_END:
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->y_scale_end));
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

  gobject_class->set_property = clutter_behaviour_scale_set_property;
  gobject_class->get_property = clutter_behaviour_scale_get_property;

  /**
   * ClutterBehaviourScale:x-scale-start:
   *
   * The initial scaling factor on the X axis for the actors.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_X_SCALE_START,
                                   g_param_spec_double ("x-scale-start",
                                                        "X Start Scale",
                                                        "Initial scale on the X axis",
                                                        0.0, G_MAXDOUBLE,
                                                        1.0,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourScale:x-scale-end:
   *
   * The final scaling factor on the X axis for the actors.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_X_SCALE_END,
                                   g_param_spec_double ("x-scale-end",
                                                        "X End Scale",
                                                        "Final scale on the X axis",
                                                        0.0, G_MAXDOUBLE,
                                                        1.0,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourScale:y-scale-start:
   *
   * The initial scaling factor on the Y axis for the actors.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_Y_SCALE_START,
                                   g_param_spec_double ("y-scale-start",
                                                        "Y Start Scale",
                                                        "Initial scale on the Y axis",
                                                        0.0, G_MAXDOUBLE,
                                                        1.0,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourScale:y-scale-end:
   *
   * The final scaling factor on the Y axis for the actors.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_Y_SCALE_END,
                                   g_param_spec_double ("y-scale-end",
                                                        "Y End Scale",
                                                        "Final scale on the Y axis",
                                                        0.0, G_MAXDOUBLE,
                                                        1.0,
                                                        CLUTTER_PARAM_READWRITE));

  behave_class->alpha_notify = clutter_behaviour_scale_alpha_notify;

  g_type_class_add_private (klass, sizeof (ClutterBehaviourScalePrivate));
}

static void
clutter_behaviour_scale_init (ClutterBehaviourScale *self)
{
  ClutterBehaviourScalePrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE (self);

  priv->x_scale_start = priv->x_scale_end = 1.0;
  priv->y_scale_start = priv->y_scale_end = 1.0;
}

/**
 * clutter_behaviour_scale_new:
 * @alpha: a #ClutterAlpha
 * @x_scale_start: initial scale factor on the X axis
 * @y_scale_start: initial scale factor on the Y axis
 * @x_scale_end: final scale factor on the X axis
 * @y_scale_end: final scale factor on the Y axis
 *
 * Creates a new  #ClutterBehaviourScale instance.
 *
 * Return value: the newly created #ClutterBehaviourScale
 *
 * Since: 0.2
 */
ClutterBehaviour *
clutter_behaviour_scale_new (ClutterAlpha   *alpha,
			     gdouble         x_scale_start,
			     gdouble         y_scale_start,
			     gdouble         x_scale_end,
			     gdouble         y_scale_end)
{
  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);

  return clutter_behaviour_scale_newx (alpha,
				       CLUTTER_FLOAT_TO_FIXED (x_scale_start),
				       CLUTTER_FLOAT_TO_FIXED (y_scale_start),
				       CLUTTER_FLOAT_TO_FIXED (x_scale_end),
				       CLUTTER_FLOAT_TO_FIXED (y_scale_end));
}

/**
 * clutter_behaviour_scale_newx:
 * @alpha: a #ClutterAlpha
 * @x_scale_start: initial scale factor on the X axis
 * @y_scale_start: initial scale factor on the Y axis
 * @x_scale_end: final scale factor on the X axis
 * @y_scale_end: final scale factor on the Y axis
 *
 * A fixed point implementation of clutter_behaviour_scale_new()
 *
 * Return value: the newly created #ClutterBehaviourScale
 *
 * Since: 0.2
 */
ClutterBehaviour *
clutter_behaviour_scale_newx (ClutterAlpha   *alpha,
			      ClutterFixed    x_scale_start,
			      ClutterFixed    y_scale_start,
			      ClutterFixed    x_scale_end,
			      ClutterFixed    y_scale_end)
{
  ClutterBehaviourScale *behave;

  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_SCALE, "alpha", alpha, NULL);

  behave->priv->x_scale_start = x_scale_start;
  behave->priv->y_scale_start = y_scale_start;
  behave->priv->x_scale_end   = x_scale_end;
  behave->priv->y_scale_end   = y_scale_end;

  return CLUTTER_BEHAVIOUR (behave);
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
 */
void
clutter_behaviour_scale_set_bounds (ClutterBehaviourScale *scale,
                                    gdouble                x_scale_start,
                                    gdouble                y_scale_start,
                                    gdouble                x_scale_end,
                                    gdouble                y_scale_end)
{
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_SCALE (scale));

  clutter_behaviour_scale_set_boundsx (scale,
                                       CLUTTER_FLOAT_TO_FIXED (x_scale_start),
                                       CLUTTER_FLOAT_TO_FIXED (y_scale_start),
                                       CLUTTER_FLOAT_TO_FIXED (x_scale_end),
                                       CLUTTER_FLOAT_TO_FIXED (y_scale_end));
}

/**
 * clutter_behaviour_scale_get_bounds:
 * @scale: a #ClutterBehaviourScale
 * @x_scale_start: return location for the initial scale factor on the X
 *   axis, or %NULL
 * @y_scale_start: return location for the initial scale factor on the Y
 *   axis, or %NULL
 * @x_scale_end: return location for the final scale factor on the X axis,
 *   or %NULL
 * @y_scale_end: return location for the final scale factor on the Y axis,
 *   or %NULL
 *
 * Retrieves the bounds used by scale behaviour.
 *
 * Since: 0.4
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
    *x_scale_start = CLUTTER_FIXED_TO_DOUBLE (priv->x_scale_start);

  if (x_scale_end)
    *x_scale_end = CLUTTER_FIXED_TO_DOUBLE (priv->x_scale_end);

  if (y_scale_start)
    *y_scale_start = CLUTTER_FIXED_TO_DOUBLE (priv->y_scale_start);

  if (y_scale_end)
    *y_scale_end = CLUTTER_FIXED_TO_DOUBLE (priv->y_scale_end);
}

/**
 * clutter_behaviour_scale_set_boundsx:
 * @scale: a #ClutterBehaviourScale
 * @x_scale_start: initial scale factor on the X axis
 * @y_scale_start: initial scale factor on the Y axis
 * @x_scale_end: final scale factor on the X axis
 * @y_scale_end: final scale factor on the Y axis
 *
 * Fixed point version of clutter_behaviour_scale_set_bounds().
 *
 * Sets the bounds used by scale behaviour.
 *
 * Since: 0.6
 */
void
clutter_behaviour_scale_set_boundsx (ClutterBehaviourScale *scale,
                                     ClutterFixed           x_scale_start,
                                     ClutterFixed           y_scale_start,
                                     ClutterFixed           x_scale_end,
                                     ClutterFixed           y_scale_end)
{
  ClutterBehaviourScalePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_SCALE (scale));

  priv = scale->priv;

  g_object_freeze_notify (G_OBJECT (scale));

  if (priv->x_scale_start != x_scale_start)
    {
      priv->x_scale_start = x_scale_start;
      g_object_notify (G_OBJECT (scale), "x-scale-start");
    }

  if (priv->y_scale_start != y_scale_start)
    {
      priv->y_scale_start = y_scale_start;
      g_object_notify (G_OBJECT (scale), "y-scale-start");
    }

  if (priv->x_scale_end != x_scale_end)
    {
      priv->x_scale_end = x_scale_end;
      g_object_notify (G_OBJECT (scale), "x-scale-end");
    }

  if (priv->y_scale_end != y_scale_end)
    {
      priv->y_scale_end = y_scale_end;
      g_object_notify (G_OBJECT (scale), "y-scale-end");
    }

  g_object_thaw_notify (G_OBJECT (scale));
}

/**
 * clutter_behaviour_scale_get_boundsx:
 * @scale: a #ClutterBehaviourScale
 * @x_scale_start: return location for the initial scale factor on the X
 *   axis, or %NULL
 * @y_scale_start: return location for the initial scale factor on the Y
 *   axis, or %NULL
 * @x_scale_end: return location for the final scale factor on the X axis,
 *   or %NULL
 * @y_scale_end: return location for the final scale factor on the Y axis,
 *   or %NULL
 *
 * Fixed point version of clutter_behaviour_scale_get_bounds().
 *
 * Retrieves the bounds used by scale behaviour.
 *
 * Since: 0.4
 */
void
clutter_behaviour_scale_get_boundsx (ClutterBehaviourScale *scale,
                                     ClutterFixed          *x_scale_start,
                                     ClutterFixed          *y_scale_start,
                                     ClutterFixed          *x_scale_end,
                                     ClutterFixed          *y_scale_end)
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

