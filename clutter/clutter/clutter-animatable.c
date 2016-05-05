/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-animatable
 * @short_description: Interface for animatable classes
 *
 * #ClutterAnimatable is an interface that allows a #GObject class
 * to control how a #ClutterAnimation will animate a property.
 *
 * Each #ClutterAnimatable should implement the
 * #ClutterAnimatableIface.interpolate_property() virtual function of the
 * interface to compute the animation state between two values of an interval
 * depending on a progress factor, expressed as a floating point value.
 *
 * If a #ClutterAnimatable is animated by a #ClutterAnimation
 * instance, the #ClutterAnimation will call
 * clutter_animatable_interpolate_property() passing the name of the
 * currently animated property; the values interval; and the progress factor.
 * The #ClutterAnimatable implementation should return the computed value for
 * the animated
 * property.
 *
 * #ClutterAnimatable is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "clutter-animatable.h"
#include "clutter-interval.h"
#include "clutter-debug.h"
#include "clutter-private.h"

#include "deprecated/clutter-animatable.h"
#include "deprecated/clutter-animation.h"

typedef ClutterAnimatableIface  ClutterAnimatableInterface;
G_DEFINE_INTERFACE (ClutterAnimatable, clutter_animatable, G_TYPE_OBJECT);

static void
clutter_animatable_default_init (ClutterAnimatableInterface *iface)
{
}

/**
 * clutter_animatable_animate_property:
 * @animatable: a #ClutterAnimatable
 * @animation: a #ClutterAnimation
 * @property_name: the name of the animated property
 * @initial_value: the initial value of the animation interval
 * @final_value: the final value of the animation interval
 * @progress: the progress factor
 * @value: return location for the animation value
 *
 * Calls the animate_property() virtual function for @animatable.
 *
 * The @initial_value and @final_value #GValue<!-- -->s must contain
 * the same type; @value must have been initialized to the same
 * type of @initial_value and @final_value.
 *
 * All implementation of the #ClutterAnimatable interface must
 * implement this function.
 *
 * Return value: %TRUE if the value has been validated and can
 *   be applied to the #ClutterAnimatable, and %FALSE otherwise
 *
 * Since: 1.0
 *
 * Deprecated: 1.8: Use clutter_animatable_interpolate_value()
 *   instead
 */
gboolean
clutter_animatable_animate_property (ClutterAnimatable *animatable,
                                     ClutterAnimation  *animation,
                                     const gchar       *property_name,
                                     const GValue      *initial_value,
                                     const GValue      *final_value,
                                     gdouble            progress,
                                     GValue            *value)
{
  ClutterAnimatableIface *iface;
  gboolean res;

  g_return_val_if_fail (CLUTTER_IS_ANIMATABLE (animatable), FALSE);
  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);
  g_return_val_if_fail (initial_value != NULL && final_value != NULL, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (initial_value) != G_TYPE_INVALID, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (final_value) != G_TYPE_INVALID, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (value) == G_VALUE_TYPE (initial_value) &&
                        G_VALUE_TYPE (value) == G_VALUE_TYPE (final_value),
                        FALSE);

  iface = CLUTTER_ANIMATABLE_GET_IFACE (animatable);
  if (iface->animate_property == NULL)
    {
      ClutterInterval *interval;

      interval = clutter_animation_get_interval (animation, property_name);
      if (interval == NULL)
        return FALSE;

      res = clutter_animatable_interpolate_value (animatable, property_name,
                                                  interval,
                                                  progress,
                                                  value);
    }
  else
    res = iface->animate_property (animatable, animation,
                                   property_name,
                                   initial_value, final_value,
                                   progress,
                                   value);

  return res;
}

/**
 * clutter_animatable_find_property:
 * @animatable: a #ClutterAnimatable
 * @property_name: the name of the animatable property to find
 *
 * Finds the #GParamSpec for @property_name
 *
 * Return value: (transfer none): The #GParamSpec for the given property
 *   or %NULL
 *
 * Since: 1.4
 */
