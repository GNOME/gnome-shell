/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corporation
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
 * SECTION:clutter-property-transition
 * @Title: ClutterPropertyTransition
 * @Short_Description: Property transitions
 *
 * #ClutterPropertyTransition is a specialized #ClutterTransition that
 * can be used to tween a property of a #ClutterAnimatable instance.
 *
 * #ClutterPropertyTransition is available since Clutter 1.10
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-property-transition.h"

#include "clutter-animatable.h"
#include "clutter-debug.h"
#include "clutter-interval.h"
#include "clutter-private.h"
#include "clutter-transition.h"

struct _ClutterPropertyTransitionPrivate
{
  char *property_name;

  GParamSpec *pspec;
};

enum
{
  PROP_0,

  PROP_PROPERTY_NAME,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterPropertyTransition, clutter_property_transition, CLUTTER_TYPE_TRANSITION)

static inline void
clutter_property_transition_ensure_interval (ClutterPropertyTransition *transition,
                                             ClutterAnimatable         *animatable,
                                             ClutterInterval           *interval)
{
  ClutterPropertyTransitionPrivate *priv = transition->priv;
  GValue *value_p;

  if (clutter_interval_is_valid (interval))
    return;

  /* if no initial value has been set, use the current value */
  value_p = clutter_interval_peek_initial_value (interval);
  if (!G_IS_VALUE (value_p))
    {
      g_value_init (value_p, clutter_interval_get_value_type (interval));
      clutter_animatable_get_initial_state (animatable,
                                            priv->property_name,
                                            value_p);
    }

  /* if no final value has been set, use the current value */
  value_p = clutter_interval_peek_final_value (interval);
  if (!G_IS_VALUE (value_p))
    {
      g_value_init (value_p, clutter_interval_get_value_type (interval));
      clutter_animatable_get_initial_state (animatable,
                                            priv->property_name,
                                            value_p);
    }
}

static void
clutter_property_transition_attached (ClutterTransition *transition,
                                      ClutterAnimatable *animatable)
{
  ClutterPropertyTransition *self = CLUTTER_PROPERTY_TRANSITION (transition);
  ClutterPropertyTransitionPrivate *priv = self->priv;
  ClutterInterval *interval;

  if (priv->property_name == NULL)
    return;

  priv->pspec =
    clutter_animatable_find_property (animatable, priv->property_name);

  if (priv->pspec == NULL)
    return;

  interval = clutter_transition_get_interval (transition);
  if (interval == NULL)
    return;

  clutter_property_transition_ensure_interval (self, animatable, interval);
}

static void
clutter_property_transition_detached (ClutterTransition *transition,
                                      ClutterAnimatable *animatable)
{
  ClutterPropertyTransition *self = CLUTTER_PROPERTY_TRANSITION (transition);
  ClutterPropertyTransitionPrivate *priv = self->priv;

  priv->pspec = NULL; 
}

static void
clutter_property_transition_compute_value (ClutterTransition *transition,
                                           ClutterAnimatable *animatable,
                                           ClutterInterval   *interval,
                                           gdouble            progress)
{
  ClutterPropertyTransition *self = CLUTTER_PROPERTY_TRANSITION (transition);
  ClutterPropertyTransitionPrivate *priv = self->priv;
  GValue value = G_VALUE_INIT;
  GType p_type, i_type;
  gboolean res;

  /* if we have a GParamSpec we also have an animatable instance */
  if (priv->pspec == NULL)
    return;

  clutter_property_transition_ensure_interval (self, animatable, interval);

  p_type = G_PARAM_SPEC_VALUE_TYPE (priv->pspec);
  i_type = clutter_interval_get_value_type (interval);

  g_value_init (&value, i_type);

  res = clutter_animatable_interpolate_value (animatable,
                                              priv->property_name,
                                              interval,
                                              progress,
                                              &value);

  if (res)
    {
      if (i_type != p_type || g_type_is_a (i_type, p_type))
        {
          if (g_value_type_transformable (i_type, p_type))
            {
              GValue transform = G_VALUE_INIT;

              g_value_init (&transform, p_type);

              if (g_value_transform (&value, &transform))
                {
                  clutter_animatable_set_final_state (animatable,
                                                      priv->property_name,
                                                      &transform);
                }
              else
                g_warning ("%s: Unable to convert a value of type '%s' from "
                           "the value type '%s' of the interval.",
                           G_STRLOC,
                           g_type_name (p_type),
                           g_type_name (i_type));

              g_value_unset (&transform);
            }
        }
      else
        clutter_animatable_set_final_state (animatable,
                                            priv->property_name,
                                            &value);
    }

  g_value_unset (&value);
}

