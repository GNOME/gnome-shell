/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
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
 * SECTION:clutter-animation
 * @short_description: Simple implicit animations
 *
 * #ClutterAnimation is an object providing simple, implicit animations
 * for #GObject<!-- -->s.
 *
 * #ClutterAnimation instances will bind a #GObject property belonging
 * to a #GObject to a #ClutterInterval, and will then use a #ClutterTimeline
 * to interpolate the property between the initial and final values of the
 * interval.
 *
 * For convenience, it is possible to use the clutter_actor_animate()
 * function call which will take care of setting up and tearing down
 * a #ClutterAnimation instance and animate an actor between its current
 * state and the specified final state.
 *
 * #ClutterAnimation is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-alpha.h"
#include "clutter-animatable.h"
#include "clutter-animation.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-interval.h"
#include "clutter-private.h"

enum
{
  PROP_0,

  PROP_OBJECT,
  PROP_MODE,
  PROP_DURATION,
  PROP_LOOP,
  PROP_TIMELINE,
  PROP_ALPHA
};

enum
{
  STARTED,
  COMPLETED,

  LAST_SIGNAL
};

#define CLUTTER_ANIMATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_ANIMATION, ClutterAnimationPrivate))

struct _ClutterAnimationPrivate
{
  GObject *object;

  GHashTable *properties;

  ClutterAlpha *alpha;

  gulong mode;

  guint loop : 1;
  guint duration;

  guint timeline_started_id;
  guint timeline_completed_id;
  guint alpha_notify_id;
};

static guint animation_signals[LAST_SIGNAL] = { 0, };

static GQuark quark_object_animation = 0;

G_DEFINE_TYPE (ClutterAnimation, clutter_animation, G_TYPE_OBJECT);

static void on_animation_weak_notify (gpointer  data,
                                      GObject  *animation_pointer);

static void
clutter_animation_finalize (GObject *gobject)
{
  ClutterAnimationPrivate *priv = CLUTTER_ANIMATION (gobject)->priv;

  CLUTTER_NOTE (ANIMATION, "Destroying properties hash table");
  g_hash_table_destroy (priv->properties);

  G_OBJECT_CLASS (clutter_animation_parent_class)->finalize (gobject);
}

static void
clutter_animation_dispose (GObject *gobject)
{
  ClutterAnimationPrivate *priv = CLUTTER_ANIMATION (gobject)->priv;
  ClutterTimeline *timeline;

  timeline = clutter_animation_get_timeline (CLUTTER_ANIMATION (gobject));
  if (timeline != NULL)
    {
      if (priv->timeline_started_id)
        {
          g_signal_handler_disconnect (timeline, priv->timeline_started_id);
          priv->timeline_started_id = 0;
        }

      if (priv->timeline_completed_id)
        {
          g_signal_handler_disconnect (timeline, priv->timeline_completed_id);
          priv->timeline_completed_id = 0;
        }
    }

  if (priv->alpha != NULL)
    {
      if (priv->alpha_notify_id)
        {
          g_signal_handler_disconnect (priv->alpha, priv->alpha_notify_id);
          priv->alpha_notify_id = 0;
        }

      g_object_unref (priv->alpha);
      priv->alpha = NULL;
    }

  if (priv->object != NULL)
    {
      g_object_weak_unref (G_OBJECT (gobject),
                           on_animation_weak_notify,
                           priv->object);
      g_object_set_qdata (priv->object, quark_object_animation, NULL);
      g_object_unref (priv->object);
      priv->object = NULL;
    }

  G_OBJECT_CLASS (clutter_animation_parent_class)->dispose (gobject);
}

