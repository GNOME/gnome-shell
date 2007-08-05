/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
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

#include "config.h"

#include "clutter-behaviour-depth.h"

#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-debug.h"
#include "clutter-private.h"

/**
 * SECTION:clutter-behaviour-depth
 * @short_description: Behaviour controlling the depth
 *
 * #ClutterBehaviourDepth is a simple #ClutterBehaviour controlling the
 * depth of a set of actors.
 *
 * The minimum and maximum depth are controlled by the
 * ClutterBehaviourDepth:min-depth and ClutterBehaviourDepth:max-depth
 * properties. The direction of the motion on the depth axis is controlled
 * by the #ClutterAlpha object. If you want to make a #ClutterActor
 * controlled by the #ClutterBehaviourDepth behaviour move from a depth of
 * 0 to a depth of 100 you will have to use an increasing alpha function,
 * like %CLUTTER_ALPHA_RAMP_INC. On the other hand, if you want to make
 * the same actor move from a depth of 100 to a depth of 0 you will have
 * to use a decreasing alpha function, like %CLUTTER_ALPHA_RAMP_DEC. Using
 * a minimum depth greater than the maximum depth and a decreasing alpha
 * function, or using a maximum depth greater than the minimum depth and
 * an increasing alpha function will result in an undefined behaviour.
 *
 * #ClutterBehaviourDepth is available since Clutter 0.4.
 */

G_DEFINE_TYPE (ClutterBehaviourDepth,
               clutter_behaviour_depth,
               CLUTTER_TYPE_BEHAVIOUR);

struct _ClutterBehaviourDepthPrivate
{
  gint min_depth;
  gint max_depth;
};

enum
{
  PROP_0,

  PROP_MIN_DEPTH,
  PROP_MAX_DEPTH
};

static void
alpha_notify_foreach (ClutterBehaviour *behaviour,
                      ClutterActor     *actor,
                      gpointer          user_data)
{
  clutter_actor_set_depth (actor, GPOINTER_TO_INT (user_data));
}

static void
clutter_behaviour_depth_alpha_notify (ClutterBehaviour *behaviour,
                                      guint32           alpha_value)
{
  ClutterBehaviourDepthPrivate *priv;
  gint depth;

  priv = CLUTTER_BEHAVIOUR_DEPTH (behaviour)->priv;

  if (priv->max_depth > priv->min_depth)
    {
      depth = alpha_value
              * (priv->max_depth - priv->min_depth)
              / CLUTTER_ALPHA_MAX_ALPHA;

      depth += priv->min_depth;
    }
  else
    {
      depth = alpha_value
              * (priv->min_depth - priv->max_depth)
              / CLUTTER_ALPHA_MAX_ALPHA;
      
      depth += priv->max_depth;
    }

  CLUTTER_NOTE (BEHAVIOUR, "alpha: %d, depth: %d", alpha_value, depth);

  clutter_behaviour_actors_foreach (behaviour,
                                    alpha_notify_foreach,
                                    GINT_TO_POINTER (depth));
}

static void
clutter_behaviour_depth_applied (ClutterBehaviour *behaviour,
                                 ClutterActor     *actor)
{
  ClutterBehaviourDepth *depth = CLUTTER_BEHAVIOUR_DEPTH (behaviour);

  clutter_actor_set_depth (actor, depth->priv->min_depth);
}

static void
clutter_behaviour_depth_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterBehaviourDepth *depth = CLUTTER_BEHAVIOUR_DEPTH (gobject);

  switch (prop_id)
    {
    case PROP_MIN_DEPTH:
      depth->priv->min_depth = g_value_get_int (value);
      break;
    case PROP_MAX_DEPTH:
      depth->priv->max_depth = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_depth_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterBehaviourDepth *depth = CLUTTER_BEHAVIOUR_DEPTH (gobject);

  switch (prop_id)
    {
    case PROP_MIN_DEPTH:
      g_value_set_int (value, depth->priv->min_depth);
      break;
    case PROP_MAX_DEPTH:
      g_value_set_int (value, depth->priv->max_depth);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_depth_class_init (ClutterBehaviourDepthClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behaviour_class = CLUTTER_BEHAVIOUR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterBehaviourDepthPrivate));

  gobject_class->set_property = clutter_behaviour_depth_set_property;
  gobject_class->get_property = clutter_behaviour_depth_get_property;

  behaviour_class->alpha_notify = clutter_behaviour_depth_alpha_notify;
  behaviour_class->applied = clutter_behaviour_depth_applied;

  /**
   * ClutterBehaviourDepth:min-depth:
   *
   * Minimum depth level to apply to the actors.
   *
   * Since: 0.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MIN_DEPTH,
                                   g_param_spec_int ("min-depth",
                                                     "Minimum Depth",
                                                     "Minimum depth to apply",
                                                     G_MININT, G_MAXINT, 0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourDepth:max-depth:
   *
   * Maximum depth level to apply to the actors.
   *
   * Since: 0.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_DEPTH,
                                   g_param_spec_int ("max-depth",
                                                     "Maximum Depth",
                                                     "Maximum depth to apply",
                                                     G_MININT, G_MAXINT, 0,
                                                     CLUTTER_PARAM_READWRITE));
}

static void
clutter_behaviour_depth_init (ClutterBehaviourDepth *depth)
{
  depth->priv = G_TYPE_INSTANCE_GET_PRIVATE (depth,
                                             CLUTTER_TYPE_BEHAVIOUR_DEPTH,
                                             ClutterBehaviourDepthPrivate);
}

/**
 * clutter_behaviour_depth_new:
 * @alpha: a #ClutterAlpha or %NULL
 * @min_depth: minimum depth level
 * @max_depth: maximum depth level
 *
 * Creates a new #ClutterBehaviourDepth which can be used to control
 * the ClutterActor:depth property of a set of #ClutterActor<!-- -->s.
 *
 * Return value: the newly created behaviour
 *
 * Since: 0.4
 */
ClutterBehaviour *
clutter_behaviour_depth_new (ClutterAlpha *alpha,
                             gint          min_depth,
                             gint          max_depth)
{
  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);

  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_DEPTH,
                       "alpha", alpha,
                       "min-depth", min_depth,
                       "max-depth", max_depth,
                       NULL);
}