static void
clutter_property_transition_set_property (GObject      *gobject,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  ClutterPropertyTransition *self = CLUTTER_PROPERTY_TRANSITION (gobject);

  switch (prop_id)
    {
    case PROP_PROPERTY_NAME:
      clutter_property_transition_set_property_name (self,
                                                     g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_property_transition_get_property (GObject    *gobject,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  ClutterPropertyTransitionPrivate *priv = CLUTTER_PROPERTY_TRANSITION (gobject)->priv;

  switch (prop_id)
    {
    case PROP_PROPERTY_NAME:
      g_value_set_string (value, priv->property_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_property_transition_finalize (GObject *gobject)
{
  ClutterPropertyTransitionPrivate *priv;

  priv = CLUTTER_PROPERTY_TRANSITION (gobject)->priv;

  g_free (priv->property_name);

  G_OBJECT_CLASS (clutter_property_transition_parent_class)->finalize (gobject);
}

static void
clutter_property_transition_class_init (ClutterPropertyTransitionClass *klass)
{
  ClutterTransitionClass *transition_class = CLUTTER_TRANSITION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  transition_class->attached = clutter_property_transition_attached;
  transition_class->detached = clutter_property_transition_detached;
  transition_class->compute_value = clutter_property_transition_compute_value;

  gobject_class->set_property = clutter_property_transition_set_property;
  gobject_class->get_property = clutter_property_transition_get_property;
  gobject_class->finalize = clutter_property_transition_finalize;

  /**
   * ClutterPropertyTransition:property-name:
   *
   * The name of the property of a #ClutterAnimatable to animate.
   *
   * Since: 1.10
   */
  obj_props[PROP_PROPERTY_NAME] =
    g_param_spec_string ("property-name",
                         P_("Property Name"),
                         P_("The name of the property to animate"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_property_transition_init (ClutterPropertyTransition *self)
{
  self->priv = clutter_property_transition_get_instance_private (self);
}

/**
 * clutter_property_transition_new:
 * @property_name: (allow-none): a property of @animatable, or %NULL
 *
 * Creates a new #ClutterPropertyTransition.
 *
 * Return value: (transfer full): the newly created #ClutterPropertyTransition.
 *   Use g_object_unref() when done
 *
 * Since: 1.10
 */
ClutterTransition *
clutter_property_transition_new (const char *property_name)
{
  return g_object_new (CLUTTER_TYPE_PROPERTY_TRANSITION,
                       "property-name", property_name,
                       NULL);
}

/**
 * clutter_property_transition_set_property_name:
 * @transition: a #ClutterPropertyTransition
 * @property_name: (allow-none): a property name
 *
 * Sets the #ClutterPropertyTransition:property-name property of @transition.
 *
 * Since: 1.10
 */
void
clutter_property_transition_set_property_name (ClutterPropertyTransition *transition,
                                               const char                *property_name)
{
  ClutterPropertyTransitionPrivate *priv;
  ClutterAnimatable *animatable;

  g_return_if_fail (CLUTTER_IS_PROPERTY_TRANSITION (transition));

  priv = transition->priv;

  if (g_strcmp0 (priv->property_name, property_name) == 0)
    return;

  g_free (priv->property_name);
  priv->property_name = g_strdup (property_name);
  priv->pspec = NULL;

  animatable =
    clutter_transition_get_animatable (CLUTTER_TRANSITION (transition));
  if (animatable != NULL)
    {
      priv->pspec = clutter_animatable_find_property (animatable,
                                                      priv->property_name);
    }

  g_object_notify_by_pspec (G_OBJECT (transition),
                            obj_props[PROP_PROPERTY_NAME]);
}

/**
 * clutter_property_transition_get_property_name:
 * @transition: a #ClutterPropertyTransition
 *
 * Retrieves the value of the #ClutterPropertyTransition:property-name
 * property.
 *
 * Return value: the name of the property being animated, or %NULL if
 *   none is set. The returned string is owned by the @transition and
 *   it should not be freed.
 *
 * Since: 1.10
 */
const char *
clutter_property_transition_get_property_name (ClutterPropertyTransition *transition)
{
  g_return_val_if_fail (CLUTTER_IS_PROPERTY_TRANSITION (transition), NULL);

  return transition->priv->property_name;
}
