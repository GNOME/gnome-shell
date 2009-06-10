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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-timeline
 * @short_description: A class for time-based events
 *
 * #ClutterTimeline is a base class for managing time based events such
 * as animations.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-master-clock.h"
#include "clutter-private.h"
#include "clutter-timeline.h"

G_DEFINE_TYPE (ClutterTimeline, clutter_timeline, G_TYPE_OBJECT);

struct _ClutterTimelinePrivate
{
  ClutterTimelineDirection direction;

  guint delay_id;

  /* The total length in milliseconds of this timeline */
  guint duration;
  guint delay;

  /* The current amount of elapsed time */
  gint elapsed_time;
  /* The elapsed time since the last frame was fired */
  gint msecs_delta;

  GHashTable *markers_by_name;

  /* Time we last advanced the elapsed time and showed a frame */
  GTimeVal last_frame_time;

  guint loop       : 1;
  guint is_playing : 1;
  /* If we've just started playing and haven't yet gotten a tick from the master clock */
  guint waiting_first_tick : 1;
};

typedef struct {
  gchar *name;
  guint msecs;
  GQuark quark;
} TimelineMarker;

enum
{
  PROP_0,

  PROP_LOOP,
  PROP_DELAY,
  PROP_DURATION,
  PROP_DIRECTION
};

enum
{
  NEW_FRAME,
  STARTED,
  PAUSED,
  COMPLETED,
  MARKER_REACHED,

  LAST_SIGNAL
};

static guint timeline_signals[LAST_SIGNAL] = { 0, };

static TimelineMarker *
timeline_marker_new (const gchar *name,
                     guint        msecs)
{
  TimelineMarker *marker = g_slice_new0 (TimelineMarker);

  marker->name = g_strdup (name);
  marker->quark = g_quark_from_string (marker->name);
  marker->msecs = msecs;

  return marker;
}

static void
timeline_marker_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      TimelineMarker *marker = data;

      g_free (marker->name);
      g_slice_free (TimelineMarker, marker);
    }
}

/* Object */