static void
clutter_animation_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ClutterAnimation *animation = CLUTTER_ANIMATION (gobject);

  switch (prop_id)
    {
    case PROP_OBJECT:
      clutter_animation_set_object (animation, g_value_get_object (value));
      break;

    case PROP_MODE:
      clutter_animation_set_mode (animation, g_value_get_ulong (value));
      break;

    case PROP_DURATION:
      clutter_animation_set_duration (animation, g_value_get_uint (value));
      break;

    case PROP_LOOP:
      clutter_animation_set_loop (animation, g_value_get_boolean (value));
      break;

    case PROP_TIMELINE:
      clutter_animation_set_timeline (animation, g_value_get_object (value));
      break;

    case PROP_ALPHA:
      clutter_animation_set_alpha (animation, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_animation_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ClutterAnimation *animation = CLUTTER_ANIMATION (gobject);
  ClutterAnimationPrivate *priv = animation->priv;

  switch (prop_id)
    {
    case PROP_OBJECT:
      g_value_set_object (value, priv->object);
      break;

    case PROP_MODE:
      g_value_set_ulong (value, clutter_animation_get_mode (animation));
      break;

    case PROP_DURATION:
      g_value_set_uint (value, clutter_animation_get_duration (animation));
      break;

    case PROP_LOOP:
      g_value_set_boolean (value, clutter_animation_get_loop (animation));
      break;

    case PROP_TIMELINE:
      g_value_set_object (value, clutter_animation_get_timeline (animation));
      break;

    case PROP_ALPHA:
      g_value_set_object (value, clutter_animation_get_alpha (animation));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_animation_class_init (ClutterAnimationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  quark_object_animation =
    g_quark_from_static_string ("clutter-actor-animation");

  g_type_class_add_private (klass, sizeof (ClutterAnimationPrivate));

  gobject_class->set_property = clutter_animation_set_property;
  gobject_class->get_property = clutter_animation_get_property;
  gobject_class->dispose = clutter_animation_dispose;
  gobject_class->finalize = clutter_animation_finalize;

  /**
   * ClutterAnimation:objct:
   *
   * The #GObject to which the animation applies.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_object ("object",
                               "Object",
                               "Object to which the animation applies",
                               G_TYPE_OBJECT,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_OBJECT, pspec);

  /**
   * ClutterAnimation:mode:
   *
   * The animation mode, either a value from #ClutterAnimationMode
   * or a value returned by clutter_alpha_register_func(). The
   * default value is %CLUTTER_LINEAR.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_ulong ("mode",
                              "Mode",
                              "The mode of the animation",
                              0, G_MAXULONG,
                              CLUTTER_LINEAR,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_MODE, pspec);

  /**
   * ClutterAnimation:duration:
   *
   * The duration of the animation, expressed in milliseconds.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_uint ("duration",
                             "Duration",
                             "Duration of the animation, in milliseconds",
                             0, G_MAXUINT, 0,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_DURATION, pspec);

  /**
   * ClutterAnimation:loop:
   *
   * Whether the animation should loop.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("loop",
                                "Loop",
                                "Whether the animation should loop",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LOOP, pspec);

  /**
   * ClutterAnimation:timeline:
   *
   * The #ClutterTimeline used by the animation.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_object ("timeline",
                               "Timeline",
                               "The timeline used by the animation",
                               CLUTTER_TYPE_TIMELINE,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TIMELINE, pspec);

  /**
   * ClutterAnimation:alpha:
   *
   * The #ClutterAlpha used by the animation.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_object ("alpha",
                               "Alpha",
                               "The alpha used by the animation",
                               CLUTTER_TYPE_ALPHA,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ALPHA, pspec);

  /**
   * ClutterAnimation::started:
   * @animation: the animation that emitted the signal
   *
   * The ::started signal is emitted once the animation has been
   * started
   *
   * Since: 1.0
   */
  animation_signals[STARTED] =
    g_signal_new (I_("started"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterAnimationClass, started),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterAniamtion::completed:
   * @animation: the animation that emitted the signal
   *
   * The ::completed signal is emitted once the animation has
   * been completed.
   *
   * The @animation instance is guaranteed to be valid for the entire
   * duration of the signal emission chain.
   *
   * Since: 1.0
   */
  animation_signals[COMPLETED] =
    g_signal_new (I_("completed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterAnimationClass, completed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
clutter_animation_init (ClutterAnimation *self)
{
  self->priv = CLUTTER_ANIMATION_GET_PRIVATE (self);

  self->priv->mode = CLUTTER_LINEAR;
  self->priv->properties =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) g_free,
                           (GDestroyNotify) g_object_unref);
}

static inline void
clutter_animation_bind_property_internal (ClutterAnimation *animation,
                                          GParamSpec       *pspec,
                                          ClutterInterval  *interval)
{
  ClutterAnimationPrivate *priv = animation->priv;

  if (!clutter_interval_validate (interval, pspec))
    {
      g_warning ("Cannot bind property '%s': the interval is out "
                 "of bounds",
                 pspec->name);
      return;
    }

  g_hash_table_insert (priv->properties,
                       g_strdup (pspec->name),
                       g_object_ref_sink (interval));
}

static inline void
clutter_animation_update_property_internal (ClutterAnimation *animation,
                                            GParamSpec       *pspec,
                                            ClutterInterval  *interval)
{
  ClutterAnimationPrivate *priv = animation->priv;

  if (!clutter_interval_validate (interval, pspec))
    {
      g_warning ("Cannot bind property '%s': the interval is out "
                 "of bounds",
                 pspec->name);
      return;
    }

  g_hash_table_replace (priv->properties,
                        g_strdup (pspec->name),
                        g_object_ref_sink (interval));
}

static GParamSpec *
clutter_animation_validate_bind (ClutterAnimation *animation,
                                 const char       *property_name,
                                 GType             argtype)
{
  ClutterAnimationPrivate *priv;
  GObjectClass *klass;
  GParamSpec *pspec;

  priv = animation->priv;

  if (G_UNLIKELY (!priv->object))
    {
      g_warning ("Cannot bind property '%s': the animation has no "
                 "object set. You need to call clutter_animation_set_object() "
                 "first to be able to bind a property",
                 property_name);
      return NULL;
    }

  if (G_UNLIKELY (clutter_animation_has_property (animation, property_name)))
    {
      g_warning ("Cannot bind property '%s': the animation already has "
                 "a bound property with the same name",
                 property_name);
      return NULL;
    }

  klass = G_OBJECT_GET_CLASS (priv->object);
  pspec = g_object_class_find_property (klass, property_name);
  if (!pspec)
    {
      g_warning ("Cannot bind property '%s': objects of type '%s' have "
                 "no such property",
                 property_name,
                 g_type_name (G_OBJECT_TYPE (priv->object)));
      return NULL;
    }

  if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("Cannot bind property '%s': the property is not writable",
                 property_name);
      return NULL;
    }

  if (!g_value_type_compatible (G_PARAM_SPEC_VALUE_TYPE (pspec),
                                argtype))
    {
      g_warning ("Cannot bind property '%s': the interval value of "
                 "type '%s' is not compatible with the property value "
                 "of type '%s'",
                 property_name,
                 g_type_name (argtype),
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      return NULL;
    }
  return pspec;
}

/**
 * clutter_animation_bind_interval:
 * @animation: a #ClutterAnimation
 * @property_name: the property to control
 * @interval: (transfer full): a #ClutterInterval
 *
 * Binds @interval to the @property_name of the #GObject
 * attached to @animation. The #ClutterAnimation will take
 * ownership of the passed #ClutterInterval.  For more information
 * about animations, see clutter_actor_animate().
 *
 * If you need to update the interval instance use
 * clutter_animation_update_property() instead.
 *
 * Return value: (transfer none): The animation itself.
 * Since: 1.0
 */
ClutterAnimation *
clutter_animation_bind_interval (ClutterAnimation *animation,
                                 const gchar      *property_name,
                                 ClutterInterval  *interval)
{
  GParamSpec *pspec;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);
  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), NULL);

  pspec = clutter_animation_validate_bind (animation, property_name,
                                           clutter_interval_get_value_type (interval));
  if (pspec == NULL)
    return NULL;

  clutter_animation_bind_property_internal (animation, pspec, interval);

  return animation;
}


/**
 * clutter_animation_bind:
 * @animation: a #ClutterAnimation
 * @property_name: the property to control
 * @final: The final value of the property
 *
 * Adds a single property with name @property_name to the
 * animation @animation.  For more information about animations,
 * see clutter_actor_animate().
 *
 * This method returns the animation primarily to make chained
 * calls convenient in language bindings.
 *
 * Return value: (transfer none): The animation itself.
 * Since: 1.0
 */
ClutterAnimation *
clutter_animation_bind (ClutterAnimation *animation,
                        const gchar      *property_name,
                        const GValue     *final)
{
  ClutterAnimationPrivate *priv;
  GParamSpec *pspec;
  ClutterInterval *interval;
  GType type;
  GValue initial = { 0, };

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  priv = animation->priv;

  type = G_VALUE_TYPE (final);
  pspec = clutter_animation_validate_bind (animation, property_name,
                                           type);
  if (pspec == NULL)
    return NULL;

  g_value_init (&initial, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (priv->object, property_name, &initial);

  interval = clutter_interval_new_with_values (type, &initial, final);
  g_value_unset (&initial);

  clutter_animation_bind_property_internal (animation, pspec, interval);

  return animation;
}


/**
 * clutter_animation_unbind_property:
 * @animation: a #ClutterAnimation
 * @property_name: name of the property
 *
 * Removes @property_name from the list of animated properties.
 *
 * Since: 1.0
 */
void
clutter_animation_unbind_property (ClutterAnimation *animation,
                                   const gchar      *property_name)
{
  ClutterAnimationPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));
  g_return_if_fail (property_name != NULL);

  priv = animation->priv;

  if (!clutter_animation_has_property (animation, property_name))
    {
      g_warning ("Cannot unbind property '%s': the animation has "
                 "no bound property with that name",
                 property_name);
      return;
    }

  g_hash_table_remove (priv->properties, property_name);
}

/**
 * clutter_animation_has_property:
 * @animation: a #ClutterAnimation
 * @property_name: name of the property
 *
 * Checks whether @animation is controlling @property_name.
 *
 * Return value: %TRUE if the property is animated by the
 *   #ClutterAnimation, %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
clutter_animation_has_property (ClutterAnimation *animation,
                                const gchar      *property_name)
{
  ClutterAnimationPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);

  priv = animation->priv;

  return g_hash_table_lookup (priv->properties, property_name) != NULL;
}

/**
 * clutter_animation_update_interval:
 * @animation: a #ClutterAnimation
 * @property_name: name of the property
 * @interval: a #ClutterInterval
 *
 * Changes the @interval for @property_name. The #ClutterAnimation
 * will take ownership of the passed #ClutterInterval.
 *
 * Since: 1.0
 */
void
clutter_animation_update_interval (ClutterAnimation *animation,
                                   const gchar      *property_name,
                                   ClutterInterval  *interval)
{
  ClutterAnimationPrivate *priv;
  GObjectClass *klass;
  GParamSpec *pspec;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));

  priv = animation->priv;

  if (!clutter_animation_has_property (animation, property_name))
    {
      g_warning ("Cannot unbind property '%s': the animation has "
                 "no bound property with that name",
                 property_name);
      return;
    }

  klass = G_OBJECT_GET_CLASS (priv->object);
  pspec = g_object_class_find_property (klass, property_name);
  if (!pspec)
    {
      g_warning ("Cannot bind property '%s': objects of type '%s' have "
                 "no such property",
                 property_name,
                 g_type_name (G_OBJECT_TYPE (priv->object)));
      return;
    }

  if (!g_value_type_compatible (G_PARAM_SPEC_VALUE_TYPE (pspec),
                                clutter_interval_get_value_type (interval)))
    {
      g_warning ("Cannot bind property '%s': the interval value of "
                 "type '%s' is not compatible with the property value "
                 "of type '%s'",
                 property_name,
                 g_type_name (clutter_interval_get_value_type (interval)),
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      return;
    }

  clutter_animation_update_property_internal (animation, pspec, interval);
}

