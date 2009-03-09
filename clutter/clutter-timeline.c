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
 *
 * Every timeline shares the same #ClutterTimeoutPool to decrease the
 * possibility of starving the main loop when using many timelines
 * at the same time; this might cause problems if you are also using
 * a library making heavy use of threads with no GLib main loop integration.
 *
 * In that case you might disable the common timeline pool by setting
 * the %CLUTTER_TIMELINE=no-pool environment variable prior to launching
 * your application.
 *
 * One way to visualise a timeline is as a path with marks along its length.
 * When creating a timeline of N frames via clutter_timeline_new(), then the
 * number of frames can be seen as the paths length, and each unit of
 * length (each frame) is delimited by a mark.
 *
 * For a non looping timeline there will be (N frames + 1) marks along its
 * length. For a looping timeline, the two ends are joined with one mark.
 * Technically this mark represents two discrete frame numbers, but for a
 * looping timeline the start and end frame numbers are considered equivalent.
 *
 * When you create a timeline it will be initialized so that the current
 * frame, as returned by clutter_timeline_get_current_frame(), will be 0.
 *
 * After starting a timeline, the first timeout is for frame number
 * one (notably it isn't zero since there is a delay before the first
 * #ClutterTimeline::new-frame signal, so re-asserting the frame number
 * zero wouldn't make sense).
 *
 * This implies that actors you intend to be affected by the timeline's
 * progress should be manually primed or positioned for frame zero which
 * will be displayed before the first timeout (if you are not careful about
 * this point you will likely see flashes of incorrect actor state in your
 * program).
 *
 * For a non looping timeline the last timeout would be for the number
 * of frames in the timeline, as returned by clutter_timeline_get_n_frames().
 *
 * For a looping timeline the timeout for the last frame would be followed
 * by a timeout for frame number 1.
 *
 * There may be times when a system is not able to meet the frame rate
 * requested for a timeline, and in this case the frame number will be
 * interpolated at the next timeout event. The interpolation is calculated from
 * the time that the timeline was started, not from the time of the last
 * timeout, so a given timeline should basically elapse in the same - real
 * world - time on any given system. An invariable here though is that
 * the last frame will always be signaled, but notably frame number 1 can
 * be interpolated past and thus never signaled.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-timeout-pool.h"
#include "clutter-timeline.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"

G_DEFINE_TYPE (ClutterTimeline, clutter_timeline, G_TYPE_OBJECT);

#define FPS_TO_INTERVAL(f) (1000 / (f))

struct _ClutterTimelinePrivate
{
  ClutterTimelineDirection direction;

  guint timeout_id;
  guint delay_id;

  gint current_frame_num;

  guint fps;
  guint n_frames;
  guint delay;

  gint skipped_frames;

  GTimeVal prev_frame_timeval;
  guint msecs_delta;

  GHashTable *markers_by_frame;
  GHashTable *markers_by_name;

  guint loop : 1;
};

typedef struct {
  gchar *name;
  guint frame_num;
  GQuark quark;
} TimelineMarker;

enum
{
  PROP_0,

  PROP_FPS,
  PROP_NUM_FRAMES,
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

static guint               timeline_signals[LAST_SIGNAL] = { 0, };
static gint                timeline_use_pool             = -1;
static ClutterTimeoutPool *timeline_pool                 = NULL;

static inline void
timeline_pool_init (void)
{
  if (timeline_use_pool == -1)
    {
      const gchar *timeline_env;

      timeline_env = g_getenv ("CLUTTER_TIMELINE");
      if (timeline_env && timeline_env[0] != '\0' &&
          strcmp (timeline_env, "no-pool") == 0)
        {
          timeline_use_pool = FALSE;
        }
      else
        {
          timeline_pool = clutter_timeout_pool_new (CLUTTER_PRIORITY_TIMELINE);
          timeline_use_pool = TRUE;
        }
    }
}

static guint
timeout_add (guint          interval,
             GSourceFunc    func,
             gpointer       data,
             GDestroyNotify notify)
{
  guint res;

  if (G_LIKELY (timeline_use_pool))
    {
      g_assert (timeline_pool != NULL);
      res = clutter_timeout_pool_add (timeline_pool,
                                      interval,
                                      func, data, notify);
    }
  else
    {
      res = clutter_threads_add_frame_source_full (CLUTTER_PRIORITY_TIMELINE,
						   interval,
						   func, data, notify);
    }

  return res;
}

static void
timeout_remove (guint tag)
{
  if (G_LIKELY (timeline_use_pool))
    {
      g_assert (timeline_pool != NULL);
      clutter_timeout_pool_remove (timeline_pool, tag);
    }
  else
    g_source_remove (tag);
}

static TimelineMarker *
timeline_marker_new (const gchar *name,
                     guint        frame_num)
{
  TimelineMarker *marker = g_slice_new0 (TimelineMarker);

  marker->name = g_strdup (name);
  marker->quark = g_quark_from_string (marker->name);
  marker->frame_num = frame_num;

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
    case PROP_FPS:
      clutter_timeline_set_speed (timeline, g_value_get_uint (value));
      break;

    case PROP_NUM_FRAMES:
      clutter_timeline_set_n_frames (timeline, g_value_get_uint (value));
      break;

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
    case PROP_FPS:
      g_value_set_uint (value, priv->fps);
      break;

    case PROP_NUM_FRAMES:
      g_value_set_uint (value, priv->n_frames);
      break;

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
  ClutterTimelinePrivate *priv = CLUTTER_TIMELINE (object)->priv;

  if (priv->markers_by_frame)
    g_hash_table_destroy (priv->markers_by_frame);

  if (priv->markers_by_name)
    g_hash_table_destroy (priv->markers_by_name);

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
      timeout_remove (priv->delay_id);
      priv->delay_id = 0;
    }

  if (priv->timeout_id)
    {
      timeout_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  G_OBJECT_CLASS (clutter_timeline_parent_class)->dispose (object);
}

static void
clutter_timeline_class_init (ClutterTimelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  timeline_pool_init ();

  object_class->set_property = clutter_timeline_set_property;
  object_class->get_property = clutter_timeline_get_property;
  object_class->finalize     = clutter_timeline_finalize;
  object_class->dispose      = clutter_timeline_dispose;

  g_type_class_add_private (klass, sizeof (ClutterTimelinePrivate));

  /**
   * ClutterTimeline:fps:
   *
   * Number of frames per second. Because of the nature of the main
   * loop used by Clutter, we can only accept a granularity of one
   * frame per millisecond.
   *
   * This value is to be considered a best approximation.
   */
  pspec = g_param_spec_uint ("fps",
                             "Frames Per Second",
                             "Frames per second",
                             1, 1000,
                             60,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FPS, pspec);

  /**
   * ClutterTimeline:num-frames:
   *
   * Total number of frames for the timeline.
   */
  pspec = g_param_spec_uint ("num-frames",
                             "Total number of frames",
                             "Total number of frames",
                             1, G_MAXUINT,
                             1,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_NUM_FRAMES, pspec);

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
   * @frame_num: the number of the new frame between 0 and
   * ClutterTimeline:num-frames
   *
   * The ::new-frame signal is emitted each time a new frame in the
   * timeline is reached.
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
   * @frame_num: the frame number
   *
   * The ::marker-reached signal is emitted each time a timeline
   * reaches a marker set with clutter_timeline_add_marker_at_frame()
   * or clutter_timeline_add_marker_at_time(). This signal is
   * detailed with the name of the marker as well, so it is
   * possible to connect a callback to the ::marker-reached signal
   * for a specific marker with:
   *
   * <informalexample><programlisting>
   *   clutter_timeline_add_marker_at_frame (timeline, "foo", 24);
   *   clutter_timeline_add_marker_at_frame (timeline, "bar", 48);
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

  priv->fps = clutter_get_default_frame_rate ();
  priv->n_frames = 0;
  priv->msecs_delta = 0;
}

static void
emit_frame_signal (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv = timeline->priv;
  gint i;

  g_signal_emit (timeline, timeline_signals[NEW_FRAME], 0,
                 priv->current_frame_num);

  /* shortcircuit here if we don't have any marker installed */
  if (priv->markers_by_name == NULL ||
      priv->markers_by_frame == NULL)
    return;

  for (i = priv->skipped_frames; i >= 0; i--)
    {
      gint frame_num;
      GSList *markers, *l;

      frame_num = priv->current_frame_num
                + (priv->direction == CLUTTER_TIMELINE_FORWARD ? -i : i);
      markers = g_hash_table_lookup (priv->markers_by_frame,
                                     GUINT_TO_POINTER (frame_num));
      for (l = markers; l; l = l->next)
        {
          TimelineMarker *marker = l->data;

          CLUTTER_NOTE (SCHEDULER, "Marker `%s' reached", marker->name);

          g_signal_emit (timeline, timeline_signals[MARKER_REACHED],
                         marker->quark,
                         marker->name,
                         marker->frame_num);
        }
    }
}

static gboolean
is_complete (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv = timeline->priv;

  return ((priv->direction == CLUTTER_TIMELINE_FORWARD) &&
          (priv->current_frame_num >= priv->n_frames)) ||
         ((priv->direction == CLUTTER_TIMELINE_BACKWARD) &&
          (priv->current_frame_num <= 0));
}

static gboolean
timeline_timeout_func (gpointer data)
{
  ClutterTimeline        *timeline = data;
  ClutterTimelinePrivate *priv;
  GTimeVal                timeval;
  guint                   n_frames, speed;
  gulong                  msecs;

  priv = timeline->priv;

  g_object_ref (timeline);

  /* Figure out potential frame skips */
  g_get_current_time (&timeval);

  CLUTTER_TIMESTAMP (SCHEDULER, "Timeline [%p] activated (cur: %d)\n",
                     timeline,
                     priv->current_frame_num);

  if (!priv->prev_frame_timeval.tv_sec)
    {
      CLUTTER_NOTE (SCHEDULER,
                    "Timeline [%p] recieved timeout before being initialised!",
                    timeline);
      priv->prev_frame_timeval = timeval;
    }

  /* Interpolate the current frame based on the timeval of the
   * previous frame */
  msecs = (timeval.tv_sec - priv->prev_frame_timeval.tv_sec) * 1000;
  msecs += (timeval.tv_usec - priv->prev_frame_timeval.tv_usec) / 1000;
  priv->msecs_delta = msecs;

  /* we need to avoid fps > 1000 */
  speed = MAX (1000 / priv->fps, 1);

  n_frames = msecs / speed;
  if (n_frames == 0)
    n_frames = 1;

  priv->skipped_frames = n_frames - 1;

  if (priv->skipped_frames)
    CLUTTER_TIMESTAMP (SCHEDULER,
                       "Timeline [%p], skipping %d frames\n",
                       timeline,
                       priv->skipped_frames);

  priv->prev_frame_timeval = timeval;

  /* Advance frames */
  if (priv->direction == CLUTTER_TIMELINE_FORWARD)
    priv->current_frame_num += n_frames;
  else
    priv->current_frame_num -= n_frames;

  /* If we have not reached the end of the timeline: */
  if (!is_complete (timeline))
    {
      /* Emit the signal */
      emit_frame_signal (timeline);

      /* Signal pauses timeline ? */
      if (!priv->timeout_id)
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
      ClutterTimelineDirection saved_direction = priv->direction;
      guint overflow_frame_num = priv->current_frame_num;
      gint end_frame;

      /* Update the current frame number in case the signal handlers
         want to take a peek. Don't count skipped frames that run past
         the end of the timeline */
      if (priv->direction == CLUTTER_TIMELINE_FORWARD)
        {
          priv->skipped_frames -= priv->current_frame_num - priv->n_frames;
          priv->current_frame_num = priv->n_frames;
        }
      else if (priv->direction == CLUTTER_TIMELINE_BACKWARD)
        {
          priv->skipped_frames += priv->current_frame_num;
          priv->current_frame_num = 0;
        }

      end_frame = priv->current_frame_num;

      /* Emit the signal */
      emit_frame_signal (timeline);

      /* Did the signal handler modify the current_frame_num */
      if (priv->current_frame_num != end_frame)
        {
          g_object_unref (timeline);
          return TRUE;
        }

      /* Note: If the new-frame signal handler paused the timeline
       * on the last frame we will still go ahead and send the
       * completed signal */
      CLUTTER_NOTE (SCHEDULER,
                    "Timeline [%p] completed (cur: %d, tot: %d, drop: %d)",
                    timeline,
                    priv->current_frame_num,
                    priv->n_frames,
                    n_frames - 1);

      if (!priv->loop && priv->timeout_id)
        {
          /* We remove the timeout now, so that the completed signal handler
           * may choose to re-start the timeline
           *
           * XXX Perhaps we should remove this earlier, and regardless
           * of priv->loop. Are we limiting the things that could be done in
           * the above new-frame signal handler */
          timeout_remove (priv->timeout_id);
          priv->timeout_id = 0;
        }

      g_signal_emit (timeline, timeline_signals[COMPLETED], 0);

      /* Again check to see if the user has manually played with
       * current_frame_num, before we finally stop or loop the timeline */

      if (priv->current_frame_num != end_frame &&
          !(/* Except allow moving from frame 0 -> n_frame (or vica-versa)
               since these are considered equivalent */
            (priv->current_frame_num == 0 && end_frame == priv->n_frames) ||
            (priv->current_frame_num == priv->n_frames && end_frame == 0)
          ))
        {
          g_object_unref (timeline);
          return TRUE;
        }

      if (priv->loop)
        {
          /* We try and interpolate smoothly around a loop */
          if (saved_direction == CLUTTER_TIMELINE_FORWARD)
            priv->current_frame_num = overflow_frame_num - priv->n_frames;
          else
            priv->current_frame_num = priv->n_frames + overflow_frame_num;

          /* Or if the direction changed, we try and bounce */
          if (priv->direction != saved_direction)
            {
              priv->current_frame_num = priv->n_frames
                                        - priv->current_frame_num;
            }

          g_object_unref (timeline);
          return TRUE;
        }
      else
        {
          clutter_timeline_rewind (timeline);

          priv->prev_frame_timeval.tv_sec = 0;
          priv->prev_frame_timeval.tv_usec = 0;

          g_object_unref (timeline);
          return FALSE;
        }
    }
}

static guint
timeline_timeout_add (ClutterTimeline *timeline,
                      guint            interval,
                      GSourceFunc      func,
                      gpointer         data,
                      GDestroyNotify   notify)
{
  ClutterTimelinePrivate *priv;
  GTimeVal timeval;

  priv = timeline->priv;

  if (priv->prev_frame_timeval.tv_sec == 0)
    {
      g_get_current_time (&timeval);
      priv->prev_frame_timeval = timeval;
    }

  priv->skipped_frames   = 0;
  priv->msecs_delta      = 0;

  return timeout_add (interval, func, data, notify);
}

static gboolean
delay_timeout_func (gpointer data)
{
  ClutterTimeline *timeline = data;
  ClutterTimelinePrivate *priv = timeline->priv;

  priv->delay_id = 0;

  priv->timeout_id = timeline_timeout_add (timeline,
                                           FPS_TO_INTERVAL (priv->fps),
                                           timeline_timeout_func,
                                           timeline, NULL);

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

  if (priv->delay_id || priv->timeout_id)
    return;

  if (priv->n_frames == 0)
    return;

  if (priv->delay)
    {
      priv->delay_id = timeout_add (priv->delay,
                                    delay_timeout_func,
                                    timeline, NULL);
    }
  else
    {
      priv->timeout_id = timeline_timeout_add (timeline,
                                               FPS_TO_INTERVAL (priv->fps),
                                               timeline_timeout_func,
                                               timeline, NULL);

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

  if (priv->delay_id)
    {
      timeout_remove (priv->delay_id);
      priv->delay_id = 0;
    }

  if (priv->timeout_id)
    {
      timeout_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  priv->prev_frame_timeval.tv_sec = 0;
  priv->prev_frame_timeval.tv_usec = 0;

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
    clutter_timeline_advance (timeline, priv->n_frames);
}

/**
 * clutter_timeline_skip:
 * @timeline: A #ClutterTimeline
 * @n_frames: Number of frames to skip
 *
 * Advance timeline by the requested number of frames.
 */
void
clutter_timeline_skip (ClutterTimeline *timeline,
                       guint            n_frames)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->direction == CLUTTER_TIMELINE_FORWARD)
    {
      priv->current_frame_num += n_frames;

      if (priv->current_frame_num > priv->n_frames)
        priv->current_frame_num = 1;
    }
  else if (priv->direction == CLUTTER_TIMELINE_BACKWARD)
    {
      priv->current_frame_num -= n_frames;

      if (priv->current_frame_num < 1)
        priv->current_frame_num = priv->n_frames - 1;
    }
}

/**
 * clutter_timeline_advance:
 * @timeline: A #ClutterTimeline
 * @frame_num: Frame number to advance to
 *
 * Advance timeline to the requested frame number.
 *
 * <note><para>The @timeline will not emit the #ClutterTimeline::new-frame
 * signal for @frame_num. The first ::new-frame signal after the call to
 * clutter_timeline_advance() will be emitted for a frame following
 * @frame_num.</para></note>
 */
void
clutter_timeline_advance (ClutterTimeline *timeline,
                          guint            frame_num)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  priv->current_frame_num = CLAMP (frame_num, 0, priv->n_frames);
}

/**
 * clutter_timeline_get_current_frame:
 * @timeline: A #ClutterTimeline
 *
 * Request the current frame number of the timeline.
 *
 * Return value: current frame number
 */
gint
clutter_timeline_get_current_frame (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  return timeline->priv->current_frame_num;
}

/**
 * clutter_timeline_get_n_frames:
 * @timeline: A #ClutterTimeline
 *
 * Request the total number of frames for the #ClutterTimeline.
 *
 * Return value: the number of frames
 */
guint
clutter_timeline_get_n_frames (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  return timeline->priv->n_frames;
}

/**
 * clutter_timeline_set_n_frames:
 * @timeline: a #ClutterTimeline
 * @n_frames: the number of frames
 *
 * Sets the total number of frames for @timeline
 */
void
clutter_timeline_set_n_frames (ClutterTimeline *timeline,
                               guint            n_frames)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (n_frames > 0);

  priv = timeline->priv;

  if (priv->n_frames != n_frames)
    {
      g_object_freeze_notify (G_OBJECT (timeline));

      priv->n_frames = n_frames;

      g_object_notify (G_OBJECT (timeline), "num-frames");
      g_object_notify (G_OBJECT (timeline), "duration");

      g_object_thaw_notify (G_OBJECT (timeline));
    }
}

