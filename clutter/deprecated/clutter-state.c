/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Øyvind Kolås <pippin@linux.intel.com>
 *
 * Copyright (C) 2009 Intel Corporation
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
 * SECTION:clutter-state
 * @short_description: State machine with animated transitions
 *
 * #ClutterState is an object controlling the tweening of properties on
 * multiple actors between a set of named states. #ClutterStateKey<!-- -->s
 * define how the properties are animated. If the source_state_name for a key
 * is NULL it is used for transition to the target state unless a specific key
 * exists for transitioning from the current state to the requested state.
 *
 * #ClutterState is available since Clutter 1.4.
 *
 * #ClutterState has been deprecated in Clutter 1.12.
 *
 * ## Using ClutterState
 *
 * The following example defines a "base" and a "hover" state in a
 * #ClutterState instance.
 *
 * |[<!-- language="C" -->
 * ClutterState *state = clutter_state_new ();
 * ClutterColor color = { 0, };
 *
 * // transition from any state to the "base" state
 * clutter_color_from_string (&color, "rgb(255, 0, 0)");
 * clutter_state_set (state, NULL, "base",
 *                    actor, "color", CLUTTER_LINEAR, &color,
 *                    actor, "scale-x", CLUTTER_EASE_IN_BOUNCE, 1.0,
 *                    actor, "scale-y", CLUTTER_EASE_IN_BOUNCE, 1.0,
 *                    NULL);
 *
 * // transition from the "base" state to the "hover" state
 * clutter_color_from_string (&color, "rgb(0, 0, 255)");
 * clutter_state_set (state, "base", "hover",
 *                    actor, "color", CLUTTER_LINEAR, &color,
 *                    actor, "scale-x", CLUTTER_EASE_OUT_BOUNCE, 1.7,
 *                    actor, "scale-y", CLUTTER_EASE_OUT_BOUNCE, 1.7,
 *                    NULL);
 *
 * // the default duration of any transition
 * clutter_state_set_duration (state, NULL, NULL, 500);
 *
 * // set "base" as the initial state
 * clutter_state_warp_to_state (state, "base");
 * ]|
 *
 * The actor then uses the #ClutterState to animate through the
 * two states using callbacks for the #ClutterActor::enter-event and
 * #ClutterActor::leave-event signals.
 *
 * |[<!-- language="C" -->
 * static gboolean
 * on_enter (ClutterActor *actor,
 *           ClutterEvent *event,
 *           ClutterState *state)
 * {
 *   clutter_state_set_state (state, "hover");
 *
 *   return CLUTTER_EVENT_STOP;
 * }
 *
 * static gboolean
 * on_leave (ClutterActor *actor,
 *           ClutterEvent *event,
 *           ClutterState *state)
 * {
 *   clutter_state_set_state (state, "base");
 *
 *   return CLUTTER_EVENT_STOP;
 * }
 *
 * ## ClutterState description for ClutterScript
 *
 * #ClutterState defines a custom `transitions` JSON object member which
 * allows describing the states.
 *
 * The `transitions` property has the following syntax:
 *
 * |[
 * {
 *   "transitions" : [
 *     {
 *       "source" : "source-state",
 *       "target" : "target-state",
 *       "duration" : milliseconds,
 *       "keys" : [
 *         [
 *           "object-id",
 *           "property-name",
 *           "easing-mode",
 *           "final-value",
 *         ],
 *         [
 *           "object-id",
 *           "property-name",
 *           "easing-mode",
 *           "final-value",
 *           pre-delay,
 *           post-delay;
 *         ],
 *         ...
 *       ]
 *     },
 *     {
 *       "source" : "source-state",
 *       "target" : "target-state",
 *       "duration" : milliseconds,
 *       "animator" : "animator-definition"
 *     },
 *     ...
 *   ]
 * }
 * ]|
 *
 * Each element of the transitions array follows the same rules and order
 * as clutter_state_set_key() function arguments.
 *
 * The source and target values control the source and target state of the
 * transition. The key and animator properties are mutually exclusive.
 *
 * The pre-delay and post-delay values are optional.
 *
 * The example below is a translation into a #ClutterScript definition of
 * the code in the #ClutterState example above.
 *
 * |[
 * {
 *   "id" : "button-state",
 *   "type" : "ClutterState",
 *   "duration" : 500,
 *   "transitions" : [
 *     {
 *       "source" : "*",
 *       "target" : "base",
 *       "keys" : [
 *         [ "button", "color", "linear", "rgb(255, 0, 0)" ],
 *         [ "button", "scale-x", "easeInBounce", 1.0 ],
 *         [ "button", "scale-y", "easeInBounce", 1.0 ]
 *       ]
 *     },
 *     {
 *       "source" : "base",
 *       "target" : "hover",
 *       "keys" : [
 *         [ "button", "color", "linear", "rgb(0, 0, 255)" ],
 *         [ "button", "scale-x", "easeOutBounce", 1.7 ],
 *         [ "button", "scale-y", "easeOutBounce", 1.7 ]
 *       ]
 *     }
 *   ]
 * }
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gobject/gvaluecollector.h>
#include <string.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "clutter-state.h"

#include "clutter-alpha.h"
#include "clutter-animatable.h"
#include "clutter-animator.h"
#include "clutter-enum-types.h"
#include "clutter-interval.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-scriptable.h"
#include "clutter-script-private.h"

typedef struct StateAnimator {
  const gchar     *source_state_name; /* interned string identifying entry */
  ClutterAnimator *animator;          /* pointer to animator itself */
} StateAnimator;

typedef struct State
{ 
  const gchar  *name;          /* interned string for this state name */
  GHashTable   *durations;     /* durations for transitions from various state
                                  names */
  GList        *keys;          /* list of all keys pertaining to transitions
                                  from other states to this one */
  GArray       *animators;     /* list of animators for transitioning from
                                * specific source states */
  ClutterState *clutter_state; /* the ClutterState object this state belongs to
                                */
} State;

struct _ClutterStatePrivate
{
  GHashTable      *states;            /* contains state objects */
  guint            duration;          /* global fallback duration */
  ClutterTimeline *timeline;          /* The timeline used for doing the
                                         progress */
  ClutterTimeline *slave_timeline;    /* a slave timeline used to compute
                                         alphas */
  const gchar     *source_state_name; /* current source state */
  State           *source_state;      /* current source_state */
  const gchar     *target_state_name; /* current target state */
  State           *target_state;      /* target state name */
  ClutterAnimator *current_animator;  /* !NULL if the current transition is
                                         overriden by an animator */
};

#define SLAVE_TIMELINE_LENGTH 10000

/*
 * ClutterStateKey:
 *
 * An opaque data structure with accessor functions.
 *
 */
typedef struct _ClutterStateKey
{ 
  GObject         *object;       /* an Gobject */
  const gchar     *property_name;/* the name of a property */
  gulong           mode;         /* alpha to use */
  GValue           value;        /* target value */
  gdouble          pre_delay;    /* fraction of duration to delay before
                                    starting */
  gdouble          pre_pre_delay;/* fraction of duration to add to
                                    pre_delay. This is used to set keys
                                    during transitions. */
  gdouble          post_delay;   /* fraction of duration to be done in */

  State           *source_state; /* source state */
  State           *target_state; /* target state */

  ClutterAlpha    *alpha;        /* The alpha this key uses for interpolation */
  ClutterInterval *interval;     /* The interval this key uses for
                                    interpolation */

  guint            is_animatable : 1;
  guint            is_inert : 1; /* set if the key is being destroyed due to
                                    weak reference */
  gint             ref_count;    /* reference count for boxed life time */
} _ClutterStateKey;

enum
{
  PROP_0,
  PROP_DURATION,
  PROP_STATE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

enum
{
  COMPLETED,
  LAST_SIGNAL
};

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

static guint state_signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE_WITH_CODE (ClutterState, clutter_state, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (ClutterState)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_iface_init))

/**
 * clutter_state_new:
 *
 * Creates a new #ClutterState
 *
 * Return value: the newly create #ClutterState instance
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
ClutterState *
clutter_state_new (void)
{
  return g_object_new (CLUTTER_TYPE_STATE, NULL);
}

static gint
sort_props_func (gconstpointer a,
                 gconstpointer b)
{
  const ClutterStateKey *pa = a;
  const ClutterStateKey *pb = b;

  if (pa->object == pb->object)
    {
      gint propnamediff = pa->property_name-pb->property_name;
      if (propnamediff == 0)
        return pb->source_state - pa->source_state;

      return propnamediff;
    }

  return pa->object - pb->object;
}

static State * clutter_state_fetch_state (ClutterState *state,
                                          const gchar  *state_name,
                                          gboolean      force_creation);
static void object_disappeared (gpointer data,
                                GObject *where_the_object_was);

static ClutterStateKey *
clutter_state_key_new (State       *state,
                       GObject     *object,
                       const gchar *property_name,
                       GParamSpec  *pspec,
                       guint        mode)
{
  ClutterStatePrivate *priv = state->clutter_state->priv;
  ClutterStateKey *state_key;
  GValue value = G_VALUE_INIT;

  state_key = g_slice_new0 (ClutterStateKey);

  state_key->target_state = state;
  state_key->object = object;
  state_key->property_name = g_intern_string (property_name);
  state_key->mode = mode;
  state_key->is_animatable = CLUTTER_IS_ANIMATABLE (object);

  state_key->alpha = clutter_alpha_new ();
  g_object_ref_sink (state_key->alpha);
  clutter_alpha_set_mode (state_key->alpha, mode);
  clutter_alpha_set_timeline (state_key->alpha, priv->slave_timeline);

  state_key->interval =
    g_object_new (CLUTTER_TYPE_INTERVAL,
                  "value-type", G_PARAM_SPEC_VALUE_TYPE (pspec),
                  NULL);
  g_object_ref_sink (state_key->interval);

  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  clutter_interval_set_initial_value (state_key->interval, &value);
  clutter_interval_set_final_value (state_key->interval, &value);
  g_value_unset (&value);

  g_object_weak_ref (object, object_disappeared,
                     state_key->target_state->clutter_state);

  return state_key;
}

static void
clutter_state_key_free (gpointer clutter_state_key)
{
  ClutterStateKey *key = clutter_state_key;

  if (key == NULL)
    return;

  key->ref_count -= 1;

  if (key->ref_count > 0)
    return;

  if (!key->is_inert)
    {
      g_object_weak_unref (key->object,
                           object_disappeared,
                           key->target_state->clutter_state);
    }

  g_value_unset (&key->value);

  g_object_unref (key->alpha);
  g_object_unref (key->interval);

  g_slice_free (ClutterStateKey, key);
}


static inline void
clutter_state_remove_key_internal (ClutterState *this,
                                   const gchar  *source_state_name,
                                   const gchar  *target_state_name,
                                   GObject      *object,
                                   const gchar  *property_name,
                                   gboolean      is_inert)
{
  GList *s, *state_list;
  State *source_state = NULL;

  source_state_name = g_intern_string (source_state_name);
  target_state_name = g_intern_string (target_state_name);
  property_name = g_intern_string (property_name);

  if (source_state_name)
    source_state = clutter_state_fetch_state (this, source_state_name, FALSE);

again_from_start:

  if (target_state_name != NULL)
    state_list = g_list_append (NULL, (gpointer) target_state_name);
  else
    state_list = clutter_state_get_states (this);

  for (s = state_list; s != NULL; s = s->next)
    {
      State *target_state;
      target_state = clutter_state_fetch_state (this, s->data, FALSE);
      /* Go through each TargetState */
      if (target_state)
        {
          GList *k = target_state->keys;

	  /* Note the safe while() loop, because we modify the list inline */
          while (k != NULL)
            {
              ClutterStateKey *key = k->data;

	      k = k->next;

              /* Check if each key matches query */
              if (   (object == NULL        || (object == key->object))
                  && (source_state == NULL  || (source_state == key->source_state))
                  && (property_name == NULL || ((property_name == key->property_name))))
                {
                  /* Remove matching key */
                  target_state->keys = g_list_remove (target_state->keys, key);
                  key->is_inert = is_inert;
                  clutter_state_key_free (key);

                  /* no more keys with transitions to this target_state*/
                  if (target_state->keys == NULL)
                    {
                      /* If this state is the current state, unset the state */
                      if (target_state == this->priv->target_state)
                        clutter_state_set_state (this, NULL);

                      /* remove any keys that exist that uses this state as a source */
		      clutter_state_remove_key_internal (this, s->data, NULL, NULL, NULL, is_inert);

                      g_hash_table_remove (this->priv->states, s->data);
                      goto again_from_start; /* we have just freed State *target_state, so
                                                need to restart removal */
                    }
                }
            }
        }
    }

  g_list_free (state_list);
}

