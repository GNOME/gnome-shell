/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Intel Corporation
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
 *   Øyvind Kolås <pippin@linux.intel.com>
 */

/**
 * SECTION:clutter-animator
 * @short_description: Multi-actor tweener
 * @See_Also: #ClutterAnimatable, #ClutterInterval, #ClutterAlpha,
 *   #ClutterTimeline
 *
 * #ClutterAnimator is an object providing declarative animations for
 * #GObject properties belonging to one or more #GObject<!-- -->s to
 * #ClutterIntervals.
 *
 * #ClutterAnimator is used to build and describe complex animations
 * in terms of "key frames". #ClutterAnimator is meant to be used
 * through the #ClutterScript definition format, but it comes with a
 * convenience C API.
 *
 * <refsect2 id="ClutterAnimator-script">
 *   <title>ClutterAnimator description for #ClutterScript</title>
 *   <para>#ClutterAnimator defines a custom "properties" property
 *   which allows describing the key frames for objects.</para>
 *   <para>The "properties" property has the following syntax:</para>
 *   <informalexample>
 *     <programlisting>
 *  {
 *    "properties" : [
 *      {
 *        "object" : &lt;id of an object&gt;,
 *        "name" : &lt;name of the property&gt;,
 *        "ease-in" : &lt;boolean&gt;,
 *        "interpolation" : &lt;#ClutterInterpolation value&gt;,
 *        "keys" : [
 *          [ &lt;progress&gt;, &lt;easing mode&gt;, &lt;final value&gt; ]
 *        ]
 *    ]
 *  }
 *     </programlisting>
 *   </informalexample>
 *   <example id="ClutterAnimator-script-example">
 *     <title>ClutterAnimator definition</title>
 *     <para>The following JSON fragment defines a #ClutterAnimator
 *     with the duration of 1 second and operating on the x and y
 *     properties of a #ClutterActor named "rect-01", with two frames
 *     for each property. The first frame will linearly move the actor
 *     from its current position to the 100, 100 position in 20 percent
 *     of the duration of the animation; the second will using a cubic
 *     easing to move the actor to the 200, 200 coordinates.</para>
 *     <programlisting>
 *  {
 *    "type" : "ClutterAnimator",
 *    "duration" : 1000,
 *    "properties" : [
 *      {
 *        "object" : "rect-01",
 *        "name" : "x",
 *        "ease-in" : true,
 *        "keys" : [
 *          [ 0.2, "linear",       100.0 ],
 *          [ 1.0, "easeOutCubic", 200.0 ]
 *        ]
 *      },
 *      {
 *        "object" : "rect-01",
 *        "name" : "y",
 *        "ease-in" : true,
 *        "keys" : [
 *          [ 0.2, "linear",       100.0 ],
 *          [ 1.0, "easeOutCubic", 200.0 ]
 *        ]
 *      }
 *    ]
 *  }
 *     </programlisting>
 *   </example>
 * </refsect2>
 *
 * #ClutterAnimator is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gobject/gvaluecollector.h>

#include "clutter-animator.h"

#include "clutter-alpha.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-interval.h"
#include "clutter-private.h"
#include "clutter-script-private.h"
#include "clutter-scriptable.h"

#define CLUTTER_ANIMATOR_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_ANIMATOR, ClutterAnimatorPrivate))

struct _ClutterAnimatorPrivate
{
  ClutterTimeline  *timeline;
  ClutterTimeline  *slave_timeline;

  GList            *score;

  GHashTable       *properties;
};

struct _ClutterAnimatorKey
{
  GObject             *object;
  const gchar         *property_name;
  guint                mode;

  GValue               value;

  /* normalized progress, between 0.0 and 1.0 */
  gdouble              progress;

  /* back-pointer to the animator which owns the key */
  ClutterAnimator     *animator;

  /* interpolation mode */
  ClutterInterpolation interpolation;

  /* ease from the current object state into the animation when it starts */
  guint                ease_in : 1;

  /* This key is already being destroyed and shouldn't
   * trigger additional weak unrefs
   */
  guint                is_inert : 1;

  gint                 ref_count;
};

enum
{
  PROP_0,

  PROP_DURATION
};

static void clutter_scriptable_init (ClutterScriptableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterAnimator,
                         clutter_animator,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_init));
/**
 * clutter_animator_new:
 *
 * Create a new #ClutterAnimator instance.
 *
 * Returns: a new #ClutterAnimator.
 */
ClutterAnimator *
clutter_animator_new (void)
{
  return g_object_new (CLUTTER_TYPE_ANIMATOR, NULL);
}

/***/

typedef struct _PropObjectKey {
  GObject      *object;
  const gchar  *property_name;
  guint         mode;
  gdouble       progress;
} PropObjectKey;

typedef struct _KeyAnimator {
  PropObjectKey       *key;
  ClutterInterval     *interval;
  ClutterAlpha        *alpha;

  GList               *current;

  gdouble              start;    /* the progress of current */
  gdouble              end;      /* until which progress it is valid */
  ClutterInterpolation interpolation;

  guint                ease_in : 1;
} KeyAnimator;

static PropObjectKey *
prop_actor_key_new (GObject     *object,
                    const gchar *property_name)
{
  PropObjectKey *key = g_slice_new0 (PropObjectKey);

  key->object = object;
  key->property_name = g_intern_string (property_name);

  return key;
}

static void
prop_actor_key_free (gpointer key)
{
  if (key != NULL)
    g_slice_free (PropObjectKey, key);
}

static void
key_animator_free (gpointer key)
{
  if (key != NULL)
    {
      KeyAnimator *key_animator = key;

      g_object_unref (key_animator->interval);
      g_object_unref (key_animator->alpha);

      g_slice_free (KeyAnimator, key_animator);
    }
}

