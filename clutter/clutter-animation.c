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
 * @See_Also: #ClutterAnimatable, #ClutterInterval, #ClutterAlpha,
 *   #ClutterTimeline
 *
 * #ClutterAnimation is an object providing simple, implicit animations
 * for #GObject<!-- -->s.
 *
 * #ClutterAnimation instances will bind one or more #GObject properties
 * belonging to a #GObject to a #ClutterInterval, and will then use a
 * #ClutterAlpha to interpolate the property between the initial and final
 * values of the interval.
 *
 * The duration of the animation is set using clutter_animation_set_duration().
 * The easing mode of the animation is set using clutter_animation_set_mode().
 *
 * If you want to control the animation you should retrieve the
 * #ClutterTimeline using clutter_animation_get_timeline() and then
 * use #ClutterTimeline functions like clutter_timeline_start(),
 * clutter_timeline_pause() or clutter_timeline_stop().
 *
 * A #ClutterAnimation will emit the #ClutterAnimation::completed signal
 * when the #ClutterTimeline used by the animation is completed; unlike
 * #ClutterTimeline, though, the #ClutterAnimation::completed will not be
 * emitted if #ClutterAnimation:loop is set to %TRUE - that is, a looping
 * animation never completes.
 *
 * If your animation depends on user control you can force its completion
 * using clutter_animation_completed().
 *
 * If the #GObject instance bound to a #ClutterAnimation implements the
 * #ClutterAnimatable interface it is possible for that instance to
 * control the way the initial and final states are interpolated.
 *
 * #ClutterAnimation<!-- -->s are distinguished from #ClutterBehaviour<!-- -->s
 * because the former can only control #GObject properties of a single
 * #GObject instance, while the latter can control multiple properties
 * using accessor functions inside the #ClutterBehaviour::alpha_notify
 * virtual function, and can control multiple #ClutterActor<!-- -->s as well.
 *
 * For convenience, it is possible to use the clutter_actor_animate()
 * function call which will take care of setting up and tearing down
 * a #ClutterAnimation instance and animate an actor between its current
 * state and the specified final state.
 *
 * <refsect2 id="clutter-AnimationMode-Script">
 *   <title>Defining ClutterAnimationMode inside ClutterScript</title>
 *   <para>When defining a #ClutterAnimation inside a ClutterScript
 *   file or string the #ClutterAnimation:mode can be defined either
 *   using the #ClutterAnimationMode enumeration values through their
 *   "nick" (the short string used inside #GEnumValue), their numeric
 *   id, or using the following strings:</para>
 *   <variablelist>
 *     <varlistentry>
 *       <term>easeInQuad, easeOutQuad, easeInOutQuad</term>
 *       <listitem><para>Corresponding to the quadratic easing
 *       modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInCubic, easeOutCubic, easeInOutCubic</term>
 *       <listitem><para>Corresponding to the cubic easing
 *       modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInQuart, easeOutQuart, easeInOutQuart</term>
 *       <listitem><para>Corresponding to the quartic easing
 *       modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInQuint, easeOutQuint, easeInOutQuint</term>
 *       <listitem><para>Corresponding to the quintic easing
 *       modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInSine, easeOutSine, easeInOutSine</term>
 *       <listitem><para>Corresponding to the sine easing
 *       modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInExpo, easeOutExpo, easeInOutExpo</term>
 *       <listitem><para>Corresponding to the exponential easing
 *       modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInCirc, easeOutCirc, easeInOutCirc</term>
 *       <listitem><para>Corresponding to the circular easing
 *       modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInElastic, easeOutElastic, easeInOutElastic</term>
 *       <listitem><para>Corresponding to the overshooting elastic
 *       easing modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInBack, easeOutBack, easeInOutBack</term>
 *       <listitem><para>Corresponding to the overshooting cubic
 *       easing modes</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>easeInBounce, easeOutBounce, easeInOutBounce</term>
 *       <listitem><para>Corresponding to the bouncing easing
 *       modes</para></listitem>
 *     </varlistentry>
 *   </variablelist>
 * </refsect2>
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
#include "clutter-scriptable.h"
#include "clutter-script-private.h"

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

  guint timeline_started_id;
  guint timeline_completed_id;
  guint alpha_notify_id;
};

static guint animation_signals[LAST_SIGNAL] = { 0, };

static GQuark quark_object_animation = 0;

static void clutter_scriptable_init (ClutterScriptableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterAnimation, clutter_animation, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_init));