/**
 * clutter_timeline_set_speed:
 * @timeline: A #ClutterTimeline
 * @fps: New speed of timeline as frames per second,
 *    between 1 and 1000
 *
 * Sets the speed of @timeline in frames per second.
 */
void
clutter_timeline_set_speed (ClutterTimeline *timeline,
                            guint            fps)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (fps > 0 && fps <= 1000);

  priv = timeline->priv;

  if (priv->fps != fps)
    {
      g_object_ref (timeline);

      priv->fps = fps;

      /* if the timeline is playing restart */
      if (priv->timeout_id)
        {
          timeout_remove (priv->timeout_id);

          priv->timeout_id = timeline_timeout_add (timeline,
                                                   FPS_TO_INTERVAL (priv->fps),
                                                   timeline_timeout_func,
                                                   timeline, NULL);
        }

      g_object_freeze_notify (G_OBJECT (timeline));

      g_object_notify (G_OBJECT (timeline), "duration");
      g_object_notify (G_OBJECT (timeline), "fps");

      g_object_thaw_notify (G_OBJECT (timeline));

      g_object_unref (timeline);
    }
}

/**
 * clutter_timeline_get_speed:
 * @timeline: a #ClutterTimeline
 *
 * Gets the frames per second played by @timeline
 *
 * Return value: the number of frames per second.
 */