static KeyAnimator *
key_animator_new (ClutterAnimator *animator,
                  PropObjectKey   *key,
                  GType            type)
{
  KeyAnimator *key_animator = g_slice_new (KeyAnimator);
  ClutterInterval *interval = g_object_new (CLUTTER_TYPE_INTERVAL,
                                            "value-type", type,
                                            NULL);

  /* we own this interval */
  g_object_ref_sink (interval);

  key_animator->interval = interval;
  key_animator->key = key;
  key_animator->alpha = clutter_alpha_new ();
  clutter_alpha_set_timeline (key_animator->alpha,
                              animator->priv->slave_timeline);

  /* as well as the alpha */
  g_object_ref_sink (key_animator->alpha);

  return key_animator;
}

static guint
prop_actor_hash (gconstpointer value)
{
  const PropObjectKey *info = value;

  return GPOINTER_TO_INT (info->property_name)
       ^ GPOINTER_TO_INT (info->object);
}

static gboolean
prop_actor_equal (gconstpointer a, gconstpointer b)
{
  const PropObjectKey *infoa = a;
  const PropObjectKey *infob = b;

  /* property name strings are interned so we can just compare pointers */
  if (infoa->object == infob->object &&
      (infoa->property_name == infob->property_name))
    return TRUE;

  return FALSE;
}

static gint
sort_actor_prop_progress_func (gconstpointer a,
                               gconstpointer b)
{
  const ClutterAnimatorKey *pa = a;
  const ClutterAnimatorKey *pb = b;

  if (pa->object == pb->object)
    {
      gint pdiff = pb->property_name - pa->property_name;

      if (pdiff)
        return pdiff;

      if (pa->progress == pb->progress)
        return 0;

      if (pa->progress > pb->progress)
        return 1;

      return -1;
    }

  return pa->object - pb->object;
}

static gint
sort_actor_prop_func (gconstpointer a,
                      gconstpointer b)
{
  const ClutterAnimatorKey *pa = a;
  const ClutterAnimatorKey *pb = b;

  if (pa->object == pb->object)
    return pa->property_name - pb->property_name;

  return pa->object - pb->object;
}


static void
object_disappeared (gpointer  data,
                    GObject  *where_the_object_was)
{
  clutter_animator_remove_key (data, where_the_object_was, NULL, -1.0);
}

static ClutterAnimatorKey *
clutter_animator_key_new (ClutterAnimator *animator,
                          GObject         *object,
                          const gchar     *property_name,
                          gdouble          progress,
                          guint            mode)
{
  ClutterAnimatorKey *animator_key;

  animator_key = g_slice_new (ClutterAnimatorKey);

  animator_key->ref_count = 1;
  animator_key->animator = animator;
  animator_key->object = object;
  animator_key->mode = mode;
  memset (&(animator_key->value), 0, sizeof (GValue));
  animator_key->progress = progress;
  animator_key->property_name = g_intern_string (property_name);
  animator_key->interpolation = CLUTTER_INTERPOLATION_LINEAR;
  animator_key->ease_in = FALSE;
  animator_key->is_inert = FALSE;

  /* keep a weak reference on the animator, so that we can release the
   * back-pointer when needed
   */
  g_object_weak_ref (object, object_disappeared,
                     animator_key->animator);

  return animator_key;
}

static gpointer
clutter_animator_key_copy (gpointer boxed)
{
  ClutterAnimatorKey *key = boxed;

  if (key != NULL)
    key->ref_count += 1;

  return key;
}

static void
clutter_animator_key_free (gpointer boxed)
{
  ClutterAnimatorKey *key = boxed;

  if (key == NULL)
    return;

  key->ref_count -= 1;

  if (key->ref_count > 0)
    return;

  if (!key->is_inert)
    g_object_weak_unref (key->object, object_disappeared, key->animator);

  g_slice_free (ClutterAnimatorKey, key);
}

static void
clutter_animator_finalize (GObject *object)
{
  ClutterAnimator *animator = CLUTTER_ANIMATOR (object);
  ClutterAnimatorPrivate *priv = animator->priv;

  g_list_foreach (priv->score, (GFunc) clutter_animator_key_free, NULL);
  g_list_free (priv->score);
  priv->score = NULL;

#if 0
  for (; priv->score;
       priv->score = g_list_remove (priv->score, priv->score->data))
    {
      clutter_animator_key_free (priv->score->data);
    }
#endif

  g_object_unref (priv->timeline);
  g_object_unref (priv->slave_timeline);

  G_OBJECT_CLASS (clutter_animator_parent_class)->finalize (object);
}

/* XXX: this is copied and slightly modified from glib,
 * there is only one way to do this. */
static GList *
list_find_custom_reverse (GList         *list,
                          gconstpointer  data,
                          GCompareFunc   func)
{
  while (list)
    {
      if (! func (list->data, data))
        return list;

      list = list->prev;
    }

  return NULL;
}

/* Ensures that the interval provided by the animator is correct
 * for the requested progress value.
 */