static void
clutter_animation_real_completed (ClutterAnimation *self)
{
  ClutterAnimationPrivate *priv = self->priv;
  ClutterAnimation *animation;
  ClutterTimeline *timeline;
  ClutterTimelineDirection direction;
  gpointer key, value;
  GHashTableIter iter;

  timeline = clutter_animation_get_timeline (self);
  direction = clutter_timeline_get_direction (timeline);

  /* explicitly set the final state of the animation */
  g_hash_table_iter_init (&iter, priv->properties);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *p_name = key;
      ClutterInterval *interval = value;
      GValue *p_value;

      if (direction == CLUTTER_TIMELINE_FORWARD)
        p_value = clutter_interval_peek_final_value (interval);
      else
        p_value = clutter_interval_peek_initial_value (interval);

      g_object_set_property (priv->object, p_name, p_value);
    }

  /* at this point, if this animation was created by clutter_actor_animate()
   * and friends, the animation will be attached to the object's data; since
   * we want to allow developers to use g_signal_connect_after("completed")
   * to concatenate a new animation, we need to remove the animation back
   * pointer here, and unref() the animation. FIXME - we might want to
   * provide a clutter_animation_attach()/clutter_animation_detach() pair
   * to let the user reattach an animation
   */
  animation = g_object_get_qdata (priv->object, quark_object_animation);
  if (animation == self)
    {
      g_object_set_qdata (priv->object, quark_object_animation, NULL);
      g_object_unref (animation);
    }
}

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

  if (priv->alpha != NULL)
    timeline = clutter_alpha_get_timeline (priv->alpha);
  else
    timeline = NULL;

  if (timeline != NULL && priv->timeline_started_id != 0)
    g_signal_handler_disconnect (timeline, priv->timeline_started_id);

  if (timeline != NULL && priv->timeline_completed_id != 0)
    g_signal_handler_disconnect (timeline, priv->timeline_completed_id);

  priv->timeline_started_id = 0;
  priv->timeline_completed_id = 0;

  if (priv->alpha != NULL)
    {
      if (priv->alpha_notify_id != 0)
        g_signal_handler_disconnect (priv->alpha, priv->alpha_notify_id);

      g_object_unref (priv->alpha);
    }

  priv->alpha_notify_id = 0;
  priv->alpha = NULL;

  if (priv->object != NULL)
    g_object_unref (priv->object);

  priv->object = NULL;

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

static gboolean
clutter_animation_parse_custom_node (ClutterScriptable *scriptable,
                                     ClutterScript     *script,
                                     GValue            *value,
                                     const gchar       *name,
                                     JsonNode          *node)
{
  if (strncmp (name, "mode", 4) == 0)
    {
      if (json_node_get_node_type (node) != JSON_NODE_VALUE)
        return FALSE;

      g_value_init (value, G_TYPE_ULONG);

      if (json_node_get_value_type (node) == G_TYPE_INT64)
        {
          g_value_set_ulong (value, json_node_get_int (node));
          return TRUE;
        }
      else if (json_node_get_value_type (node) == G_TYPE_STRING)
        {
          const gchar *str = json_node_get_string (node);
          gulong mode = CLUTTER_LINEAR;

          mode = clutter_script_resolve_animation_mode (str);
          g_value_set_ulong (value, mode);

          return TRUE;
        }
      else
        g_warning ("Expected an integer id or a string id for "
                   "the ClutterAnimation mode property");
    }

  return FALSE;
}

static void
clutter_scriptable_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_animation_parse_custom_node;
}