static void
clutter_timeline_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  ClutterTimeline        *timeline;
  ClutterTimelinePrivate *priv;

  timeline = CLUTTER_TIMELINE(object);
  priv = timeline->priv;

  switch (prop_id)
    {
    case PROP_LOOP:
      priv->loop = g_value_get_boolean (value);
      break;

    case PROP_DELAY:
      priv->delay = g_value_get_uint (value);
      break;

    case PROP_DURATION:
      clutter_timeline_set_duration (timeline, g_value_get_uint (value));
      break;

    case PROP_DIRECTION:
      clutter_timeline_set_direction (timeline, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_timeline_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  ClutterTimeline        *timeline;
  ClutterTimelinePrivate *priv;

  timeline = CLUTTER_TIMELINE(object);
  priv = timeline->priv;

  switch (prop_id)
    {
    case PROP_LOOP:
      g_value_set_boolean (value, priv->loop);
      break;

    case PROP_DELAY:
      g_value_set_uint (value, priv->delay);
      break;

    case PROP_DURATION:
      g_value_set_uint (value, clutter_timeline_get_duration (timeline));
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_timeline_finalize (GObject *object)
{
  ClutterTimeline *self = CLUTTER_TIMELINE (object);
  ClutterTimelinePrivate *priv = self->priv;
  ClutterMasterClock *master_clock;

  if (priv->markers_by_name)
    g_hash_table_destroy (priv->markers_by_name);

  if (priv->is_playing)
    {
      master_clock = _clutter_master_clock_get_default ();
      _clutter_master_clock_remove_timeline (master_clock, self);
    }

  G_OBJECT_CLASS (clutter_timeline_parent_class)->finalize (object);
}

static void
clutter_timeline_dispose (GObject *object)
{
  ClutterTimeline *self = CLUTTER_TIMELINE(object);
  ClutterTimelinePrivate *priv;

  priv = self->priv;

  if (priv->delay_id)
    {
      g_source_remove (priv->delay_id);
      priv->delay_id = 0;
    }

  G_OBJECT_CLASS (clutter_timeline_parent_class)->dispose (object);
}

static void
clutter_timeline_class_init (ClutterTimelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->set_property = clutter_timeline_set_property;
  object_class->get_property = clutter_timeline_get_property;
  object_class->finalize     = clutter_timeline_finalize;
  object_class->dispose      = clutter_timeline_dispose;

  g_type_class_add_private (klass, sizeof (ClutterTimelinePrivate));

  /**
   * ClutterTimeline:loop:
   *
   * Whether the timeline should automatically rewind and restart.
   */
  pspec = g_param_spec_boolean ("loop",
                                "Loop",
                                "Should the timeline automatically restart",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_LOOP, pspec);

  /**
   * ClutterTimeline:delay:
   *
   * A delay, in milliseconds, that should be observed by the
   * timeline before actually starting.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_uint ("delay",
                             "Delay",
                             "Delay before start",
                             0, G_MAXUINT,
                             0,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DELAY, pspec);

  /**
   * ClutterTimeline:duration:
   *
   * Duration of the timeline in milliseconds, depending on the
   * ClutterTimeline:fps value.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_uint ("duration",
                             "Duration",
                             "Duration of the timeline in milliseconds",
                             0, G_MAXUINT,
                             1000,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DURATION, pspec);

  /**
   * ClutterTimeline:direction:
   *
   * The direction of the timeline, either %CLUTTER_TIMELINE_FORWARD or
   * %CLUTTER_TIMELINE_BACKWARD.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_enum ("direction",
                             "Direction",
                             "Direction of the timeline",
                             CLUTTER_TYPE_TIMELINE_DIRECTION,
                             CLUTTER_TIMELINE_FORWARD,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DIRECTION, pspec);

  /**
   * ClutterTimeline::new-frame:
   * @timeline: the timeline which received the signal
   * @msecs: the elapsed time between 0 and duration
   *
   * The ::new-frame signal is emitted for each timeline running
   * timeline before a new frame is drawn to give animations a chance
   * to update the scene.
   */
  timeline_signals[NEW_FRAME] =
    g_signal_new (I_("new-frame"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTimelineClass, new_frame),
		  NULL, NULL,
		  clutter_marshal_VOID__INT,
		  G_TYPE_NONE,
		  1, G_TYPE_INT);
  /**
   * ClutterTimeline::completed:
   * @timeline: the #ClutterTimeline which received the signal
   *
   * The ::completed signal is emitted when the timeline reaches the
   * number of frames specified by the ClutterTimeline:num-frames property.
   */
  timeline_signals[COMPLETED] =
    g_signal_new (I_("completed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTimelineClass, completed),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterTimeline::started:
   * @timeline: the #ClutterTimeline which received the signal
   *
   * The ::started signal is emitted when the timeline starts its run.
   * This might be as soon as clutter_timeline_start() is invoked or
   * after the delay set in the ClutterTimeline:delay property has
   * expired.
   */
  timeline_signals[STARTED] =
    g_signal_new (I_("started"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTimelineClass, started),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterTimeline::paused:
   * @timeline: the #ClutterTimeline which received the signal
   *
   * The ::paused signal is emitted when clutter_timeline_pause() is invoked.
   */
  timeline_signals[PAUSED] =
    g_signal_new (I_("paused"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTimelineClass, paused),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterTimeline::marker-reached:
   * @timeline: the #ClutterTimeline which received the signal
   * @marker_name: the name of the marker reached
   * @msecs: the elapsed time
   *
   * The ::marker-reached signal is emitted each time a timeline
   * reaches a marker set with
   * clutter_timeline_add_marker_at_time(). This signal is detailed
   * with the name of the marker as well, so it is possible to connect
   * a callback to the ::marker-reached signal for a specific marker
   * with:
   *
   * <informalexample><programlisting>
   *   clutter_timeline_add_marker_at_time (timeline, "foo", 500);
   *   clutter_timeline_add_marker_at_time (timeline, "bar", 750);
   *
   *   g_signal_connect (timeline, "marker-reached",
   *                     G_CALLBACK (each_marker_reached), NULL);
   *   g_signal_connect (timeline, "marker-reached::foo",
   *                     G_CALLBACK (foo_marker_reached), NULL);
   *   g_signal_connect (timeline, "marker-reached::bar",
   *                     G_CALLBACK (bar_marker_reached), NULL);
   * </programlisting></informalexample>
   *
   * In the example, the first callback will be invoked for both
   * the "foo" and "bar" marker, while the second and third callbacks
   * will be invoked for the "foo" or "bar" markers, respectively.
   *
   * Since: 0.8
   */
  timeline_signals[MARKER_REACHED] =
    g_signal_new (I_("marker-reached"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                  G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ClutterTimelineClass, marker_reached),
                  NULL, NULL,
                  clutter_marshal_VOID__STRING_INT,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_INT);
}

static void
clutter_timeline_init (ClutterTimeline *self)
{
  ClutterTimelinePrivate *priv;

  self->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, CLUTTER_TYPE_TIMELINE,
                                 ClutterTimelinePrivate);

  priv->duration = 0;
  priv->delay = 0;
  priv->elapsed_time = 0;
}

static gboolean
have_passed_time (ClutterTimeline *timeline,
                  gint new_time,
                  gint msecs)
{
  ClutterTimelinePrivate *priv = timeline->priv;

  /* Ignore markers that are outside the duration of the timeline */
  if (msecs < 0 || msecs > priv->duration)
    return FALSE;

  if (priv->direction == CLUTTER_TIMELINE_FORWARD)
    {
      /* We need to special case when a marker is added at the
         beginning of the timeline */
      if (msecs == 0 &&
          priv->msecs_delta > 0 &&
          new_time - priv->msecs_delta <= 0)
        return TRUE;

      /* If the timeline is looping then we need to check for wrap
         around */
      if (priv->loop && new_time >= priv->duration)
        return (msecs > new_time - priv->msecs_delta ||
                msecs <= new_time % priv->duration);

      /* Otherwise it's just a simple test if the time is in range of
         the previous time and the new time */
      return (msecs > new_time - priv->msecs_delta
              && msecs <= new_time);
    }
  else
    {
      /* We need to special case when a marker is added at the
         end of the timeline */
      if (msecs == priv->duration &&
          priv->msecs_delta > 0 &&
          new_time + priv->msecs_delta >= priv->duration)
        return TRUE;

      /* If the timeline is looping then we need to check for wrap
         around */
      if (priv->loop && new_time <= 0)
        return (msecs < new_time + priv->msecs_delta ||
                msecs >= (priv->duration -
                          (-new_time % priv->duration)));

      /* Otherwise it's just a simple test if the time is in range of
         the previous time and the new time */
      return (msecs >= new_time
              && msecs < new_time + priv->msecs_delta);
    }
}

struct CheckIfMarkerHitClosure
{
  ClutterTimeline *timeline;
  gint new_time;
};

static void
check_if_marker_hit (const gchar *name,
                     TimelineMarker *marker,
                     struct CheckIfMarkerHitClosure *data)
{
  if (have_passed_time (data->timeline, data->new_time, marker->msecs))
    {
      CLUTTER_NOTE (SCHEDULER, "Marker '%s' reached", name);

      g_signal_emit (data->timeline, timeline_signals[MARKER_REACHED],
                     marker->quark,
                     name,
                     marker->msecs);
    }
}

static void
emit_frame_signal (ClutterTimeline *timeline, gint new_time)
{
  ClutterTimelinePrivate *priv = timeline->priv;
  struct CheckIfMarkerHitClosure data;

  /* Emit the signal */
  g_signal_emit (timeline, timeline_signals[NEW_FRAME], 0,
                 priv->elapsed_time);

  /* shortcircuit here if we don't have any marker installed */
  if (priv->markers_by_name == NULL)
    return;

  data.timeline = timeline;
  data.new_time = new_time;

  g_hash_table_foreach (priv->markers_by_name,
                        (GHFunc) check_if_marker_hit,
                        &data);
}

static gboolean
is_complete (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv = timeline->priv;

  return (priv->direction == CLUTTER_TIMELINE_FORWARD
          ? priv->elapsed_time >= priv->duration
          : priv->elapsed_time <= 0);
}

static void
set_is_playing (ClutterTimeline *timeline,
                gboolean         is_playing)
{
  ClutterTimelinePrivate *priv = timeline->priv;
  ClutterMasterClock *master_clock;

  is_playing = is_playing != FALSE;

  if (is_playing == priv->is_playing)
    return;

  priv->is_playing = is_playing;
  master_clock = _clutter_master_clock_get_default ();
  if (priv->is_playing)
    {
      _clutter_master_clock_add_timeline (master_clock, timeline);
      priv->waiting_first_tick = TRUE;
    }
  else
    {
      _clutter_master_clock_remove_timeline (master_clock, timeline);
    }
}

static gboolean
clutter_timeline_do_frame (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  priv = timeline->priv;

  g_object_ref (timeline);

  CLUTTER_TIMESTAMP (SCHEDULER, "Timeline [%p] activated (cur: %d)\n",
                     timeline,
                     priv->elapsed_time);

  /* Advance time */
  if (priv->direction == CLUTTER_TIMELINE_FORWARD)
    priv->elapsed_time += priv->msecs_delta;
  else
    priv->elapsed_time -= priv->msecs_delta;

  /* If we have not reached the end of the timeline: */
  if (!is_complete (timeline))
    {
      /* Emit the signal */
      emit_frame_signal (timeline, priv->elapsed_time);

      /* Signal pauses timeline ? */
      if (!priv->is_playing)
        {
          g_object_unref (timeline);
          return FALSE;
        }

      g_object_unref (timeline);
      return TRUE;
    }
  else
    {
      /* Handle loop or stop */
      gint end_msecs;
      gint overflow_msecs = priv->elapsed_time;

      /* Update the current elapsed time in case the signal handlers
       * want to take a peek */
      if (priv->loop)
        {
          /* We try and interpolate smoothly around a loop */
          if (priv->direction == CLUTTER_TIMELINE_FORWARD)
            priv->elapsed_time = priv->elapsed_time % priv->duration;
          else
            priv->elapsed_time = (priv->duration -
                                  (-priv->elapsed_time % priv->duration));
        }
      else if (priv->direction == CLUTTER_TIMELINE_FORWARD)
        priv->elapsed_time = priv->duration;
      else if (priv->direction == CLUTTER_TIMELINE_BACKWARD)
        priv->elapsed_time = 0;

      end_msecs = priv->elapsed_time;

      /* Check if the markers have been hit using the old value of
         elapsed time so it will use the right value for the previous
         elapsed time */
      emit_frame_signal (timeline, overflow_msecs);

      /* Did the signal handler modify the elapsed time? */
      if (priv->elapsed_time != end_msecs)
        {
          g_object_unref (timeline);
          return TRUE;
        }

      /* Note: If the new-frame signal handler paused the timeline
       * on the last frame we will still go ahead and send the
       * completed signal */
      CLUTTER_NOTE (SCHEDULER,
                    "Timeline [%p] completed (cur: %d, tot: %d)",
                    timeline,
                    priv->elapsed_time,
                    priv->msecs_delta);

      if (!priv->loop && priv->is_playing)
        {
          /* We remove the timeout now, so that the completed signal handler
           * may choose to re-start the timeline
           *
           * XXX Perhaps we should remove this earlier, and regardless
           * of priv->loop. Are we limiting the things that could be done in
           * the above new-frame signal handler */
	  set_is_playing (timeline, FALSE);
        }

      g_signal_emit (timeline, timeline_signals[COMPLETED], 0);

      /* Again check to see if the user has manually played with
       * the elapsed time, before we finally stop or loop the timeline */

      if (priv->elapsed_time != end_msecs &&
          !(/* Except allow changing time from 0 -> duration (or vice-versa)
               since these are considered equivalent */
            (priv->elapsed_time == 0 && end_msecs == priv->duration) ||
            (priv->elapsed_time == priv->duration && end_msecs == 0)
          ))
        {
          g_object_unref (timeline);
          return TRUE;
        }

      if (!priv->loop)
        clutter_timeline_rewind (timeline);

      g_object_unref (timeline);
      return priv->loop;
    }
}

static gboolean
delay_timeout_func (gpointer data)
{
  ClutterTimeline *timeline = data;
  ClutterTimelinePrivate *priv = timeline->priv;

  priv->delay_id = 0;
  priv->msecs_delta = 0;
  set_is_playing (timeline, TRUE);

  g_signal_emit (timeline, timeline_signals[STARTED], 0);

  return FALSE;
}

/**
 * clutter_timeline_start:
 * @timeline: A #ClutterTimeline
 *
 * Starts the #ClutterTimeline playing.
 **/
void
clutter_timeline_start (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->delay_id || priv->is_playing)
    return;

  if (priv->duration == 0)
    return;

  if (priv->delay)
    priv->delay_id = clutter_threads_add_timeout (priv->delay,
                                                  delay_timeout_func,
                                                  timeline);
  else
    {
      priv->msecs_delta = 0;
      set_is_playing (timeline, TRUE);

      g_signal_emit (timeline, timeline_signals[STARTED], 0);
    }
}

/**
 * clutter_timeline_pause:
 * @timeline: A #ClutterTimeline
 *
 * Pauses the #ClutterTimeline on current frame
 **/
void
clutter_timeline_pause (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->delay_id == 0 && !priv->is_playing)
    return;

  if (priv->delay_id)
    {
      g_source_remove (priv->delay_id);
      priv->delay_id = 0;
    }

  priv->msecs_delta = 0;
  set_is_playing (timeline, FALSE);

  g_signal_emit (timeline, timeline_signals[PAUSED], 0);
}

/**
 * clutter_timeline_stop:
 * @timeline: A #ClutterTimeline
 *
 * Stops the #ClutterTimeline and moves to frame 0
 **/
void
clutter_timeline_stop (ClutterTimeline *timeline)
{
  clutter_timeline_pause (timeline);
  clutter_timeline_rewind (timeline);
}

/**
 * clutter_timeline_set_loop:
 * @timeline: a #ClutterTimeline
 * @loop: %TRUE for enable looping
 *
 * Sets whether @timeline should loop.
 */
void
clutter_timeline_set_loop (ClutterTimeline *timeline,
			   gboolean         loop)
{
  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  if (timeline->priv->loop != loop)
    {
      timeline->priv->loop = loop;

      g_object_notify (G_OBJECT (timeline), "loop");
    }
}

/**
 * clutter_timeline_get_loop:
 * @timeline: a #ClutterTimeline
 *
 * Gets whether @timeline is looping
 *
 * Return value: %TRUE if the timeline is looping
 */
gboolean
clutter_timeline_get_loop (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), FALSE);

  return timeline->priv->loop;
}

/**
 * clutter_timeline_rewind:
 * @timeline: A #ClutterTimeline
 *
 * Rewinds #ClutterTimeline to the first frame if its direction is
 * %CLUTTER_TIMELINE_FORWARD and the last frame if it is
 * %CLUTTER_TIMELINE_BACKWARD.
 */
void
clutter_timeline_rewind (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->direction == CLUTTER_TIMELINE_FORWARD)
    clutter_timeline_advance (timeline, 0);
  else if (priv->direction == CLUTTER_TIMELINE_BACKWARD)
    clutter_timeline_advance (timeline, priv->duration);
}

/**
 * clutter_timeline_skip:
 * @timeline: A #ClutterTimeline
 * @msecs: Amount of time to skip
 *
 * Advance timeline by the requested time in milliseconds
 */
void
clutter_timeline_skip (ClutterTimeline *timeline,
                       guint            msecs)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->direction == CLUTTER_TIMELINE_FORWARD)
    {
      priv->elapsed_time += msecs;

      if (priv->elapsed_time > priv->duration)
        priv->elapsed_time = 1;
    }
  else if (priv->direction == CLUTTER_TIMELINE_BACKWARD)
    {
      priv->elapsed_time -= msecs;

      if (priv->elapsed_time < 1)
        priv->elapsed_time = priv->duration - 1;
    }

  priv->msecs_delta = 0;
}

