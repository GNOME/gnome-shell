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

static void
scale_frame_foreach (ClutterActor          *actor,
		     ClutterBehaviourScale *behave)
{
  ClutterFixed                    scale, factor;
  guint32                         alpha;
  ClutterBehaviourScalePrivate   *priv;
  ClutterBehaviour               *_behave;
  gint                            sw, sh, w, h;

  priv = CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE (behave);
  _behave = CLUTTER_BEHAVIOUR (behave);

  alpha = clutter_alpha_get_alpha (clutter_behaviour_get_alpha (_behave));

  /* FIXME: use all fixed if possible
  factor = CLUTTER_FIXED_DIV(CLUTTER_INT_TO_FIXED(alpha/2),
			     CLUTTER_INT_TO_FIXED(CLUTTER_ALPHA_MAX_ALPHA/2));
  */

  factor = CLUTTER_FLOAT_TO_FIXED ((gdouble) alpha / CLUTTER_ALPHA_MAX_ALPHA);

  scale = CLUTTER_FIXED_MUL (factor, (priv->scale_end - priv->scale_begin));

  scale += priv->scale_begin;

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
      CLUTTER_DBG ("%i vs %i\n", sw, w);
      clutter_actor_move_by (actor, sw - w, sh - h);
    default:
      break;
    }
}

static void
clutter_behaviour_scale_alpha_notify (ClutterBehaviour *behave,
                                      guint32           alpha_value)
{
  clutter_behaviour_actors_foreach (behave,
                                    (GFunc) scale_frame_foreach,
                                    CLUTTER_BEHAVIOUR_SCALE (behave));
}

static void
clutter_behaviour_scale_class_init (ClutterBehaviourScaleClass *klass)
{
  ClutterBehaviourClass *behave_class;

  behave_class = (ClutterBehaviourClass*) klass;

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
 * @gravity: FIXME
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
