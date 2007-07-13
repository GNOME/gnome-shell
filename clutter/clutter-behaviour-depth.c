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
 * Since: 0.4
 */

G_DEFINE_TYPE (ClutterBehaviourDepth,
               clutter_behaviour_depth,
               CLUTTER_TYPE_BEHAVIOUR);

struct _ClutterBehaviourDepthPrivate
{
  gint depth_start;
  gint depth_end;
};

enum
{
  PROP_0,

  PROP_DEPTH_START,
  PROP_DEPTH_END
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
  gint delta, depth;

  priv = CLUTTER_BEHAVIOUR_DEPTH (behaviour)->priv;

  if (priv->depth_end > priv->depth_start)
    delta = priv->depth_end - priv->depth_start;
  else
    delta = priv->depth_start - priv->depth_end;

  depth = alpha_value * delta / CLUTTER_ALPHA_MAX_ALPHA;
  depth += ((priv->depth_end > priv->depth_start) ? priv->depth_start
                                                  : priv->depth_end);

  CLUTTER_NOTE (BEHAVIOUR, "alpha: %d, depth: %d", alpha_value, depth);

  clutter_behaviour_actors_foreach (behaviour,
                                    alpha_notify_foreach,
                                    GINT_TO_POINTER (depth));
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
    case PROP_DEPTH_START:
      depth->priv->depth_start = g_value_get_int (value);
      break;
    case PROP_DEPTH_END:
      depth->priv->depth_end = g_value_get_int (value);
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
    case PROP_DEPTH_START:
      g_value_set_int (value, depth->priv->depth_start);
      break;
    case PROP_DEPTH_END:
      g_value_set_int (value, depth->priv->depth_end);
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

  /**
   * ClutterBehaviourDepth:depth-start:
   *
   * Minimum depth level to apply to the actors.
   *
   * Since: 0.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DEPTH_START,
                                   g_param_spec_int ("depth-start",
                                                     "Minimum Depth",
                                                     "Minimum depth to apply",
                                                     G_MININT, G_MAXINT, 0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourDepth:depth-end:
   *
   * Maximum depth level to apply to the actors.
   *
   * Since: 0.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DEPTH_END,
                                   g_param_spec_int ("depth-end",
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
 * @depth_start: minimum depth level
 * @depth_end: maximum depth level
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
                             gint          depth_start,
                             gint          depth_end)
{
  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);

  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_DEPTH,
                       "alpha", alpha,
                       "depth-start", depth_start,
                       "depth-end", depth_end,
                       NULL);
}


