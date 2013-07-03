/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012 Intel Corporation
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-keyframe-transition
 * @Title: ClutterKeyframeTransition
 * @Short_Description: Keyframe property transition
 *
 * #ClutterKeyframeTransition allows animating a property by defining
 * "key frames": values at a normalized position on the transition
 * duration.
 *
 * The #ClutterKeyframeTransition interpolates the value of the property
 * to which it's bound across these key values.
 *
 * Setting up a #ClutterKeyframeTransition means providing the times,
 * values, and easing modes between these key frames, for instance:
 *
 * |[
 *   ClutterTransition *keyframe;
 *
 *   keyframe = clutter_keyframe_transition_new ("opacity");
 *   clutter_transition_set_from (keyframe, G_TYPE_UINT, 255);
 *   clutter_transition_set_to (keyframe, G_TYPE_UINT, 0);
 *   clutter_keyframe_transition_set (CLUTTER_KEYFRAME_TRANSITION (keyframe),
 *                                    G_TYPE_UINT,
 *                                    1, /&ast; number of key frames &ast;/
 *                                    0.5, 128, CLUTTER_EASE_IN_OUT_CUBIC);
 * ]|
 *
 * The example above sets up a keyframe transition for the #ClutterActor:opacity
 * property of a #ClutterActor; the transition starts and sets the value of the
 * property to fully transparent; between the start of the transition and its mid
 * point, it will animate the property to half opacity, using an easy in/easy out
 * progress. Once the transition reaches the mid point, it will linearly fade the
 * actor out until it reaches the end of the transition.
 *
 * The #ClutterKeyframeTransition will add an implicit key frame between the last
 * and the 1.0 value, to interpolate to the final value of the transition's
 * interval.
 *
 * #ClutterKeyframeTransition is available since Clutter 1.12.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-keyframe-transition.h"

#include "clutter-debug.h"
#include "clutter-easing.h"
#include "clutter-interval.h"
#include "clutter-private.h"
#include "clutter-timeline.h"

#include <math.h>
#include <gobject/gvaluecollector.h>

typedef struct _KeyFrame
{
  double key;

  double start;
  double end;

  ClutterAnimationMode mode;

  ClutterInterval *interval;
} KeyFrame;

struct _ClutterKeyframeTransitionPrivate
{
  GArray *frames;

  gint current_frame;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterKeyframeTransition,
                            clutter_keyframe_transition,
                            CLUTTER_TYPE_PROPERTY_TRANSITION)

static void
key_frame_free (gpointer data)
{
  if (data != NULL)
    {
      KeyFrame *key = data;

      g_object_unref (key->interval);
    }
}

static int
sort_by_key (gconstpointer a,
             gconstpointer b)
{
  const KeyFrame *k_a = a;
  const KeyFrame *k_b = b;

  if (fabs (k_a->key - k_b->key) < 0.0001)
    return 0;

  if (k_a->key > k_a->key)
    return 1;

  return -1;
}

static inline void
clutter_keyframe_transition_sort_frames (ClutterKeyframeTransition *transition)
{
  if (transition->priv->frames != NULL)
    g_array_sort (transition->priv->frames, sort_by_key);
}

static inline void
clutter_keyframe_transition_init_frames (ClutterKeyframeTransition *transition,
                                         gssize                     n_key_frames)
{
  ClutterKeyframeTransitionPrivate *priv = transition->priv;
  guint i;

  priv->frames = g_array_sized_new (FALSE, FALSE,
                                    sizeof (KeyFrame),
                                    n_key_frames);
  g_array_set_clear_func (priv->frames, key_frame_free);

  /* we add an implicit key frame that goes to 1.0, so that the
   * user doesn't have to do that an can simply add key frames
   * in between 0.0 and 1.0
   */
  for (i = 0; i < n_key_frames + 1; i++)
    {
      KeyFrame frame;

      if (i == n_key_frames)
        frame.key = 1.0;
      else
        frame.key = 0.0;

      frame.mode = CLUTTER_LINEAR;
      frame.interval = NULL;

      g_array_insert_val (priv->frames, i, frame);
    }
}