static void
clutter_animation_class_init (ClutterAnimationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  quark_object_animation =
    g_quark_from_static_string ("clutter-actor-animation");

  g_type_class_add_private (klass, sizeof (ClutterAnimationPrivate));

  klass->completed = clutter_animation_real_completed;

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
   * ClutterAnimation::completed:
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

  if (!g_value_type_compatible (G_PARAM_SPEC_VALUE_TYPE (pspec), argtype))
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
 *
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
  pspec = clutter_animation_validate_bind (animation, property_name, type);
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
      g_warning ("Cannot update property '%s': the animation has "
                 "no bound property with that name",
                 property_name);
      return;
    }

  klass = G_OBJECT_GET_CLASS (priv->object);
  pspec = g_object_class_find_property (klass, property_name);
  if (!pspec)
    {
      g_warning ("Cannot update property '%s': objects of type '%s' have "
                 "no such property",
                 property_name,
                 g_type_name (G_OBJECT_TYPE (priv->object)));
      return;
    }

  if (!g_value_type_compatible (G_PARAM_SPEC_VALUE_TYPE (pspec),
                                clutter_interval_get_value_type (interval)))
    {
      g_warning ("Cannot update property '%s': the interval value of "
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
 * clutter_animation_update:
 * @animation: a #ClutterAnimation
 * @property_name: name of the property
 * @final: The final value of the property
 *
 * Updates the @final value of the interval for @property_name
 *
 * Return value: (transfer none): The animation itself.
 *
 * Since: 1.0
 */
ClutterAnimation *
clutter_animation_update (ClutterAnimation *animation,
                          const gchar      *property_name,
                          const GValue     *final)
{
  ClutterAnimationPrivate *priv;
  ClutterInterval *interval;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);
  g_return_val_if_fail (final != NULL, NULL);
  g_return_val_if_fail (G_VALUE_TYPE (final) != G_TYPE_INVALID, NULL);

  priv = animation->priv;

  interval = clutter_animation_get_interval (animation, property_name);
  if (interval == NULL)
    {
      g_warning ("Cannot update property '%s': the animation has "
                 "no bound property with that name",
                 property_name);
      return NULL;
    }

  if (!g_value_type_compatible (G_VALUE_TYPE (final),
                                clutter_interval_get_value_type (interval)))
    {
      g_warning ("Cannot update property '%s': the interval value of "
                 "type '%s' is not compatible with the property value "
                 "of type '%s'",
                 property_name,
                 g_type_name (clutter_interval_get_value_type (interval)),
                 g_type_name (G_VALUE_TYPE (final)));
      return NULL;
    }

  clutter_interval_set_final_value (interval, final);

  return animation;
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
  ClutterAnimationPrivate *priv;
  GList *properties, *p;
  gdouble alpha_value;
  gboolean is_animatable = FALSE;
  ClutterAnimatable *animatable = NULL;

  /* make sure the animation survives the notification */
  g_object_ref (animation);

  priv = animation->priv;

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

  g_object_unref (animation);
}

static ClutterAlpha *
clutter_animation_get_alpha_internal (ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv = animation->priv;

  if (priv->alpha == NULL)
    {
      ClutterAlpha *alpha;

      alpha = clutter_alpha_new ();
      clutter_alpha_set_mode (alpha, CLUTTER_LINEAR);

      priv->alpha_notify_id =
        g_signal_connect (alpha, "notify::alpha",
                          G_CALLBACK (on_alpha_notify),
                          animation);

      priv->alpha = g_object_ref_sink (alpha);

      g_object_notify (G_OBJECT (animation), "alpha");
    }

  return priv->alpha;
}

static ClutterTimeline *
clutter_animation_get_timeline_internal (ClutterAnimation *animation)
{
  ClutterAnimationPrivate *priv = animation->priv;
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;

  alpha = clutter_animation_get_alpha_internal (animation);
  timeline = clutter_alpha_get_timeline (alpha);
  if (timeline != NULL)
    return timeline;

  timeline = g_object_new (CLUTTER_TYPE_TIMELINE, NULL);

  priv->timeline_started_id =
    g_signal_connect (timeline, "started",
                      G_CALLBACK (on_timeline_started),
                      animation);

  priv->timeline_completed_id =
    g_signal_connect (timeline, "completed",
                      G_CALLBACK (on_timeline_completed),
                      animation);

  clutter_alpha_set_timeline (alpha, timeline);

  /* the alpha owns the timeline now */
  g_object_unref (timeline);

  g_object_notify (G_OBJECT (animation), "timeline");

  return timeline;
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
  g_return_if_fail (object == NULL || G_IS_OBJECT (object));

  priv = animation->priv;

  if (priv->object != NULL)
    {
      g_object_set_qdata (priv->object, quark_object_animation, NULL);

      g_object_unref (priv->object);
      priv->object = NULL;
    }

  if (object != NULL)
    priv->object = g_object_ref (object);

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

/**
 * clutter_animation_set_mode:
 * @animation: a #ClutterAnimation
 * @mode: an animation mode logical id
 *
 * Sets the animation @mode of @animation. The animation @mode is
 * a logical id, either coming from the #ClutterAnimationMode enumeration
 * or the return value of clutter_alpha_register_func().
 *
 * This function will also set #ClutterAnimation:alpha if needed.
 *
 * Since: 1.0
 */
void
clutter_animation_set_mode (ClutterAnimation *animation,
                            gulong            mode)
{
  ClutterAlpha *alpha;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));

  g_object_freeze_notify (G_OBJECT (animation));

  alpha = clutter_animation_get_alpha_internal (animation);
  clutter_alpha_set_mode (alpha, mode);

  g_object_notify (G_OBJECT (animation), "mode");

  g_object_thaw_notify (G_OBJECT (animation));
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
  ClutterAlpha *alpha;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), CLUTTER_LINEAR);

  alpha = clutter_animation_get_alpha_internal (animation);

  return clutter_alpha_get_mode (alpha);
}