/**
 * clutter_timeline_advance:
 * @timeline: A #ClutterTimeline
 * @msecs: Time to advance to
 *
 * Advance timeline to the requested point. The point is given as a
 * time in milliseconds since the timeline started.
 *
 * <note><para>The @timeline will not emit the #ClutterTimeline::new-frame
 * signal for the given time. The first ::new-frame signal after the call to
 * clutter_timeline_advance() will be emit the skipped markers.
 * </para></note>
 */
void
clutter_timeline_advance (ClutterTimeline *timeline,
                          guint            msecs)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  priv->elapsed_time = CLAMP (msecs, 0, priv->duration);
}

/**
 * clutter_timeline_get_elapsed_time:
 * @timeline: A #ClutterTimeline
 *
 * Request the current time position of the timeline.
 *
 * Return value: current elapsed time in milliseconds.
 */
guint
clutter_timeline_get_elapsed_time (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  return timeline->priv->elapsed_time;
}

/**
 * clutter_timeline_is_playing:
 * @timeline: A #ClutterTimeline
 *
 * Queries state of a #ClutterTimeline.
 *
 * Return value: %TRUE if timeline is currently playing
 */
gboolean
clutter_timeline_is_playing (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), FALSE);

  return timeline->priv->is_playing;
}

/**
 * clutter_timeline_clone:
 * @timeline: #ClutterTimeline to duplicate.
 *
 * Create a new #ClutterTimeline instance which has property values
 * matching that of supplied timeline. The cloned timeline will not
 * be started and will not be positioned to the current position of
 * @timeline: you will have to start it with clutter_timeline_start().
 *
 * Return Value: a new #ClutterTimeline, cloned from @timeline
 *
 * Since: 0.4
 */