static inline void
clutter_keyframe_transition_update_frames (ClutterKeyframeTransition *transition)
{
  ClutterKeyframeTransitionPrivate *priv = transition->priv;
  guint i;

  if (priv->frames == NULL)
    return;

  for (i = 0; i < priv->frames->len; i++)
    {
      KeyFrame *cur_frame = &g_array_index (priv->frames, KeyFrame, i);
      KeyFrame *prev_frame;

      if (i > 0)
        prev_frame = &g_array_index (priv->frames, KeyFrame, i - 1);
      else
        prev_frame = NULL;

      if (prev_frame != NULL)
        {
          cur_frame->start = prev_frame->key;

          if (prev_frame->interval != NULL)
            {
              const GValue *value;

              value = clutter_interval_peek_final_value (prev_frame->interval);

              if (cur_frame->interval != NULL)
                clutter_interval_set_initial_value (cur_frame->interval, value);
              else
                {
                  cur_frame->interval =
                    clutter_interval_new_with_values (G_VALUE_TYPE (value), value, NULL);
                }
            }
        }
      else
        cur_frame->start = 0.0;

      cur_frame->end = cur_frame->key;
    }
}

static void
clutter_keyframe_transition_compute_value (ClutterTransition *transition,
                                           ClutterAnimatable *animatable,
                                           ClutterInterval   *interval,
                                           gdouble            progress)
{
  ClutterKeyframeTransition *self = CLUTTER_KEYFRAME_TRANSITION (transition);
  ClutterTimeline *timeline = CLUTTER_TIMELINE (transition);
  ClutterKeyframeTransitionPrivate *priv = self->priv;
  ClutterTransitionClass *parent_class;
  ClutterTimelineDirection direction;
  ClutterInterval *real_interval;
  gdouble real_progress;
  double t, d, p;
  KeyFrame *cur_frame = NULL;

  real_interval = interval;
  real_progress = progress;

  /* if we don't have any keyframe, we behave like our parent class */
  if (priv->frames == NULL)
    goto out;

  direction = clutter_timeline_get_direction (timeline);

  /* we need a normalized linear value */
  t = clutter_timeline_get_elapsed_time (timeline);
  d = clutter_timeline_get_duration (timeline);
  p = t / d;

  if (priv->current_frame < 0)
    {
      if (direction == CLUTTER_TIMELINE_FORWARD)
        priv->current_frame = 0;
      else
        priv->current_frame = priv->frames->len - 1;
    }

  cur_frame = &g_array_index (priv->frames, KeyFrame, priv->current_frame);

  /* skip to the next key frame, depending on the direction of the timeline */
  if (direction == CLUTTER_TIMELINE_FORWARD)
    {
      if (p > cur_frame->end)
        {
          priv->current_frame = MIN (priv->current_frame + 1,
                                     priv->frames->len - 1);

          cur_frame = &g_array_index (priv->frames, KeyFrame, priv->current_frame);
       }
    }
  else
    {
      if (p < cur_frame->start)
        {
          priv->current_frame = MAX (priv->current_frame - 1, 0);

          cur_frame = &g_array_index (priv->frames, KeyFrame, priv->current_frame);
        }
    }

  /* if we are at the boundaries of the transition, use the from and to
   * value from the transition
   */
  if (priv->current_frame == 0)
    {
      const GValue *value;

      value = clutter_interval_peek_initial_value (interval);
      clutter_interval_set_initial_value (cur_frame->interval, value);
    }
  else if (priv->current_frame == priv->frames->len - 1)
    {
      const GValue *value;

      cur_frame->mode = clutter_timeline_get_progress_mode (timeline);

      value = clutter_interval_peek_final_value (interval);
      clutter_interval_set_final_value (cur_frame->interval, value);
    }

  /* update the interval to be used to interpolate the property */
  real_interval = cur_frame->interval;

  /* normalize the progress */
  real_progress = (p - cur_frame->start) / (cur_frame->end - cur_frame->start);

#ifdef CLUTTER_ENABLE_DEBUG
  if (CLUTTER_HAS_DEBUG (ANIMATION))
    {
      char *from, *to;
      const GValue *value;

      value = clutter_interval_peek_initial_value (cur_frame->interval);
      from = g_strdup_value_contents (value);

      value = clutter_interval_peek_final_value (cur_frame->interval);
      to = g_strdup_value_contents (value);

      CLUTTER_NOTE (ANIMATION,
                    "cur_frame [%d] => { %g, %s, %s %s %s } - "
                    "progress: %g, sub-progress: %g\n",
                    priv->current_frame,
                    cur_frame->key,
                    clutter_get_easing_name_for_mode (cur_frame->mode),
                    from,
                    direction == CLUTTER_TIMELINE_FORWARD ? "->" : "<-",
                    to,
                    p, real_progress);

      g_free (from);
      g_free (to);
    }
#endif /* CLUTTER_ENABLE_DEBUG */

out:
  parent_class =
    CLUTTER_TRANSITION_CLASS (clutter_keyframe_transition_parent_class);
  parent_class->compute_value (transition, animatable, real_interval, real_progress);
}