static void
object_disappeared (gpointer data,
                    GObject *where_the_object_was)
{
  clutter_state_remove_key_internal (data, NULL, NULL,
                                     (gpointer) where_the_object_was,
                                     NULL,
                                     TRUE);
}


static void
state_free (gpointer data)
{
  State *state = data;

  for (; state->keys;
       state->keys = g_list_remove (state->keys, state->keys->data))
    clutter_state_key_free (state->keys->data);

  g_array_free (state->animators, TRUE);
  g_hash_table_destroy (state->durations);
  g_free (state);
}

static State *
state_new (ClutterState *clutter_state,
           const gchar  *name)
{
  State *state;

  state = g_new0 (State, 1);
  state->clutter_state = clutter_state;
  state->name = name;
  state->animators = g_array_new (TRUE, TRUE, sizeof (StateAnimator));
  state->durations = g_hash_table_new (g_direct_hash, g_direct_equal);

  return state;
}

static void 
clutter_state_finalize (GObject *object)
{
  ClutterStatePrivate *priv = CLUTTER_STATE (object)->priv;

  g_hash_table_destroy (priv->states);

  g_object_unref (priv->timeline);
  g_object_unref (priv->slave_timeline);

  G_OBJECT_CLASS (clutter_state_parent_class)->finalize (object);
}

static void
clutter_state_completed (ClutterTimeline *timeline,
                         ClutterState    *state)
{
  ClutterStatePrivate *priv = state->priv;

  if (priv->current_animator)
    {
      clutter_animator_set_timeline (priv->current_animator, NULL);
      priv->current_animator = NULL;
    }

  g_signal_emit (state, state_signals[COMPLETED], 0);
}