ClutterTimeline *
clutter_timeline_clone (ClutterTimeline *timeline)
{
  ClutterTimeline *copy;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);

  copy = g_object_new (CLUTTER_TYPE_TIMELINE,
                       "duration", clutter_timeline_get_duration (timeline),
                       "loop", clutter_timeline_get_loop (timeline),
                       "delay", clutter_timeline_get_delay (timeline),
                       "direction", clutter_timeline_get_direction (timeline),
                       NULL);

  return copy;
}

/**
 * clutter_timeline_new:
 * @msecs: Duration of the timeline in milliseconds
 *
 * Creates a new #ClutterTimeline with a duration of @msecs.
 *
 * Return value: the newly created #ClutterTimeline instance. Use
 *   g_object_unref() when done using it
 *
 * Since: 0.6
 */
ClutterTimeline *
clutter_timeline_new (guint msecs)
{
  return g_object_new (CLUTTER_TYPE_TIMELINE,
                       "duration", msecs,
                       NULL);
}

/**
 * clutter_timeline_get_delay:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the delay set using clutter_timeline_set_delay().
 *
 * Return value: the delay in milliseconds.
 *
 * Since: 0.4
 */
guint
clutter_timeline_get_delay (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  return timeline->priv->delay;
}

/**
 * clutter_timeline_set_delay:
 * @timeline: a #ClutterTimeline
 * @msecs: delay in milliseconds
 *
 * Sets the delay, in milliseconds, before @timeline should start.
 *
 * Since: 0.4
 */