/**
 * clutter_animation_get_interval:
 * @animation: a #ClutterAnimation
 * @property_name: name of the property
 *
 * Retrieves the #ClutterInterval associated to @property_name
 * inside @animation.
 *
 * Return value: (transfer none): a #ClutterInterval or %NULL if no
 *   property with the same name was found. The returned interval is
 *   owned by the #ClutterAnimation and should not be unreferenced
 *
 * Since: 1.0
 */
ClutterInterval *
clutter_animation_get_interval (ClutterAnimation *animation,
                                const gchar      *property_name)
{
  ClutterAnimationPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  priv = animation->priv;

  return g_hash_table_lookup (priv->properties, property_name);
}

static void
on_timeline_started (ClutterTimeline  *timeline,
                     ClutterAnimation *animation)
{
  g_signal_emit (animation, animation_signals[STARTED], 0);
}

static void
on_timeline_completed (ClutterTimeline  *timeline,
                       ClutterAnimation *animation)
{
  CLUTTER_NOTE (ANIMATION, "Timeline [%p] complete", timeline);

  if (!clutter_animation_get_loop (animation))
    g_signal_emit (animation, animation_signals[COMPLETED], 0);
}

static void
on_alpha_notify (GObject          *gobject,
                 GParamSpec       *pspec,
                 ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv = animation->priv;
  GList *properties, *p;
  gdouble alpha_value;
  gboolean is_animatable = FALSE;
  ClutterAnimatable *animatable = NULL;

  alpha_value = clutter_alpha_get_alpha (CLUTTER_ALPHA (gobject));

  if (CLUTTER_IS_ANIMATABLE (priv->object))
    {
      animatable = CLUTTER_ANIMATABLE (priv->object);
      is_animatable = TRUE;
    }

  g_object_freeze_notify (priv->object);

  properties = g_hash_table_get_keys (priv->properties);
  for (p = properties; p != NULL; p = p->next)
    {
      const gchar *p_name = p->data;
      ClutterInterval *interval;
      GValue value = { 0, };
      gboolean apply;

      interval = g_hash_table_lookup (priv->properties, p_name);
      g_assert (CLUTTER_IS_INTERVAL (interval));

      g_value_init (&value, clutter_interval_get_value_type (interval));

      if (is_animatable)
        {
          const GValue *initial, *final;

          initial = clutter_interval_peek_initial_value (interval);
          final   = clutter_interval_peek_final_value (interval);

          apply = clutter_animatable_animate_property (animatable, animation,
                                                       p_name,
                                                       initial, final,
                                                       alpha_value,
                                                       &value);
        }
      else
        {
          apply = clutter_interval_compute_value (interval,
                                                  alpha_value,
                                                  &value);
        }

      if (apply)
        g_object_set_property (priv->object, p_name, &value);

      g_value_unset (&value);
    }

  g_list_free (properties);

  g_object_thaw_notify (priv->object);
}

static ClutterAlpha *
clutter_animation_get_alpha_internal (ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv = animation->priv;

  if (priv->alpha == NULL)
    {
      ClutterAlpha *alpha;

      alpha = clutter_alpha_new ();
      clutter_alpha_set_mode (alpha, priv->mode);

      priv->alpha_notify_id =
        g_signal_connect (alpha, "notify::alpha",
                          G_CALLBACK (on_alpha_notify),
                          animation);

      priv->alpha = g_object_ref_sink (alpha);
    }

  return priv->alpha;
}

static ClutterTimeline *
clutter_animation_get_timeline_internal (ClutterAnimation *animation)
{
  ClutterAlpha *alpha;

  alpha = clutter_animation_get_alpha_internal (animation);

  return clutter_alpha_get_timeline (alpha);
}

static inline void
clutter_animation_create_timeline (ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv = animation->priv;
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;

  alpha = clutter_animation_get_alpha_internal (animation);

  timeline = g_object_new (CLUTTER_TYPE_TIMELINE,
                           "duration", priv->duration,
                           "loop", priv->loop,
                           NULL);
  clutter_alpha_set_timeline (alpha, timeline);

  priv->timeline_started_id =
    g_signal_connect (timeline, "started",
                      G_CALLBACK (on_timeline_started),
                      animation);

  priv->timeline_completed_id =
    g_signal_connect (timeline, "completed",
                      G_CALLBACK (on_timeline_completed),
                      animation);

  /* since we are creating it ourselves, we can offload
   * the ownership of the timeline to the alpha itself
   */
  g_object_unref (timeline);
}

/*
 * Removes the animation pointer from the qdata section of the
 * actor attached to the animation
 */
static void
on_animation_weak_notify (gpointer  data,
                          GObject  *animation_pointer)
{
  GObject *actor = data;

  CLUTTER_NOTE (ANIMATION, "Removing Animation from actor %d[%p]",
                clutter_actor_get_gid (CLUTTER_ACTOR (actor)),
                actor);

  g_object_set_qdata (actor, quark_object_animation, NULL);
}

/**
 * clutter_animation_new:
 *
 * Creates a new #ClutterAnimation instance. You should set the
 * #GObject to be animated using clutter_animation_set_object(),
 * set the duration with clutter_animation_set_duration() and the
 * easing mode using clutter_animation_set_mode().
 *
 * Use clutter_animation_bind() or clutter_animation_bind_interval()
 * to define the properties to be animated. The interval and the
 * animated properties can be updated at runtime.
 *
 * The clutter_actor_animate() and relative family of functions provide
 * an easy way to animate a #ClutterActor and automatically manage the
 * lifetime of a #ClutterAnimation instance, so you should consider using
 * those functions instead of manually creating an animation.
 *
 * Return value: the newly created #ClutterAnimation. Use g_object_unref()
 *   to release the associated resources
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_animation_new (void)
{
  return g_object_new (CLUTTER_TYPE_ANIMATION, NULL);
}

/**
 * clutter_animation_set_object:
 * @animation: a #ClutterAnimation
 * @object: a #GObject
 *
 * Attaches @animation to @object. The #ClutterAnimation will take a
 * reference on @object.
 *
 * Since: 1.0
 */