static void
clutter_state_new_frame (ClutterTimeline *timeline,
                         gint             msecs,
                         ClutterState    *state)
{
  ClutterStatePrivate *priv = state->priv;
  GList *k;
  gdouble progress;
  const gchar *curprop = NULL;
  GObject *curobj = NULL;
  gboolean found_specific = FALSE;

  if (priv->current_animator)
    return;

  progress = clutter_timeline_get_progress (timeline);

  for (k = priv->target_state->keys; k; k = k->next)
    {
      ClutterStateKey *key = k->data;
      gdouble sub_progress;

      if ((curprop && !(curprop == key->property_name)) ||
          key->object != curobj)
        {
          curprop = key->property_name;
          curobj = key->object;
          found_specific = FALSE;
        }

      if (!found_specific)
        {
          if (key->source_state != NULL &&
              key->source_state->name != NULL &&
              priv->source_state_name != NULL && 
              g_str_equal (priv->source_state_name, key->source_state->name))
            {
              found_specific = TRUE;
            }

          if (found_specific || key->source_state == NULL)
            {
              gdouble pre_delay = key->pre_delay + key->pre_pre_delay;

              sub_progress = (progress - pre_delay)
                           / (1.0 - (pre_delay + key->post_delay));

              if (sub_progress >= 0.0)
                {
                  if (sub_progress >= 1.0)
                    sub_progress = 1.0;

                  clutter_timeline_advance (priv->slave_timeline,
                                            sub_progress * SLAVE_TIMELINE_LENGTH);
                  sub_progress = clutter_alpha_get_alpha (key->alpha);

                  if (key->is_animatable)
                    {
                      ClutterAnimatable *animatable;
                      GValue value = G_VALUE_INIT;
                      gboolean res;

                      animatable = CLUTTER_ANIMATABLE (key->object);

                      g_value_init (&value, clutter_state_key_get_property_type (key));

                      res =
                        clutter_animatable_interpolate_value (animatable,
                                                              key->property_name,
                                                              key->interval,
                                                              sub_progress,
                                                              &value);

                      if (res)
                        clutter_animatable_set_final_state (animatable,
                                                            key->property_name,
                                                            &value);

                      g_value_unset (&value);
                    }
                  else
                    {
                      const GValue *value;

                      value = clutter_interval_compute (key->interval, sub_progress);
                      if (value != NULL)
                        g_object_set_property (key->object, key->property_name, value);
                    }
                }

              /* XXX: should the target value of the default destination be
               * used even when found a specific source_state key?
               */
            }
        }
    }
}


static ClutterTimeline *
clutter_state_change (ClutterState *state,
                      const gchar  *target_state_name,
                      gboolean      animate)
{
  ClutterStatePrivate *priv;
  ClutterAnimator     *animator;
  State               *new_state;
  guint                duration;
  GList               *k;

  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);

  priv = state->priv;

  /* If we've been asked to change state to NULL, reset the
   * ClutterState to its initial state, but leave the keys
   * alone.
   */
  if (!target_state_name)
    {
      if (!priv->target_state)
        return NULL;

      priv->source_state_name = priv->target_state_name = NULL;
      priv->source_state = priv->target_state = NULL;

      clutter_timeline_stop (priv->timeline);
      clutter_timeline_rewind (priv->timeline);

      if (priv->current_animator)
        {
          clutter_animator_set_timeline (priv->current_animator, NULL);
          priv->current_animator = NULL;
        }

      return NULL;
    }

  target_state_name = g_intern_string (target_state_name);

  if (target_state_name == priv->target_state_name)
    {
      /* Avoid transitioning if the desired state is already current,
       * unless we're warping to it and the state transition is in
       * progress (in that case, immediately warp to the state).
       */
      if (!clutter_timeline_is_playing (priv->timeline) || animate)
        return priv->timeline;
    }

  if (priv->current_animator != NULL)
    {
      clutter_animator_set_timeline (priv->current_animator, NULL);
      priv->current_animator = NULL;
    }

  priv->source_state_name = priv->target_state_name;
  priv->target_state_name = target_state_name;

  g_object_notify_by_pspec (G_OBJECT (state), obj_props[PROP_STATE]);

  duration = clutter_state_get_duration (state,
                                         priv->source_state_name,
                                         priv->target_state_name);
  clutter_timeline_set_duration (priv->timeline, duration);

  new_state = clutter_state_fetch_state (state, target_state_name, FALSE);
  if (new_state == NULL)
    {
      g_warning ("State '%s' not found", target_state_name);

      return NULL;
    }

  animator = clutter_state_get_animator (state,
                                         priv->source_state_name,
                                         priv->target_state_name);
  priv->target_state = new_state;

  if (animator == NULL && new_state->keys == NULL)
    animator = clutter_state_get_animator (state, NULL,
                                           priv->target_state_name);

  if (animator != NULL)
    {
      /* we've got an animator overriding the tweened animation */
      priv->current_animator = animator;
      clutter_animator_set_timeline (animator, priv->timeline);
    }
  else
    {
      for (k = new_state->keys; k != NULL; k = k->next)
        {
          ClutterStateKey *key = k->data;
          GValue initial = G_VALUE_INIT;

          /* Reset the pre-pre-delay - this is only used for setting keys
           * during transitions.
           */
          key->pre_pre_delay = 0;

          g_value_init (&initial, clutter_interval_get_value_type (key->interval));

          if (key->is_animatable)
            {
              ClutterAnimatable *animatable;

              animatable = CLUTTER_ANIMATABLE (key->object);
              clutter_animatable_get_initial_state (animatable,
                                                    key->property_name,
                                                    &initial);
            }
          else
            g_object_get_property (key->object, key->property_name, &initial);

          if (clutter_alpha_get_mode (key->alpha) != key->mode)
            clutter_alpha_set_mode (key->alpha, key->mode);

          clutter_interval_set_initial_value (key->interval, &initial);
          clutter_interval_set_final_value (key->interval, &key->value);

          g_value_unset (&initial);
        }
    }

  if (!animate)
    {
      clutter_timeline_stop (priv->timeline);
      clutter_timeline_advance (priv->timeline, duration);

      /* emit signals, to change properties, and indicate that the
       * state change is complete */
      g_signal_emit_by_name (priv->timeline, "new-frame",
                             GINT_TO_POINTER (duration),
                             NULL);
      g_signal_emit_by_name (priv->timeline, "completed", NULL);
    }
  else
    {
      clutter_timeline_stop (priv->timeline);
      clutter_timeline_rewind (priv->timeline);
      clutter_timeline_start (priv->timeline);
    }

  return priv->timeline;
}