guint
clutter_timeline_get_speed (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  return timeline->priv->fps;
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

  return (timeline->priv->timeout_id != 0);
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
                       "fps", clutter_timeline_get_speed (timeline),
                       "num-frames", clutter_timeline_get_n_frames (timeline),
                       "loop", clutter_timeline_get_loop (timeline),
                       "delay", clutter_timeline_get_delay (timeline),
                       "direction", clutter_timeline_get_direction (timeline),
                       NULL);

  return copy;
}

/**
 * clutter_timeline_new_for_duration:
 * @msecs: Duration of the timeline in milliseconds
 *
 * Creates a new #ClutterTimeline with a duration of @msecs using
 * the value of the ClutterTimeline:fps property to compute the
 * equivalent number of frames.
 *
 * Return value: the newly created #ClutterTimeline
 *
 * Since: 0.6
 */
ClutterTimeline *
clutter_timeline_new_for_duration (guint msecs)
{
  return g_object_new (CLUTTER_TYPE_TIMELINE,
                       "duration", msecs,
                       NULL);
}

/**
 * clutter_timeline_new:
 * @n_frames: the number of frames
 * @fps: the number of frames per second
 *
 * Create a new  #ClutterTimeline instance.
 *
 * Return Value: a new #ClutterTimeline
 */