/**
 * clutter_animation_set_duration:
 * @animation: a #ClutterAnimation
 * @msecs: the duration in milliseconds
 *
 * Sets the duration of @animation in milliseconds.
 *
 * This function will set #ClutterAnimation:alpha and
 * #ClutterAnimation:timeline if needed.
 *
 * Since: 1.0
 */
void
clutter_animation_set_duration (ClutterAnimation *animation,
                                gint              msecs)
{
  ClutterTimeline *timeline;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));

  g_object_freeze_notify (G_OBJECT (animation));

  timeline = clutter_animation_get_timeline_internal (animation);
  clutter_timeline_set_duration (timeline, msecs);
  clutter_timeline_rewind (timeline);

  g_object_notify (G_OBJECT (animation), "duration");

  g_object_thaw_notify (G_OBJECT (animation));
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
 * This function will set #ClutterAnimation:alpha and
 * #ClutterAnimation:timeline if needed.
 *
 * Since: 1.0
 */
void
clutter_animation_set_loop (ClutterAnimation *animation,
                            gboolean          loop)
{
  ClutterTimeline *timeline;

  g_return_if_fail (CLUTTER_IS_ANIMATION (animation));

  g_object_freeze_notify (G_OBJECT (animation));

  timeline = clutter_animation_get_timeline_internal (animation);
  clutter_timeline_set_loop (timeline, loop);

  g_object_notify (G_OBJECT (animation), "loop");

  g_object_thaw_notify (G_OBJECT (animation));
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
  ClutterTimeline *timeline;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), FALSE);

  timeline = clutter_animation_get_timeline_internal (animation);

  return clutter_timeline_get_loop (timeline);
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
  ClutterTimeline *timeline;

  g_return_val_if_fail (CLUTTER_IS_ANIMATION (animation), 0);

  timeline = clutter_animation_get_timeline_internal (animation);

  return clutter_timeline_get_duration (timeline);
}