/**
 * clutter_state_set_state:
 * @state: a #ClutterState
 * @target_state_name: the state to transition to
 *
 * Change the current state of #ClutterState to @target_state_name.
 *
 * The state will animate during its transition, see
 * #clutter_state_warp_to_state for animation-free state switching.
 *
 * Setting a %NULL state will stop the current animation and unset
 * the current state, but keys will be left intact.
 *
 * Return value: (transfer none): the #ClutterTimeline that drives the
 *   state transition. The returned timeline is owned by the #ClutterState
 *   and it should not be unreferenced
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
ClutterTimeline *
clutter_state_set_state (ClutterState *state,
                         const gchar  *target_state_name)
{
  return clutter_state_change (state, target_state_name, TRUE);
}

/**
 * clutter_state_warp_to_state:
 * @state: a #ClutterState
 * @target_state_name: the state to transition to
 *
 * Change to the specified target state immediately with no animation.
 *
 * See clutter_state_set_state().
 *
 * Return value: (transfer none): the #ClutterTimeline that drives the
 *   state transition. The returned timeline is owned by the #ClutterState
 *   and it should not be unreferenced
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
ClutterTimeline *
clutter_state_warp_to_state (ClutterState *state,
                             const gchar  *target_state_name)
{
  return clutter_state_change (state, target_state_name, FALSE);
}

static GParamSpec *
get_property_from_object (GObject     *gobject,
                          const gchar *property_name)
{
  GParamSpec *pspec;

  if (CLUTTER_IS_ANIMATABLE (gobject))
    {
      ClutterAnimatable *animatable = CLUTTER_ANIMATABLE (gobject);

      pspec = clutter_animatable_find_property (animatable, property_name);
    }
  else
    {
      GObjectClass *klass = G_OBJECT_GET_CLASS (gobject);

      pspec = g_object_class_find_property (klass, property_name);
    }

  if (pspec == NULL)
    {
      g_warning ("Cannot bind property '%s': objects of type '%s' "
                 "do not have this property",
                 property_name,
                 G_OBJECT_TYPE_NAME (gobject));
      return NULL;
    }

  if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("Cannot bind property '%s' of object of type '%s': "
                 "the property is not writable",
                 property_name,
                 G_OBJECT_TYPE_NAME (gobject));
      return NULL;
    }

  if (!(pspec->flags & G_PARAM_READABLE))
    {
      g_warning ("Cannot bind property '%s' of object of type '%s': "
                 "the property is not readable",
                 property_name,
                 G_OBJECT_TYPE_NAME (gobject));
      return NULL;
    }

  if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_warning ("Cannot bind property '%s' of object of type '%s': "
                 "the property is set as constructor-only",
                 property_name,
                 G_OBJECT_TYPE_NAME (gobject));
      return NULL;
    }

  return pspec;
}

/**
 * clutter_state_set:
 * @state: a #ClutterState instance.
 * @source_state_name: (allow-none): the name of the source state keys are being added for
 * @target_state_name: the name of the target state keys are being added for
 * @first_object: a #GObject
 * @first_property_name: a property of @first_object to specify a key for
 * @first_mode: the id of the alpha function to use
 * @...: the value @first_property_name should have in @target_state_name,
 *   followed by object, property name, mode, value tuples, terminated
 *   by %NULL
 *
 * Adds multiple keys to a named state of a #ClutterState instance, specifying
 * the easing mode and value a given property of an object should have at a
 * given progress of the animation.
 *
 * The mode specified is the easing mode used when going to from the previous
 * key to the specified key.
 *
 * For instance, the code below:
 *
 * |[
 *   clutter_state_set (state, NULL, "hover",
 *                      button, "opacity", CLUTTER_LINEAR, 255,
 *                      button, "scale-x", CLUTTER_EASE_OUT_CUBIC, 1.2,
 *                      button, "scale-y", CLUTTER_EASE_OUT_CUBIC, 1.2,
 *                      NULL);
 * ]|
 *
 * will create a transition from any state (a @source_state_name or NULL is
 * treated as a wildcard) and a state named "hover"; the
 * button object will have the #ClutterActor:opacity
 * property animated to a value of 255 using %CLUTTER_LINEAR as the animation
 * mode, and the #ClutterActor:scale-x and #ClutterActor:scale-y properties
 * animated to a value of 1.2 using %CLUTTER_EASE_OUT_CUBIC as the animation
 * mode. To change the state (and start the transition) you can use the
 * clutter_state_set_state() function:
 *
 * |[
 *   clutter_state_set_state (state, "hover");
 * ]|
 *
 * If a given object, state_name, property tuple already exist in the
 * #ClutterState instance, then the mode and value will be replaced with
 * the new specified values.
 *
 * If a property name is prefixed with "delayed::" two additional
 * arguments per key are expected: a value relative to the full state time
 * to pause before transitioning and a similar value to pause after
 * transitioning, e.g.:
 *
 * |[
 *   clutter_state_set (state, "hover", "toggled",
 *                      button, "delayed::scale-x", CLUTTER_LINEAR, 1.0, 0.2, 0.2,
 *                      button, "delayed::scale-y", CLUTTER_LINEAR, 1.0, 0.2, 0.2,
 *                      NULL);
 * ]|
 *
 * will pause for 20% of the duration of the transition before animating,
 * and 20% of the duration after animating.
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
void
clutter_state_set (ClutterState *state,
                   const gchar  *source_state_name,
                   const gchar  *target_state_name,
                   gpointer      first_object,
                   const gchar  *first_property_name,
                   gulong        first_mode,
                   ...)
{
  gpointer object;
  const gchar *property_name;
  gulong mode;
  va_list args;

  g_return_if_fail (CLUTTER_IS_STATE (state));

  object = first_object;

  property_name = first_property_name;
  mode = first_mode;

  g_return_if_fail (G_IS_OBJECT (first_object));
  g_return_if_fail (first_property_name);

  va_start (args, first_mode);

  while (object != NULL)
    {
      GParamSpec *pspec;
      GValue value = G_VALUE_INIT;
      gchar *error = NULL;
      gboolean is_delayed = FALSE;

      if (g_str_has_prefix (property_name, "delayed::"))
        {
          property_name = strstr (property_name, "::") + 2;
          is_delayed = TRUE;
        }

      pspec = get_property_from_object (object, property_name);
      if (pspec == NULL)
        break;

      G_VALUE_COLLECT_INIT (&value, G_PARAM_SPEC_VALUE_TYPE (pspec),
                            args, 0,
                            &error);

      if (error != NULL)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      if (is_delayed)
        {
          gdouble pre_delay = va_arg (args, gdouble);
          gdouble post_delay = va_arg (args, gdouble);

          clutter_state_set_key (state,
                                 source_state_name,
                                 target_state_name,
                                 object,
                                 property_name,
                                 mode,
                                 &value,
                                 pre_delay,
                                 post_delay);
        }
      else
        {
          clutter_state_set_key (state,
                                 source_state_name,
                                 target_state_name,
                                 object,
                                 property_name,
                                 mode,
                                 &value,
                                 0.0,
                                 0.0);
        }

      g_value_unset (&value);

      object = va_arg (args, gpointer);
      if (object != NULL)
        {
          property_name = va_arg (args, gchar*);
          mode = va_arg (args, gulong);
        }
    }

  va_end (args);
}

static void
clutter_state_set_key_internal (ClutterState    *state,
                                ClutterStateKey *key)
{
  ClutterStatePrivate *priv = state->priv;
  State *target_state = key->target_state;
  GList *old_item = NULL;

  if ((old_item = g_list_find_custom (target_state->keys,
                                      key,
                                      sort_props_func)))
    {
      ClutterStateKey *old_key = old_item->data;

      target_state->keys = g_list_remove (target_state->keys, old_key);
      clutter_state_key_free (old_key);
    }

  target_state->keys = g_list_insert_sorted (target_state->keys,
                                             key,
                                             sort_props_func);

  /* If the current target state is modified, we have some work to do.
   *
   * If the animation is running, we add a key to the current animation
   * with a delay of the current duration so that the new animation will
   * animate into place.
   *
   * If the animation isn't running, but the state is set, we immediately
   * warp to that state.
   */
  if (key->target_state == priv->target_state)
    {
      if (!clutter_timeline_is_playing (priv->timeline))
        {
          /* We can warp to the state by setting a NULL state, then setting
           * the target state again.
           */
          clutter_state_change (state, NULL, FALSE);
          clutter_state_change (state, target_state->name, FALSE);
        }
      else
        {
          /* Set the ClutterInterval associated with the state */
          GValue initial = G_VALUE_INIT;
          gdouble progress = clutter_timeline_get_progress (priv->timeline);

          g_value_init (&initial,
                        clutter_interval_get_value_type (key->interval));

          if (key->is_animatable)
            {
              ClutterAnimatable *animatable;

              animatable = CLUTTER_ANIMATABLE (key->object);
              clutter_animatable_get_initial_state (animatable,
                                                    key->property_name,
                                                    &initial);
            }
          else
            g_object_get_property (key->object, key->property_name, &initial);

          if (clutter_alpha_get_mode (key->alpha) != key->mode)
            clutter_alpha_set_mode (key->alpha, key->mode);

          clutter_interval_set_initial_value (key->interval, &initial);
          clutter_interval_set_final_value (key->interval, &key->value);

          g_value_unset (&initial);

          /* Set the delay as if the interval had just begun */
          if (progress > key->pre_delay)
            key->pre_pre_delay = MIN (progress - key->pre_delay,
                                      1.0 - key->post_delay);
        }
    }
}