void
clutter_timeline_set_delay (ClutterTimeline *timeline,
                            guint            msecs)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->delay != msecs)
    {
      priv->delay = msecs;
      g_object_notify (G_OBJECT (timeline), "delay");
    }
}

/**
 * clutter_timeline_get_duration:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the duration of a #ClutterTimeline in milliseconds.
 * See clutter_timeline_set_duration().
 *
 * Return value: the duration of the timeline, in milliseconds.
 *
 * Since: 0.6
 */
guint
clutter_timeline_get_duration (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  priv = timeline->priv;

  return priv->duration;
}

/**
 * clutter_timeline_set_duration:
 * @timeline: a #ClutterTimeline
 * @msecs: duration of the timeline in milliseconds
 *
 * Sets the duration of the timeline, in milliseconds. The speed
 * of the timeline depends on the ClutterTimeline:fps setting.
 *
 * Since: 0.6
 */
void
clutter_timeline_set_duration (ClutterTimeline *timeline,
                               guint            msecs)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (msecs > 0);

  priv = timeline->priv;

  if (priv->duration != msecs)
    {
      priv->duration = msecs;

      g_object_notify (G_OBJECT (timeline), "duration");
    }
}

/**
 * clutter_timeline_get_progress:
 * @timeline: a #ClutterTimeline
 *
 * The position of the timeline in a [0, 1] interval.
 *
 * Return value: the position of the timeline.
 *
 * Since: 0.6
 */