/**
 * clutter_animation_set_timeline:
 * @animation: a #ClutterAnimation
 * @timeline: a #ClutterTimeline, or %NULL to unset the
 *   current #ClutterTimeline
 *
 * Sets the #ClutterTimeline used by @animation.
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

  if (priv->alpha != NULL)
    cur_timeline = clutter_alpha_get_timeline (priv->alpha);
  else
    cur_timeline = NULL;

  if (cur_timeline == timeline)
    return;

  g_object_freeze_notify (G_OBJECT (animation));

  if (cur_timeline != NULL && priv->timeline_started_id != 0)
    g_signal_handler_disconnect (cur_timeline, priv->timeline_started_id);

  if (cur_timeline != NULL && priv->timeline_completed_id != 0)
    g_signal_handler_disconnect (cur_timeline, priv->timeline_completed_id);

  priv->timeline_started_id = 0;
  priv->timeline_completed_id = 0;

  alpha = clutter_animation_get_alpha_internal (animation);
  clutter_alpha_set_timeline (alpha, timeline);
  g_object_notify (G_OBJECT (animation), "timeline");
  g_object_notify (G_OBJECT (animation), "duration");
  g_object_notify (G_OBJECT (animation), "loop");

  if (timeline)
    {
      priv->timeline_started_id =
        g_signal_connect (timeline, "started",
                          G_CALLBACK (on_timeline_started),
                          animation);
      priv->timeline_completed_id =
        g_signal_connect (timeline, "completed",
                          G_CALLBACK (on_timeline_completed),
                          animation);
    }

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
 * @alpha: a #ClutterAlpha, or %NULL to unset the current #ClutterAlpha
 *
 * Sets @alpha as the #ClutterAlpha used by @animation.
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

  g_object_freeze_notify (G_OBJECT (animation));

  if (priv->alpha != NULL)
    timeline = clutter_alpha_get_timeline (priv->alpha);
  else
    timeline = NULL;

  /* disconnect the old timeline first */
  if (timeline != NULL && priv->timeline_started_id != 0)
    {
      g_signal_handler_disconnect (timeline, priv->timeline_started_id);
      priv->timeline_started_id = 0;
    }

  if (timeline != NULL && priv->timeline_completed_id != 0)
    {
      g_signal_handler_disconnect (timeline, priv->timeline_completed_id);
      priv->timeline_completed_id = 0;
    }

  /* then we need to disconnect the signal handler from the old alpha */
  if (priv->alpha_notify_id != 0)
    {
      g_signal_handler_disconnect (priv->alpha, priv->alpha_notify_id);
      priv->alpha_notify_id = 0;
    }

  if (priv->alpha != NULL)
    {
      /* this will take care of any reference we hold on the timeline */
      g_object_unref (priv->alpha);
      priv->alpha = NULL;
    }

  if (alpha == NULL)
    goto out;

  priv->alpha = g_object_ref_sink (alpha);
  priv->alpha_notify_id =
    g_signal_connect (priv->alpha, "notify::alpha",
                      G_CALLBACK (on_alpha_notify),
                      animation);

  /* if the alpha has a timeline then we use it, otherwise we create one */
  timeline = clutter_alpha_get_timeline (priv->alpha);
  if (timeline != NULL)
    {
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
    timeline = clutter_animation_get_timeline_internal (animation);

out:
  /* emit all relevant notifications */
  g_object_notify (G_OBJECT (animation), "mode");
  g_object_notify (G_OBJECT (animation), "duration");
  g_object_notify (G_OBJECT (animation), "loop");
  g_object_notify (G_OBJECT (animation), "alpha");
  g_object_notify (G_OBJECT (animation), "timeline");

  g_object_thaw_notify (G_OBJECT (animation));
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

  return clutter_animation_get_alpha_internal (animation);
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
                                  GParamSpec       *pspec,
                                  gboolean          is_fixed)
{
  ClutterAnimationPrivate *priv = animation->priv;
  GValue real_value = { 0, };

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
  gboolean is_fixed = FALSE;

  klass = G_OBJECT_GET_CLASS (priv->object);

  for (i = 0; i < n_properties; i++)
    {
      const gchar *property_name = properties[i];
      GParamSpec *pspec;

      if (g_str_has_prefix (property_name, "fixed::"))
        {
          property_name += 7; /* strlen("fixed::") */
          is_fixed = TRUE;
        }

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
                                        pspec,
                                        is_fixed);
    }
}

static const struct
{
  const gchar *name;
  GConnectFlags flags;
} signal_prefixes[] =
  {
    { "::", 0 },
    { "-swapped::", G_CONNECT_SWAPPED },
    { "-after::", G_CONNECT_AFTER },
    { "-swapped-after::", G_CONNECT_SWAPPED | G_CONNECT_AFTER }
  };

static gboolean
clutter_animation_has_signal_prefix (const gchar *property_name,
                                     GConnectFlags *flags,
                                     int *offset)
{
  int i;

  if (!g_str_has_prefix (property_name, "signal"))
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (signal_prefixes); i++)
    if (g_str_has_prefix (property_name + 6, signal_prefixes[i].name))
      {
        *offset = strlen (signal_prefixes[i].name) + 6;
        *flags = signal_prefixes[i].flags;
        return TRUE;
      }

  return FALSE;
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
      gboolean is_fixed = FALSE;
      GConnectFlags flags;
      int offset;

      if (clutter_animation_has_signal_prefix (property_name,
                                               &flags,
                                               &offset))
        {
          const gchar *signal_name = property_name + offset;
          GCallback callback = va_arg (var_args, GCallback);
          gpointer  userdata = va_arg (var_args, gpointer);

          g_signal_connect_data (animation, signal_name,
                                 callback, userdata,
                                 NULL, flags);
        }
      else
        {
          if (g_str_has_prefix (property_name, "fixed::"))
            {
              property_name += 7; /* strlen("fixed::") */
              is_fixed = TRUE;
            }

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
                                            pspec,
                                            is_fixed);
        }

      property_name = va_arg (var_args, gchar*);
    }
}