/*
 * clutter_state_fetch_state:
 * @state: a #ClutterState
 * @state_name: the name of the state to be retrieved
 * @create: %TRUE if the state should be instantiated if not found
 *
 * Retrieves the #State structure for @state_name inside the given
 * #ClutterState instance
 *
 * If @state_name is %NULL and @create is %TRUE then NULL will
 * be returned.
 *
 * Return value: a #State structure for the given name, or %NULL
 */
static State *
clutter_state_fetch_state (ClutterState *state,
                           const gchar  *state_name,
                           gboolean      create)
{
  ClutterStatePrivate *priv = state->priv;
  State *retval;

  if (state_name == NULL)
    {
      return NULL;
    }
  else
    state_name = g_intern_string (state_name);

  retval = g_hash_table_lookup (priv->states, state_name);
  if (retval == NULL && create)
    {
      retval = state_new (state, state_name);
      g_hash_table_insert (priv->states, (gpointer) state_name, retval);
    }

  return retval;
}

/**
 * clutter_state_set_key:
 * @state: a #ClutterState instance.
 * @source_state_name: (allow-none): the source transition to specify
 *   transition for, or %NULL to specify the default fallback when a
 *   more specific source state doesn't exist.
 * @target_state_name: the name of the transition to set a key value for.
 * @object: the #GObject to set a key for
 * @property_name: the property to set a key for
 * @mode: the id of the alpha function to use
 * @value: the value for property_name of object in state_name
 * @pre_delay: relative time of the transition to be idle in the beginning
 *   of the transition
 * @post_delay: relative time of the transition to be idle in the end of
 *   the transition
 *
 * Sets one specific end key for a state name, @object, @property_name
 * combination.
 *
 * Return value: (transfer none): the #ClutterState instance, allowing
 *   chaining of multiple calls
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
ClutterState *
clutter_state_set_key (ClutterState  *state,
                       const gchar   *source_state_name,
                       const gchar   *target_state_name,
                       GObject       *object,
                       const gchar   *property_name,
                       guint          mode,
                       const GValue  *value,
                       gdouble        pre_delay,
                       gdouble        post_delay)
{
  GParamSpec *pspec;
  ClutterStateKey *state_key;
  State *source_state = NULL;
  State *target_state;

  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (property_name, NULL);
  g_return_val_if_fail (value, NULL);

  pspec = get_property_from_object (object, property_name);
  if (pspec == NULL)
    return state;

  source_state = clutter_state_fetch_state (state, source_state_name, TRUE);
  target_state = clutter_state_fetch_state (state, target_state_name, TRUE);

  property_name = g_intern_string (property_name);
  state_key = clutter_state_key_new (target_state,
                                     object, property_name, pspec,
                                     mode);

  state_key->source_state = source_state;
  state_key->pre_delay = pre_delay;
  state_key->post_delay = post_delay;

  g_value_init (&state_key->value, G_VALUE_TYPE (value));
  g_value_copy (value, &state_key->value);

  clutter_state_set_key_internal (state, state_key);

  return state;
}

/**
 * clutter_state_get_states:
 * @state: a #ClutterState instance.
 *
 * Gets a list of all the state names managed by this #ClutterState.
 *
 * Return value: (transfer container) (element-type utf8): a newly allocated
 *   #GList of state names. The contents of the returned #GList are owned
 *   by the #ClutterState and should not be modified or freed. Use
 *   g_list_free() to free the resources allocated by the returned list when
 *   done using it
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
GList *
clutter_state_get_states (ClutterState *state)
{
  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);
  return g_hash_table_get_keys (state->priv->states);
}

/**
 * clutter_state_get_keys:
 * @state: a #ClutterState instance.
 * @source_state_name: (allow-none): the source transition name to query,
 *   or %NULL for all source states
 * @target_state_name: (allow-none): the target transition name to query,
 *   or %NULL for all target states
 * @object: (allow-none): the specific object instance to list keys for,
 *   or %NULL for all managed objects
 * @property_name: (allow-none): the property name to search for, or %NULL
 *   for all properties.
 *
 * Returns a list of pointers to opaque structures with accessor functions
 * that describe the keys added to an animator.
 *
 * Return value: (transfer container) (element-type Clutter.StateKey): a
 *   newly allocated #GList of #ClutterStateKey<!-- -->s. The contents of
 *   the returned list are owned by the #ClutterState and should not be
 *   modified or freed. Use g_list_free() to free the resources allocated
 *   by the returned list when done using it
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
GList *
clutter_state_get_keys (ClutterState *state,
                        const gchar  *source_state_name,
                        const gchar  *target_state_name,
                        GObject      *object,
                        const gchar  *property_name)
{
  GList *s, *state_list;
  GList *targets = NULL;
  State *source_state = NULL;

  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);

  source_state_name = g_intern_string (source_state_name);
  target_state_name = g_intern_string (target_state_name);
  property_name = g_intern_string (property_name);

  if (target_state_name != NULL)
    state_list = g_list_append (NULL, (gpointer) target_state_name);
  else
    state_list = clutter_state_get_states (state);

  if (source_state_name)
    source_state = clutter_state_fetch_state (state, source_state_name, FALSE);

  for (s = state_list; s != NULL; s = s->next)
    {
      State *target_state;

      target_state = clutter_state_fetch_state (state, s->data, FALSE);
      if (target_state != NULL)
        {
          GList *k;

          for (k = target_state->keys; k; k = k->next)
            {
              ClutterStateKey *key = k->data;

              if ((object == NULL || (object == key->object)) &&
                  (source_state_name == NULL ||
                   source_state == key->source_state) &&
                  (property_name == NULL ||
                   (property_name == key->property_name)))
                {
                  targets = g_list_prepend (targets, key);
                }
            }
        }
    }

  g_list_free (state_list);

  return g_list_reverse (targets);
}


/**
 * clutter_state_remove_key:
 * @state: a #ClutterState instance.
 * @source_state_name: (allow-none): the source state name to query,
 *   or %NULL for all source states
 * @target_state_name: (allow-none): the target state name to query,
 *   or %NULL for all target states
 * @object: (allow-none): the specific object instance to list keys for,
 *   or %NULL for all managed objects
 * @property_name: (allow-none): the property name to search for,
 *   or %NULL for all properties.
 *
 * Removes all keys matching the search criteria passed in arguments.
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
void
clutter_state_remove_key (ClutterState *state,
                          const gchar  *source_state_name,
                          const gchar  *target_state_name,
                          GObject      *object,
                          const gchar  *property_name)
{
  g_return_if_fail (CLUTTER_IS_STATE (state));

  clutter_state_remove_key_internal (state,
                                     source_state_name, target_state_name,
                                     object, property_name,
                                     FALSE);
}

/**
 * clutter_state_get_timeline:
 * @state: a #ClutterState
 *
 * Gets the timeline driving the #ClutterState
 *
 * Return value: (transfer none): the #ClutterTimeline that drives
 *   the state change animations. The returned timeline is owned
 *   by the #ClutterState and it should not be unreferenced directly
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
ClutterTimeline *
clutter_state_get_timeline (ClutterState *state)
{
  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);

  return state->priv->timeline;
}


static void
clutter_state_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterState *state = CLUTTER_STATE (object);

  switch (prop_id)
    {
      case PROP_STATE:
        clutter_state_set_state (state, g_value_get_string (value));
        break;
      case PROP_DURATION:
        state->priv->duration = g_value_get_uint (value);
        break;
      default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clutter_state_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterState *state = CLUTTER_STATE (object);
  switch (prop_id)
    {
      case PROP_STATE:
        g_value_set_string (value,
                            clutter_state_get_state (state));
        break;
      case PROP_DURATION:
        g_value_set_uint (value, state->priv->duration);
        break;
      default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clutter_state_class_init (ClutterStateClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  gobject_class->finalize     = clutter_state_finalize;
  gobject_class->set_property = clutter_state_set_property;
  gobject_class->get_property = clutter_state_get_property;

  /**
   * ClutterState::completed:
   * @state: the #ClutterState that emitted the signal
   *
   * The ::completed signal is emitted when a #ClutterState reaches
   * the target state specified by clutter_state_set_state() or
   * clutter_state_warp_to_state().
   *
   * Since: 1.4
   * Deprecated: 1.12: Use #ClutterKeyframeTransition and
   *   #ClutterTransitionGroup instead
   */
  state_signals[COMPLETED] =
    g_signal_new (I_("completed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterStateClass, completed),
                  NULL, NULL,
                  _clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterState:state:
   *
   * The currently set target state, setting it causes the
   * state machine to transition to the new state, use
   * clutter_state_warp_to_state() to change state without
   * a transition.
   *
   * Since: 1.4
   * Deprecated: 1.12: Use #ClutterKeyframeTransition and
   *   #ClutterTransitionGroup instead
   */
  pspec = g_param_spec_string ("state",
                               P_("State"),
                               P_("Currently set state, (transition to this state might not be complete)"),
                               NULL,
                               CLUTTER_PARAM_READWRITE);
  obj_props[PROP_STATE] = pspec;
  g_object_class_install_property (gobject_class, PROP_STATE, pspec);

  /**
   * ClutterState:duration:
   *
   * Default duration used if an duration has not been specified for a specific
   * source/target state pair. The values is in milliseconds.
   *
   * Since: 1.4
   * Deprecated: 1.12: Use #ClutterKeyframeTransition and
   *   #ClutterTransitionGroup instead
   */
  pspec = g_param_spec_uint ("duration",
                             P_("Duration"),
                             P_("Default transition duration"),
                             0, 86400000, 1000,
                             CLUTTER_PARAM_READWRITE);
  obj_props[PROP_DURATION] = pspec;
  g_object_class_install_property (gobject_class, PROP_DURATION, pspec);
}

static void
clutter_state_init (ClutterState *self)
{
  ClutterStatePrivate *priv;

  priv = self->priv = clutter_state_get_instance_private (self);

  priv->states = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL,
                                        state_free);

  self->priv->source_state_name = NULL;
  self->priv->target_state_name = NULL;

  self->priv->duration = 1000;

  priv->timeline = clutter_timeline_new (1000);
  g_signal_connect (priv->timeline, "new-frame",
                    G_CALLBACK (clutter_state_new_frame),
                    self);
  g_signal_connect (priv->timeline, "completed",
                    G_CALLBACK (clutter_state_completed),
                    self);

  priv->slave_timeline = clutter_timeline_new (SLAVE_TIMELINE_LENGTH);
}