static void
clutter_keyframe_transition_started (ClutterTimeline *timeline)
{
  ClutterKeyframeTransition *transition;

  transition = CLUTTER_KEYFRAME_TRANSITION (timeline);

  transition->priv->current_frame = -1;

  clutter_keyframe_transition_sort_frames (transition);
  clutter_keyframe_transition_update_frames (transition);
}

static void
clutter_keyframe_transition_completed (ClutterTimeline *timeline)
{
  ClutterKeyframeTransitionPrivate *priv;

  priv = CLUTTER_KEYFRAME_TRANSITION (timeline)->priv;

  priv->current_frame = -1;
}

static void
clutter_keyframe_transition_finalize (GObject *gobject)
{
  ClutterKeyframeTransitionPrivate *priv;

  priv = CLUTTER_KEYFRAME_TRANSITION (gobject)->priv;

  if (priv->frames != NULL)
    g_array_unref (priv->frames);

  G_OBJECT_CLASS (clutter_keyframe_transition_parent_class)->finalize (gobject);
}

static void
clutter_keyframe_transition_class_init (ClutterKeyframeTransitionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterTimelineClass *timeline_class = CLUTTER_TIMELINE_CLASS (klass);
  ClutterTransitionClass *transition_class = CLUTTER_TRANSITION_CLASS (klass);

  gobject_class->finalize = clutter_keyframe_transition_finalize;

  timeline_class->started = clutter_keyframe_transition_started;
  timeline_class->completed = clutter_keyframe_transition_completed;

  transition_class->compute_value = clutter_keyframe_transition_compute_value;
}

static void
clutter_keyframe_transition_init (ClutterKeyframeTransition *self)
{
  self->priv = clutter_keyframe_transition_get_instance_private (self);
}

/**
 * clutter_keyframe_transition_new:
 * @property_name: the property to animate
 *
 * Creates a new #ClutterKeyframeTransition for @property_name.
 *
 * Return value: (transfer full): the newly allocated
 *   #ClutterKeyframeTransition instance. Use g_object_unref() when
 *   done to free its resources.
 *
 * Since: 1.12
 */
ClutterTransition *
clutter_keyframe_transition_new (const char *property_name)
{
  return g_object_new (CLUTTER_TYPE_KEYFRAME_TRANSITION,
                       "property-name", property_name,
                       NULL);
}

/**
 * clutter_keyframe_transition_set_key_frames:
 * @transition: a #ClutterKeyframeTransition
 * @n_key_frames: the number of values
 * @key_frames: (array length=n_key_frames): an array of keys between 0.0
 *   and 1.0, one for each key frame
 *
 * Sets the keys for each key frame inside @transition.
 *
 * If @transition does not hold any key frame, @n_key_frames key frames
 * will be created; if @transition already has key frames, @key_frames must
 * have at least as many elements as the number of key frames.
 *
 * Since: 1.12
 */
void
clutter_keyframe_transition_set_key_frames (ClutterKeyframeTransition *transition,
                                            guint                      n_key_frames,
                                            const double              *key_frames)
{
  ClutterKeyframeTransitionPrivate *priv;
  guint i;

  g_return_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition));
  g_return_if_fail (n_key_frames > 0);
  g_return_if_fail (key_frames != NULL);

  priv = transition->priv;

  if (priv->frames == NULL)
    clutter_keyframe_transition_init_frames (transition, n_key_frames);
  else
    g_return_if_fail (n_key_frames == priv->frames->len - 1);

  for (i = 0; i < n_key_frames; i++)
    {
      KeyFrame *frame = &g_array_index (priv->frames, KeyFrame, i);

      frame->key = key_frames[i];
    }
}