gdouble
clutter_timeline_get_progress (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0.0);

  priv = timeline->priv;

  return (gdouble) priv->elapsed_time / (gdouble) priv->duration;
}

/**
 * clutter_timeline_get_progressx:
 * @timeline: a #ClutterTimeline
 *
 * Fixed point version of clutter_timeline_get_progress().
 *
 * Return value: the position of the timeline as a fixed point value
 *
 * Since: 0.6
 */
CoglFixed
clutter_timeline_get_progressx (ClutterTimeline *timeline)
{
  return COGL_FIXED_FROM_DOUBLE (clutter_timeline_get_progress (timeline));
}

/**
 * clutter_timeline_get_direction:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the direction of the timeline set with
 * clutter_timeline_set_direction().
 *
 * Return value: the direction of the timeline
 *
 * Since: 0.6
 */
ClutterTimelineDirection
clutter_timeline_get_direction (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline),
                        CLUTTER_TIMELINE_FORWARD);

  return timeline->priv->direction;
}

/**
 * clutter_timeline_set_direction:
 * @timeline: a #ClutterTimeline
 * @direction: the direction of the timeline
 *
 * Sets the direction of @timeline, either %CLUTTER_TIMELINE_FORWARD or
 * %CLUTTER_TIMELINE_BACKWARD.
 *
 * Since: 0.6
 */