/**
 * clutter_state_get_animator:
 * @state: a #ClutterState instance.
 * @source_state_name: the name of a source state
 * @target_state_name: the name of a target state
 *
 * Retrieves the #ClutterAnimator that is being used for transitioning
 * between the two states, if any has been set
 *
 * Return value: (transfer none): a #ClutterAnimator instance, or %NULL
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
ClutterAnimator *
clutter_state_get_animator (ClutterState *state,
                            const gchar  *source_state_name,
                            const gchar  *target_state_name)
{
  State *target_state;
  guint i;

  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);

  source_state_name = g_intern_string (source_state_name);
  if (source_state_name == g_intern_static_string (""))
    source_state_name = NULL;

  target_state_name = g_intern_string (target_state_name);

  target_state = clutter_state_fetch_state (state, target_state_name, FALSE);
  if (target_state == NULL)
    return NULL;

  for (i = 0; i < target_state->animators->len; i++)
    {
      const StateAnimator *animator;

      animator = &g_array_index (target_state->animators, StateAnimator, i);
      if (animator->source_state_name == source_state_name)
        return animator->animator;
    }

  return NULL;
}

/**
 * clutter_state_set_animator:
 * @state: a #ClutterState instance.
 * @source_state_name: the name of a source state
 * @target_state_name: the name of a target state
 * @animator: (allow-none): a #ClutterAnimator instance, or %NULL to
 *   unset an existing #ClutterAnimator
 *
 * Specifies a #ClutterAnimator to be used when transitioning between
 * the two named states.
 *
 * The @animator allows specifying a transition between the state that is
 * more elaborate than the basic transitions allowed by the tweening of
 * properties defined in the #ClutterState keys.
 *
 * If @animator is %NULL it will unset an existing animator.
 *
 * #ClutterState will take a reference on the passed @animator, if any
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
void
clutter_state_set_animator (ClutterState    *state,
                            const gchar     *source_state_name,
                            const gchar     *target_state_name,
                            ClutterAnimator *animator)
{
  State *target_state;
  guint i;

  g_return_if_fail (CLUTTER_IS_STATE (state));

  source_state_name = g_intern_string (source_state_name);
  target_state_name = g_intern_string (target_state_name);

  target_state = clutter_state_fetch_state (state, target_state_name, TRUE);
  if (target_state == NULL)
    return;
  
  for (i = 0; target_state->animators->len; i++)
    {
      StateAnimator *a;

      a = &g_array_index (target_state->animators, StateAnimator, i);
      if (a->source_state_name == source_state_name)
        {
          g_object_unref (a->animator);

          if (animator != NULL)
            a->animator = g_object_ref (animator);
          else
            {
              /* remove the matched animator if passed NULL */
              g_array_remove_index (target_state->animators, i);
            }

          return;
        }
    }

  if (animator != NULL)
    {
      StateAnimator state_animator = {
        source_state_name,
        g_object_ref (animator)
      };

      g_array_append_val (target_state->animators, state_animator);
    }
}

static gpointer
clutter_state_key_copy (gpointer boxed)
{
  if (boxed != NULL)
    {
      ClutterStateKey *key = boxed;

      key->ref_count += 1;
    }

  return boxed;
}

G_DEFINE_BOXED_TYPE (ClutterStateKey, clutter_state_key,
                     clutter_state_key_copy,
                     clutter_state_key_free);