static void
animation_animator_ensure_animator (ClutterAnimator *animator,
                                    KeyAnimator     *key_animator,
                                    PropObjectKey   *key,
                                    gdouble          progress)
{

  if (progress > key_animator->end)
    {
      while (progress > key_animator->end)
        {
          ClutterAnimatorKey *initial_key, *next_key;
          GList *initial, *next;

          initial = g_list_find_custom (key_animator->current->next,
                                        key,
                                        sort_actor_prop_func);
          g_assert (initial != NULL);

          initial_key = initial->data;

          clutter_interval_set_initial_value (key_animator->interval,
                                              &initial_key->value);
          key_animator->current = initial;
          key_animator->start = initial_key->progress;

          next = g_list_find_custom (initial->next,
                                     key,
                                     sort_actor_prop_func);
          if (next)
            {
              next_key = next->data;

              key_animator->end = next_key->progress;
            }
          else
            {
              next_key = initial_key;

              key_animator->end = 1.0;
            }

          clutter_interval_set_final_value (key_animator->interval,
                                            &next_key->value);

          if ((clutter_alpha_get_mode (key_animator->alpha) != next_key->mode))
            clutter_alpha_set_mode (key_animator->alpha, next_key->mode);
        }
    }
  else if (progress < key_animator->start)
    {
      while (progress < key_animator->start)
        {
          ClutterAnimatorKey *initial_key, *next_key;
          GList *initial;
          GList *old = key_animator->current;

          initial = list_find_custom_reverse (key_animator->current->prev,
                                              key,
                                              sort_actor_prop_func);
          g_assert (initial != NULL);

          initial_key = initial->data;

          clutter_interval_set_initial_value (key_animator->interval,
                                              &initial_key->value);
          key_animator->current = initial;
          key_animator->end = key_animator->start;
          key_animator->start = initial_key->progress;

          if (old)
            {
              next_key = old->data;

              key_animator->end = next_key->progress;
            }
          else
            {
              next_key = initial_key;

              key_animator->end = 1.0;
            }

          clutter_interval_set_final_value (key_animator->interval,
                                            &next_key->value);
          if ((clutter_alpha_get_mode (key_animator->alpha) != next_key->mode))
            clutter_alpha_set_mode (key_animator->alpha, next_key->mode);
        }
    }
}

/* XXX - this might be useful as an internal function exposed somewhere */
static gdouble
cubic_interpolation (const gdouble dx,
                     const gdouble prev,
                     const gdouble j,
                     const gdouble next,
                     const gdouble nextnext)
{
  return (((( - prev + 3 * j - 3 * next + nextnext ) * dx +
            ( 2 * prev - 5 * j + 4 * next - nextnext ) ) * dx +
            ( - prev + next ) ) * dx + (j + j) ) / 2.0;
}

/* try to get a floating point key value from a key for a property,
 * failing use the closest key in that direction or the starting point.
 */
static gfloat
list_try_get_rel (GList *list,
                  gint   count)
{
  ClutterAnimatorKey *key;
  GList *iter = list;
  GList *best = list;

  if (count > 0)
    {
      while (count -- && iter != NULL)
        {
          iter = g_list_find_custom (iter->next, list->data,
                                     sort_actor_prop_func);
          if (iter != NULL)
            best = iter;
        }
    }
  else
    {
      while (count ++ < 0 && iter != NULL)
        {
          iter = list_find_custom_reverse (iter->prev, list->data,
                                           sort_actor_prop_func);
          if (iter != NULL)
            best = iter;
        }
    }

  if (best != NULL && best->data != NULL)
    {
      key = best->data;

      return g_value_get_float (&(key->value));
    }

  return 0;
}

static void
animation_animator_new_frame (ClutterTimeline  *timeline,
                              gint              msecs,
                              ClutterAnimator  *animator)
{
  gdouble progress;
  GHashTableIter iter;
  gpointer key, value;

  progress  = 1.0 * msecs / clutter_timeline_get_duration (timeline);

  /* for each property that is managed figure out the GValue to set,
   * avoid creating new ClutterInterval's for each interval crossed
   */
  g_hash_table_iter_init (&iter, animator->priv->properties);

  key = value = NULL;
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PropObjectKey      *prop_actor_key = key;
      KeyAnimator        *key_animator   = value;
      ClutterAnimatorKey *start_key;
      gdouble             sub_progress;

      animation_animator_ensure_animator (animator, key_animator,
                                          key,
                                          progress);
      start_key = key_animator->current->data;

      sub_progress = (progress - key_animator->start)
                   / (key_animator->end - key_animator->start);

      /* do not change values if we're not active yet (delay) */
      if (sub_progress >= 0.0 && sub_progress <= 1.0)
        {
          GValue cvalue = { 0, };
          GType int_type;

          g_value_init (&cvalue, G_VALUE_TYPE (&start_key->value));

          clutter_timeline_advance (animator->priv->slave_timeline,
                                    sub_progress * 10000);

          sub_progress = clutter_alpha_get_alpha (key_animator->alpha);
          int_type = clutter_interval_get_value_type (key_animator->interval);

          if (key_animator->interpolation == CLUTTER_INTERPOLATION_CUBIC &&
              int_type == G_TYPE_FLOAT)
            {
              gdouble prev, current, next, nextnext;
              gdouble res;

              if ((key_animator->ease_in == FALSE ||
                  (key_animator->ease_in &&
                   list_find_custom_reverse (key_animator->current->prev,
                                             key_animator->current->data,
                                             sort_actor_prop_func))))
                {
                  current = g_value_get_float (&start_key->value);
                  prev = list_try_get_rel (key_animator->current, -1);
                }
              else
                {
                  /* interpolated and easing in */
                  clutter_interval_get_initial_value (key_animator->interval,
                                                      &cvalue);
                  prev = current = g_value_get_float (&cvalue);
                }

               next = list_try_get_rel (key_animator->current, 1);
               nextnext = list_try_get_rel (key_animator->current, 2);
               res = cubic_interpolation (sub_progress, prev, current, next,
                                          nextnext);

               g_value_set_float (&cvalue, res);
            }
          else
            clutter_interval_compute_value (key_animator->interval,
                                            sub_progress,
                                            &cvalue);

          g_object_set_property (prop_actor_key->object,
                                 prop_actor_key->property_name,
                                 &cvalue);

          g_value_unset (&cvalue);
        }
    }
}