void
clutter_animation_set_object (ClutterAnimation *animation,
                              GObject          *object)
{
  ClutterAnimationPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));
  g_return_if_fail (G_IS_OBJECT (object));

  priv = animation->priv;

  if (priv->object)
    {
      g_object_weak_unref (G_OBJECT (animation),
                           on_animation_weak_notify,
                           priv->object);
      g_object_set_qdata (priv->object, quark_object_animation, NULL);
      g_object_unref (priv->object);
    }

  priv->object = g_object_ref (object);
  g_object_weak_ref (G_OBJECT (animation),
                     on_animation_weak_notify,
                     priv->object);
  g_object_set_qdata_full (G_OBJECT (priv->object),
                           quark_object_animation,
                           animation,
                           NULL);

  g_object_notify (G_OBJECT (animation), "object");
}

/**
 * clutter_animation_get_object:
 * @animation: a #ClutterAnimation
 *
 * Retrieves the #GObject attached to @animation.
 *
 * Return value: (transfer none): a #GObject
 *
 * Since: 1.0
 */
GObject *
clutter_animation_get_object (ClutterAnimation *animation)
{
  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), NULL);

  return animation->priv->object;
}

static inline void
clutter_animation_set_mode_internal (ClutterAnimation *animation,
                                     gulong            mode)
{
  ClutterAnimationPrivate *priv = animation->priv;
  ClutterAlpha *alpha;

  priv->mode = mode;

  alpha = clutter_animation_get_alpha_internal (animation);
  clutter_alpha_set_mode (alpha, priv->mode);

  g_object_notify (G_OBJECT (animation), "mode");
}

/**
 * clutter_animation_set_mode:
 * @animation: a #ClutterAnimation
 * @mode: an animation mode logical id
 *
 * Sets the animation @mode of @animation. The animation @mode is
 * a logical id, either coming from the #ClutterAnimationMode enumeration
 * or the return value of clutter_alpha_register_func().
 *
 * Since: 1.0
 */
void
clutter_animation_set_mode (ClutterAnimation *animation,
                            gulong            mode)
{
  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));

  clutter_animation_set_mode_internal (animation, mode);
}

/**
 * clutter_animation_get_mode:
 * @animation: a #ClutterAnimation
 *
 * Retrieves the animation mode of @animation, as set by
 * clutter_animation_set_mode().
 *
 * Return value: the mode for the animation
 *
 * Since: 1.0
 */
gulong
clutter_animation_get_mode (ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), CLUTTER_LINEAR);

  priv = animation->priv;

  if (priv->alpha != NULL)
    return clutter_alpha_get_mode (priv->alpha);

  return priv->mode;
}

/**
 * clutter_animation_set_duration:
 * @animation: a #ClutterAnimation
 * @msecs: the duration in milliseconds
 *
 * Sets the duration of @animation in milliseconds.
 *
 * Since: 1.0
 */
void
clutter_animation_set_duration (ClutterAnimation *animation,
                                gint              msecs)
{
  ClutterAnimationPrivate *priv;
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));

  priv = animation->priv;

  priv->duration = msecs;

  alpha = clutter_animation_get_alpha_internal (animation);
  timeline = clutter_alpha_get_timeline (alpha);
  if (timeline == NULL)
    clutter_animation_create_timeline (animation);
  else
    {
      gboolean was_playing;

      was_playing = clutter_timeline_is_playing (timeline);
      if (was_playing)
        clutter_timeline_stop (timeline);

      clutter_timeline_set_duration (timeline, priv->duration);

      if (was_playing)
        clutter_timeline_start (timeline);
    }

  g_object_notify (G_OBJECT (animation), "duration");
}

/**
 * clutter_animation_set_loop:
 * @animation: a #ClutterAnimation
 * @loop: %TRUE if the animation should loop
 *
 * Sets whether @animation should loop over itself once finished.
 *
 * A looping #ClutterAnimation will not emit the #ClutterAnimation::completed
 * signal when finished.
 *
 * Since: 1.0
 */
void
clutter_animation_set_loop (ClutterAnimation *animation,
                            gboolean          loop)
{
  ClutterAnimationPrivate *priv;
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));

  priv = animation->priv;

  alpha = clutter_animation_get_alpha_internal (animation);
  timeline = clutter_alpha_get_timeline (alpha);
  if (timeline == NULL)
    {
      priv->loop = loop;

      clutter_animation_create_timeline (animation);
    }
  else
    clutter_timeline_set_loop (timeline, loop);

  g_object_notify (G_OBJECT (animation), "loop");
}

/**
 * clutter_animation_get_loop:
 * @animation: a #ClutterAnimation
 *
 * Retrieves whether @animation is looping.
 *
 * Return value: %TRUE if the animation is looping
 *
 * Since: 1.0
 */
gboolean
clutter_animation_get_loop (ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), FALSE);

  priv = animation->priv;

  if (priv->alpha != NULL)
    {
      ClutterTimeline *timeline;

      timeline = clutter_alpha_get_timeline (priv->alpha);
      if (timeline != NULL)
        return clutter_timeline_get_loop (timeline);
    }

  return priv->loop;
}

/**
 * clutter_animation_get_duration:
 * @animation: a #ClutterAnimation
 *
 * Retrieves the duration of @animation, in milliseconds.
 *
 * Return value: the duration of the animation
 *
 * Since: 1.0
 */
guint
clutter_animation_get_duration (ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), 0);

  priv = animation->priv;

  if (priv->alpha != NULL)
    {
      ClutterTimeline *timeline;

      timeline = clutter_alpha_get_timeline (priv->alpha);
      if (timeline != NULL)
        return clutter_timeline_get_duration (timeline);
    }

  return priv->duration;
}

/**
 * clutter_animation_set_timeline:
 * @animation: a #ClutterAnimation
 * @timeline: a #ClutterTimeline or %NULL
 *
 * Sets the #ClutterTimeline used by @animation.
 *
 * The #ClutterAnimation:duration and #ClutterAnimation:loop properties
 * will be set using the corresponding #ClutterTimeline properties as a
 * side effect.
 *
 * If @timeline is %NULL a new #ClutterTimeline will be constructed
 * using the current values of the #ClutterAnimation:duration and
 * #ClutterAnimation:loop properties.
 *
 * Since: 1.0
 */
void
clutter_animation_set_timeline (ClutterAnimation *animation,
                                ClutterTimeline  *timeline)
{
  ClutterAnimationPrivate *priv;
  ClutterTimeline *cur_timeline;
  ClutterAlpha *alpha;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));
  g_return_if_fail (timeline == NULL || CLUTTER_IS_TIMELINE (timeline));

  priv = animation->priv;

  cur_timeline = clutter_animation_get_timeline_internal (animation);
  if (cur_timeline == timeline)
    return;

  g_object_freeze_notify (G_OBJECT (animation));

  if (priv->timeline_started_id != 0)
    g_signal_handler_disconnect (cur_timeline, priv->timeline_started_id);

  if (priv->timeline_completed_id != 0)
    g_signal_handler_disconnect (cur_timeline, priv->timeline_completed_id);

  priv->timeline_started_id = 0;
  priv->timeline_completed_id = 0;

  if (timeline == NULL)
    {
      timeline = g_object_new (CLUTTER_TYPE_TIMELINE,
                               "duration", priv->duration,
                               "loop", priv->loop,
                               NULL);
    }
  else
    {
      priv->duration = clutter_timeline_get_duration (timeline);
      g_object_notify (G_OBJECT (animation), "duration");

      priv->loop = clutter_timeline_get_loop (timeline);
      g_object_notify (G_OBJECT (animation), "loop");
    }

  alpha = clutter_animation_get_alpha_internal (animation);
  clutter_alpha_set_timeline (alpha, timeline);
  g_object_notify (G_OBJECT (animation), "timeline");

  priv->timeline_started_id =
    g_signal_connect (timeline, "started",
                      G_CALLBACK (on_timeline_started),
                      animation);
  priv->timeline_completed_id =
    g_signal_connect (timeline, "completed",
                      G_CALLBACK (on_timeline_completed),
                      animation);

  g_object_thaw_notify (G_OBJECT (animation));
}