/**
 * clutter_state_key_get_pre_delay:
 * @state_key: a #ClutterStateKey
 *
 * Retrieves the pause before transitioning starts as a fraction of
 * the total transition time.
 *
 * Return value: the pre delay used before starting the transition.
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
gdouble
clutter_state_key_get_pre_delay (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key != NULL, 0.0);

  return state_key->pre_delay;
}

/**
 * clutter_state_key_get_post_delay:
 * @state_key: a #ClutterStateKey
 *
 * Retrieves the duration of the pause after transitioning is complete
 * as a fraction of the total transition time.
 *
 * Return value: the post delay, used after doing the transition.
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
gdouble
clutter_state_key_get_post_delay (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key != NULL, 0.0);

  return state_key->post_delay;
}

/**
 * clutter_state_key_get_mode:
 * @state_key: a #ClutterStateKey
 *
 * Retrieves the easing mode used for @state_key.
 *
 * Return value: the mode of a #ClutterStateKey
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
gulong
clutter_state_key_get_mode (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key != NULL, 0);

  return state_key->mode;
}

/**
 * clutter_state_key_get_value:
 * @state_key: a #ClutterStateKey
 * @value: a #GValue initialized with the correct type for the @state_key
 *
 * Retrieves a copy of the value for a #ClutterStateKey.
 *
 * The #GValue needs to be already initialized for the value type
 * of the property or to a type that allow transformation from the value
 * type of the key.
 *
 * Use g_value_unset() when done.
 *
 * Return value: %TRUE if the value was successfully retrieved,
 *   and %FALSE otherwise
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
gboolean
clutter_state_key_get_value (const ClutterStateKey *state_key,
                             GValue                *value)
{
  g_return_val_if_fail (state_key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (value) != G_TYPE_INVALID, FALSE);

  if (!g_type_is_a (G_VALUE_TYPE (&state_key->value), G_VALUE_TYPE (value)))
    {
      if (g_value_type_compatible (G_VALUE_TYPE (&state_key->value),
                                   G_VALUE_TYPE (value)))
        {
          g_value_copy (&state_key->value, value);
          return TRUE;
        }

      if (g_value_type_transformable (G_VALUE_TYPE (&state_key->value),
                                      G_VALUE_TYPE (value)))
        {
          if (g_value_transform (&state_key->value, value))
            return TRUE;
        }

      g_warning ("%s: Unable to convert from %s to %s for the "
                 "property '%s' of object %s in the state key",
                 G_STRLOC,
                 g_type_name (G_VALUE_TYPE (&state_key->value)),
                 g_type_name (G_VALUE_TYPE (value)),
                 state_key->property_name,
                 G_OBJECT_TYPE_NAME (state_key->object));

      return FALSE;
    }
  else
    g_value_copy (&state_key->value, value);

  return TRUE;
}

/**
 * clutter_state_key_get_object:
 * @state_key: a #ClutterStateKey
 *
 * Retrieves the object instance this #ClutterStateKey applies to.
 *
 * Return value: (transfer none): the object this state key applies to.
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
GObject *
clutter_state_key_get_object (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key, NULL);

  return state_key->object;
}

/**
 * clutter_state_key_get_property_name:
 * @state_key: a #ClutterStateKey
 *
 * Retrieves the name of the property this #ClutterStateKey applies to
 *
 * Return value: the name of the property. The returned string is owned
 *   by the #ClutterStateKey and should never be modified or freed
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
const gchar *
clutter_state_key_get_property_name (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key, NULL);

  return state_key->property_name;
}

/**
 * clutter_state_key_get_source_state_name:
 * @state_key: a #ClutterStateKey
 *
 * Retrieves the name of the source state of the @state_key
 *
 * Return value: the name of the source state for this key, or %NULL
 *   if this is the generic state key for the given property when
 *   transitioning to the target state. The returned string is owned
 *   by the #ClutterStateKey and should never be modified or freed
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
const gchar *
clutter_state_key_get_source_state_name (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key, NULL);

  if (state_key->source_state != NULL)
    return state_key->source_state->name;

  return NULL;
}

/**
 * clutter_state_key_get_target_state_name:
 * @state_key: a #ClutterStateKey
 *
 * Get the name of the source state this #ClutterStateKey contains,
 * or NULL if this is the generic state key for the given property
 * when transitioning to the target state.
 *
 * Return value: the name of the source state for this key, or NULL if
 *   the key is generic
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
const gchar *
clutter_state_key_get_target_state_name (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key, NULL);

  return state_key->target_state->name;
}

/**
 * clutter_state_key_get_property_type:
 * @key: a #ClutterStateKey
 *
 * Retrieves the #GType of the property a key applies to
 *
 * You can use this type to initialize the #GValue to pass to
 * clutter_state_key_get_value()
 *
 * Return value: the #GType of the property
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
GType
clutter_state_key_get_property_type (const ClutterStateKey *key)
{
  g_return_val_if_fail (key != NULL, G_TYPE_INVALID);

  return G_VALUE_TYPE (&key->value);
}


/**
 * clutter_state_set_duration:
 * @state: a #ClutterState
 * @source_state_name: (allow-none): the name of the source state, or %NULL
 * @target_state_name: (allow-none): the name of the target state, or %NULL
 * @duration: the duration of the transition, in milliseconds
 *
 * Sets the duration of a transition.
 *
 * If both state names are %NULL the default duration for @state is set.
 *
 * If only @target_state_name is specified, the passed @duration becomes
 * the default duration for transitions to the target state.
 *
 * If both states names are specified, the passed @duration only applies
 * to the specified transition.
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
void
clutter_state_set_duration (ClutterState *state,
                            const gchar  *source_state_name,
                            const gchar  *target_state_name,
                            guint         duration)
{
  State *target_state;

  g_return_if_fail (CLUTTER_IS_STATE (state));

  source_state_name = g_intern_string (source_state_name);
  if (source_state_name == g_intern_static_string (""))
    source_state_name = NULL;

  target_state_name = g_intern_string (target_state_name);
  if (target_state_name == g_intern_static_string (""))
    target_state_name = NULL;

  if (target_state_name == NULL)
    {
      state->priv->duration = duration;
      return;
    }

  target_state = clutter_state_fetch_state (state, target_state_name, FALSE);
  if (target_state != NULL)
    {
      if (source_state_name != NULL)
        g_hash_table_insert (target_state->durations,
                             (gpointer) source_state_name,
                             GINT_TO_POINTER (duration));
      else
        g_hash_table_insert (target_state->durations,
                             NULL,
                             GINT_TO_POINTER (duration));
    }
}

/**
 * clutter_state_get_duration:
 * @state: a #ClutterState
 * @source_state_name: (allow-none): the name of the source state to
 *   get the duration of, or %NULL
 * @target_state_name: (allow-none): the name of the source state to
 *   get the duration of, or %NULL
 *
 * Queries the duration used for transitions between a source and
 * target state pair
 *
 * The semantics for the query are the same as the semantics used for
 * setting the duration with clutter_state_set_duration()
 * 
 * Return value: the duration, in milliseconds
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
guint
clutter_state_get_duration (ClutterState *state,
                            const gchar  *source_state_name,
                            const gchar  *target_state_name)
{
  State *target_state;
  guint ret = 0;

  g_return_val_if_fail (CLUTTER_IS_STATE (state), 0);

  source_state_name = g_intern_string (source_state_name);
  if (source_state_name == g_intern_static_string (""))
    source_state_name = NULL;

  target_state_name = g_intern_string (target_state_name);
  if (target_state_name == g_intern_static_string (""))
    target_state_name = NULL;

  if (target_state_name == NULL)
    return state->priv->duration;

  target_state = clutter_state_fetch_state (state, target_state_name, FALSE);
  if (target_state != NULL)
    {
      if (source_state_name)
        {
          ret = GPOINTER_TO_INT (g_hash_table_lookup (target_state->durations,
                                                      source_state_name));
          if(!ret)
            ret = GPOINTER_TO_INT (g_hash_table_lookup (target_state->durations,
                                                        NULL));
        }
      else
        ret = GPOINTER_TO_INT (g_hash_table_lookup (target_state->durations,
                                                    NULL));
    }

  if (!ret)
    ret = state->priv->duration;

  return ret;
}

/**
 * clutter_state_get_state:
 * @state: a #ClutterState
 *
 * Queries the currently set target state.
 *
 * During a transition this function will return the target of the transition.
 *
 * This function is useful when called from handlers of the
 * #ClutterState::completed signal.
 * 
 * Return value: a string containing the target state. The returned string
 *   is owned by the #ClutterState and should not be modified or freed
 *
 * Since: 1.4
 * Deprecated: 1.12: Use #ClutterKeyframeTransition and
 *   #ClutterTransitionGroup instead
 */