static void
animation_animator_started (ClutterTimeline *timeline,
                            ClutterAnimator *animator)
{
  GList *k;

  /* Ensure that animators exist for all involved properties */
  for (k = animator->priv->score; k != NULL; k = k->next)
    {
      ClutterAnimatorKey *key = k->data;
      KeyAnimator        *key_animator;
      PropObjectKey      *prop_actor_key;

      prop_actor_key = prop_actor_key_new (key->object, key->property_name);
      key_animator = g_hash_table_lookup (animator->priv->properties,
                                          prop_actor_key);
      if (key_animator)
        {
          prop_actor_key_free (prop_actor_key);
        }
      else
        {
          GObjectClass *klass = G_OBJECT_GET_CLASS (key->object);
          GParamSpec *pspec;

          pspec = g_object_class_find_property (klass, key->property_name);

          key_animator = key_animator_new (animator, prop_actor_key,
                                           G_PARAM_SPEC_VALUE_TYPE (pspec));
          g_hash_table_insert (animator->priv->properties,
                               prop_actor_key,
                               key_animator);
        }
    }

  /* initialize animator with initial list pointers */
  {
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init (&iter, animator->priv->properties);
    while (g_hash_table_iter_next (&iter, &key, &value))
      {
        KeyAnimator *key_animator = value;
        ClutterAnimatorKey *initial_key, *next_key;
        GList *initial;
        GList *next;

        initial = g_list_find_custom (animator->priv->score,
                                      key,
                                      sort_actor_prop_func);
        g_assert (initial != NULL);
        initial_key = initial->data;
        clutter_interval_set_initial_value (key_animator->interval,
                                            &initial_key->value);

        key_animator->current       = initial;
        key_animator->start         = initial_key->progress;
        key_animator->ease_in       = initial_key->ease_in;
        key_animator->interpolation = initial_key->interpolation;

        if (key_animator->ease_in)
          {
            GValue cvalue = { 0, };
            GType int_type;

            int_type = clutter_interval_get_value_type (key_animator->interval);
            g_value_init (&cvalue, int_type);

            g_object_get_property (initial_key->object,
                                   initial_key->property_name,
                                   &cvalue);

            clutter_interval_set_initial_value (key_animator->interval,
                                                &cvalue);

            g_value_unset (&cvalue);
          }

        next = g_list_find_custom (initial->next, key, sort_actor_prop_func);
        if (next)
          {
            next_key = next->data;
            key_animator->end = next_key->progress;
          }
        else
          {
            next_key = initial_key;
            key_animator->end = 1.0;
          }

        clutter_interval_set_final_value (key_animator->interval,
                                          &next_key->value);
        if ((clutter_alpha_get_mode (key_animator->alpha) != next_key->mode))
          clutter_alpha_set_mode (key_animator->alpha, next_key->mode);
      }
  }
}

/**
 * clutter_animator_set_timeline:
 * @animator: a #ClutterAnimator
 * @timeline: a #ClutterTimeline
 *
 * Sets an external timeline that will be used for driving the animation
 *
 * Since: 1.2
 */
void
clutter_animator_set_timeline (ClutterAnimator *animator,
                               ClutterTimeline *timeline)
{
  ClutterAnimatorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ANIMATOR (animator));

  priv = animator->priv;

  if (priv->timeline != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->timeline,
                                            animation_animator_new_frame,
                                            animator);
      g_signal_handlers_disconnect_by_func (priv->timeline,
                                            animation_animator_started,
                                            animator);
      g_object_unref (priv->timeline);
    }

  priv->timeline = timeline;
  if (timeline != NULL)
    {
      g_object_ref_sink (priv->timeline);

      g_signal_connect (priv->timeline, "new-frame",
                        G_CALLBACK (animation_animator_new_frame),
                        animator);
      g_signal_connect (priv->timeline, "started",
                        G_CALLBACK (animation_animator_started),
                        animator);
    }
}

/**
 * clutter_animator_get_timeline:
 * @animator: a #ClutterAnimator
 *
 * Get the timeline hooked up for driving the #ClutterAnimator
 *
 * Return value: the #ClutterTimeline that drives the animator.
 */
ClutterTimeline *
clutter_animator_get_timeline (ClutterAnimator *animator)
{
  g_return_val_if_fail (CLUTTER_IS_ANIMATOR (animator), NULL);
  return animator->priv->timeline;
}

/**
 * clutter_animator_run:
 * @animator: a #ClutterAnimator
 *
 * Start the ClutterAnimator, this is a thin wrapper that rewinds
 * and starts the animators current timeline.
 *
 * Return value: the #ClutterTimeline that drives the animator.
 */
ClutterTimeline *
clutter_animator_run (ClutterAnimator *animator)
{
  ClutterAnimatorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ANIMATOR (animator), NULL);

  priv = animator->priv;

  clutter_timeline_rewind (priv->timeline);
  clutter_timeline_start (priv->timeline);

  return priv->timeline;
}

/**
 * clutter_animator_set_duration:
 * @animator: a #ClutterAnimator
 * @duration: milliseconds a run of the animator should last.
 *
 * Runs the timeline of the #ClutterAnimator with a duration in msecs
 * as specified.
 *
 * Since: 1.2
 */
void
clutter_animator_set_duration (ClutterAnimator *animator,
                               guint            duration)
{
  g_return_if_fail (CLUTTER_IS_ANIMATOR (animator));

  clutter_timeline_set_duration (animator->priv->timeline, duration);
}

/**
 * clutter_animator_get_duration:
 * @animator: a #ClutterAnimator
 *
 * Retrieves the current duration of an animator
 *
 * Return value: the duration of the animation, in milliseconds
 *
 * Since: 1.2
 */
guint
clutter_animator_get_duration  (ClutterAnimator *animator)
{
  g_return_val_if_fail (CLUTTER_IS_ANIMATOR (animator), 0);

  return clutter_timeline_get_duration (animator->priv->timeline);
}

/**
 * clutter_animator_set:
 * @animator: a #ClutterAnimator
 * @first_object: a #GObject
 * @first_property_name: the property to specify a key for
 * @first_mode: the id of the alpha function to use
 * @first_progress: at which stage of the animation this value applies; the
 *   range is a normalized floating point value between 0 and 1
 * @VarArgs: the value first_property_name should have for first_object
 *   at first_progress, followed by more (object, property_name, mode,
 *   progress, value) tuples, followed by %NULL
 *
 * Adds multiple keys to a #ClutterAnimator, specifying the value a given
 * property should have at a given progress of the animation. The mode
 * specified is the mode used when going to this key from the previous key of
 * the @property_name
 *
 * If a given (object, property, progress) tuple already exist the mode and
 * value will be replaced with the new values.
 *
 * Since: 1.2
 */