/**
 * clutter_keyframe_transition_set_values:
 * @transition: a #ClutterKeyframeTransition
 * @n_values: the number of values
 * @values: (array length=n_values): an array of values, one for each
 *   key frame
 *
 * Sets the values for each key frame inside @transition.
 *
 * If @transition does not hold any key frame, @n_values key frames will
 * be created; if @transition already has key frames, @values must have
 * at least as many elements as the number of key frames.
 *
 * Since: 1.12
 */
void
clutter_keyframe_transition_set_values (ClutterKeyframeTransition *transition,
                                        guint                      n_values,
                                        const GValue              *values)
{
  ClutterKeyframeTransitionPrivate *priv;
  guint i;

  g_return_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition));
  g_return_if_fail (n_values > 0);
  g_return_if_fail (values != NULL);

  priv = transition->priv;

  if (priv->frames == NULL)
    clutter_keyframe_transition_init_frames (transition, n_values);
  else
    g_return_if_fail (n_values == priv->frames->len - 1);

  for (i = 0; i < n_values; i++)
    {
      KeyFrame *frame = &g_array_index (priv->frames, KeyFrame, i);

      if (frame->interval)
        clutter_interval_set_final_value (frame->interval, &values[i]);
      else
        frame->interval =
          clutter_interval_new_with_values (G_VALUE_TYPE (&values[i]), NULL,
                                            &values[i]);
    }
}

/**
 * clutter_keyframe_transition_set_modes:
 * @transition: a #ClutterKeyframeTransition
 * @n_modes: the number of easing modes
 * @modes: (array length=n_modes): an array of easing modes, one for
 *   each key frame
 *
 * Sets the easing modes for each key frame inside @transition.
 *
 * If @transition does not hold any key frame, @n_modes key frames will
 * be created; if @transition already has key frames, @modes must have
 * at least as many elements as the number of key frames.
 *
 * Since: 1.12
 */
void
clutter_keyframe_transition_set_modes (ClutterKeyframeTransition  *transition,
                                       guint                       n_modes,
                                       const ClutterAnimationMode *modes)
{
  ClutterKeyframeTransitionPrivate *priv;
  guint i;

  g_return_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition));
  g_return_if_fail (n_modes > 0);
  g_return_if_fail (modes != NULL);

  priv = transition->priv;

  if (priv->frames == NULL)
    clutter_keyframe_transition_init_frames (transition, n_modes);
  else
    g_return_if_fail (n_modes == priv->frames->len - 1);

  for (i = 0; i < n_modes; i++)
    {
      KeyFrame *frame = &g_array_index (priv->frames, KeyFrame, i);

      frame->mode = modes[i];
    }
}

/**
 * clutter_keyframe_transition_set: (skip)
 * @transition: a #ClutterKeyframeTransition
 * @gtype: the type of the values to use for the key frames
 * @n_key_frames: the number of key frames between the initial
 *   and final values
 * @...: a list of tuples, containing the key frame index, the value
 *   at the key frame, and the animation mode
 *
 * Sets the key frames of the @transition.
 *
 * This variadic arguments function is a convenience for C developers;
 * language bindings should use clutter_keyframe_transition_set_key_frames(),
 * clutter_keyframe_transition_set_modes(), and
 * clutter_keyframe_transition_set_values() instead.
 *
 * Since: 1.12
 */