const gchar *
clutter_state_get_state (ClutterState *state)
{
  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);

  return state->priv->target_state_name;
}

typedef struct _ParseClosure {
  ClutterState *state;
  ClutterScript *script;

  GValue *value;

  gboolean result;
} ParseClosure;

static void
parse_state_transition (JsonArray *array,
                        guint      index_,
                        JsonNode  *element,
                        gpointer   data)
{
  ParseClosure *clos = data;
  JsonObject *object;
  const gchar *source_name, *target_name;
  State *source_state, *target_state;
  JsonArray *keys;
  GSList *valid_keys = NULL;
  GList *array_keys, *k;

  if (JSON_NODE_TYPE (element) != JSON_NODE_OBJECT)
    {
      g_warning ("The 'transitions' member of a ClutterState description "
                 "should be an array of objects, but the element %d of the "
                 "array is of type '%s'. The element will be ignored.",
                 index_,
                 json_node_type_name (element));
      return;
    }

  object = json_node_get_object (element);

  if (!json_object_has_member (object, "source") ||
      !json_object_has_member (object, "target") ||
      !(json_object_has_member (object, "keys") ||
        json_object_has_member (object, "animator")))
    {
      g_warning ("The transition description at index %d is missing one "
                 "of the mandatory members: source, target and keys or "
                 "animator", index_);
      return;
    }

  source_name = json_object_get_string_member (object, "source");
  source_state = clutter_state_fetch_state (clos->state, source_name, TRUE);

  target_name = json_object_get_string_member (object, "target");
  target_state = clutter_state_fetch_state (clos->state, target_name, TRUE);

  if (json_object_has_member (object, "duration"))
    {
      guint duration = json_object_get_int_member (object, "duration");

      clutter_state_set_duration (clos->state,
                                  source_name, target_name,
                                  duration);
    }

  if (json_object_has_member (object, "animator"))
    {
      const gchar *id_ = json_object_get_string_member (object, "animator");
      GObject *animator;

      animator = clutter_script_get_object (clos->script, id_);
      if (animator == NULL)
        {
          g_warning ("No object with id '%s' has been defined.", id_);
          return;
        }

      clutter_state_set_animator (clos->state,
                                  source_name,
                                  target_name,
                                  CLUTTER_ANIMATOR (animator));
    }

  if (!json_object_has_member (object, "keys"))
    return;

  keys = json_object_get_array_member (object, "keys");
  if (keys == NULL && !json_object_has_member (object, "animator"))
    {
      g_warning ("The transition description at index %d has an invalid "
                 "key member of type '%s' when an array was expected.",
                 index_,
                 json_node_type_name (json_object_get_member (object, "keys")));
      return;
    }

  if (G_IS_VALUE (clos->value))
    valid_keys = g_slist_reverse (g_value_get_pointer (clos->value));
  else
    g_value_init (clos->value, G_TYPE_POINTER);

  array_keys = json_array_get_elements (keys);
  for (k = array_keys; k != NULL; k = k->next)
    {
      JsonNode *node = k->data;
      JsonArray *key = json_node_get_array (node);
      ClutterStateKey *state_key;
      GObject *gobject;
      GParamSpec *pspec;
      const gchar *id_;
      const gchar *property;
      gulong mode;
      gboolean res;

      id_ = json_array_get_string_element (key, 0);
      gobject = clutter_script_get_object (clos->script, id_);
      if (gobject == NULL)
        {
          g_warning ("No object with id '%s' has been defined.", id_);
          continue;
        }

      property = json_array_get_string_element (key, 1);
      pspec = get_property_from_object (gobject, property);
      if (pspec == NULL)
        {
          g_warning ("The object of type '%s' and name '%s' has no "
                     "property named '%s'.",
                     G_OBJECT_TYPE_NAME (gobject),
                     id_,
                     property);
          continue;
        }

      mode = _clutter_script_resolve_animation_mode (json_array_get_element (key, 2));

      state_key = clutter_state_key_new (target_state,
                                         gobject, property, pspec,
                                         mode);

      res = _clutter_script_parse_node (clos->script,
                                        &(state_key->value),
                                        property,
                                        json_array_get_element (key, 3),
                                        pspec);
      if (!res)
        {
          g_warning ("Unable to parse the key value for the "
                     "property '%s' of object '%s' at index %d",
                     property,
                     id_,
                     index_);
          clutter_state_key_free (state_key);
          continue;
        }

      switch (json_array_get_length (key))
        {
        case 5:
          state_key->pre_delay = json_array_get_double_element (key, 4);
          state_key->post_delay = 0.0;
          break;

        case 6:
          state_key->pre_delay = json_array_get_double_element (key, 4);
          state_key->post_delay = json_array_get_double_element (key, 5);
          break;

        default:
          state_key->pre_delay = 0.0;
          state_key->post_delay = 0.0;
          break;
        }

      state_key->source_state = source_state;

      valid_keys = g_slist_prepend (valid_keys, state_key);
    }

  g_list_free (array_keys);

  g_value_set_pointer (clos->value, g_slist_reverse (valid_keys));

  clos->result = TRUE;
}

static gboolean
clutter_state_parse_custom_node (ClutterScriptable *scriptable,
                                 ClutterScript     *script,
                                 GValue            *value,
                                 const gchar       *name,
                                 JsonNode          *node)
{
  ParseClosure clos;

  if (strcmp (name, "transitions") != 0)
    return FALSE;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return FALSE;

  clos.state = CLUTTER_STATE (scriptable);
  clos.script = script;
  clos.value = value;
  clos.result = FALSE;

  json_array_foreach_element (json_node_get_array (node),
                              parse_state_transition,
                              &clos);

  return clos.result;
}

static void
clutter_state_set_custom_property (ClutterScriptable *scriptable,
                                   ClutterScript     *script,
                                   const gchar       *name,
                                   const GValue      *value)
{
  if (strcmp (name, "transitions") == 0)
    {
      ClutterState *state = CLUTTER_STATE (scriptable);
      GSList *keys = g_value_get_pointer (value);
      GSList *k;

      for (k = keys; k != NULL; k = k->next)
        clutter_state_set_key_internal (state, k->data);

      g_slist_free (keys);
    }
  else
    g_object_set_property (G_OBJECT (scriptable), name, value);
}

static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_state_parse_custom_node;
  iface->set_custom_property = clutter_state_set_custom_property;
}