void
clutter_animator_set (ClutterAnimator *animator,
                      gpointer         first_object,
                      const gchar     *first_property_name,
                      guint            first_mode,
                      gdouble          first_progress,
                      ...)
{
  GObject      *object;
  const gchar  *property_name;
  guint         mode;
  gdouble       progress;
  va_list       args;

  g_return_if_fail (CLUTTER_IS_ANIMATOR (animator));

  object = first_object;
  property_name = first_property_name;
  mode = first_mode;
  progress = first_progress;

  va_start (args, first_progress);

  while (object != NULL)
    {
      GParamSpec *pspec;
      GObjectClass *klass;
      GValue value = { 0, };
      gchar *error = NULL;

      g_return_if_fail (object);
      g_return_if_fail (property_name);

      klass = G_OBJECT_GET_CLASS (object);
      pspec = g_object_class_find_property (klass, property_name);

      if (!pspec)
        {
          g_warning ("Cannot bind property '%s': object of type '%s' "
                     "do not have this property",
                     property_name, G_OBJECT_TYPE_NAME (object));
          break;
        }

#if GLIB_CHECK_VERSION (2, 23, 2)
      G_VALUE_COLLECT_INIT (&value, G_PARAM_SPEC_VALUE_TYPE (pspec),
                            args, 0,
                            &error);
#else
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, args, 0, &error);
#endif /* GLIB_CHECK_VERSION (2, 23, 2) */

      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      clutter_animator_set_key (animator,
                                object,
                                property_name,
                                mode,
                                progress,
                                &value);

      object= va_arg (args, GObject *);
      if (object)
        {
          property_name = va_arg (args, gchar*);
          if (!property_name)
           {
             g_warning ("%s: expected a property name", G_STRLOC);
             break;
           }
          mode = va_arg (args, guint);
          progress = va_arg (args, gdouble);
        }
    }

  va_end (args);
}

static inline void
clutter_animator_set_key_internal (ClutterAnimator    *animator,
                                   ClutterAnimatorKey *key)
{
  ClutterAnimatorPrivate *priv = animator->priv;
  GList *old_item;

  old_item = g_list_find_custom (priv->score, key,
                                 sort_actor_prop_progress_func);

  /* replace the key if we already have a similar one */
  if (old_item != NULL)
    {
      ClutterAnimatorKey *old_key = old_item->data;

      clutter_animator_key_free (old_key);

      priv->score = g_list_remove (priv->score, old_key);
    }

  priv->score = g_list_insert_sorted (priv->score, key,
                                      sort_actor_prop_progress_func);
}

/**
 * clutter_animator_set_key:
 * @animator: a #ClutterAnimator
 * @object: a #GObject
 * @property_name: the property to specify a key for
 * @mode: the id of the alpha function to use
 * @progress: the normalized range at which stage of the animation this
 *   value applies
 * @value: the value property_name should have at progress.
 *
 * Sets a single key in the #ClutterAnimator for the @property_name of
 * @object at @progress.
 *
 * See also: clutter_animator_set()
 *
 * Return value: (transfer none): The animator instance
 *
 * Since: 1.2
 */
ClutterAnimator *
clutter_animator_set_key (ClutterAnimator *animator,
                          GObject         *object,
                          const gchar     *property_name,
                          guint            mode,
                          gdouble          progress,
                          const GValue    *value)
{
  ClutterAnimatorKey *animator_key;

  g_return_val_if_fail (CLUTTER_IS_ANIMATOR (animator), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (property_name, NULL);
  g_return_val_if_fail (value, NULL);

  property_name = g_intern_string (property_name);

  animator_key = clutter_animator_key_new (animator,
                                           object, property_name,
                                           progress,
                                           mode);

  g_value_init (&animator_key->value, G_VALUE_TYPE (value));
  g_value_copy (value, &animator_key->value);

  clutter_animator_set_key_internal (animator, animator_key);

  return animator;
}

/**
 * clutter_animator_get_keys:
 * @animator: a #ClutterAnimator instance
 * @object: a #GObject to search for or NULL for all
 * @property_name: a specific property name to query for or NULL for all
 * @progress: a specific progress to search for or a negative value for all
 *
 * Returns a list of pointers to opaque structures with accessor functions
 * that describe the keys added to an animator.
 *
 * Return value: (transfer container) (element-type ClutterAnimatorKey): a
 *   list of #ClutterAnimatorKey<!-- -->s; the contents of the list are owned
 *   by the #ClutterAnimator, but you should free the returned list when done,
 *   using g_list_free()
 *
 * Since: 1.2
 */
GList *
clutter_animator_get_keys (ClutterAnimator *animator,
                           GObject         *object,/* or NULL for all */
                           const gchar     *property_name,
                           gdouble          progress)
{
  GList *keys = NULL;
  GList *k;

  g_return_val_if_fail (CLUTTER_IS_ANIMATOR (animator), NULL);

  property_name = g_intern_string (property_name);

  for (k = animator->priv->score; k; k = k->next)
    {
      ClutterAnimatorKey *key = k->data;

      if ((object == NULL || (object == key->object)) &&
          (property_name == NULL || ((property_name == key->property_name))) &&
          (progress < 0          || (progress == key->progress)))
        {
          keys = g_list_prepend (keys, key);
        }
    }

  return g_list_reverse (keys);
}

/**
 * clutter_animator_remove:
 * @object: a #GObject to search for or NULL for all
 * @property_name: a specific property name to query for or NULL for all
 * @progress: a specific progress to search for or a negative value for all
 *
 * Removes all keys matching the conditions specificed in the arguments.
 *
 * Since: 1.2
 */
void
clutter_animator_remove_key (ClutterAnimator *animator,
                             GObject         *object,
                             const gchar     *property_name,
                             gdouble          progress)
{
  ClutterAnimatorPrivate *priv;
  GList *k;

  g_return_if_fail (CLUTTER_IS_ANIMATOR (animator));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property_name != NULL);

  property_name = g_intern_string (property_name);

  priv = animator->priv;

  for (k = priv->score; k != NULL; k = k->next)
    {
      ClutterAnimatorKey *key = k->data;

      if ((object == NULL        || (object == key->object)) &&
          (property_name == NULL || ((property_name == key->property_name))) &&
          (progress < 0          || (progress == key->progress))
         )
        {
          key->is_inert = TRUE;

          clutter_animator_key_free (key);

          /* FIXME: non performant since we reiterate the list many times */
          k = priv->score = g_list_remove (priv->score, key);
        }
    }

  if (object)
    {
      GHashTableIter iter;
      gpointer key, value;

again:
      g_hash_table_iter_init (&iter, priv->properties);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          PropObjectKey *prop_actor_key = key;
          if (prop_actor_key->object == object)
            {
              g_hash_table_remove (priv->properties, key);
              goto again;
            }
        }
    }
}