void
clutter_timeline_set_direction (ClutterTimeline          *timeline,
                                ClutterTimelineDirection  direction)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->direction != direction)
    {
      priv->direction = direction;

      if (priv->elapsed_time == 0)
        priv->elapsed_time = priv->duration;

      g_object_notify (G_OBJECT (timeline), "direction");
    }
}

/**
 * clutter_timeline_get_delta:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the amount of time elapsed since the last
 * ClutterTimeline::new-frame signal.
 *
 * This function is only useful inside handlers for the ::new-frame
 * signal, and its behaviour is undefined if the timeline is not
 * playing.
 *
 * Return value: the amount of time in milliseconds elapsed since the
 * last frame
 *
 * Since: 0.6
 */
guint
clutter_timeline_get_delta (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  if (!clutter_timeline_is_playing (timeline))
    return 0;

  priv = timeline->priv;

  return timeline->priv->msecs_delta;
}

/*
 * clutter_timeline_do_tick
 * @timeline: a #ClutterTimeline
 * @tick_time: time of advance
 *
 * Advances @timeline based on the time passed in @msecs. This
 * function is called by the master clock. The @timeline will use this
 * interval to emit the #ClutterTimeline::new-frame signal and
 * eventually skip frames.
 */
void
clutter_timeline_do_tick (ClutterTimeline *timeline,
			  GTimeVal        *tick_time)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->waiting_first_tick)
    {
      priv->last_frame_time = *tick_time;
      priv->waiting_first_tick = FALSE;
    }
  else
    {
      gint msecs =
	(tick_time->tv_sec - priv->last_frame_time.tv_sec) * 1000
        + (tick_time->tv_usec - priv->last_frame_time.tv_usec) / 1000;

      if (msecs != 0)
	{
	  /* Avoid accumulating error */
	  g_time_val_add (&priv->last_frame_time, msecs * 1000L);
	  priv->msecs_delta = msecs;
	  clutter_timeline_do_frame (timeline);
	}
    }
}

static inline void
clutter_timeline_add_marker_internal (ClutterTimeline *timeline,
                                      const gchar     *marker_name,
                                      guint            msecs)
{
  ClutterTimelinePrivate *priv = timeline->priv;
  TimelineMarker *marker;

  /* create the hash table that will hold the markers */
  if (G_UNLIKELY (priv->markers_by_name == NULL))
    priv->markers_by_name = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   NULL,
                                                   timeline_marker_free);

  marker = g_hash_table_lookup (priv->markers_by_name, marker_name);
  if (G_UNLIKELY (marker))
    {
      g_warning ("A marker named '%s' already exists at time %d",
                 marker->name,
                 marker->msecs);
      return;
    }

  marker = timeline_marker_new (marker_name, msecs);
  g_hash_table_insert (priv->markers_by_name, marker->name, marker);
}

/**
 * clutter_timeline_add_marker_at_time:
 * @timeline: a #ClutterTimeline
 * @marker_name: the unique name for this marker
 * @msecs: position of the marker in milliseconds
 *
 * Adds a named marker that will be hit when the timeline has been
 * running for @msecs milliseconds. Markers are unique string
 * identifiers for a given time. Once @timeline reaches
 * @msecs, it will emit a ::marker-reached signal for each marker
 * attached to that time.
 *
 * A marker can be removed with clutter_timeline_remove_marker(). The
 * timeline can be advanced to a marker using
 * clutter_timeline_advance_to_marker().
 *
 * Since: 0.8
 */
void
clutter_timeline_add_marker_at_time (ClutterTimeline *timeline,
                                     const gchar     *marker_name,
                                     guint            msecs)
{
  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);
  g_return_if_fail (msecs <= clutter_timeline_get_duration (timeline));

  clutter_timeline_add_marker_internal (timeline, marker_name, msecs);
}

struct CollectMarkersClosure
{
  guint msecs;
  GArray *markers;
};

static void
collect_markers (const gchar *key,
                 TimelineMarker *marker,
                 struct CollectMarkersClosure *data)
{
  if (marker->msecs == data->msecs)
    {
      gchar *name_copy = g_strdup (key);
      g_array_append_val (data->markers, name_copy);
    }
}

