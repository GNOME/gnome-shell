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
 * #ClutterState controls the tweening of properties on multiple
 * actors between a set of named states.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-state.h"

#include <gobject/gvaluecollector.h>
#include <string.h>

#include "clutter-alpha.h"
#include "clutter-animator.h"
#include "clutter-enum-types.h"
#include "clutter-interval.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

G_DEFINE_TYPE (ClutterState, clutter_state, G_TYPE_OBJECT);

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
  gdouble          post_delay;   /* fraction of duration to be done in */

  State           *source_state; /* source state */
  State           *target_state; /* target state */

  ClutterAlpha    *alpha;        /* The alpha this key uses for interpolation */
  ClutterInterval *interval;     /* The interval this key uses for
                                    interpolation */

  guint            is_inert : 1; /* set if the key is being destroyed due to
                                    weak reference */
  gint             ref_count;    /* reference count for boxed life time */
} _ClutterStateKey;

enum
{
  PROP_0,
  PROP_TARGET_STATE
};

enum
{
  COMPLETED,
  LAST_SIGNAL
};

static guint state_signals[LAST_SIGNAL] = {0, };

#define CLUTTER_STATE_GET_PRIVATE(obj)            \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_STATE,                \
               ClutterStatePrivate))

/**
 * clutter_state_new:
 *
 * Creates a new #ClutterState
 *
 * Return value: the newly create #ClutterState instance
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

static void object_disappeared (gpointer data,
                                GObject *where_the_object_was);

static ClutterStateKey *
clutter_state_key_new (State       *state,
                       GObject     *object,
                       const gchar *property_name,
                       GParamSpec  *pspec,
                       guint        mode)
{
  ClutterStateKey *state_key;
  GValue value = {0, };

  state_key = g_slice_new0 (ClutterStateKey);

  state_key->target_state = state;
  state_key->object = object;
  state_key->property_name = g_intern_string (property_name);
  state_key->mode = mode;

  state_key->alpha = clutter_alpha_new ();
  g_object_ref_sink (state_key->alpha);
  clutter_alpha_set_mode (state_key->alpha, mode);
  clutter_alpha_set_timeline (state_key->alpha,
                              state->clutter_state->priv->slave_timeline);

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
    g_object_weak_unref (key->object, object_disappeared,
                         key->target_state->clutter_state);
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
    source_state = g_hash_table_lookup (this->priv->states,
                                        source_state_name);

  if (target_state_name != NULL)
    state_list = g_list_append (NULL, (gpointer) target_state_name);
  else
    state_list = clutter_state_get_states (this);

  for (s = state_list; s != NULL; s = s->next)
    {
      State *target_state;
      target_state = g_hash_table_lookup (this->priv->states, s->data);

      if (target_state)
        {
          GList *k;

          for (k = target_state->keys; k; k = k->next)
            {
              ClutterStateKey *key = k->data;

              if ((object == NULL || (object == key->object)) &&
                  (source_state == NULL ||
                   (source_state == key->source_state)) &&
                  (property_name == NULL ||
                   ((property_name == key->property_name))))
                {
                  k = target_state->keys =
                    g_list_remove (target_state->keys, key);

                  key->is_inert = is_inert;
                  clutter_state_key_free (key);
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

  g_array_unref (state->animators);
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
  GList       *k;
  gdouble      progress;
  const gchar *curprop = NULL;
  GObject     *curobj = NULL;
  gboolean     found_specific = FALSE;

  if (state->priv->current_animator)
    return;

  progress = clutter_timeline_get_progress (timeline);

  for (k = state->priv->target_state->keys; k; k = k->next)
    {
      ClutterStateKey *key = k->data;
      GValue           value = {0,};
      gdouble          sub_progress;

      if ((curprop && !(curprop == key->property_name)) ||
          key->object != curobj)
        {
          curprop = key->property_name;
          curobj = key->object;
          found_specific = FALSE;
        }

      if (!found_specific)
        {
          if (key->source_state &&
              key->source_state->name &&
              g_str_equal (state->priv->source_state_name,
                           key->source_state->name))
            {
              found_specific = TRUE;
            }

          if (found_specific || key->source_state == NULL)
            {
              sub_progress = (progress - key->pre_delay) /
                             (1.0 - (key->pre_delay + key->post_delay));

              if (sub_progress >= 0.0)
                {
                  if (sub_progress >= 1.0)
                    sub_progress = 1.0;
                  clutter_timeline_advance (state->priv->slave_timeline,
                                            sub_progress * SLAVE_TIMELINE_LENGTH);
                  sub_progress = clutter_alpha_get_alpha (key->alpha);

                  g_value_init (&value,
                               clutter_interval_get_value_type (key->interval));
                  clutter_interval_compute_value (key->interval, sub_progress,
                                                  &value);
                  g_object_set_property (key->object, key->property_name,
                                         &value);
                  g_value_unset (&value);
                }
              /* XXX: should the target value of the default destination be
               * used even when found a specific source_state key?
               */
            }
        }
    }
}