/**
 * clutter_animation_get_timeline:
 * @animation: a #ClutterAnimation
 *
 * Retrieves the #ClutterTimeline used by @animation
 *
 * Return value: (transfer none): the timeline used by the animation
 *
 * Since: 1.0
 */
ClutterTimeline *
clutter_animation_get_timeline (ClutterAnimation *animation)
{
  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), NULL);

  return clutter_animation_get_timeline_internal (animation);
}

/**
 * clutter_animation_set_alpha:
 * @animation: a #ClutterAnimation
 * @alpha: a #ClutterAlpha, or %NULL
 *
 * Sets @alpha as the #ClutterAlpha used by @animation.
 *
 * If @alpha is %NULL, a new #ClutterAlpha will be constructed from
 * the current value of the #ClutterAnimation:mode and the current
 * #ClutterAnimation:timeline.
 *
 * If @alpha is not %NULL, the #ClutterAnimation will take ownership
 * of the #ClutterAlpha instance.
 *
 * Since: 1.0
 */
void
clutter_animation_set_alpha (ClutterAnimation *animation,
                             ClutterAlpha     *alpha)
{
  ClutterAnimationPrivate *priv;
  ClutterTimeline *timeline;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));
  g_return_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha));

  priv = animation->priv;

  if (priv->alpha == alpha)
    return;

  /* retrieve the old timeline, if any */
  if (priv->alpha != NULL)
    {
      timeline = clutter_alpha_get_timeline (priv->alpha);
      if (timeline != NULL)
        g_object_ref (timeline);
    }
  else
    timeline = NULL;

  if (alpha == NULL)
    {
      /* this will create a new alpha */
      alpha = clutter_animation_get_alpha_internal (animation);

      /* if we had a timeline before, we should have the same timeline now */
      if (timeline != NULL)
        {
          clutter_alpha_set_timeline (alpha, timeline);
          g_object_unref (timeline);
        }
      else
        clutter_animation_create_timeline (animation);
    }
  else
    {
      if (timeline != NULL)
        {
          /* if we had a timeline before then we need to disconnect
           * the signal handlers from it
           */
          if (priv->timeline_started_id != 0)
            g_signal_handler_disconnect (timeline, priv->timeline_started_id);

          if (priv->timeline_completed_id != 0)
            g_signal_handler_disconnect (timeline, priv->timeline_completed_id);

          /* we don't need this timeline anymore */
          g_object_unref (timeline);
        }

      /* then we need to disconnect the signal handler from the old alpha */
      if (priv->alpha_notify_id != 0)
        {
          g_signal_handler_disconnect (priv->alpha, priv->alpha_notify_id);
          priv->alpha_notify_id = 0;
        }

      if (priv->alpha != NULL)
        {
          g_object_unref (priv->alpha);
          priv->alpha = NULL;
        }

      priv->alpha = g_object_ref_sink (alpha);
      priv->alpha_notify_id =
        g_signal_connect (priv->alpha, "notify::value",
                          G_CALLBACK (on_alpha_notify),
                          animation);

      /* if the alpha has a timeline then we use it */
      timeline = clutter_alpha_get_timeline (priv->alpha);
      if (timeline != NULL)
        {
          priv->duration = clutter_timeline_get_duration (timeline);
          priv->loop = clutter_timeline_get_loop (timeline);

          priv->timeline_started_id =
            g_signal_connect (timeline, "started",
                              G_CALLBACK (on_timeline_started),
                              animation);
          priv->timeline_completed_id =
            g_signal_connect (timeline, "completed",
                              G_CALLBACK (on_timeline_completed),
                              animation);
        }
      else
        clutter_animation_create_timeline (animation);
    }

  /* emit all relevant notifications */
  g_object_notify (G_OBJECT (animation), "mode");
  g_object_notify (G_OBJECT (animation), "duration");
  g_object_notify (G_OBJECT (animation), "loop");
  g_object_notify (G_OBJECT (animation), "alpha");
  g_object_notify (G_OBJECT (animation), "timeline");
}

/**
 * clutter_animation_get_alpha:
 * @animation: a #ClutterAnimation
 *
 * Retrieves the #ClutterAlpha used by @animation.
 *
 * Return value: (transfer none): the alpha object used by the animation
 *
 * Since: 1.0
 */
ClutterAlpha *
clutter_animation_get_alpha (ClutterAnimation *animation)
{
  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), NULL);

  return animation->priv->alpha;
}

/**
 * clutter_animation_completed:
 * @animation: a #ClutterAnimation
 *
 * Emits the ::completed signal on @animation
 *
 * When using this function with a #ClutterAnimation created
 * by the clutter_actor_animate() family of functions, @animation
 * will be unreferenced and it will not be valid anymore,
 * unless g_object_ref() was called before calling this function
 * or unless a reference was taken inside a handler for the
 * #ClutterAnimation::completed signal
 *
 * Since: 1.0
 */
void
clutter_animation_completed (ClutterAnimation *animation)
{
  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));

  g_signal_emit (animation, animation_signals[COMPLETED], 0);
}

static void
on_animation_completed (ClutterAnimation *animation)
{
  CLUTTER_NOTE (ANIMATION, "Animation[%p] completed, unreferencing",
                animation);

  g_object_unref (animation);
}

/*
 * starts the timeline
 */
static void
clutter_animation_start (ClutterAnimation *animation)
{
  ClutterTimeline *timeline;

  timeline = clutter_animation_get_timeline_internal (animation);

  if (G_LIKELY (timeline != NULL))
    clutter_timeline_start (timeline);
  else
    {
      /* sanity check */
      g_warning (G_STRLOC ": no timeline found, unable to start the animation");
    }
}