ClutterTimeline*
clutter_timeline_new (guint n_frames,
		      guint fps)
{
  g_return_val_if_fail (n_frames > 0, NULL);
  g_return_val_if_fail (fps > 0 && fps <= 1000, NULL);

  return g_object_new (CLUTTER_TYPE_TIMELINE,
		       "fps", fps,
		       "num-frames", n_frames,
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

  return priv->n_frames * 1000 / priv->fps;
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
  guint n_frames;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  n_frames = msecs * priv->fps / 1000;
  if (n_frames < 1)
    n_frames = 1;

  /* this will notify :duration as well */
  clutter_timeline_set_n_frames (timeline, n_frames);
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

  return (gdouble) priv->current_frame_num / (gdouble) priv->n_frames;
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

      if (priv->current_frame_num == 0)
        priv->current_frame_num = priv->n_frames;

      g_object_notify (G_OBJECT (timeline), "direction");
    }
}

/**
 * clutter_timeline_get_delta:
 * @timeline: a #ClutterTimeline
 * @msecs: return location for the milliseconds elapsed since the last
 *   frame, or %NULL
 *
 * Retrieves the number of frames and the amount of time elapsed since
 * the last ClutterTimeline::new-frame signal.
 *
 * This function is only useful inside handlers for the ::new-frame
 * signal, and its behaviour is undefined if the timeline is not
 * playing.
 *
 * Return value: the amount of frames elapsed since the last one
 *
 * Since: 0.6
 */