GParamSpec *
clutter_animatable_find_property (ClutterAnimatable *animatable,
                                  const gchar       *property_name)
{
  ClutterAnimatableIface *iface;

  g_return_val_if_fail (CLUTTER_IS_ANIMATABLE (animatable), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  CLUTTER_NOTE (ANIMATION, "Looking for property '%s'", property_name);

  iface = CLUTTER_ANIMATABLE_GET_IFACE (animatable);
  if (iface->find_property != NULL)
    return iface->find_property (animatable, property_name);

  return g_object_class_find_property (G_OBJECT_GET_CLASS (animatable),
                                       property_name);
}

/**
 * clutter_animatable_get_initial_state:
 * @animatable: a #ClutterAnimatable
 * @property_name: the name of the animatable property to retrieve
 * @value: a #GValue initialized to the type of the property to retrieve
 *
 * Retrieves the current state of @property_name and sets @value with it
 *
 * Since: 1.4
 */
void
clutter_animatable_get_initial_state (ClutterAnimatable *animatable,
                                      const gchar       *property_name,
                                      GValue            *value)
{
  ClutterAnimatableIface *iface;

  g_return_if_fail (CLUTTER_IS_ANIMATABLE (animatable));
  g_return_if_fail (property_name != NULL);

  CLUTTER_NOTE (ANIMATION, "Getting initial state of '%s'", property_name);

  iface = CLUTTER_ANIMATABLE_GET_IFACE (animatable);
  if (iface->get_initial_state != NULL)
    iface->get_initial_state (animatable, property_name, value);
  else
    g_object_get_property (G_OBJECT (animatable), property_name, value);
}

/**
 * clutter_animatable_set_final_state:
 * @animatable: a #ClutterAnimatable
 * @property_name: the name of the animatable property to set
 * @value: the value of the animatable property to set
 *
 * Sets the current state of @property_name to @value
 *
 * Since: 1.4
 */
void
clutter_animatable_set_final_state (ClutterAnimatable *animatable,
                                    const gchar       *property_name,
                                    const GValue      *value)
{
  ClutterAnimatableIface *iface;

  g_return_if_fail (CLUTTER_IS_ANIMATABLE (animatable));
  g_return_if_fail (property_name != NULL);

  CLUTTER_NOTE (ANIMATION, "Setting state of property '%s'", property_name);

  iface = CLUTTER_ANIMATABLE_GET_IFACE (animatable);
  if (iface->set_final_state != NULL)
    iface->set_final_state (animatable, property_name, value);
  else
    g_object_set_property (G_OBJECT (animatable), property_name, value);
}

/**
 * clutter_animatable_interpolate_value:
 * @animatable: a #ClutterAnimatable
 * @property_name: the name of the property to interpolate
 * @interval: a #ClutterInterval with the animation range
 * @progress: the progress to use to interpolate between the
 *   initial and final values of the @interval
 * @value: (out): return location for an initialized #GValue
 *   using the same type of the @interval
 *
 * Asks a #ClutterAnimatable implementation to interpolate a
 * a named property between the initial and final values of
 * a #ClutterInterval, using @progress as the interpolation
 * value, and store the result inside @value.
 *
 * This function should be used for every property animation
 * involving #ClutterAnimatable<!-- -->s.
 *
 * This function replaces clutter_animatable_animate_property().
 *
 * Return value: %TRUE if the interpolation was successful,
 *   and %FALSE otherwise
 *
 * Since: 1.8
 */
gboolean
clutter_animatable_interpolate_value (ClutterAnimatable *animatable,
                                      const gchar       *property_name,
                                      ClutterInterval   *interval,
                                      gdouble            progress,
                                      GValue            *value)
{
  ClutterAnimatableIface *iface;

  g_return_val_if_fail (CLUTTER_IS_ANIMATABLE (animatable), FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  CLUTTER_NOTE (ANIMATION, "Interpolating '%s' (progress: %.3f)",
                property_name,
                progress);

  iface = CLUTTER_ANIMATABLE_GET_IFACE (animatable);
  if (iface->interpolate_value != NULL)
    {
      return iface->interpolate_value (animatable, property_name,
                                       interval,
                                       progress,
                                       value);
    }
  else
    return clutter_interval_compute_value (interval, progress, value);
}