static ClutterAnimation *
animation_create_for_actor (ClutterActor *actor)
{
  ClutterAnimation *animation;
  GObject *object = G_OBJECT (actor);

  animation = g_object_get_qdata (object, quark_object_animation);
  if (animation == NULL)
    {
      animation = clutter_animation_new ();
      clutter_animation_set_object (animation, object);
      g_object_set_qdata (object, quark_object_animation, animation);

      CLUTTER_NOTE (ANIMATION,
                    "Created new Animation [%p] for actor [%p]",
                    animation,
                    actor);
    }
  else
    {
      CLUTTER_NOTE (ANIMATION,
                    "Reusing Animation [%p] for actor [%p]",
                    animation,
                    actor);
    }

  return animation;
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

  animation = animation_create_for_actor (actor);
  clutter_animation_set_alpha (animation, alpha);

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

  animation = animation_create_for_actor (actor);
  clutter_animation_set_mode (animation, mode);
  clutter_animation_set_timeline (animation, timeline);

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
 *                          "width", 100.0,
 *                          "height", 100.0,
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
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN_SINE, 100,
 *                          "rotation-angle-z", 360.0,
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
 * If a name argument starts with "signal::", "signal-after::" or
 * "signal-swapped::" the two following arguments are used as callback
 * function and data for a signal handler installed on the
 * #ClutterAnimation object for the specified signal name, for
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
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 100,
 *                          "opacity", 0,
 *                          "signal::completed", on_animation_completed, actor,
 *                          NULL);
 * ]|
 *
 * The "signal::" modifier is the equivalent of using g_signal_connect();
 * the "signal-after::" modifier is the equivalent of using
 * g_signal_connect_after(); the "signal-swapped::" modifier is the equivalent
 * of using g_signal_connect_swapped(). The clutter_actor_animate() function
 * will not keep track of multiple connections to the same signal, so it is
 * your responsability to avoid them when calling clutter_actor_animate()
 * multiple times on the same actor.
 *
 * Calling this function on an actor that is already being animated
 * will cause the current animation to change with the new final values,
 * the new easing mode and the new duration - that is, this code:
 *
 * |[
 *   clutter_actor_animate (actor, CLUTTER_LINEAR, 250,
 *                          "width", 100.0,
 *                          "height", 100.0,
 *                          NULL);
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 500,
 *                          "x", 100.0,
 *                          "y", 100.0,
 *                          "width", 200.0,
 *                          NULL);
 * ]|
 *
 * is the equivalent of:
 *
 * |[
 *   clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 500,
 *                          "x", 100.0,
 *                          "y", 100.0,
 *                          "width", 200.0,
 *                          "height", 100.0,
 *                          NULL);
 * ]|
 *
 * <note>Unless the animation is looping, the #ClutterAnimation created by
 * clutter_actor_animate() will become invalid as soon as it is
 * complete.</note>
 *
 * Since the created #ClutterAnimation instance attached to @actor
 * is guaranteed to be valid throughout the #ClutterAnimation::completed
 * signal emission chain, you will not be able to create a new animation
 * using clutter_actor_animate() on the same @actor from within the
 * #ClutterAnimation::completed signal handler unless you use
 * g_signal_connect_after() to connect the callback function, for instance:
 *
 * |[
 *   static void
 *   on_animation_completed (ClutterAnimation *animation,
 *                           ClutterActor     *actor)
 *   {
 *     clutter_actor_animate (actor, CLUTTER_EASE_OUT_CUBIC, 250,
 *                            "x", 500.0,
 *                            "y", 500.0,
 *                            NULL);
 *   }
 *
 *     ...
 *     animation = clutter_actor_animate (actor, CLUTTER_EASE_IN_CUBIC, 250,
 *                                        "x", 100.0,
 *                                        "y", 100.0,
 *                                        NULL);
 *     g_signal_connect (animation, "completed",
 *                       G_CALLBACK (on_animation_completed),
 *                       actor);
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

  animation = animation_create_for_actor (actor);
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
 * @properties: (array length=n_properties) (element-type utf8): a vector
 *    containing the property names to set
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

  animation = animation_create_for_actor (actor);
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
 * @properties: (array length=n_properties) (element-type utf8): a vector
 *    containing the property names to set
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

  animation = animation_create_for_actor (actor);
  clutter_animation_set_mode (animation, mode);
  clutter_animation_set_timeline (animation, timeline);
  clutter_animation_setupv (animation, n_properties, properties, values);
  clutter_animation_start (animation);

  return animation;
}

/**
 * clutter_actor_animate_with_alphav:
 * @actor: a #ClutterActor
 * @alpha: a #ClutterAlpha
 * @n_properties: number of property names and values
 * @properties: (array length=n_properties) (element-type utf8): a vector
 *    containing the property names to set
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

  animation = animation_create_for_actor (actor);
  clutter_animation_set_alpha (animation, alpha);
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