guint
clutter_timeline_get_delta (ClutterTimeline *timeline,
                            guint           *msecs)
{
  ClutterTimelinePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  if (!clutter_timeline_is_playing (timeline))
    {
      if (msecs)
        *msecs = 0;

      return 0;
    }

  priv = timeline->priv;

  if (msecs)
    *msecs = timeline->priv->msecs_delta;

  return priv->skipped_frames + 1;
}

static inline void
clutter_timeline_add_marker_internal (ClutterTimeline *timeline,
                                      const gchar     *marker_name,
                                      guint            frame_num)
{
  ClutterTimelinePrivate *priv = timeline->priv;
  TimelineMarker *marker;
  GSList *markers;

  /* create the hash tables that will hold the markers */
  if (G_UNLIKELY (priv->markers_by_name == NULL ||
                  priv->markers_by_frame == NULL))
    {
      priv->markers_by_name = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     NULL,
                                                     timeline_marker_free);
      priv->markers_by_frame = g_hash_table_new (NULL, NULL);
    }

  marker = g_hash_table_lookup (priv->markers_by_name, marker_name);
  if (G_UNLIKELY (marker))
    {
      g_warning ("A marker named `%s' already exists on frame %d",
                 marker->name,
                 marker->frame_num);
      return;
    }

  marker = timeline_marker_new (marker_name, frame_num);
  g_hash_table_insert (priv->markers_by_name, marker->name, marker);

  markers = g_hash_table_lookup (priv->markers_by_frame,
                                 GUINT_TO_POINTER (frame_num));
  if (!markers)
    {
      markers = g_slist_prepend (NULL, marker);
      g_hash_table_insert (priv->markers_by_frame,
                           GUINT_TO_POINTER (frame_num),
                           markers);
    }
  else
    {
      markers = g_slist_prepend (markers, marker);
      g_hash_table_replace (priv->markers_by_frame,
                            GUINT_TO_POINTER (frame_num),
                            markers);
    }
}