/**
 * clutter_timeline_list_markers:
 * @timeline: a #ClutterTimeline
 * @msecs: the time to check, or -1
 * @n_markers: the number of markers returned
 *
 * Retrieves the list of markers at time @msecs. If @frame_num is a
 * negative integer, all the markers attached to @timeline will be
 * returned.
 *
 * Return value: (array zero-terminated=1 length=n_markers): a newly
 *   allocated, %NULL terminated string array containing the names of
 *   the markers. Use g_strfreev() when done.
 *
 * Since: 0.8
 */
gchar **
clutter_timeline_list_markers (ClutterTimeline *timeline,
                               gint             msecs,
                               gsize           *n_markers)
{
  ClutterTimelinePrivate *priv;
  gchar **retval = NULL;
  gsize i;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);

  priv = timeline->priv;

  if (G_UNLIKELY (priv->markers_by_name == NULL))
    {
      if (n_markers)
        *n_markers = 0;

      return NULL;
    }

  if (msecs < 0)
    {
      GList *markers, *l;

      markers = g_hash_table_get_keys (priv->markers_by_name);
      retval = g_new0 (gchar*, g_list_length (markers) + 1);

      for (i = 0, l = markers; l != NULL; i++, l = l->next)
        retval[i] = g_strdup (l->data);

      g_list_free (markers);
    }
  else
    {
      struct CollectMarkersClosure data;

      data.msecs = msecs;
      data.markers = g_array_new (TRUE, FALSE, sizeof (gchar *));

      g_hash_table_foreach (priv->markers_by_name,
                            (GHFunc) collect_markers,
                            &data);

      i = data.markers->len;
      retval = (gchar **) g_array_free (data.markers, FALSE);
    }

  if (n_markers)
    *n_markers = i;

  return retval;
}

/**
 * clutter_timeline_advance_to_marker:
 * @timeline: a #ClutterTimeline
 * @marker_name: the name of the marker
 *
 * Advances @timeline to the time of the given @marker_name.
 *
 * <note><para>Like clutter_timeline_advance(), this function will not
 * emit the #ClutterTimeline::new-frame for the time where @marker_name
 * is set, nor it will emit #ClutterTimeline::marker-reached for
 * @marker_name.</para></note>
 *
 * Since: 0.8
 */
void
clutter_timeline_advance_to_marker (ClutterTimeline *timeline,
                                    const gchar     *marker_name)
{
  ClutterTimelinePrivate *priv;
  TimelineMarker *marker;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);

  priv = timeline->priv;

  if (G_UNLIKELY (priv->markers_by_name == NULL))
    {
      g_warning ("No marker named '%s' found.", marker_name);
      return;
    }

  marker = g_hash_table_lookup (priv->markers_by_name, marker_name);
  if (!marker)
    {
      g_warning ("No marker named '%s' found.", marker_name);
      return;
    }

  clutter_timeline_advance (timeline, marker->msecs);
}

/**
 * clutter_timeline_remove_marker:
 * @timeline: a #ClutterTimeline
 * @marker_name: the name of the marker to remove
 *
 * Removes @marker_name, if found, from @timeline.
 *
 * Since: 0.8
 */
void
clutter_timeline_remove_marker (ClutterTimeline *timeline,
                                const gchar     *marker_name)
{
  ClutterTimelinePrivate *priv;
  TimelineMarker *marker;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);

  priv = timeline->priv;

  if (G_UNLIKELY (priv->markers_by_name == NULL))
    {
      g_warning ("No marker named '%s' found.", marker_name);
      return;
    }

  marker = g_hash_table_lookup (priv->markers_by_name, marker_name);
  if (!marker)
    {
      g_warning ("No marker named '%s' found.", marker_name);
      return;
    }

  /* this will take care of freeing the marker as well */
  g_hash_table_remove (priv->markers_by_name, marker_name);
}

/**
 * clutter_timeline_has_marker:
 * @timeline: a #ClutterTimeline
 * @marker_name: the name of the marker
 *
 * Checks whether @timeline has a marker set with the given name.
 *
 * Return value: %TRUE if the marker was found
 *
 * Since: 0.8
 */
gboolean
clutter_timeline_has_marker (ClutterTimeline *timeline,
                             const gchar     *marker_name)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), FALSE);
  g_return_val_if_fail (marker_name != NULL, FALSE);

  if (G_UNLIKELY (timeline->priv->markers_by_name == NULL))
    return FALSE;

  return NULL != g_hash_table_lookup (timeline->priv->markers_by_name,
                                      marker_name);
}