typedef struct _ParseClosure {
  ClutterAnimator *animator;
  ClutterScript *script;

  GValue *value;

  gboolean result;
} ParseClosure;

static ClutterInterpolation
resolve_interpolation (JsonNode *node)
{
  if ((JSON_NODE_TYPE (node) != JSON_NODE_VALUE))
    return CLUTTER_INTERPOLATION_LINEAR;

  if (json_node_get_value_type (node) == G_TYPE_INT64)
    {
      return json_node_get_int (node);
    }
  else if (json_node_get_value_type (node) == G_TYPE_STRING)
    {
      const gchar *str = json_node_get_string (node);
      gboolean res;
      gint enum_value;

      res = clutter_script_enum_from_string (CLUTTER_TYPE_INTERPOLATION,
                                             str,
                                             &enum_value);
      if (res)
        return enum_value;
    }

  return CLUTTER_INTERPOLATION_LINEAR;
}

static void
parse_animator_property (JsonArray *array,
                         guint      index_,
                         JsonNode  *element,
                         gpointer   data)
{
  ParseClosure *clos = data;
  JsonObject *object;
  JsonArray *keys;
  GObject *gobject;
  const gchar *id, *pname;
  GObjectClass *klass;
  GParamSpec *pspec;
  GSList *valid_keys = NULL;
  GList *k;
  ClutterInterpolation interpolation = CLUTTER_INTERPOLATION_LINEAR;
  gboolean ease_in = FALSE;

  if (JSON_NODE_TYPE (element) != JSON_NODE_OBJECT)
    {
      g_warning ("The 'properties' member of a ClutterAnimator description "
                 "should be an array of objects, but the element %d of the "
                 "array is of type '%s'. The element will be ignored.",
                 index_,
                 json_node_type_name (element));
      return;
    }

  object = json_node_get_object (element);

  if (!json_object_has_member (object, "object") ||
      !json_object_has_member (object, "name") ||
      !json_object_has_member (object, "keys"))
    {
      g_warning ("The property description at index %d is missing one of "
                 "the mandatory fields: object, name and keys",
                 index_);
      return;
    }

  id = json_object_get_string_member (object, "object");
  gobject = clutter_script_get_object (clos->script, id);
  if (gobject == NULL)
    {
      g_warning ("No object with id '%s' has been defined.", id);
      return;
    }

  pname = json_object_get_string_member (object, "name");
  klass = G_OBJECT_GET_CLASS (gobject);
  pspec = g_object_class_find_property (klass, pname);
  if (pspec == NULL)
    {
      g_warning ("The object of type '%s' and name '%s' has no "
                 "property named '%s'",
                 G_OBJECT_TYPE_NAME (gobject),
                 id,
                 pname);
      return;
    }

  if (json_object_has_member (object, "ease-in"))
    ease_in = json_object_get_boolean_member (object, "ease-in");

  if (json_object_has_member (object, "interpolation"))
    {
      JsonNode *node = json_object_get_member (object, "interpolation");

      interpolation = resolve_interpolation (node);
    }

  keys = json_object_get_array_member (object, "keys");
  if (keys == NULL)
    {
      g_warning ("The property description at index %d has an invalid "
                 "key field of type '%s' when an array was expected.",
                 index_,
                 json_node_type_name (json_object_get_member (object, "keys")));
      return;
    }

  if (G_IS_VALUE (clos->value))
    valid_keys = g_slist_reverse (g_value_get_pointer (clos->value));
  else
    g_value_init (clos->value, G_TYPE_POINTER);

  for (k = json_array_get_elements (keys);
       k != NULL;
       k = k->next)
    {
      JsonNode *node = k->data;
      JsonArray *key = json_node_get_array (node);
      ClutterAnimatorKey *animator_key;
      gdouble progress;
      gulong mode;
      gboolean res;

      progress = json_array_get_double_element (key, 0);
      mode = clutter_script_resolve_animation_mode (json_array_get_element (key, 1));

      animator_key = clutter_animator_key_new (clos->animator,
                                               gobject,
                                               pname,
                                               progress,
                                               mode);

      res = clutter_script_parse_node (clos->script,
                                       &(animator_key->value),
                                       pname,
                                       json_array_get_element (key, 2),
                                       pspec);
      if (!res)
        {
          g_warning ("Unable to parse the key value for the "
                     "property '%s' (progress: %.2f) at index %d",
                     pname,
                     progress,
                     index_);
          continue;
        }

      animator_key->ease_in = ease_in;
      animator_key->interpolation = interpolation;

      valid_keys = g_slist_prepend (valid_keys, animator_key);
    }

  g_value_set_pointer (clos->value, g_slist_reverse (valid_keys));

  clos->result = TRUE;
}