static void
clutter_animation_setup_property (ClutterAnimation *animation,
                                  const gchar      *property_name,
                                  const GValue     *value,
                                  GParamSpec       *pspec)
{
  ClutterAnimationPrivate *priv = animation->priv;
  gboolean is_fixed = FALSE;
  GValue real_value = { 0, };

  /* fixed properties will not be animated */
  if (g_str_has_prefix (property_name, "fixed::"))
    {
      is_fixed = TRUE;
      property_name += 7; /* strlen("fixed::") */
    }

  if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_warning ("Cannot bind property '%s': the property is "
                 "construct-only",
                 property_name);
      return;
    }

  if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("Cannot bind property '%s': the property is "
                 "not writable",
                 property_name);
      return;
    }

  /* initialize the real value that will be used to store the
   * final state of the animation
   */
  g_value_init (&real_value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  /* if it's not the same type of the GParamSpec value, try to
   * convert it using the GValue transformation API, otherwise
   * just copy it
   */
  if (!g_type_is_a (G_VALUE_TYPE (value), G_VALUE_TYPE (&real_value)))
    {
      if (!g_value_type_compatible (G_VALUE_TYPE (value),
                                    G_VALUE_TYPE (&real_value)) &&
          !g_value_type_compatible (G_VALUE_TYPE (&real_value),
                                    G_VALUE_TYPE (value)))
        {
          g_warning ("%s: Unable to convert from %s to %s for "
                     "the property '%s' of object %s",
                     G_STRLOC,
                     g_type_name (G_VALUE_TYPE (value)),
                     g_type_name (G_VALUE_TYPE (&real_value)),
                     property_name,
                     G_OBJECT_TYPE_NAME (priv->object));
          g_value_unset (&real_value);
          return;
        }

      if (!g_value_transform (value, &real_value))
        {
          g_warning ("%s: Unable to transform from %s to %s",
                     G_STRLOC,
                     g_type_name (G_VALUE_TYPE (value)),
                     g_type_name (G_VALUE_TYPE (&real_value)));
          g_value_unset (&real_value);
          return;
        }
    }
  else
    g_value_copy (value, &real_value);

  /* create an interval and bind it to the property, in case
   * it's not a fixed property, otherwise just set it
   */
  if (G_LIKELY (!is_fixed))
    {
      ClutterInterval *interval;
      GValue cur_value = { 0, };

      g_value_init (&cur_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (priv->object, property_name, &cur_value);

      interval =
        clutter_interval_new_with_values (G_PARAM_SPEC_VALUE_TYPE (pspec),
                                          &cur_value,
                                          &real_value);

      if (!clutter_animation_has_property (animation, pspec->name))
        clutter_animation_bind_property_internal (animation,
                                                  pspec,
                                                  interval);
      else
        clutter_animation_update_property_internal (animation,
                                                    pspec,
                                                    interval);

      g_value_unset (&cur_value);
    }
  else
    g_object_set_property (priv->object, property_name, &real_value);

  g_value_unset (&real_value);
}

static void
clutter_animation_setupv (ClutterAnimation    *animation,
                          gint                 n_properties,
                          const gchar * const  properties[],
                          const GValue        *values)
{
  ClutterAnimationPrivate *priv = animation->priv;
  GObjectClass *klass;
  gint i;

  klass = G_OBJECT_GET_CLASS (priv->object);

  for (i = 0; i < n_properties; i++)
    {
      const gchar *property_name = properties[i];
      GParamSpec *pspec;

      if (g_str_has_prefix (property_name, "fixed::"))
        property_name += 7; /* strlen("fixed::") */

      pspec = g_object_class_find_property (klass, property_name);
      if (!pspec)
        {
          g_warning ("Cannot bind property '%s': objects of type '%s' do "
                     "not have this property",
                     property_name,
                     g_type_name (G_OBJECT_TYPE (priv->object)));
          break;
        }

      clutter_animation_setup_property (animation, property_name,
                                        &values[i],
                                        pspec);
    }
}

static void
clutter_animation_setup_valist (ClutterAnimation *animation,
                                const gchar      *first_property_name,
                                va_list           var_args)
{
  ClutterAnimationPrivate *priv = animation->priv;
  GObjectClass *klass;
  const gchar *property_name;

  klass = G_OBJECT_GET_CLASS (priv->object);

  property_name = first_property_name;
  while (property_name != NULL)
    {
      GParamSpec *pspec;
      GValue final = { 0, };
      gchar *error = NULL;

      if (g_str_has_prefix (property_name, "signal::"))
        {
          const gchar *signal_name = property_name + 8;
          GCallback callback = va_arg (var_args, GCallback);
          gpointer  userdata = va_arg (var_args, gpointer);

          g_signal_connect (animation, signal_name, callback, userdata);
        }
      else
        {
          if (g_str_has_prefix (property_name, "fixed::"))
            property_name += 7; /* strlen("fixed::") */

          pspec = g_object_class_find_property (klass, property_name);
          if (!pspec)
            {
              g_warning ("Cannot bind property '%s': objects of type '%s' do "
                         "not have this property",
                         property_name,
                         g_type_name (G_OBJECT_TYPE (priv->object)));
              break;
            }

          g_value_init (&final, G_PARAM_SPEC_VALUE_TYPE (pspec));
          G_VALUE_COLLECT (&final, var_args, 0, &error);
          if (error)
            {
              g_warning ("%s: %s", G_STRLOC, error);
              g_free (error);
              break;
            }

          clutter_animation_setup_property (animation, property_name,
                                            &final,
                                            pspec);
        }

      property_name = va_arg (var_args, gchar*);
    }
}

/**
 * clutter_actor_animate_with_alpha:
 * @actor: a #ClutterActor
 * @alpha: a #ClutterAlpha
 * @first_property_name: the name of a property
 * @VarArgs: a %NULL terminated list of property names and
 *   property values
 *
 * Animates the given list of properties of @actor between the current
 * value for each property and a new final value. The animation has a
 * definite behaviour given by the passed @alpha.
 *
 * See clutter_actor_animate() for further details.
 *
 * This function is useful if you want to use an existing #ClutterAlpha
 * to animate @actor.
 *
 * Return value: (transfer none): a #ClutterAnimation object. The object is owned by the
 *   #ClutterActor and should not be unreferenced with g_object_unref()
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_actor_animate_with_alpha (ClutterActor *actor,
                                  ClutterAlpha *alpha,
                                  const gchar  *first_property_name,
                                  ...)
{
  ClutterAnimation *animation;
  ClutterTimeline *timeline;
  va_list args;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), NULL);
  g_return_val_if_fail (first_property_name != NULL, NULL);

  timeline = clutter_alpha_get_timeline (alpha);
  if (timeline == NULL)
    {
      g_warning ("The passed ClutterAlpha does not have an "
                 "associated ClutterTimeline.");
      return NULL;
    }

  animation = g_object_get_qdata (G_OBJECT (actor), quark_object_animation);
  if (animation == NULL)
    {
      animation = clutter_animation_new ();

      g_signal_connect (animation, "completed",
                        G_CALLBACK (on_animation_completed),
                        NULL);

      CLUTTER_NOTE (ANIMATION, "Created new Animation [%p]", animation);
    }
  else
    CLUTTER_NOTE (ANIMATION, "Reusing Animation [%p]", animation);

  clutter_animation_set_alpha (animation, alpha);
  clutter_animation_set_object (animation, G_OBJECT (actor));

  va_start (args, first_property_name);
  clutter_animation_setup_valist (animation, first_property_name, args);
  va_end (args);

  clutter_animation_start (animation);

  return animation;
}

/**
 * clutter_actor_animate_with_timeline:
 * @actor: a #ClutterActor
 * @mode: an animation mode logical id
 * @timeline: a #ClutterTimeline
 * @first_property_name: the name of a property
 * @VarArgs: a %NULL terminated list of property names and
 *   property values
 *
 * Animates the given list of properties of @actor between the current
 * value for each property and a new final value. The animation has a
 * definite duration given by @timeline and a speed given by the @mode.
 *
 * See clutter_actor_animate() for further details.
 *
 * This function is useful if you want to use an existing timeline
 * to animate @actor.
 *
 * Return value: (transfer none): a #ClutterAnimation object. The object is
 *    owned by the #ClutterActor and should not be unreferenced with
 *    g_object_unref()
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_actor_animate_with_timeline (ClutterActor    *actor,
                                     gulong           mode,
                                     ClutterTimeline *timeline,
                                     const gchar     *first_property_name,
                                     ...)
{
  ClutterAnimation *animation;
  va_list args;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);
  g_return_val_if_fail (first_property_name != NULL, NULL);

  animation = g_object_get_qdata (G_OBJECT (actor), quark_object_animation);
  if (animation == NULL)
    {
      animation = clutter_animation_new ();

      g_signal_connect (animation, "completed",
                        G_CALLBACK (on_animation_completed),
                        NULL);

      CLUTTER_NOTE (ANIMATION, "Created new Animation [%p]", animation);
    }
  else
    CLUTTER_NOTE (ANIMATION, "Reusing Animation [%p]", animation);

  clutter_animation_set_timeline (animation, timeline);
  clutter_animation_set_mode (animation, mode);
  clutter_animation_set_object (animation, G_OBJECT (actor));

  va_start (args, first_property_name);
  clutter_animation_setup_valist (animation, first_property_name, args);
  va_end (args);

  clutter_animation_start (animation);

  return animation;
}

/**
 * clutter_actor_animate:
 * @actor: a #ClutterActor
 * @mode: an animation mode logical id
 * @duration: duration of the animation, in milliseconds
 * @first_property_name: the name of a property
 * @VarArgs: a %NULL terminated list of property names and
 *   property values
 *
 * Animates the given list of properties of @actor between the current
 * value for each property and a new final value. The animation has a
 * definite duration and a speed given by the @mode.
 *
 * For example, this:
 *
 * |[
 *   clutter_actor_animate (rectangle, CLUTTER_LINEAR, 250,
 *                          "width", 100,
 *                          "height", 100,
 *                          NULL);
 * ]|
 *
 * will make width and height properties of the #ClutterActor "rectangle"
 * grow linearly between the current value and 100 pixels, in 250 milliseconds.
 *
 * The animation @mode is a logical id, either from the #ClutterAnimationMode
 * enumeration of from clutter_alpha_register_func().
 *
 * All the properties specified will be animated between the current value
 * and the final value. If a property should be set at the beginning of
 * the animation but not updated during the animation, it should be prefixed
 * by the "fixed::" string, for instance:
 *
 * |[
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN, 100,
 *                          "rotation-angle-z", 360,
 *                          "fixed::rotation-center-z", &amp;center,
 *                          NULL);
 * ]|
 *
 * Will animate the "rotation-angle-z" property between the current value
 * and 360 degrees, and set the "rotation-center-z" property to the fixed
 * value of the #ClutterVertex "center".
 *
 * This function will implicitly create a #ClutterAnimation object which
 * will be assigned to the @actor and will be returned to the developer
 * to control the animation or to know when the animation has been
 * completed.
 *
 * If a name argument starts with "signal::" the two following arguments
 * are used as callback function and userdata for a signal handler installed
 * on the #ClutterAnimation object for the specified signal name, for
 * instance:
 *
 * |[
 *
 *   static void
 *   on_animation_completed (ClutterAnimation *animation,
 *                           ClutterActor     *actor)
 *   {
 *     clutter_actor_hide (actor);
 *   }
 *
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN, 100,
 *                          "opacity", 0,
 *                          "signal::completed", on_animation_completed, actor,
 *                          NULL);
 * ]|
 *
 * Calling this function on an actor that is already being animated
 * will cause the current animation to change with the new final values,
 * the new easing mode and the new duration - that is, this code:
 *
 * |[
 *   clutter_actor_animate (actor, CLUTTER_LINEAR, 250,
 *                          "width", 100,
 *                          "height", 100,
 *                          NULL);
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 500,
 *                          "x", 100,
 *                          "y", 100,
 *                          "width", 200,
 *                          NULL);
 * ]|
 *
 * is the equivalent of:
 *
 * |[
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 500,
 *                          "x", 100,
 *                          "y", 100,
 *                          "width", 200,
 *                          "height", 100,
 *                          NULL);
 * ]|
 *
 * <note>Unless the animation is looping, it will become invalid as soon
 * as it is complete. To avoid this, you should keep a reference on the
 * returned value using g_object_ref(). If you want to keep the animation
 * alive across multiple cycles, you also have to add a reference each
 * time the #ClutterAnimation::completed signal is emitted.</note>
 *
 * Since the created #ClutterAnimation instance attached to @actor
 * is guaranteed to be valid throughout the #ClutterAnimation::completed
 * signal emission chain, you will not be able to create a new animation
 * using clutter_actor_animate() on the same @actor from within the
 * #ClutterAnimation::completed signal handler. Instead, you should use
 * clutter_threads_add_idle() to install an idle handler and call
 * clutter_actor_animate() in the handler, for instance:
 *
 * |[
 *   static gboolean
 *   queue_animation (gpointer data)
 *   {
 *     ClutterActor *actor = data;
 *
 *     clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 250,
 *                            "width", 200,
 *                            "height", 200,
 *                            NULL);
 *
 *     return FALSE;
 *   }
 *
 *   static void
 *   on_animation_completed (ClutterAnimation *animation)
 *   {
 *     clutter_threads_add_idle (queue_animation,
 *                               clutter_animation_get_object (animation));
 *   }
 *
 *     ...
 *     animation = clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 250,
 *                                        "x", 100,
 *                                        "y", 100,
 *                                        NULL);
 *     g_signal_connect (animation, "completed",
 *                       G_CALLBACK (on_animation_completed),
 *                       NULL);
 *     ...
 * ]|
 *
 * Return value: (transfer none): a #ClutterAnimation object. The object is
 *   owned by the #ClutterActor and should not be unreferenced with
 *   g_object_unref()
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_actor_animate (ClutterActor *actor,
                       gulong        mode,
                       guint         duration,
                       const gchar  *first_property_name,
                       ...)
{
  ClutterAnimation *animation;
  va_list args;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);
  g_return_val_if_fail (mode != CLUTTER_CUSTOM_MODE, NULL);
  g_return_val_if_fail (duration > 0, NULL);
  g_return_val_if_fail (first_property_name != NULL, NULL);

  animation = g_object_get_qdata (G_OBJECT (actor), quark_object_animation);
  if (animation == NULL)
    {
      /* if there is no animation already attached to the actor,
       * create one and set up the timeline and alpha using the
       * current values for duration, mode and loop
       */
      animation = clutter_animation_new ();
      clutter_animation_set_object (animation, G_OBJECT (actor));

      g_signal_connect (animation, "completed",
                        G_CALLBACK (on_animation_completed),
                        NULL);

      CLUTTER_NOTE (ANIMATION, "Created new Animation [%p]", animation);
    }
  else
    CLUTTER_NOTE (ANIMATION, "Reusing Animation [%p]", animation);

  /* force the update of duration and mode using the new
   * values coming from the parameters of this function
   */
  clutter_animation_set_mode (animation, mode);
  clutter_animation_set_duration (animation, duration);

  va_start (args, first_property_name);
  clutter_animation_setup_valist (animation, first_property_name, args);
  va_end (args);

  clutter_animation_start (animation);

  return animation;
}