/**
 * clutter_timeline_add_marker_at_frame:
 * @timeline: a #ClutterTimeline
 * @marker_name: the unique name for this marker
 * @frame_num: the marker's frame
 *
 * Adds a named marker at @frame_num. Markers are unique string identifiers
 * for a specific frame. Once @timeline reaches @frame_num, it will emit
 * a ::marker-reached signal for each marker attached to that frame.
 *
 * A marker can be removed with clutter_timeline_remove_marker(). The
 * timeline can be advanced to a marker using
 * clutter_timeline_advance_to_marker().
 *
 * Since: 0.8
 */
void
clutter_timeline_add_marker_at_frame (ClutterTimeline *timeline,
                                      const gchar     *marker_name,
                                      guint            frame_num)
{
  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);
  g_return_if_fail (frame_num <= clutter_timeline_get_n_frames (timeline));

  clutter_timeline_add_marker_internal (timeline, marker_name, frame_num);
}

/**
 * clutter_timeline_add_marker_at_time:
 * @timeline: a #ClutterTimeline
 * @marker_name: the unique name for this marker
 * @msecs: position of the marker in milliseconds
 *
 * Time-based variant of clutter_timeline_add_marker_at_frame().
 *
 * Adds a named marker at @msecs.
 *
 * Since: 0.8
 */