static gboolean
clutter_animator_parse_custom_node (ClutterScriptable *scriptable,
                                    ClutterScript     *script,
                                    GValue            *value,
                                    const gchar       *name,
                                    JsonNode          *node)
{
  ParseClosure parse_closure;

  if (strcmp (name, "properties") != 0)
    return FALSE;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return FALSE;

  parse_closure.animator = CLUTTER_ANIMATOR (scriptable);
  parse_closure.script = script;
  parse_closure.value = value;
  parse_closure.result = FALSE;

  json_array_foreach_element (json_node_get_array (node),
                              parse_animator_property,
                              &parse_closure);

  /* we return TRUE if we had at least one key parsed */

  return parse_closure.result;
}

static void
clutter_animator_set_custom_property (ClutterScriptable *scriptable,
                                      ClutterScript     *script,
                                      const gchar       *name,
                                      const GValue      *value)
{
  if (strcmp (name, "properties") == 0)
    {
      ClutterAnimator *animator = CLUTTER_ANIMATOR (scriptable);
      GSList *keys = g_value_get_pointer (value);
      GSList *k;

      for (k = keys; k != NULL; k = k->next)
        clutter_animator_set_key_internal (animator, k->data);

      g_slist_free (keys);
    }
  else
    g_object_set_property (G_OBJECT (scriptable), name, value);
}

static void
clutter_scriptable_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_animator_parse_custom_node;
  iface->set_custom_property = clutter_animator_set_custom_property;
}

static void
clutter_animator_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ClutterAnimator *self = CLUTTER_ANIMATOR (gobject);

  switch (prop_id)
    {
    case PROP_DURATION:
      clutter_animator_set_duration (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_animator_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ClutterAnimatorPrivate *priv = CLUTTER_ANIMATOR (gobject)->priv;

  switch (prop_id)
    {
    case PROP_DURATION:
      g_value_set_uint (value, clutter_timeline_get_duration (priv->timeline));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_animator_class_init (ClutterAnimatorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterAnimatorPrivate));

  gobject_class->set_property = clutter_animator_set_property;
  gobject_class->get_property = clutter_animator_get_property;
  gobject_class->finalize = clutter_animator_finalize;

  /**
   * ClutterAnimator:duration:
   *
   * The duration of the #ClutterTimeline used by the #ClutterAnimator
   * to drive the animation
   *
   * Since: 1.2
   */
  pspec = g_param_spec_uint ("duration",
                             "Duration",
                             "The duration of the animation",
                             0, G_MAXUINT,
                             2000,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_DURATION, pspec);
}

static void
clutter_animator_init (ClutterAnimator *animator)
{
  ClutterAnimatorPrivate *priv;

  animator->priv = priv = CLUTTER_ANIMATOR_GET_PRIVATE (animator);

  priv->properties = g_hash_table_new_full (prop_actor_hash,
                                            prop_actor_equal,
                                            prop_actor_key_free,
                                            key_animator_free);

  clutter_animator_set_timeline (animator, clutter_timeline_new (2000));

  priv->slave_timeline = clutter_timeline_new (10000);
  g_object_ref_sink (priv->slave_timeline);
}


/**
 * clutter_animator_property_get_ease_in:
 * @animator: a #ClutterAnimatorKey
 * @object: a #GObject
 * @property_name: the name of a property on object
 *
 * Checks if a property value is to be eased into the animation.
 *
 * Return value: %TRUE if the property is eased in
 *
 * Since: 1.2
 */
gboolean
clutter_animator_property_get_ease_in (ClutterAnimator *animator,
                                       GObject         *object,
                                       const gchar     *property_name)
{
  ClutterAnimatorKey  key, *initial_key;
  GList              *initial;

  g_return_val_if_fail (CLUTTER_IS_ANIMATOR (animator), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (property_name, FALSE);

  key.object        = object;
  key.property_name = g_intern_string (property_name);
  initial = g_list_find_custom (animator->priv->score, &key,
                                sort_actor_prop_func);
  if (initial != NULL)
    {
      initial_key = initial->data;

      return initial_key->ease_in;
    }

  return FALSE;
}

/**
 * clutter_animator_property_set_ease_in:
 * @animator: a #ClutterAnimatorKey
 * @object: a #GObject
 * @property_name: the name of a property on object
 * @ease_in: we are going to be easing in this property
 *
 * Sets whether a property value is to be eased into the animation.
 *
 * Since: 1.2
 */
void
clutter_animator_property_set_ease_in (ClutterAnimator *animator,
                                       GObject         *object,
                                       const gchar     *property_name,
                                       gboolean         ease_in)
{
  ClutterAnimatorKey  key, *initial_key;
  GList              *initial;

  g_return_if_fail (CLUTTER_IS_ANIMATOR (animator));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property_name);

  key.object        = object;
  key.property_name = g_intern_string (property_name);
  initial = g_list_find_custom (animator->priv->score, &key,
                                sort_actor_prop_func);
  if (initial)
    {
      initial_key = initial->data;
      initial_key->ease_in = ease_in;
    }
  else
    g_warning ("The animator has no object of type '%s' with a "
               "property named '%s'",
               G_OBJECT_TYPE_NAME (object),
               property_name);
}


/**
 * clutter_animator_property_set_interpolation:
 * @animator: a #ClutterAnimatorKey
 * @object: a #GObject
 * @property_name: the name of a property on object
 * @interpolation: the #ClutterInterpolation to use
 *
 * Get the interpolation used by animator for a property on a particular
 * object.
 *
 * Returns: a ClutterInterpolation value.
 * Since: 1.2
 */
ClutterInterpolation
clutter_animator_property_get_interpolation (ClutterAnimator      *animator,
                                             GObject              *object,
                                             const gchar          *property_name,
                                             ClutterInterpolation  interpolation)
{
  GList              *initial;
  ClutterAnimatorKey  key, *initial_key;

  g_return_val_if_fail (CLUTTER_IS_ANIMATOR (animator),
                        CLUTTER_INTERPOLATION_LINEAR);
  g_return_val_if_fail (G_IS_OBJECT (object),
                        CLUTTER_INTERPOLATION_LINEAR);
  g_return_val_if_fail (property_name,
                        CLUTTER_INTERPOLATION_LINEAR);

  key.object        = object;
  key.property_name = g_intern_string (property_name);
  initial = g_list_find_custom (animator->priv->score, &key,
                                sort_actor_prop_func);
  if (initial)
    {
      initial_key = initial->data;

      return initial_key->interpolation;
    }

  return CLUTTER_INTERPOLATION_LINEAR;
}

/**
 * clutter_animator_property_set_interpolation:
 * @animator: a #ClutterAnimatorKey
 * @object: a #GObject
 * @property_name: the name of a property on object
 * @interpolation: the #ClutterInterpolation to use
 *
 * Set the interpolation method to use, CLUTTER_INTERPOLATION_LINEAR causes the
 * values to linearly change between the values, CLUTTER_INTERPOLATION_CUBIC
 * causes the values to smoothly change between the values.
 *
 * Since: 1.2
 */
void
clutter_animator_property_set_interpolation (ClutterAnimator      *animator,
                                             GObject              *object,
                                             const gchar          *property_name,
                                             ClutterInterpolation  interpolation)
{
  GList              *initial;
  ClutterAnimatorKey  key, *initial_key;

  g_return_if_fail (CLUTTER_IS_ANIMATOR (animator));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property_name);

  key.object        = object;
  key.property_name = g_intern_string (property_name);
  initial = g_list_find_custom (animator->priv->score, &key,
                                sort_actor_prop_func);
  if (initial)
    {
      initial_key = initial->data;
      initial_key->interpolation = interpolation;
    }
}