/**
 * clutter_actor_animatev:
 * @actor: a #ClutterActor
 * @mode: an animation mode logical id
 * @duration: duration of the animation, in milliseconds
 * @n_properties: number of property names and values
 * @properties: (array length=n_properties): a vector containing the
 *    property names to set
 * @values: (array length=n_properies): a vector containing the
 *    property values to set
 *
 * Animates the given list of properties of @actor between the current
 * value for each property and a new final value. The animation has a
 * definite duration and a speed given by the @mode.
 *
 * This is the vector-based variant of clutter_actor_animate(), useful
 * for language bindings.
 *
 * <warning>Unlike clutter_actor_animate(), this function will not
 * allow you to specify "signal::" names and callbacks.</warning>
 *
 * Return value: (transfer none): a #ClutterAnimation object. The object is
 *   owned by the #ClutterActor and should not be unreferenced with
 *   g_object_unref()
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_actor_animatev (ClutterActor        *actor,
                        gulong               mode,
                        guint                duration,
                        gint                 n_properties,
                        const gchar * const  properties[],
                        const GValue        *values)
{
  ClutterAnimation *animation;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);
  g_return_val_if_fail (mode != CLUTTER_CUSTOM_MODE, NULL);
  g_return_val_if_fail (duration > 0, NULL);
  g_return_val_if_fail (properties != NULL, NULL);
  g_return_val_if_fail (values != NULL, NULL);

  animation = g_object_get_qdata (G_OBJECT (actor), quark_object_animation);
  if (animation == NULL)
    {
      /* if there is no animation already attached to the actor,
       * create one and set up the timeline and alpha using the
       * current values for duration, mode and loop
       */
      animation = clutter_animation_new ();
      clutter_animation_set_object (animation, G_OBJECT (actor));

      g_signal_connect (animation, "completed",
                        G_CALLBACK (on_animation_completed),
                        NULL);

      CLUTTER_NOTE (ANIMATION, "Created new Animation [%p]", animation);
    }
  else
    CLUTTER_NOTE (ANIMATION, "Reusing Animation [%p]", animation);

  /* force the update of duration and mode using the new
   * values coming from the parameters of this function
   */
  clutter_animation_set_mode (animation, mode);
  clutter_animation_set_duration (animation, duration);
  clutter_animation_setupv (animation, n_properties, properties, values);
  clutter_animation_start (animation);

  return animation;
}

