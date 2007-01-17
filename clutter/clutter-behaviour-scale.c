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
 * @short_description: A behaviour class interpolating actors size between
 * two values.
 *
 * #ClutterBehaviourPath interpolates actors size between two values.
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

/**
 * SECTION:clutter-behaviour-scale
 * @short_description: Behaviour controlling the scale of a set of actors
 *
 * FIXME
 */

G_DEFINE_TYPE (ClutterBehaviourScale,
               clutter_behaviour_scale,
	       CLUTTER_TYPE_BEHAVIOUR);

struct _ClutterBehaviourScalePrivate
{
  ClutterFixed scale_begin;
  ClutterFixed scale_end;

  ClutterGravity gravity;
};

#define CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE(obj)        \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),      \
               CLUTTER_TYPE_BEHAVIOUR_SCALE,            \
               ClutterBehaviourScalePrivate))

enum
{
  PROP_0,

  PROP_SCALE_BEGIN,
  PROP_SCALE_END,
  PROP_SCALE_GRAVITY
};

static void
scale_frame_foreach (ClutterBehaviour *behaviour,
                     ClutterActor     *actor,
		     gpointer          data)
{
  ClutterBehaviourScalePrivate *priv;
  gint sw, sh, w, h;
  guint scale = GPOINTER_TO_UINT (data);

  priv = CLUTTER_BEHAVIOUR_SCALE (behaviour)->priv;

  clutter_actor_set_scalex (actor, scale, scale);

  if (priv->gravity == CLUTTER_GRAVITY_NONE ||
      priv->gravity == CLUTTER_GRAVITY_NORTH_WEST)
    return;

  clutter_actor_get_abs_size (actor, (guint*) &sw, (guint*) &sh);
  clutter_actor_get_size (actor, (guint*) &w, (guint*) &h);

  switch (priv->gravity)
    {
    case CLUTTER_GRAVITY_NORTH:
      break;
    case CLUTTER_GRAVITY_NORTH_EAST:
      break;
    case CLUTTER_GRAVITY_EAST:
      break;
    case CLUTTER_GRAVITY_SOUTH_EAST:
      break;
    case CLUTTER_GRAVITY_SOUTH:
      break;
    case CLUTTER_GRAVITY_SOUTH_WEST:
      break;
    case CLUTTER_GRAVITY_WEST:
      break;
    case CLUTTER_GRAVITY_CENTER:
      CLUTTER_NOTE (MISC, "gravity %i vs %i\n", sw, w);
      /* 
       * FIXME: This is actually broken for anything other than 0,0 
      */
      clutter_actor_set_position (actor, (w - sw)/2, (h - sh)/2);
    default:
      break;
    }
}

static void
clutter_behaviour_scale_alpha_notify (ClutterBehaviour *behave,
                                      guint32           alpha_value)
{
  ClutterFixed                  scale, factor;
  ClutterBehaviourScalePrivate *priv;

  priv = CLUTTER_BEHAVIOUR_SCALE (behave)->priv;

  factor = CLUTTER_INT_TO_FIXED (alpha_value) / CLUTTER_ALPHA_MAX_ALPHA;
  scale = CLUTTER_FIXED_MUL (factor, (priv->scale_end - priv->scale_begin));
  scale += priv->scale_begin;

  clutter_behaviour_actors_foreach (behave,
                                    scale_frame_foreach,
                                    GUINT_TO_POINTER (scale));
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
    case PROP_SCALE_BEGIN:
      priv->scale_begin = CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value));
      break;
    case PROP_SCALE_END:
      priv->scale_end = CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value));
      break;
    case PROP_SCALE_GRAVITY:
      priv->gravity = g_value_get_enum (value);
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
    case PROP_SCALE_BEGIN:
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->scale_begin));
      break;
    case PROP_SCALE_END:
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->scale_end));
      break;
    case PROP_SCALE_GRAVITY:
      g_value_set_enum (value, priv->gravity);
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
   * ClutterBehaviourScale:scale-begin:
   *
   * The initial scaling factor for the actors.
   * 
   * Since: 0.2
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SCALE_BEGIN,
                                   g_param_spec_double ("scale-begin",
                                                        "Scale Begin",
                                                        "Initial scale",
                                                        0.0, G_MAXDOUBLE,
                                                        1.0,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourScale:scale-end:
   *
   * The final scaling factor for the actors.
   *
   * Since: 0.2
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SCALE_END,
                                   g_param_spec_double ("scale-end",
                                                        "Scale End",
                                                        "Final scale",
                                                        0.0, G_MAXDOUBLE,
                                                        1.0,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourScale:gravity:
   *
   * The gravity of the scaling.
   *
   * Since: 0.2
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SCALE_GRAVITY,
                                   g_param_spec_enum ("scale-gravity",
                                                      "Scale Gravity",
                                                      "The gravity of the scaling",
                                                      CLUTTER_TYPE_GRAVITY,
                                                      CLUTTER_GRAVITY_CENTER,
                                                      CLUTTER_PARAM_READWRITE));


  behave_class->alpha_notify = clutter_behaviour_scale_alpha_notify;

  g_type_class_add_private (klass, sizeof (ClutterBehaviourScalePrivate));
}

static void
clutter_behaviour_scale_init (ClutterBehaviourScale *self)
{
  ClutterBehaviourScalePrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE (self);
}

/**
 * clutter_behaviour_scale_new:
 * @alpha: a #ClutterAlpha
 * @scale_begin: initial scale factor
 * @scale_end: final scale factor
 * @gravity: FIXME: Not currently implemented
 *
 * Creates a new  #ClutterBehaviourScale instance.
 *
 * Return value: the newly created #ClutterBehaviourScale
 *
 * Since: 0.2
 */
ClutterBehaviour *
clutter_behaviour_scale_new (ClutterAlpha   *alpha,
			     gdouble         scale_begin,
			     gdouble         scale_end,
			     ClutterGravity  gravity)
{
  return clutter_behaviour_scale_newx (alpha,
				       CLUTTER_FLOAT_TO_FIXED (scale_begin),
				       CLUTTER_FLOAT_TO_FIXED (scale_end),
				       gravity);
}

/**
 * clutter_behaviour_scale_newx:
 * @alpha: a #ClutterAlpha
 * @scale_begin: initial scale factor
 * @scale_end: final scale factor
 * @gravity: FIXME: Not currently implemented
 *
 * A fixed point implementation of clutter_behaviour_scale_new()
 *
 * Return value: the newly created #ClutterBehaviourScale
 *
 * Since: 0.2
 */
ClutterBehaviour *
clutter_behaviour_scale_newx (ClutterAlpha   *alpha,
			      ClutterFixed    scale_begin,
			      ClutterFixed    scale_end,
			      ClutterGravity  gravity)
{
  ClutterBehaviourScale *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_SCALE, 
                         "alpha", alpha,
			 NULL);

  behave->priv->scale_begin = scale_begin;
  behave->priv->scale_end   = scale_end;
  behave->priv->gravity     = gravity;

  return CLUTTER_BEHAVIOUR (behave);
}