GType
clutter_animator_key_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    our_type = g_boxed_type_register_static (I_("ClutterAnimatorKey"),
                                             clutter_animator_key_copy,
                                             clutter_animator_key_free);

  return our_type;
}


/**
 * clutter_animator_key_get_object:
 * @key: a #ClutterAnimatorKey
 *
 * Retrieves the object a key applies to.
 *
 * Return value: (transfer none): the object an animator_key exist for.
 *
 * Since: 1.2
 */
GObject *
clutter_animator_key_get_object (const ClutterAnimatorKey *key)
{
  g_return_val_if_fail (key != NULL, NULL);

  return key->object;
}

/**
 * clutter_animator_key_get_property_name:
 * @key: a #ClutterAnimatorKey
 *
 * Retrieves the name of the property a key applies to.
 *
 * Return value: the name of the property an animator_key exist for.
 *
 * Since: 1.2
 */
G_CONST_RETURN gchar *
clutter_animator_key_get_property_name (const ClutterAnimatorKey *key)
{
  g_return_val_if_fail (key != NULL, NULL);

  return key->property_name;
}

/**
 * clutter_animator_key_get_property_type:
 * @key: a #ClutterAnimatorKey
 *
 * Retrieves the #GType of the property a key applies to
 *
 * You can use this type to initialize the #GValue to pass to
 * clutter_animator_key_get_value()
 *
 * Return value: the #GType of the property
 *
 * Since: 1.2
 */
GType
clutter_animator_key_get_property_type (const ClutterAnimatorKey *key)
{
  g_return_val_if_fail (key != NULL, G_TYPE_INVALID);

  return G_VALUE_TYPE (&key->value);
}

/**
 * clutter_animator_key_get_mode:
 * @key: a #ClutterAnimatorKey
 *
 * Retrieves the mode of a #ClutterAnimator key, for the first key of a
 * property for an object this represents the whether the animation is
 * open ended and or curved for the remainding keys for the property it
 * represents the easing mode.
 *
 * Return value: the mode of a #ClutterAnimatorKey
 *
 * Since: 1.2
 */
gulong
clutter_animator_key_get_mode (const ClutterAnimatorKey *key)
{
  g_return_val_if_fail (key != NULL, 0);

  return key->mode;
}

/**
 * clutter_animator_key_get_progress:
 * @key: a #ClutterAnimatorKey
 *
 * Retrieves the progress of an clutter_animator_key
 *
 * Return value: the progress defined for a #ClutterAnimator key.
 *
 * Since: 1.2
 */
gdouble
clutter_animator_key_get_progress (const ClutterAnimatorKey *key)
{
  g_return_val_if_fail (key != NULL, 0.0);

  return key->progress;
}

/**
 * clutter_animator_key_get_value:
 * @key: a #ClutterAnimatorKey
 * @value: a #GValue initialized with the correct type for the animator key
 *
 * Retrieves a copy of the value for a #ClutterAnimatorKey.
 *
 * The passed in #GValue needs to be already initialized for the value
 * type of the key or to a type that allow transformation from the value
 * type of the key.
 *
 * Use g_value_unset() when done.
 *
 * Return value: %TRUE if the passed #GValue was successfully set, and
 *   %FALSE otherwise
 *
 * Since: 1.2
 */
gboolean
clutter_animator_key_get_value (const ClutterAnimatorKey *key,
                                GValue                   *value)
{
  GType gtype;

  g_return_val_if_fail (key != NULL, FALSE);

  gtype = G_VALUE_TYPE (&key->value);

  if (g_value_type_compatible (gtype, G_VALUE_TYPE (value)))
    {
      g_value_copy (&key->value, value);
      return TRUE;
    }

  if (g_value_type_transformable (gtype, G_VALUE_TYPE (value)))
    {
      if (g_value_transform (&key->value, value))
        return TRUE;
    }

  return FALSE;
}
