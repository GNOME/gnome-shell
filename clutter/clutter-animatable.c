/**
 * SECTION:clutter-animatable
 * @short_description: Interface for animatable classes
 *
 * #ClutterAnimatable is an interface that allows a #GObject class
 * to control how a #ClutterAnimation will animate a property.
 *
 * Each #ClutterAnimatable should implement the animate_property()
 * virtual function of the interface to compute the animation state
 * between two values of an interval depending on a progress factor,
 * expressed as a floating point value.
 *
 * If a #ClutterAnimatable is animated by a #ClutterAnimation
 * instance, the #ClutterAnimation will call
 * clutter_animatable_animate_property() passing the name of the
 * currently animated property; the initial and final values of
 * the animation interval; the progress factor. The #ClutterAnimatable
 * implementation should return the computed value for the animated
 * property.
 *
 * #ClutterAnimatable is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-animatable.h"
#include "clutter-debug.h"
#include "clutter-private.h"

GType
clutter_animatable_get_type (void)
{
  static GType a_type = 0;

  if (G_UNLIKELY (a_type == 0))
    a_type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ClutterAnimatable"),
                                            sizeof (ClutterAnimatableIface),
                                            NULL, 0, NULL, 0);

  return a_type;
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
 * Since: 1.0
 */
void
clutter_animatable_animate_property (ClutterAnimatable *animatable,
                                     ClutterAnimation  *animation,
                                     const gchar       *property_name,
                                     const GValue      *initial_value,
                                     const GValue      *final_value,
                                     gdouble            progress,
                                     GValue            *value)
{
  g_return_if_fail (CLUTTER_IS_ANIMATABLE (animatable));
  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (initial_value != NULL && final_value != NULL);
  g_return_if_fail (G_VALUE_TYPE (initial_value) != G_TYPE_INVALID);
  g_return_if_fail (G_VALUE_TYPE (final_value) != G_TYPE_INVALID);
  g_return_if_fail (value != NULL);
  g_return_if_fail (G_VALUE_TYPE (value) == G_VALUE_TYPE (initial_value) &&
                    G_VALUE_TYPE (value) == G_VALUE_TYPE (final_value));

  CLUTTER_ANIMATABLE_GET_IFACE (animatable)->animate_property (animatable,
                                                               animation,
                                                               property_name,
                                                               initial_value,
                                                               final_value,
                                                               progress,
                                                               value);
}