/**
 * clutter_state_change:
 * @state_name: a #ClutterState
 * @animate: whether we should animate the transition or not
 *
 * Change the current state of #ClutterState to @state_name
 *
 * If @animate is %FALSE, the state transition will happen immediately;
 * otherwise, the state transition will be animated over the duration
 * set using clutter_state_set_duration()
 *
 * Return value: the #ClutterTimeline that drives the state transition
 *
 * Since: 1.4
 */
ClutterTimeline *
clutter_state_change (ClutterState *this,
                      const gchar  *target_state_name,
                      gboolean      animate)
{
  ClutterStatePrivate *priv = this->priv;
  State *state;

  g_return_val_if_fail (CLUTTER_IS_STATE (this), NULL);
  g_return_val_if_fail (target_state_name != NULL, NULL);

  if (target_state_name == NULL)
    target_state_name = g_intern_static_string ("default");

  target_state_name = g_intern_string (target_state_name);
  if (priv->target_state_name == NULL)
    priv->target_state_name = g_intern_static_string ("default");

  if (target_state_name == priv->target_state_name)
    {
      /* Avoid transitioning if the desired state is already current */
      return priv->timeline;
    }

  if (priv->current_animator)
    {
      clutter_animator_set_timeline (priv->current_animator, NULL);
      priv->current_animator = NULL;
    }

  priv->source_state_name = priv->target_state_name;
  priv->target_state_name = target_state_name;

  g_object_notify (G_OBJECT (this), "target-state");

  if (animate)
    {
      guint duration;

      duration = clutter_state_get_duration (this,
                                             priv->source_state_name,
                                             priv->target_state_name);
      clutter_timeline_set_duration (priv->timeline, duration);
    }
  else
    clutter_timeline_set_duration (priv->timeline, 1);

  state = g_hash_table_lookup (priv->states, target_state_name);
  g_return_val_if_fail (state, NULL);

  {
    ClutterAnimator *animator;

    animator = clutter_state_get_animator (this,
                                           priv->source_state_name,
                                           priv->target_state_name);
    if (animator != NULL)
      {
        priv->current_animator = animator;
        clutter_animator_set_timeline (animator, priv->timeline);

        clutter_timeline_stop (priv->timeline);
        clutter_timeline_rewind (priv->timeline);
        clutter_timeline_start (priv->timeline);

        return priv->timeline;
      }
  }

  if (state)
    {
      GList *k;

      for (k = state->keys; k; k = k->next)
        {
          ClutterStateKey *key = k->data;
          GValue initial = {0,};

          g_value_init (&initial,
                        clutter_interval_get_value_type (key->interval));

          g_object_get_property (key->object, key->property_name, &initial);
          if (clutter_alpha_get_mode (key->alpha) != key->mode)
            clutter_alpha_set_mode (key->alpha, key->mode);

          clutter_interval_set_initial_value (key->interval, &initial);
          clutter_interval_set_final_value (key->interval, &key->value);

          g_value_unset (&initial);
        }

       priv->target_state = state;
       clutter_timeline_rewind (priv->timeline);
       clutter_timeline_start (priv->timeline);
    }
  else
    g_warning ("State '%s' not found", target_state_name);

  return priv->timeline;
}