/**
 * clutter_actor_animate_with_timelinev:
 * @actor: a #ClutterActor
 * @mode: an animation mode logical id
 * @timeline: a #ClutterTimeline
 * @n_properties: number of property names and values
 * @properties: (array length=n_properties): a vector containing the
 *    property names to set
 * @values: (array length=n_properies): a vector containing the
 *    property values to set
 *
 * Animates the given list of properties of @actor between the current
 * value for each property and a new final value. The animation has a
 * definite duration given by @timeline and a speed given by the @mode.
 *
 * See clutter_actor_animate() for further details.
 *
 * This function is useful if you want to use an existing timeline
 * to animate @actor.
 *
 * This is the vector-based variant of clutter_actor_animate_with_timeline(),
 * useful for language bindings.
 *
 * <warning>Unlike clutter_actor_animate_with_timeline(), this function
 * will not allow you to specify "signal::" names and callbacks.</warning>
 *
 * Return value: (transfer none): a #ClutterAnimation object. The object is
 *    owned by the #ClutterActor and should not be unreferenced with
 *    g_object_unref()
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_actor_animate_with_timelinev (ClutterActor        *actor,
                                      gulong               mode,
                                      ClutterTimeline     *timeline,
                                      gint                 n_properties,
                                      const gchar * const  properties[],
                                      const GValue        *values)
{
  ClutterAnimation *animation;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);
  g_return_val_if_fail (properties != NULL, NULL);
  g_return_val_if_fail (values != NULL, NULL);

  animation = g_object_get_qdata (G_OBJECT (actor), quark_object_animation);
  if (animation == NULL)
    {
      animation = clutter_animation_new ();

      g_signal_connect (animation, "completed",
                        G_CALLBACK (on_animation_completed),
                        NULL);

      CLUTTER_NOTE (ANIMATION, "Created new Animation [%p]", animation);
    }
  else
    CLUTTER_NOTE (ANIMATION, "Reusing Animation [%p]", animation);

  clutter_animation_set_timeline (animation, timeline);
  clutter_animation_set_mode (animation, mode);
  clutter_animation_set_object (animation, G_OBJECT (actor));
  clutter_animation_setupv (animation, n_properties, properties, values);
  clutter_animation_start (animation);

  return animation;
}

/**
 * clutter_actor_animate_with_alphav:
 * @actor: a #ClutterActor
 * @alpha: a #ClutterAlpha
 * @n_properties: number of property names and values
 * @properties: (array length=n_properties): a vector containing the
 *    property names to set
 * @values: (array length=n_properies): a vector containing the
 *    property values to set
 *
 * Animates the given list of properties of @actor between the current
 * value for each property and a new final value. The animation has a
 * definite behaviour given by the passed @alpha.
 *
 * See clutter_actor_animate() for further details.
 *
 * This function is useful if you want to use an existing #ClutterAlpha
 * to animate @actor.
 *
 * This is the vector-based variant of clutter_actor_animate_with_alpha(),
 * useful for language bindings.
 *
 * <warning>Unlike clutter_actor_animate_with_alpha(), this function will
 * not allow you to specify "signal::" names and callbacks.</warning>
 *
 * Return value: (transfer none): a #ClutterAnimation object. The object is owned by the
 *   #ClutterActor and should not be unreferenced with g_object_unref()
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_actor_animate_with_alphav (ClutterActor        *actor,
                                   ClutterAlpha        *alpha,
                                   gint                 n_properties,
                                   const gchar * const  properties[],
                                   const GValue        *values)
{
  ClutterAnimation *animation;
  ClutterTimeline *timeline;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), NULL);
  g_return_val_if_fail (properties != NULL, NULL);
  g_return_val_if_fail (values != NULL, NULL);

  timeline = clutter_alpha_get_timeline (alpha);
  if (timeline == NULL)
    {
      g_warning ("The passed ClutterAlpha does not have an "
                 "associated ClutterTimeline.");
      return NULL;
    }

  animation = g_object_get_qdata (G_OBJECT (actor), quark_object_animation);
  if (animation == NULL)
    {
      animation = clutter_animation_new ();

      g_signal_connect (animation, "completed",
                        G_CALLBACK (on_animation_completed),
                        NULL);

      CLUTTER_NOTE (ANIMATION, "Created new Animation [%p]", animation);
    }
  else
    CLUTTER_NOTE (ANIMATION, "Reusing Animation [%p]", animation);

  clutter_animation_set_alpha (animation, alpha);
  clutter_animation_set_object (animation, G_OBJECT (actor));
  clutter_animation_setupv (animation, n_properties, properties, values);
  clutter_animation_start (animation);

  return animation;
}

/**
 * clutter_actor_get_animation:
 * @actor: a #ClutterActor
 *
 * Retrieves the #ClutterAnimation used by @actor, if clutter_actor_animate()
 * has been called on @actor.
 *
 * Return value: (transfer none): a #ClutterAnimation, or %NULL
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_actor_get_animation (ClutterActor *actor)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  return g_object_get_qdata (G_OBJECT (actor), quark_object_animation);
}