void
clutter_timeline_add_marker_at_time (ClutterTimeline *timeline,
                                     const gchar     *marker_name,
                                     guint            msecs)
{
  guint frame_num;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);
  g_return_if_fail (msecs <= clutter_timeline_get_duration (timeline));

  frame_num = msecs * timeline->priv->fps / 1000;

  clutter_timeline_add_marker_internal (timeline, marker_name, frame_num);
}

/**
 * clutter_timeline_list_markers:
 * @timeline: a #ClutterTimeline
 * @frame_num: the frame number to check, or -1
 * @n_markers: the number of markers returned
 *
 * Retrieves the list of markers at @frame_num. If @frame_num is a
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
                               gint             frame_num,
                               gsize           *n_markers)
{
  ClutterTimelinePrivate *priv;
  gchar **retval = NULL;
  gsize i;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);

  priv = timeline->priv;

  if (G_UNLIKELY (priv->markers_by_name == NULL ||
                  priv->markers_by_frame == NULL))
    {
      if (n_markers)
        *n_markers = 0;

      return NULL;
    }

  if (frame_num < 0)
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
      GSList *markers, *l;

      markers = g_hash_table_lookup (priv->markers_by_frame,
                                     GUINT_TO_POINTER (frame_num));
      retval = g_new0 (gchar*, g_slist_length (markers) + 1);

      for (i = 0, l = markers; l != NULL; i++, l = l->next)
        retval[i] = g_strdup (((TimelineMarker *) l->data)->name);
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
 * Advances @timeline to the frame of the given @marker_name.
 *
 * <note><para>Like clutter_timeline_advance(), this function will not
 * emit the #ClutterTimeline::new-frame for the frame where @marker_name
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

  if (G_UNLIKELY (priv->markers_by_name == NULL ||
                  priv->markers_by_frame == NULL))
    {
      g_warning ("No marker named `%s' found.", marker_name);
      return;
    }

  marker = g_hash_table_lookup (priv->markers_by_name, marker_name);
  if (!marker)
    {
      g_warning ("No marker named `%s' found.", marker_name);
      return;
    }

  clutter_timeline_advance (timeline, marker->frame_num);
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
  GSList *markers;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);

  priv = timeline->priv;

  if (G_UNLIKELY (priv->markers_by_name == NULL ||
                  priv->markers_by_frame == NULL))
    {
      g_warning ("No marker named `%s' found.", marker_name);
      return;
    }

  marker = g_hash_table_lookup (priv->markers_by_name, marker_name);
  if (!marker)
    {
      g_warning ("No marker named `%s' found.", marker_name);
      return;
    }

  /* remove from the list of markers at the same frame */
  markers = g_hash_table_lookup (priv->markers_by_frame,
                                 GUINT_TO_POINTER (marker->frame_num));
  if (G_LIKELY (markers))
    {
      markers = g_slist_remove (markers, marker);
      if (!markers)
        {
          /* no markers left, remove the slot */
          g_hash_table_remove (priv->markers_by_frame,
                               GUINT_TO_POINTER (marker->frame_num));
        }
      else
        g_hash_table_replace (priv->markers_by_frame,
                              GUINT_TO_POINTER (marker->frame_num),
                              markers);
    }
  else
    {
      /* uh-oh, dangling marker; this should never happen */
      g_warning ("Dangling marker %s at frame %d",
                 marker->name,
                 marker->frame_num);
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

  if (G_UNLIKELY (timeline->priv->markers_by_name == NULL ||
                  timeline->priv->markers_by_frame == NULL))
    return FALSE;

  return NULL != g_hash_table_lookup (timeline->priv->markers_by_name,
                                      marker_name);
}