/**
 * clutter_state_set:
 * @state: a #ClutterState instance.
 * @source_state_name: the name of the source state keys are being added for
 * @target_state_name: the name of the target state keys are being added for
 * @first_object: a #GObject
 * @first_property_name: a property of @first_object to specify a key for
 * @first_mode: the id of the alpha function to use
 * @Varargs: the value @first_property_name should have in @state_name,
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
 * If a given object, state_name, property tuple already exist then the mode
 * and value will be replaced with the new specified values.
 *
 * If a the property_name is prefixed with "delayed::" two additional
 * arguments per key are expected: a value relative to the full state time
 * to pause before transitioning and a similar value to pause after
 * transitioning.
 *
 * Since: 1.4
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
  GObjectClass *klass;
  gpointer      object;
  const gchar  *property_name;
  gulong        mode;
  va_list       args;

  object = first_object;

  property_name = first_property_name;
  mode = first_mode;
  va_start (args, first_mode);

  g_return_if_fail (G_IS_OBJECT (first_object));
  g_return_if_fail (first_property_name);

  while (object != NULL)
    {
      GParamSpec *pspec;
      GValue value = { 0, };
      gchar *error = NULL;
      const gchar *real_property_name = property_name;

      klass = G_OBJECT_GET_CLASS (object);

      if (g_str_has_prefix (property_name, "delayed::"))
        real_property_name = strstr (property_name, "::") + 2;

      pspec = g_object_class_find_property (klass, real_property_name);
      if (pspec == NULL)
        {
          g_warning ("Cannot bind property '%s': objects of type '%s' "
                     "do not have this property",
                     real_property_name,
                     G_OBJECT_TYPE_NAME (object));
          break;
        }

#if GLIB_CHECK_VERSION (2, 23, 2)
      G_VALUE_COLLECT_INIT (&value, G_PARAM_SPEC_VALUE_TYPE (pspec),
                            args, 0,
                            &error);
#else
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, args, 0, &error);
#endif /* GLIB_CHECK_VERSION */

      if (error != NULL)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      if (g_str_has_prefix (property_name, "delayed::"))
        {
          gdouble pre_delay = va_arg (args, gdouble);
          gdouble post_delay = va_arg (args, gdouble);

          clutter_state_set_key (state,
                                 source_state_name,
                                 target_state_name,
                                 object,
                                 real_property_name,
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

      object = va_arg (args, gpointer);
      if (object != NULL)
        {
          property_name = va_arg (args, gchar*);
          mode = va_arg (args, gulong);
        }
    }

  va_end (args);
}

/**
 * clutter_state_set_key:
 * @this: a #ClutterState instance.
 * @source_state_name: the source transition to specify transition for
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
 * Sets one specific end key for a state_name, object, property_name
 * combination.
 *
 * Return value: the #ClutterState instance, allowing chaining of
 *   multiple calls
 *
 * Since: 1.4
 */
