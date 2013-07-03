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
 * SECTION:clutter-behaviour-opacity
 * @Title: ClutterBehaviourOpacity
 * @short_description: A behaviour controlling opacity
 * @Deprecated: 1.6: Use clutter_actor_animate() instead.
 *
 * #ClutterBehaviourOpacity controls the opacity of a set of actors.
 *
 * Since: 0.2
 *
 * Deprecated: 1.6: Use the #ClutterActor:opacity property and
 *   clutter_actor_animate(), or #ClutterAnimator, or #ClutterState
 *   instead.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "clutter-alpha.h"
#include "clutter-behaviour.h"
#include "clutter-behaviour-opacity.h"
#include "clutter-private.h"
#include "clutter-debug.h"

struct _ClutterBehaviourOpacityPrivate
{
  guint8 opacity_start;
  guint8 opacity_end;
};

enum
{
  PROP_0,

  PROP_OPACITY_START,
  PROP_OPACITY_END,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (ClutterBehaviourOpacity,
                            clutter_behaviour_opacity,
                            CLUTTER_TYPE_BEHAVIOUR)

static void
alpha_notify_foreach (ClutterBehaviour *behaviour,
		      ClutterActor     *actor,
		      gpointer          data)
{
  clutter_actor_set_opacity (actor, GPOINTER_TO_UINT(data));
}

static void
clutter_behaviour_alpha_notify (ClutterBehaviour *behave,
                                gdouble           alpha_value)
{
  ClutterBehaviourOpacityPrivate *priv;
  guint8 opacity;

  priv = CLUTTER_BEHAVIOUR_OPACITY (behave)->priv;

  opacity = alpha_value
            * (priv->opacity_end - priv->opacity_start)
            + priv->opacity_start;

  CLUTTER_NOTE (ANIMATION, "alpha: %.4f, opacity: %u",
                alpha_value,
                opacity);

  clutter_behaviour_actors_foreach (behave,
				    alpha_notify_foreach,
				    GUINT_TO_POINTER ((guint) opacity));
}

static void
clutter_behaviour_opacity_set_property (GObject      *gobject,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ClutterBehaviourOpacity *self = CLUTTER_BEHAVIOUR_OPACITY (gobject);

  switch (prop_id)
    {
    case PROP_OPACITY_START:
      clutter_behaviour_opacity_set_bounds (self,
                                            g_value_get_uint (value),
                                            self->priv->opacity_end);
      break;

    case PROP_OPACITY_END:
      clutter_behaviour_opacity_set_bounds (self,
                                            self->priv->opacity_start,
                                            g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_opacity_get_property (GObject    *gobject,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ClutterBehaviourOpacity *self = CLUTTER_BEHAVIOUR_OPACITY (gobject);

  switch (prop_id)
    {
    case PROP_OPACITY_START:
      g_value_set_uint (value, self->priv->opacity_start);
      break;

    case PROP_OPACITY_END:
      g_value_set_uint (value, self->priv->opacity_end);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_opacity_class_init (ClutterBehaviourOpacityClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behave_class = CLUTTER_BEHAVIOUR_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_behaviour_opacity_set_property;
  gobject_class->get_property = clutter_behaviour_opacity_get_property;

  /**
   * ClutterBehaviourOpacity:opacity-start:
   *
   * Initial opacity level of the behaviour.
   *
   * Since: 0.2
   *
   * Deprecated: 1.6
   */
  pspec = g_param_spec_uint ("opacity-start",
                             P_("Opacity Start"),
                             P_("Initial opacity level"),
                             0, 255,
                             0,
                             CLUTTER_PARAM_READWRITE);
  obj_props[PROP_OPACITY_START] = pspec;
  g_object_class_install_property (gobject_class, PROP_OPACITY_START, pspec);

  /**
   * ClutterBehaviourOpacity:opacity-end:
   *
   * Final opacity level of the behaviour.
   *
   * Since: 0.2
   *
   * Deprecated: 1.6
   */
  pspec = g_param_spec_uint ("opacity-end",
                             P_("Opacity End"),
                             P_("Final opacity level"),
                             0, 255,
                             0,
                             CLUTTER_PARAM_READWRITE);
  obj_props[PROP_OPACITY_END] = pspec;
  g_object_class_install_property (gobject_class, PROP_OPACITY_END, pspec);

  behave_class->alpha_notify = clutter_behaviour_alpha_notify;
}

static void
clutter_behaviour_opacity_init (ClutterBehaviourOpacity *self)
{
  self->priv = clutter_behaviour_opacity_get_instance_private (self);
}

/**
 * clutter_behaviour_opacity_new:
 * @alpha: (allow-none): a #ClutterAlpha instance, or %NULL
 * @opacity_start: minimum level of opacity
 * @opacity_end: maximum level of opacity
 *
 * Creates a new #ClutterBehaviourOpacity object, driven by @alpha
 * which controls the opacity property of every actor, making it
 * change in the interval between @opacity_start and @opacity_end.
 *
 * If @alpha is not %NULL, the #ClutterBehaviour will take ownership
 * of the #ClutterAlpha instance. In the case when @alpha is %NULL,
 * it can be set later with clutter_behaviour_set_alpha().
 *
 * Return value: the newly created #ClutterBehaviourOpacity
 *
 * Since: 0.2
 *
 * Deprecated: 1.6
 */
ClutterBehaviour *
clutter_behaviour_opacity_new (ClutterAlpha *alpha,
			       guint8        opacity_start,
			       guint8        opacity_end)
{
  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_OPACITY,
                       "alpha", alpha,
                       "opacity-start", opacity_start,
                       "opacity-end", opacity_end,
                       NULL);
}

/**
 * clutter_behaviour_opacity_set_bounds:
 * @behaviour: a #ClutterBehaviourOpacity
 * @opacity_start: minimum level of opacity
 * @opacity_end: maximum level of opacity
 *
 * Sets the initial and final levels of the opacity applied by @behaviour
 * on each actor it controls.
 *
 * Since: 0.6
 *
 * Deprecated: 1.6
 */
void
clutter_behaviour_opacity_set_bounds (ClutterBehaviourOpacity *behaviour,
                                      guint8                   opacity_start,
                                      guint8                   opacity_end)
{
  ClutterBehaviourOpacityPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_OPACITY (behaviour));

  priv = behaviour->priv;

  g_object_freeze_notify (G_OBJECT (behaviour));

  if (priv->opacity_start != opacity_start)
    {
      priv->opacity_start = opacity_start;

      g_object_notify_by_pspec (G_OBJECT (behaviour), obj_props[PROP_OPACITY_START]);
    }

  if (priv->opacity_end != opacity_end)
    {
      priv->opacity_end = opacity_end;

      g_object_notify_by_pspec (G_OBJECT (behaviour), obj_props[PROP_OPACITY_END]);
    }

  g_object_thaw_notify (G_OBJECT (behaviour));
}

/**
 * clutter_behaviour_opacity_get_bounds:
 * @behaviour: a #ClutterBehaviourOpacity
 * @opacity_start: (out): return location for the minimum level of opacity, or %NULL
 * @opacity_end: (out): return location for the maximum level of opacity, or %NULL
 *
 * Gets the initial and final levels of the opacity applied by @behaviour
 * on each actor it controls.
 *
 * Since: 0.6
 *
 * Deprecated: 1.6
 */
void
clutter_behaviour_opacity_get_bounds (ClutterBehaviourOpacity *behaviour,
                                      guint8                  *opacity_start,
                                      guint8                  *opacity_end)
{
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_OPACITY (behaviour));

  if (opacity_start)
    *opacity_start = behaviour->priv->opacity_start;

  if (opacity_end)
    *opacity_end = behaviour->priv->opacity_end;
}