void
clutter_keyframe_transition_set (ClutterKeyframeTransition *transition,
                                 GType                      gtype,
                                 guint                      n_key_frames,
                                 ...)
{
  ClutterKeyframeTransitionPrivate *priv;
  va_list args;
  guint i;

  g_return_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition));
  g_return_if_fail (gtype != G_TYPE_INVALID);
  g_return_if_fail (n_key_frames > 0);

  priv = transition->priv;

  if (priv->frames == NULL)
    clutter_keyframe_transition_init_frames (transition, n_key_frames);
  else
    g_return_if_fail (n_key_frames == priv->frames->len - 1);

  va_start (args, n_key_frames);

  for (i = 0; i < n_key_frames; i++)
    {
      KeyFrame *frame = &g_array_index (priv->frames, KeyFrame, i);
      GValue value = G_VALUE_INIT;
      char *error = NULL;

      frame->key = va_arg (args, double);

      G_VALUE_COLLECT_INIT (&value, gtype, args, 0, &error);
      if (error != NULL)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      frame->mode = va_arg (args, ClutterAnimationMode);

      g_clear_object (&frame->interval);
      frame->interval = clutter_interval_new_with_values (gtype, NULL, &value);

      g_value_unset (&value);
    }

  va_end (args);
}

/**
 * clutter_keyframe_transition_clear:
 * @transition: a #ClutterKeyframeTransition
 *
 * Removes all key frames from @transition.
 *
 * Since: 1.12
 */
void
clutter_keyframe_transition_clear (ClutterKeyframeTransition *transition)
{
  g_return_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition));

  if (transition->priv->frames != NULL)
    {
      g_array_unref (transition->priv->frames);
      transition->priv->frames = NULL;
    }
}

/**
 * clutter_keyframe_transition_get_n_key_frames:
 * @transition: a #ClutterKeyframeTransition
 *
 * Retrieves the number of key frames inside @transition.
 *
 * Return value: the number of key frames
 *
 * Since: 1.12
 */
guint
clutter_keyframe_transition_get_n_key_frames (ClutterKeyframeTransition *transition)
{
  g_return_val_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition), 0);

  if (transition->priv->frames == NULL)
    return 0;

  return transition->priv->frames->len - 1;
}

/**
 * clutter_keyframe_transition_set_key_frame:
 * @transition: a #ClutterKeyframeTransition
 * @index_: the index of the key frame
 * @key: the key of the key frame
 * @mode: the easing mode of the key frame
 * @value: a #GValue containing the value of the key frame
 *
 * Sets the details of the key frame at @index_ inside @transition.
 *
 * The @transition must already have a key frame at @index_, and @index_
 * must be smaller than the number of key frames inside @transition.
 *
 * Since: 1.12
 */
void
clutter_keyframe_transition_set_key_frame (ClutterKeyframeTransition *transition,
                                           guint                      index_,
                                           double                     key,
                                           ClutterAnimationMode       mode,
                                           const GValue              *value)
{
  ClutterKeyframeTransitionPrivate *priv;
  KeyFrame *frame;

  g_return_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition));

  priv = transition->priv;
  g_return_if_fail (priv->frames != NULL);
  g_return_if_fail (index_ < priv->frames->len - 1);

  frame = &g_array_index (priv->frames, KeyFrame, index_);
  frame->key = key;
  frame->mode = mode;
  clutter_interval_set_final_value (frame->interval, value);
}

/**
 * clutter_keyframe_transition_get_key_frame:
 * @transition: a #ClutterKeyframeTransition
 * @index_: the index of the key frame
 * @key: (out) (allow-none): return location for the key, or %NULL
 * @mode: (out) (allow-none): return location for the easing mode, or %NULL
 * @value: (out caller-allocates): a #GValue initialized with the type of
 *   the values
 *
 * Retrieves the details of the key frame at @index_ inside @transition.
 *
 * The @transition must already have key frames set, and @index_ must be
 * smaller than the number of key frames.
 *
 * Since: 1.12
 */
void
clutter_keyframe_transition_get_key_frame (ClutterKeyframeTransition *transition,
                                           guint                      index_,
                                           double                    *key,
                                           ClutterAnimationMode      *mode,
                                           GValue                    *value)
{
  ClutterKeyframeTransitionPrivate *priv;
  const KeyFrame *frame;

  g_return_if_fail (CLUTTER_IS_KEYFRAME_TRANSITION (transition));

  priv = transition->priv;
  g_return_if_fail (priv->frames != NULL);
  g_return_if_fail (index_ < priv->frames->len - 1);

  frame = &g_array_index (priv->frames, KeyFrame, index_);

  if (key != NULL)
    *key = frame->key;

  if (mode != NULL)
    *mode = frame->mode;

  if (value != NULL)
    clutter_interval_get_final_value (frame->interval, value);
}