ClutterState *
clutter_state_set_key (ClutterState  *this,
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
  GList           *old_item;
  State           *source_state = NULL;
  State           *target_state;

  g_return_val_if_fail (CLUTTER_IS_STATE (this), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (property_name, NULL);
  g_return_val_if_fail (value, NULL);

  if (target_state_name == NULL)
    target_state_name = "default";

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                        property_name);
  if (pspec == NULL || !(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("No writable property '%s' for object type '%s' found",
                 property_name,
                 G_OBJECT_TYPE_NAME (object));
      return NULL;
    }

  source_state_name = g_intern_string (source_state_name);
  target_state_name = g_intern_string (target_state_name);
  property_name = g_intern_string (property_name);

  target_state = g_hash_table_lookup (this->priv->states,
                                      target_state_name);

  if (!target_state)
    {
      target_state = state_new (this, target_state_name);
      g_hash_table_insert (this->priv->states,
                           (gpointer)target_state_name, target_state);
    }

  if (source_state_name)
    {
      source_state = g_hash_table_lookup (this->priv->states,
                                          source_state_name);
      if (!source_state)
        {
          source_state = state_new (this, source_state_name);
          g_hash_table_insert (this->priv->states,
                               (gpointer)source_state_name, source_state);
        }
    }

  state_key = clutter_state_key_new (target_state,
                                     object, property_name, pspec,
                                     mode);

  state_key->source_state = source_state;
  state_key->pre_delay = pre_delay;
  state_key->post_delay = post_delay;

  g_value_init (&state_key->value, G_VALUE_TYPE (value));
  g_value_copy (value, &state_key->value);

  if ((old_item = g_list_find_custom (target_state->keys,
                                      state_key,
                                      sort_props_func)))
    {
      ClutterStateKey *old_key = old_item->data;

      target_state->keys = g_list_remove (target_state->keys, old_key);
      clutter_state_key_free (old_key);
    }

  target_state->keys = g_list_insert_sorted (target_state->keys,
                                             state_key,
                                             sort_props_func);
  return this;
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
 * Return value: (transfer container) (element-type ClutterStateKey): a
 *   newly allocated #GList of #ClutterStateKey<!-- -->s. The contents of
 *   the returned list are owned by the #ClutterState and should not be
 *   modified or freed. Use g_list_free() to free the resources allocated
 *   by the returned list when done using it
 *
 * Since: 1.4
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
    source_state = g_hash_table_lookup (state->priv->states,
                                        source_state_name);

  for (s = state_list; s; s=s->next)
    {
      State *target_state;
      target_state = g_hash_table_lookup (state->priv->states, s->data);

      if (target_state)
        {
          GList *k;

          for (k = target_state->keys; k; k = k->next)
            {
              ClutterStateKey *key = k->data;

              if (   (object == NULL || (object == key->object))
                  && (source_state_name == NULL
                      ||source_state == key->source_state)
                  && (property_name == NULL
                      ||(property_name == key->property_name))
                  )
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
      case PROP_TARGET_STATE:
        clutter_state_change (state, g_value_get_string (value), TRUE);
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
      case PROP_TARGET_STATE:
        g_value_set_string (value,
                            clutter_state_get_target_state (state));
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

  g_type_class_add_private (klass, sizeof (ClutterStatePrivate));

  gobject_class->finalize     = clutter_state_finalize;
  gobject_class->set_property = clutter_state_set_property;
  gobject_class->get_property = clutter_state_get_property;

  /**
   * ClutterState::completed:
   * @state: the #ClutterState that emitted the signal
   *
   * The ::completed signal is emitted when a #ClutterState reaches
   * the target state specified by clutter_state_change()
   *
   * Since: 1.4
   */
  state_signals[COMPLETED] =
    g_signal_new (I_("completed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterStateClass, completed),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterState:target-state:
   *
   * The currently set target state, setting it causes the
   * state machine to transition to the new state, use
   * clutter_state_change() directly to directly jump to
   * a state.
   */
  pspec = g_param_spec_string ("target-state",
                               "Target State",
                               "Currently set state",
                               "default",
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TARGET_STATE, pspec);
}

static void
clutter_state_init (ClutterState *self)
{
  ClutterStatePrivate *priv;

  priv = self->priv = CLUTTER_STATE_GET_PRIVATE (self);

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
 */
ClutterAnimator *
clutter_state_get_animator (ClutterState *state,
                            const gchar  *source_state_name,
                            const gchar  *target_state_name)
{
  State         *target_state;
  StateAnimator *animators;
  gint i;

  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);

  source_state_name = g_intern_string (source_state_name);
  if (source_state_name == g_intern_static_string ("default") ||
      source_state_name == g_intern_static_string (""))
    source_state_name = NULL;

  target_state_name = g_intern_string (target_state_name);

  target_state = g_hash_table_lookup (state->priv->states, target_state_name);
  if (target_state == NULL)
    return NULL;
  
  animators = (StateAnimator*)target_state->animators->data;

  for (i = 0; animators[i].animator; i++)
    {
      if (animators[i].source_state_name == source_state_name)
        return animators[i].animator;
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
 * more elaborate than the basic transitions other allowed by the simple
 * tweening of properties defined in the #ClutterState keys.
 *
 * If @animator is %NULL it will unset an existing animator.
 *
 * #ClutterState will take a reference on the passed @animator, if any
 *
 * Since: 1.4
 */
void
clutter_state_set_animator (ClutterState    *state,
                            const gchar     *source_state_name,
                            const gchar     *target_state_name,
                            ClutterAnimator *animator)
{
  State *target_state;
  StateAnimator *animators;
  gint i;

  g_return_if_fail (CLUTTER_IS_STATE (state));

  source_state_name = g_intern_string (source_state_name);
  target_state_name = g_intern_string (target_state_name);

  target_state = g_hash_table_lookup (state->priv->states, target_state_name);
  if (target_state == NULL)
    return;
  
  animators = (StateAnimator *) target_state->animators->data;
  for (i = 0; animators[i].animator; i++)
    {
      if (animators[i].source_state_name == source_state_name)
        {
          g_object_unref (animators[i].animator);

          if (animator != NULL)
            animators[i].animator = g_object_ref (animator);
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
      StateAnimator state_animator =
                              {source_state_name, g_object_ref (animator)};
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

GType
clutter_state_key_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    our_type = g_boxed_type_register_static (I_("ClutterStateKey"),
                                             clutter_state_key_copy,
                                             clutter_state_key_free);

  return our_type;
}

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
 * Copies of the value for a #ClutterStateKey into @value
 *
 * The #GValue needs to be already initialized for the value type
 * of the property or to a transformable type
 *
 * Since: 1.4
 */
void
clutter_state_key_get_value (const ClutterStateKey *state_key,
                             GValue                *value)
{
  g_return_if_fail (state_key != NULL);
  g_return_if_fail (value != NULL);
  g_return_if_fail (G_VALUE_TYPE (value) != G_TYPE_INVALID);

  if (!g_type_is_a (G_VALUE_TYPE (&state_key->value), G_VALUE_TYPE (value)))
    {
      if (g_value_type_compatible (G_VALUE_TYPE (&state_key->value),
                                   G_VALUE_TYPE (value)))
        {
          g_value_copy (&state_key->value, value);
          return;
        }

      if (g_value_type_transformable (G_VALUE_TYPE (&state_key->value),
                                      G_VALUE_TYPE (value)))
        {
          if (g_value_transform (&state_key->value, value))
            return;
        }

      g_warning ("%s: Unable to convert from %s to %s for the "
                 "property '%s' of object %s in the state key",
                 G_STRLOC,
                 g_type_name (G_VALUE_TYPE (&state_key->value)),
                 g_type_name (G_VALUE_TYPE (value)),
                 state_key->property_name,
                 G_OBJECT_TYPE_NAME (state_key->object));
    }
  else
    g_value_copy (&state_key->value, value);
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
 */
G_CONST_RETURN gchar *
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
 */
G_CONST_RETURN gchar *
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
 */
G_CONST_RETURN gchar *
clutter_state_key_get_target_state_name (const ClutterStateKey *state_key)
{
  g_return_val_if_fail (state_key, NULL);

  return state_key->target_state->name;
}

/**
 * clutter_state_set_duration:
 * @state: a #ClutterState
 * @source_state_name: the name of the source state to set duration for or NULL
 * @target_state_name: the name of the source state to set duration for or NULL
 * @duration: milliseconds for transition between source_state and target_state.
 *
 * If both state names are NULL the default duration for ClutterState is set,
 * if only target_state_name is specified this becomes the default duration
 * for transitions to this state. When both are specified the change only
 * applies to this transition.
 *
 * Since: 1.4
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
  if (source_state_name == g_intern_static_string ("default") ||
      source_state_name == g_intern_static_string (""))
    source_state_name = NULL;

  target_state_name = g_intern_string (target_state_name);
  if (target_state_name == g_intern_static_string ("default") ||
      target_state_name == g_intern_static_string (""))
    target_state_name = NULL;

  if (target_state_name == NULL)
    {
      state->priv->duration = duration;
      return;
    }

  target_state = g_hash_table_lookup (state->priv->states, target_state_name);
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
  if (source_state_name == g_intern_static_string ("default") ||
      source_state_name == g_intern_static_string (""))
    source_state_name = NULL;

  target_state_name = g_intern_string (target_state_name);
  if (target_state_name == g_intern_static_string ("default") ||
      target_state_name == g_intern_static_string (""))
    target_state_name = NULL;

  if (target_state_name == NULL)
    return state->priv->duration;

  target_state = g_hash_table_lookup (state->priv->states,
                                      target_state_name);
  if (target_state != NULL)
    {
      if (source_state_name)
        ret = GPOINTER_TO_INT (g_hash_table_lookup (target_state->durations,
                                                    source_state_name));
      else
        ret = GPOINTER_TO_INT (g_hash_table_lookup (target_state->durations,
                                                    NULL));
    }

  if (!ret)
    ret = state->priv->duration;

  return ret;
}

/**
 * clutter_state_get_target_state:
 * @state: a #ClutterState
 *
 * Queries the currently set target state.
 *
 * During a transition this function will also return the current target.
 *
 * This function is useful when called from handlers of the
 * #ClutterState::completed signal.
 * 
 * Return value: a string containing the target state. The returned string
 *   is owned by the #ClutterState and should not be modified or freed
 *
 * Since: 1.4
 */
G_CONST_RETURN gchar *
clutter_state_get_target_state (ClutterState *state)
{
  g_return_val_if_fail (CLUTTER_IS_STATE (state), NULL);

  return state->priv->target_state_name;
}
