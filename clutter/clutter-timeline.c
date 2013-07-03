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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

/**
 * SECTION:clutter-timeline
 * @short_description: A class for time-based events
 * @see_also: #ClutterAnimation, #ClutterAnimator, #ClutterState
 *
 * #ClutterTimeline is a base class for managing time-based event that cause
 * Clutter to redraw a stage, such as animations.
 *
 * Each #ClutterTimeline instance has a duration: once a timeline has been
 * started, using clutter_timeline_start(), it will emit a signal that can
 * be used to update the state of the actors.
 *
 * It is important to note that #ClutterTimeline is not a generic API for
 * calling closures after an interval; each Timeline is tied into the master
 * clock used to drive the frame cycle. If you need to schedule a closure
 * after an interval, see clutter_threads_add_timeout() instead.
 *
 * Users of #ClutterTimeline should connect to the #ClutterTimeline::new-frame
 * signal, which is emitted each time a timeline is advanced during the maste
 * clock iteration. The #ClutterTimeline::new-frame signal provides the time
 * elapsed since the beginning of the timeline, in milliseconds. A normalized
 * progress value can be obtained by calling clutter_timeline_get_progress().
 * By using clutter_timeline_get_delta() it is possible to obtain the wallclock
 * time elapsed since the last emission of the #ClutterTimeline::new-frame
 * signal.
 *
 * Initial state can be set up by using the #ClutterTimeline::started signal,
 * while final state can be set up by using the #ClutterTimeline::stopped
 * signal. The #ClutterTimeline guarantees the emission of at least a single
 * #ClutterTimeline::new-frame signal, as well as the emission of the
 * #ClutterTimeline::completed signal every time the #ClutterTimeline reaches
 * its #ClutterTimeline:duration.
 *
 * It is possible to connect to specific points in the timeline progress by
 * adding <emphasis>markers</emphasis> using clutter_timeline_add_marker_at_time()
 * and connecting to the #ClutterTimeline::marker-reached signal.
 *
 * Timelines can be made to loop once they reach the end of their duration, by
 * using clutter_timeline_set_repeat_count(); a looping timeline will still
 * emit the #ClutterTimeline::completed signal once it reaches the end of its
 * duration at each repeat. If you want to be notified of the end of the last
 * repeat, use the #ClutterTimeline::stopped signal.
 *
 * Timelines have a #ClutterTimeline:direction: the default direction is
 * %CLUTTER_TIMELINE_FORWARD, and goes from 0 to the duration; it is possible
 * to change the direction to %CLUTTER_TIMELINE_BACKWARD, and have the timeline
 * go from the duration to 0. The direction can be automatically reversed
 * when reaching completion by using the #ClutterTimeline:auto-reverse property.
 *
 * Timelines are used in the Clutter animation framework by classes like
 * #ClutterAnimation, #ClutterAnimator, and #ClutterState.
 *
 * <refsect2 id="timeline-script">
 *  <title>Defining Timelines in ClutterScript</title>
 *  <para>A #ClutterTimeline can be described in #ClutterScript like any
 *  other object. Additionally, it is possible to define markers directly
 *  inside the JSON definition by using the <emphasis>markers</emphasis>
 *  JSON object member, such as:</para>
 *  <informalexample><programlisting><![CDATA[
{
  "type" : "ClutterTimeline",
  "duration" : 1000,
  "markers" : [
    { "name" : "quarter", "time" : 250 },
    { "name" : "half-time", "time" : 500 },
    { "name" : "three-quarters", "time" : 750 }
  ]
}
 *  ]]></programlisting></informalexample>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-timeline.h"

#include "clutter-debug.h"
#include "clutter-easing.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-master-clock.h"
#include "clutter-private.h"
#include "clutter-scriptable.h"

#include "deprecated/clutter-timeline.h"

struct _ClutterTimelinePrivate
{
  ClutterTimelineDirection direction;

  guint delay_id;

  /* The total length in milliseconds of this timeline */
  guint duration;
  guint delay;

  /* The current amount of elapsed time */
  gint64 elapsed_time;

  /* The elapsed time since the last frame was fired */
  gint64 msecs_delta;

  GHashTable *markers_by_name;

  /* Time we last advanced the elapsed time and showed a frame */
  gint64 last_frame_time;

  /* How many times the timeline should repeat */
  gint repeat_count;

  /* The number of times the timeline has repeated */
  gint current_repeat;

  ClutterTimelineProgressFunc progress_func;
  gpointer progress_data;
  GDestroyNotify progress_notify;
  ClutterAnimationMode progress_mode;

  /* step() parameters */
  gint n_steps;
  ClutterStepMode step_mode;

  /* cubic-bezier() parameters */
  ClutterPoint cb_1;
  ClutterPoint cb_2;

  guint is_playing         : 1;

  /* If we've just started playing and haven't yet gotten
   * a tick from the master clock
   */
  guint waiting_first_tick : 1;
  guint auto_reverse       : 1;
};

typedef struct {
  gchar *name;
  GQuark quark;

  union {
    guint msecs;
    gdouble progress;
  } data;

  guint is_relative : 1;
} TimelineMarker;

enum
{
  PROP_0,

  PROP_LOOP,
  PROP_DELAY,
  PROP_DURATION,
  PROP_DIRECTION,
  PROP_AUTO_REVERSE,
  PROP_REPEAT_COUNT,
  PROP_PROGRESS_MODE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

enum
{
  NEW_FRAME,
  STARTED,
  PAUSED,
  COMPLETED,
  MARKER_REACHED,
  STOPPED,

  LAST_SIGNAL
};

static guint timeline_signals[LAST_SIGNAL] = { 0, };

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterTimeline, clutter_timeline, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (ClutterTimeline)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_iface_init))

static TimelineMarker *
timeline_marker_new_time (const gchar *name,
                          guint        msecs)
{
  TimelineMarker *marker = g_slice_new (TimelineMarker);

  marker->name = g_strdup (name);
  marker->quark = g_quark_from_string (marker->name);
  marker->is_relative = FALSE;
  marker->data.msecs = msecs;

  return marker;
}

static TimelineMarker *
timeline_marker_new_progress (const gchar *name,
                              gdouble      progress)
{
  TimelineMarker *marker = g_slice_new (TimelineMarker);

  marker->name = g_strdup (name);
  marker->quark = g_quark_from_string (marker->name);
  marker->is_relative = TRUE;
  marker->data.progress = CLAMP (progress, 0.0, 1.0);

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

/*< private >
 * clutter_timeline_add_marker_internal:
 * @timeline: a #ClutterTimeline
 * @marker: a TimelineMarker
 *
 * Adds @marker into the hash table of markers for @timeline.
 *
 * The TimelineMarker will either be added or, in case of collisions
 * with another existing marker, freed. In any case, this function
 * assumes the ownership of the passed @marker.
 */
static inline void
clutter_timeline_add_marker_internal (ClutterTimeline *timeline,
                                      TimelineMarker  *marker)
{
  ClutterTimelinePrivate *priv = timeline->priv;
  TimelineMarker *old_marker;

  /* create the hash table that will hold the markers */
  if (G_UNLIKELY (priv->markers_by_name == NULL))
    priv->markers_by_name = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   NULL,
                                                   timeline_marker_free);

  old_marker = g_hash_table_lookup (priv->markers_by_name, marker->name);
  if (old_marker != NULL)
    {
      guint msecs;

      if (old_marker->is_relative)
        msecs = old_marker->data.progress * priv->duration;
      else
        msecs = old_marker->data.msecs;

      g_warning ("A marker named '%s' already exists at time %d",
                 old_marker->name,
                 msecs);
      timeline_marker_free (marker);
      return;
    }

  g_hash_table_insert (priv->markers_by_name, marker->name, marker);
}

static inline void
clutter_timeline_set_loop_internal (ClutterTimeline *timeline,
                                    gboolean         loop)
{
  gint old_repeat_count;

  old_repeat_count = timeline->priv->repeat_count;

  if (loop)
    clutter_timeline_set_repeat_count (timeline, -1);
  else
    clutter_timeline_set_repeat_count (timeline, 0);

  if (old_repeat_count != timeline->priv->repeat_count)
    g_object_notify_by_pspec (G_OBJECT (timeline), obj_props[PROP_LOOP]);
}

/* Scriptable */
typedef struct _ParseClosure {
  ClutterTimeline *timeline;
  ClutterScript *script;
  GValue *value;
  gboolean result;
} ParseClosure;

static void
parse_timeline_markers (JsonArray *array,
                        guint      index_,
                        JsonNode  *element,
                        gpointer   data)
{
  ParseClosure *clos = data;
  JsonObject *object;
  TimelineMarker *marker;
  GList *markers;

  if (JSON_NODE_TYPE (element) != JSON_NODE_OBJECT)
    {
      g_warning ("The 'markers' member of a ClutterTimeline description "
                 "should be an array of objects, but the element %d of the "
                 "array is of type '%s'. The element will be ignored.",
                 index_,
                 json_node_type_name (element));
      return;
    }

  object = json_node_get_object (element);

  if (!(json_object_has_member (object, "name") &&
        (json_object_has_member (object, "time") ||
         json_object_has_member (object, "progress"))))
    {
      g_warning ("The marker definition in a ClutterTimeline description "
                 "must be an object with the 'name' and either the 'time' "
                 "or the 'progress' members, but the element %d of the "
                 "'markers' array does not have any of them.",
                 index_);
      return;
    }

  if (G_IS_VALUE (clos->value))
    markers = g_value_get_pointer (clos->value);
  else
    {
      g_value_init (clos->value, G_TYPE_POINTER);
      markers = NULL;
    }

  if (json_object_has_member (object, "time"))
    marker = timeline_marker_new_time (json_object_get_string_member (object, "name"),
                                       json_object_get_int_member (object, "time"));
  else
    marker = timeline_marker_new_progress (json_object_get_string_member (object, "name"),
                                           json_object_get_double_member (object, "progress"));

  markers = g_list_prepend (markers, marker);

  g_value_set_pointer (clos->value, markers);

  clos->result = TRUE;
}

static gboolean
clutter_timeline_parse_custom_node (ClutterScriptable *scriptable,
                                    ClutterScript     *script,
                                    GValue            *value,
                                    const gchar       *name,
                                    JsonNode          *node)
{
  ParseClosure clos;

  if (strcmp (name, "markers") != 0)
    return FALSE;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return FALSE;

  clos.timeline = CLUTTER_TIMELINE (scriptable);
  clos.script = script;
  clos.value = value;
  clos.result = FALSE;

  json_array_foreach_element (json_node_get_array (node),
                              parse_timeline_markers,
                              &clos);

  return clos.result;
}

static void
clutter_timeline_set_custom_property (ClutterScriptable *scriptable,
                                      ClutterScript     *script,
                                      const gchar       *name,
                                      const GValue      *value)
{
  if (strcmp (name, "markers") == 0)
    {
      ClutterTimeline *timeline = CLUTTER_TIMELINE (scriptable);
      GList *markers = g_value_get_pointer (value);
      GList *m;

      /* the list was created through prepend() */
      markers = g_list_reverse (markers);

      for (m = markers; m != NULL; m = m->next)
        clutter_timeline_add_marker_internal (timeline, m->data);

      g_list_free (markers);
    }
  else
    g_object_set_property (G_OBJECT (scriptable), name, value);
}


static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_timeline_parse_custom_node;
  iface->set_custom_property = clutter_timeline_set_custom_property;
}

/* Object */

static void
clutter_timeline_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  ClutterTimeline *timeline = CLUTTER_TIMELINE (object);

  switch (prop_id)
    {
    case PROP_LOOP:
      clutter_timeline_set_loop_internal (timeline, g_value_get_boolean (value));
      break;

    case PROP_DELAY:
      clutter_timeline_set_delay (timeline, g_value_get_uint (value));
      break;

    case PROP_DURATION:
      clutter_timeline_set_duration (timeline, g_value_get_uint (value));
      break;

    case PROP_DIRECTION:
      clutter_timeline_set_direction (timeline, g_value_get_enum (value));
      break;

    case PROP_AUTO_REVERSE:
      clutter_timeline_set_auto_reverse (timeline, g_value_get_boolean (value));
      break;

    case PROP_REPEAT_COUNT:
      clutter_timeline_set_repeat_count (timeline, g_value_get_int (value));
      break;

    case PROP_PROGRESS_MODE:
      clutter_timeline_set_progress_mode (timeline, g_value_get_enum (value));
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
  ClutterTimeline *timeline = CLUTTER_TIMELINE (object);
  ClutterTimelinePrivate *priv = timeline->priv;

  switch (prop_id)
    {
    case PROP_LOOP:
      g_value_set_boolean (value, priv->repeat_count != 0);
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

    case PROP_AUTO_REVERSE:
      g_value_set_boolean (value, priv->auto_reverse);
      break;

    case PROP_REPEAT_COUNT:
      g_value_set_int (value, priv->repeat_count);
      break;

    case PROP_PROGRESS_MODE:
      g_value_set_enum (value, priv->progress_mode);
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

  if (priv->progress_notify != NULL)
    {
      priv->progress_notify (priv->progress_data);
      priv->progress_func = NULL;
      priv->progress_data = NULL;
      priv->progress_notify = NULL;
    }

  G_OBJECT_CLASS (clutter_timeline_parent_class)->dispose (object);
}

static void
clutter_timeline_class_init (ClutterTimelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /**
   * ClutterTimeline:loop:
   *
   * Whether the timeline should automatically rewind and restart.
   *
   * As a side effect, setting this property to %TRUE will set the
   * #ClutterTimeline:repeat-count property to -1, while setting this
   * property to %FALSE will set the #ClutterTimeline:repeat-count
   * property to 0.
   *
   * Deprecated: 1.10: Use the #ClutterTimeline:repeat-count property instead.
   */
  obj_props[PROP_LOOP] =
    g_param_spec_boolean ("loop",
                          P_("Loop"),
                          P_("Should the timeline automatically restart"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE | G_PARAM_DEPRECATED);

  /**
   * ClutterTimeline:delay:
   *
   * A delay, in milliseconds, that should be observed by the
   * timeline before actually starting.
   *
   * Since: 0.4
   */
  obj_props[PROP_DELAY] =
    g_param_spec_uint ("delay",
                       P_("Delay"),
                       P_("Delay before start"),
                       0, G_MAXUINT,
                       0,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterTimeline:duration:
   *
   * Duration of the timeline in milliseconds, depending on the
   * ClutterTimeline:fps value.
   *
   * Since: 0.6
   */
  obj_props[PROP_DURATION] =
    g_param_spec_uint ("duration",
                       P_("Duration"),
                       P_("Duration of the timeline in milliseconds"),
                       0, G_MAXUINT,
                       1000,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterTimeline:direction:
   *
   * The direction of the timeline, either %CLUTTER_TIMELINE_FORWARD or
   * %CLUTTER_TIMELINE_BACKWARD.
   *
   * Since: 0.6
   */
  obj_props[PROP_DIRECTION] =
    g_param_spec_enum ("direction",
                       P_("Direction"),
                       P_("Direction of the timeline"),
                       CLUTTER_TYPE_TIMELINE_DIRECTION,
                       CLUTTER_TIMELINE_FORWARD,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterTimeline:auto-reverse:
   *
   * If the direction of the timeline should be automatically reversed
   * when reaching the end.
   *
   * Since: 1.6
   */
  obj_props[PROP_AUTO_REVERSE] =
    g_param_spec_boolean ("auto-reverse",
                          P_("Auto Reverse"),
                          P_("Whether the direction should be reversed when reaching the end"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE);

  /**
   * ClutterTimeline:repeat-count:
   *
   * Defines how many times the timeline should repeat.
   *
   * If the repeat count is 0, the timeline does not repeat.
   *
   * If the repeat count is set to -1, the timeline will repeat until it is
   * stopped.
   *
   * Since: 1.10
   */
  obj_props[PROP_REPEAT_COUNT] =
    g_param_spec_int ("repeat-count",
                      P_("Repeat Count"),
                      P_("How many times the timeline should repeat"),
                      -1, G_MAXINT,
                      0,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterTimeline:progress-mode:
   *
   * Controls the way a #ClutterTimeline computes the normalized progress.
   *
   * Since: 1.10
   */
  obj_props[PROP_PROGRESS_MODE] =
    g_param_spec_enum ("progress-mode",
                       P_("Progress Mode"),
                       P_("How the timeline should compute the progress"),
                       CLUTTER_TYPE_ANIMATION_MODE,
                       CLUTTER_LINEAR,
                       CLUTTER_PARAM_READWRITE);

  object_class->dispose = clutter_timeline_dispose;
  object_class->finalize = clutter_timeline_finalize;
  object_class->set_property = clutter_timeline_set_property;
  object_class->get_property = clutter_timeline_get_property;
  g_object_class_install_properties (object_class, PROP_LAST, obj_props);

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
		  _clutter_marshal_VOID__INT,
		  G_TYPE_NONE,
		  1, G_TYPE_INT);
  /**
   * ClutterTimeline::completed:
   * @timeline: the #ClutterTimeline which received the signal
   *
   * The #ClutterTimeline::completed signal is emitted when the timeline's
   * elapsed time reaches the value of the #ClutterTimeline:duration
   * property.
   *
   * This signal will be emitted even if the #ClutterTimeline is set to be
   * repeating.
   *
   * If you want to get notification on whether the #ClutterTimeline has
   * been stopped or has finished its run, including its eventual repeats,
   * you should use the #ClutterTimeline::stopped signal instead.
   */
  timeline_signals[COMPLETED] =
    g_signal_new (I_("completed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTimelineClass, completed),
		  NULL, NULL,
		  _clutter_marshal_VOID__VOID,
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
		  _clutter_marshal_VOID__VOID,
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
		  _clutter_marshal_VOID__VOID,
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
                  _clutter_marshal_VOID__STRING_INT,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_INT);
  /**
   * ClutterTimeline::stopped:
   * @timeline: the #ClutterTimeline that emitted the signal
   * @is_finished: %TRUE if the signal was emitted at the end of the
   *   timeline.
   *
   * The #ClutterTimeline::stopped signal is emitted when the timeline
   * has been stopped, either because clutter_timeline_stop() has been
   * called, or because it has been exhausted.
   *
   * This is different from the #ClutterTimeline::completed signal,
   * which gets emitted after every repeat finishes.
   *
   * If the #ClutterTimeline has is marked as infinitely repeating,
   * this signal will never be emitted.
   *
   * Since: 1.12
   */
  timeline_signals[STOPPED] =
    g_signal_new (I_("stopped"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTimelineClass, stopped),
		  NULL, NULL,
		  _clutter_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);
}

static void
clutter_timeline_init (ClutterTimeline *self)
{
  self->priv = clutter_timeline_get_instance_private (self);

  self->priv->progress_mode = CLUTTER_LINEAR;

  /* default steps() parameters are 1, end */
  self->priv->n_steps = 1;
  self->priv->step_mode = CLUTTER_STEP_MODE_END;

  /* default cubic-bezier() paramereters are (0, 0, 1, 1) */
  clutter_point_init (&self->priv->cb_1, 0, 0);
  clutter_point_init (&self->priv->cb_2, 1, 1);
}

struct CheckIfMarkerHitClosure
{
  ClutterTimeline *timeline;
  ClutterTimelineDirection direction;
  gint new_time;
  gint duration;
  gint delta;
};

static gboolean
have_passed_time (const struct CheckIfMarkerHitClosure *data,
                  gint msecs)
{
  /* Ignore markers that are outside the duration of the timeline */
  if (msecs < 0 || msecs > data->duration)
    return FALSE;

  if (data->direction == CLUTTER_TIMELINE_FORWARD)
    {
      /* We need to special case when a marker is added at the
         beginning of the timeline */
      if (msecs == 0 &&
          data->delta > 0 &&
          data->new_time - data->delta <= 0)
        return TRUE;

      /* Otherwise it's just a simple test if the time is in range of
         the previous time and the new time */
      return (msecs > data->new_time - data->delta &&
              msecs <= data->new_time);
    }
  else
    {
      /* We need to special case when a marker is added at the
         end of the timeline */
      if (msecs == data->duration &&
          data->delta > 0 &&
          data->new_time + data->delta >= data->duration)
        return TRUE;

      /* Otherwise it's just a simple test if the time is in range of
         the previous time and the new time */
      return (msecs >= data->new_time &&
              msecs < data->new_time + data->delta);
    }
}

static void
check_if_marker_hit (const gchar *name,
                     TimelineMarker *marker,
                     struct CheckIfMarkerHitClosure *data)
{
  gint msecs;

  if (marker->is_relative)
    msecs = (gdouble) data->duration * marker->data.progress;
  else
    msecs = marker->data.msecs;

  if (have_passed_time (data, msecs))
    {
      CLUTTER_NOTE (SCHEDULER, "Marker '%s' reached", name);

      g_signal_emit (data->timeline, timeline_signals[MARKER_REACHED],
                     marker->quark,
                     name,
                     msecs);
    }
}

static void
check_markers (ClutterTimeline *timeline,
               gint delta)
{
  ClutterTimelinePrivate *priv = timeline->priv;
  struct CheckIfMarkerHitClosure data;

  /* shortcircuit here if we don't have any marker installed */
  if (priv->markers_by_name == NULL)
    return;

  /* store the details of the timeline so that changing them in a
     marker signal handler won't affect which markers are hit */
  data.timeline = timeline;
  data.direction = priv->direction;
  data.new_time = priv->elapsed_time;
  data.duration = priv->duration;
  data.delta = delta;

  g_hash_table_foreach (priv->markers_by_name,
                        (GHFunc) check_if_marker_hit,
                        &data);
}

static void
emit_frame_signal (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv = timeline->priv;

  /* see bug https://bugzilla.gnome.org/show_bug.cgi?id=654066 */
  gint elapsed = (gint) priv->elapsed_time;

  CLUTTER_NOTE (SCHEDULER, "Emitting ::new-frame signal on timeline[%p]", timeline);

  g_signal_emit (timeline, timeline_signals[NEW_FRAME], 0, elapsed);
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

  is_playing = !!is_playing;

  if (is_playing == priv->is_playing)
    return;

  priv->is_playing = is_playing;

  master_clock = _clutter_master_clock_get_default ();
  if (priv->is_playing)
    {
      _clutter_master_clock_add_timeline (master_clock, timeline);
      priv->waiting_first_tick = TRUE;
      priv->current_repeat = 0;
    }
  else
    _clutter_master_clock_remove_timeline (master_clock, timeline);
}

static gboolean
clutter_timeline_do_frame (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  priv = timeline->priv;

  g_object_ref (timeline);

  CLUTTER_NOTE (SCHEDULER, "Timeline [%p] activated (elapsed time: %ld)\n",
                timeline,
                (long) priv->elapsed_time);

  /* Advance time */
  if (priv->direction == CLUTTER_TIMELINE_FORWARD)
    priv->elapsed_time += priv->msecs_delta;
  else
    priv->elapsed_time -= priv->msecs_delta;

  /* If we have not reached the end of the timeline: */
  if (!is_complete (timeline))
    {
      /* Emit the signal */
      emit_frame_signal (timeline);
      check_markers (timeline, priv->msecs_delta);

      g_object_unref (timeline);

      return priv->is_playing;
    }
  else
    {
      /* Handle loop or stop */
      ClutterTimelineDirection saved_direction = priv->direction;
      gint elapsed_time_delta = priv->msecs_delta;
      guint overflow_msecs = priv->elapsed_time;
      gint end_msecs;

      /* Update the current elapsed time in case the signal handlers
       * want to take a peek. If we clamp elapsed time, then we need
       * to correpondingly reduce elapsed_time_delta to reflect the correct
       * range of times */
      if (priv->direction == CLUTTER_TIMELINE_FORWARD)
	{
	  elapsed_time_delta -= (priv->elapsed_time - priv->duration);
	  priv->elapsed_time = priv->duration;
	}
      else if (priv->direction == CLUTTER_TIMELINE_BACKWARD)
	{
	  elapsed_time_delta -= - priv->elapsed_time;
	  priv->elapsed_time = 0;
	}

      end_msecs = priv->elapsed_time;

      /* Emit the signal */
      emit_frame_signal (timeline);
      check_markers (timeline, elapsed_time_delta);

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
                    "Timeline [%p] completed (cur: %ld, tot: %ld)",
                    timeline,
                    (long) priv->elapsed_time,
                    (long) priv->msecs_delta);

      if (priv->is_playing &&
          (priv->repeat_count == 0 ||
           priv->repeat_count == priv->current_repeat))
        {
          /* We stop the timeline now, so that the completed signal handler
           * may choose to re-start the timeline
           *
           * XXX Perhaps we should do this earlier, and regardless of
           * priv->repeat_count. Are we limiting the things that could be
           * done in the above new-frame signal handler?
           */
          set_is_playing (timeline, FALSE);

          g_signal_emit (timeline, timeline_signals[COMPLETED], 0);
          g_signal_emit (timeline, timeline_signals[STOPPED], 0, TRUE);
        }
      else
        g_signal_emit (timeline, timeline_signals[COMPLETED], 0);

      priv->current_repeat += 1;

      if (priv->auto_reverse)
        {
          /* :auto-reverse changes the direction of the timeline */
          if (priv->direction == CLUTTER_TIMELINE_FORWARD)
            priv->direction = CLUTTER_TIMELINE_BACKWARD;
          else
            priv->direction = CLUTTER_TIMELINE_FORWARD;

          g_object_notify_by_pspec (G_OBJECT (timeline),
                                    obj_props[PROP_DIRECTION]);
        }

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

      if (priv->repeat_count != 0)
        {
          /* We try and interpolate smoothly around a loop */
          if (saved_direction == CLUTTER_TIMELINE_FORWARD)
            priv->elapsed_time = overflow_msecs - priv->duration;
          else
            priv->elapsed_time = priv->duration + overflow_msecs;

          /* Or if the direction changed, we try and bounce */
          if (priv->direction != saved_direction)
            priv->elapsed_time = priv->duration - priv->elapsed_time;

          /* If we have overflowed then we are changing the elapsed
             time without emitting the new frame signal so we need to
             check for markers again */
          check_markers (timeline,
                         priv->direction == CLUTTER_TIMELINE_FORWARD
                           ? priv->elapsed_time
                           : priv->duration - priv->elapsed_time);

          g_object_unref (timeline);
          return TRUE;
        }
      else
        {
          clutter_timeline_rewind (timeline);

          g_object_unref (timeline);
          return FALSE;
        }
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
  gboolean was_playing;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  /* we check the is_playing here because pause() will return immediately
   * if the timeline wasn't playing, so we don't know if it was actually
   * stopped, and yet we still don't want to emit a ::stopped signal if
   * the timeline was not playing in the first place.
   */
  was_playing = timeline->priv->is_playing;

  clutter_timeline_pause (timeline);
  clutter_timeline_rewind (timeline);

  if (was_playing)
    g_signal_emit (timeline, timeline_signals[STOPPED], 0, FALSE);
}

/**
 * clutter_timeline_set_loop:
 * @timeline: a #ClutterTimeline
 * @loop: %TRUE for enable looping
 *
 * Sets whether @timeline should loop.
 *
 * This function is equivalent to calling clutter_timeline_set_repeat_count()
 * with -1 if @loop is %TRUE, and with 0 if @loop is %FALSE.
 *
 * Deprecated: 1.10: Use clutter_timeline_set_repeat_count() instead.
 */
void
clutter_timeline_set_loop (ClutterTimeline *timeline,
			   gboolean         loop)
{
  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  clutter_timeline_set_loop_internal (timeline, loop);
}

/**
 * clutter_timeline_get_loop:
 * @timeline: a #ClutterTimeline
 *
 * Gets whether @timeline is looping
 *
 * Return value: %TRUE if the timeline is looping
 *
 * Deprecated: 1.10: Use clutter_timeline_get_repeat_count() instead.
 */
gboolean
clutter_timeline_get_loop (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), FALSE);

  return timeline->priv->repeat_count != 0;
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
 * the original @timeline: you will have to start it with clutter_timeline_start().
 *
 * <note><para>The only cloned properties are:</para>
 * <itemizedlist>
 *   <listitem><simpara>#ClutterTimeline:duration</simpara></listitem>
 *   <listitem><simpara>#ClutterTimeline:loop</simpara></listitem>
 *   <listitem><simpara>#ClutterTimeline:delay</simpara></listitem>
 *   <listitem><simpara>#ClutterTimeline:direction</simpara></listitem>
 * </itemizedlist></note>
 *
 * Return value: (transfer full): a new #ClutterTimeline, cloned
 *   from @timeline
 *
 * Since: 0.4
 *
 * Deprecated: 1.10: Use clutter_timeline_new() or g_object_new()
 *   instead
 */
ClutterTimeline *
clutter_timeline_clone (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);

  return g_object_new (CLUTTER_TYPE_TIMELINE,
                       "duration", timeline->priv->duration,
                       "loop", timeline->priv->repeat_count != 0,
                       "delay", timeline->priv->delay,
                       "direction", timeline->priv->direction,
                       NULL);
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
      g_object_notify_by_pspec (G_OBJECT (timeline), obj_props[PROP_DELAY]);
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

      g_object_notify_by_pspec (G_OBJECT (timeline), obj_props[PROP_DURATION]);
    }
}

/**
 * clutter_timeline_get_progress:
 * @timeline: a #ClutterTimeline
 *
 * The position of the timeline in a normalized [-1, 2] interval.
 *
 * The return value of this function is determined by the progress
 * mode set using clutter_timeline_set_progress_mode(), or by the
 * progress function set using clutter_timeline_set_progress_func().
 *
 * Return value: the normalized current position in the timeline.
 *
 * Since: 0.6
 */
gdouble
clutter_timeline_get_progress (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0.0);

  priv = timeline->priv;

  /* short-circuit linear progress */
  if (priv->progress_func == NULL)
    return (gdouble) priv->elapsed_time / (gdouble) priv->duration;
  else
    return priv->progress_func (timeline,
                                (gdouble) priv->elapsed_time,
                                (gdouble) priv->duration,
                                priv->progress_data);
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

      g_object_notify_by_pspec (G_OBJECT (timeline), obj_props[PROP_DIRECTION]);
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
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  if (!clutter_timeline_is_playing (timeline))
    return 0;

  return timeline->priv->msecs_delta;
}

void
_clutter_timeline_advance (ClutterTimeline *timeline,
                           gint64           tick_time)
{
  ClutterTimelinePrivate *priv = timeline->priv;

  g_object_ref (timeline);

  priv->msecs_delta = tick_time;
  priv->is_playing = TRUE;

  clutter_timeline_do_frame (timeline);

  priv->is_playing = FALSE;

  g_object_unref (timeline);
}

/*< private >
 * clutter_timeline_do_tick
 * @timeline: a #ClutterTimeline
 * @tick_time: time of advance
 *
 * Advances @timeline based on the time passed in @tick_time. This
 * function is called by the master clock. The @timeline will use this
 * interval to emit the #ClutterTimeline::new-frame signal and
 * eventually skip frames.
 */
void
_clutter_timeline_do_tick (ClutterTimeline *timeline,
                           gint64           tick_time)
{
  ClutterTimelinePrivate *priv;

  priv = timeline->priv;

  /* Check the is_playing variable before performing the timeline tick.
   * This is necessary, as if a timeline is stopped in response to a
   * master-clock generated signal of a different timeline, this code can
   * still be reached.
   */
  if (!priv->is_playing)
    return;

  if (priv->waiting_first_tick)
    {
      priv->last_frame_time = tick_time;
      priv->msecs_delta = 0;
      priv->waiting_first_tick = FALSE;
      clutter_timeline_do_frame (timeline);
    }
  else
    {
      gint64 msecs;

      msecs = tick_time - priv->last_frame_time;

      /* if the clock rolled back between ticks we need to
       * account for it; the best course of action, since the
       * clock roll back can happen by any arbitrary amount
       * of milliseconds, is to drop a frame here
       */
      if (msecs < 0)
        {
          priv->last_frame_time = tick_time;
          return;
        }

      if (msecs != 0)
	{
	  /* Avoid accumulating error */
          priv->last_frame_time += msecs;
	  priv->msecs_delta = msecs;
	  clutter_timeline_do_frame (timeline);
	}
    }
}

/**
 * clutter_timeline_add_marker:
 * @timeline: a #ClutterTimeline
 * @marker_name: the unique name for this marker
 * @progress: the normalized value of the position of the martke
 *
 * Adds a named marker that will be hit when the timeline has reached
 * the specified @progress.
 *
 * Markers are unique string identifiers for a given position on the
 * timeline. Once @timeline reaches the given @progress of its duration,
 * if will emit a ::marker-reached signal for each marker attached to
 * that particular point.
 *
 * A marker can be removed with clutter_timeline_remove_marker(). The
 * timeline can be advanced to a marker using
 * clutter_timeline_advance_to_marker().
 *
 * See also: clutter_timeline_add_marker_at_time()
 *
 * Since: 1.14
 */
void
clutter_timeline_add_marker (ClutterTimeline *timeline,
                             const gchar     *marker_name,
                             gdouble          progress)
{
  TimelineMarker *marker;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);

  marker = timeline_marker_new_progress (marker_name, progress);
  clutter_timeline_add_marker_internal (timeline, marker);
}

/**
 * clutter_timeline_add_marker_at_time:
 * @timeline: a #ClutterTimeline
 * @marker_name: the unique name for this marker
 * @msecs: position of the marker in milliseconds
 *
 * Adds a named marker that will be hit when the timeline has been
 * running for @msecs milliseconds.
 *
 * Markers are unique string identifiers for a given position on the
 * timeline. Once @timeline reaches the given @msecs, it will emit
 * a ::marker-reached signal for each marker attached to that position.
 *
 * A marker can be removed with clutter_timeline_remove_marker(). The
 * timeline can be advanced to a marker using
 * clutter_timeline_advance_to_marker().
 *
 * See also: clutter_timeline_add_marker()
 *
 * Since: 0.8
 */
void
clutter_timeline_add_marker_at_time (ClutterTimeline *timeline,
                                     const gchar     *marker_name,
                                     guint            msecs)
{
  TimelineMarker *marker;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);
  g_return_if_fail (msecs <= clutter_timeline_get_duration (timeline));

  marker = timeline_marker_new_time (marker_name, msecs);
  clutter_timeline_add_marker_internal (timeline, marker);
}

struct CollectMarkersClosure
{
  guint duration;
  guint msecs;
  GArray *markers;
};

static void
collect_markers (const gchar *key,
                 TimelineMarker *marker,
                 struct CollectMarkersClosure *data)
{
  guint msecs;

  if (marker->is_relative)
    msecs = marker->data.progress * data->duration;
  else
    msecs = marker->data.msecs;

  if (msecs == data->msecs)
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
 * Retrieves the list of markers at time @msecs. If @msecs is a
 * negative integer, all the markers attached to @timeline will be
 * returned.
 *
 * Return value: (transfer full) (array zero-terminated=1 length=n_markers):
 *   a newly allocated, %NULL terminated string array containing the names
 *   of the markers. Use g_strfreev() when done.
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

      data.duration = priv->duration;
      data.msecs = msecs;
      data.markers = g_array_new (TRUE, FALSE, sizeof (gchar *));

      g_hash_table_foreach (priv->markers_by_name,
                            (GHFunc) collect_markers,
                            &data);

      i = data.markers->len;
      retval = (gchar **) (void *) g_array_free (data.markers, FALSE);
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
  guint msecs;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (marker_name != NULL);

  priv = timeline->priv;

  if (G_UNLIKELY (priv->markers_by_name == NULL))
    {
      g_warning ("No marker named '%s' found.", marker_name);
      return;
    }

  marker = g_hash_table_lookup (priv->markers_by_name, marker_name);
  if (marker == NULL)
    {
      g_warning ("No marker named '%s' found.", marker_name);
      return;
    }

  if (marker->is_relative)
    msecs = marker->data.progress * priv->duration;
  else
    msecs = marker->data.msecs;

  clutter_timeline_advance (timeline, msecs);
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

/**
 * clutter_timeline_set_auto_reverse:
 * @timeline: a #ClutterTimeline
 * @reverse: %TRUE if the @timeline should reverse the direction
 *
 * Sets whether @timeline should reverse the direction after the
 * emission of the #ClutterTimeline::completed signal.
 *
 * Setting the #ClutterTimeline:auto-reverse property to %TRUE is the
 * equivalent of connecting a callback to the #ClutterTimeline::completed
 * signal and changing the direction of the timeline from that callback;
 * for instance, this code:
 *
 * |[
 * static void
 * reverse_timeline (ClutterTimeline *timeline)
 * {
 *   ClutterTimelineDirection dir = clutter_timeline_get_direction (timeline);
 *
 *   if (dir == CLUTTER_TIMELINE_FORWARD)
 *     dir = CLUTTER_TIMELINE_BACKWARD;
 *   else
 *     dir = CLUTTER_TIMELINE_FORWARD;
 *
 *   clutter_timeline_set_direction (timeline, dir);
 * }
 * ...
 *   timeline = clutter_timeline_new (1000);
 *   clutter_timeline_set_repeat_count (timeline, -1);
 *   g_signal_connect (timeline, "completed",
 *                     G_CALLBACK (reverse_timeline),
 *                     NULL);
 * ]|
 *
 * can be effectively replaced by:
 *
 * |[
 *   timeline = clutter_timeline_new (1000);
 *   clutter_timeline_set_repeat_count (timeline, -1);
 *   clutter_timeline_set_auto_reverse (timeline);
 * ]|
 *
 * Since: 1.6
 */
void
clutter_timeline_set_auto_reverse (ClutterTimeline *timeline,
                                   gboolean         reverse)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  reverse = !!reverse;

  priv = timeline->priv;

  if (priv->auto_reverse != reverse)
    {
      priv->auto_reverse = reverse;

      g_object_notify_by_pspec (G_OBJECT (timeline),
                                obj_props[PROP_AUTO_REVERSE]);
    }
}

/**
 * clutter_timeline_get_auto_reverse:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the value set by clutter_timeline_set_auto_reverse().
 *
 * Return value: %TRUE if the timeline should automatically reverse, and
 *   %FALSE otherwise
 *
 * Since: 1.6
 */
gboolean
clutter_timeline_get_auto_reverse (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), FALSE);

  return timeline->priv->auto_reverse;
}

/**
 * clutter_timeline_set_repeat_count:
 * @timeline: a #ClutterTimeline
 * @count: the number of times the timeline should repeat
 *
 * Sets the number of times the @timeline should repeat.
 *
 * If @count is 0, the timeline never repeats.
 *
 * If @count is -1, the timeline will always repeat until
 * it's stopped.
 *
 * Since: 1.10
 */
void
clutter_timeline_set_repeat_count (ClutterTimeline *timeline,
                                   gint             count)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (count >= -1);

  priv = timeline->priv;

  if (priv->repeat_count != count)
    {
      priv->repeat_count = count;

      g_object_notify_by_pspec (G_OBJECT (timeline),
                                obj_props[PROP_REPEAT_COUNT]);
    }
}

/**
 * clutter_timeline_get_repeat_count:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the number set using clutter_timeline_set_repeat_count().
 *
 * Return value: the number of repeats
 *
 * Since: 1.10
 */
gint
clutter_timeline_get_repeat_count (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  return timeline->priv->repeat_count;
}

/**
 * clutter_timeline_set_progress_func:
 * @timeline: a #ClutterTimeline
 * @func: (scope notified) (allow-none): a progress function, or %NULL
 * @data: (closure): data to pass to @func
 * @notify: a function to be called when the progress function is removed
 *    or the timeline is disposed
 *
 * Sets a custom progress function for @timeline. The progress function will
 * be called by clutter_timeline_get_progress() and will be used to compute
 * the progress value based on the elapsed time and the total duration of the
 * timeline.
 *
 * If @func is not %NULL, the #ClutterTimeline:progress-mode property will
 * be set to %CLUTTER_CUSTOM_MODE.
 *
 * If @func is %NULL, any previously set progress function will be unset, and
 * the #ClutterTimeline:progress-mode property will be set to %CLUTTER_LINEAR.
 *
 * Since: 1.10
 */
void
clutter_timeline_set_progress_func (ClutterTimeline             *timeline,
                                    ClutterTimelineProgressFunc  func,
                                    gpointer                     data,
                                    GDestroyNotify               notify)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->progress_notify != NULL)
    priv->progress_notify (priv->progress_data);

  priv->progress_func = func;
  priv->progress_data = data;
  priv->progress_notify = notify;

  if (priv->progress_func != NULL)
    priv->progress_mode = CLUTTER_CUSTOM_MODE;
  else
    priv->progress_mode = CLUTTER_LINEAR;

  g_object_notify_by_pspec (G_OBJECT (timeline), obj_props[PROP_PROGRESS_MODE]);
}

static gdouble
clutter_timeline_progress_func (ClutterTimeline *timeline,
                                gdouble          elapsed,
                                gdouble          duration,
                                gpointer         user_data G_GNUC_UNUSED)
{
  ClutterTimelinePrivate *priv = timeline->priv;

  /* parametrized easing functions need to be handled separately */
  switch (priv->progress_mode)
    {
    case CLUTTER_STEPS:
      if (priv->step_mode == CLUTTER_STEP_MODE_START)
        return clutter_ease_steps_start (elapsed, duration, priv->n_steps);
      else if (priv->step_mode == CLUTTER_STEP_MODE_END)
        return clutter_ease_steps_end (elapsed, duration, priv->n_steps);
      else
        g_assert_not_reached ();
      break;

    case CLUTTER_STEP_START:
      return clutter_ease_steps_start (elapsed, duration, 1);

    case CLUTTER_STEP_END:
      return clutter_ease_steps_end (elapsed, duration, 1);

    case CLUTTER_CUBIC_BEZIER:
      return clutter_ease_cubic_bezier (elapsed, duration,
                                        priv->cb_1.x, priv->cb_1.y,
                                        priv->cb_2.x, priv->cb_2.y);

    case CLUTTER_EASE:
      return clutter_ease_cubic_bezier (elapsed, duration,
                                        0.25, 0.1, 0.25, 1.0);

    case CLUTTER_EASE_IN:
      return clutter_ease_cubic_bezier (elapsed, duration,
                                        0.42, 0.0, 1.0, 1.0);

    case CLUTTER_EASE_OUT:
      return clutter_ease_cubic_bezier (elapsed, duration,
                                        0.0, 0.0, 0.58, 1.0);

    case CLUTTER_EASE_IN_OUT:
      return clutter_ease_cubic_bezier (elapsed, duration,
                                        0.42, 0.0, 0.58, 1.0);

    default:
      break;
    }

  return clutter_easing_for_mode (priv->progress_mode, elapsed, duration);
}

/**
 * clutter_timeline_set_progress_mode:
 * @timeline: a #ClutterTimeline
 * @mode: the progress mode, as a #ClutterAnimationMode
 *
 * Sets the progress function using a value from the #ClutterAnimationMode
 * enumeration. The @mode cannot be %CLUTTER_CUSTOM_MODE or bigger than
 * %CLUTTER_ANIMATION_LAST.
 *
 * Since: 1.10
 */
void
clutter_timeline_set_progress_mode (ClutterTimeline      *timeline,
                                    ClutterAnimationMode  mode)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (mode < CLUTTER_ANIMATION_LAST);
  g_return_if_fail (mode != CLUTTER_CUSTOM_MODE);

  priv = timeline->priv;

  if (priv->progress_mode == mode)
    return;

  if (priv->progress_notify != NULL)
    priv->progress_notify (priv->progress_data);

  priv->progress_mode = mode;

  /* short-circuit linear progress */
  if (priv->progress_mode != CLUTTER_LINEAR)
    priv->progress_func = clutter_timeline_progress_func;
  else
    priv->progress_func = NULL;

  priv->progress_data = NULL;
  priv->progress_notify = NULL;

  g_object_notify_by_pspec (G_OBJECT (timeline), obj_props[PROP_PROGRESS_MODE]);
}

/**
 * clutter_timeline_get_progress_mode:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the progress mode set using clutter_timeline_set_progress_mode()
 * or clutter_timeline_set_progress_func().
 *
 * Return value: a #ClutterAnimationMode
 *
 * Since: 1.10
 */
ClutterAnimationMode
clutter_timeline_get_progress_mode (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), CLUTTER_LINEAR);

  return timeline->priv->progress_mode;
}

/**
 * clutter_timeline_get_duration_hint:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the full duration of the @timeline, taking into account the
 * current value of the #ClutterTimeline:repeat-count property.
 *
 * If the #ClutterTimeline:repeat-count property is set to -1, this function
 * will return %G_MAXINT64.
 *
 * The returned value is to be considered a hint, and it's only valid
 * as long as the @timeline hasn't been changed.
 *
 * Return value: the full duration of the #ClutterTimeline
 *
 * Since: 1.10
 */
gint64
clutter_timeline_get_duration_hint (ClutterTimeline *timeline)
{
  ClutterTimelinePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  priv = timeline->priv;

  if (priv->repeat_count == 0)
    return priv->duration;
  else if (priv->repeat_count < 0)
    return G_MAXINT64;
  else
    return priv->repeat_count * priv->duration;
}

/**
 * clutter_timeline_get_current_repeat:
 * @timeline: a #ClutterTimeline
 *
 * Retrieves the current repeat for a timeline.
 *
 * Repeats start at 0.
 *
 * Return value: the current repeat
 *
 * Since: 1.10
 */
gint
clutter_timeline_get_current_repeat (ClutterTimeline *timeline)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  return timeline->priv->current_repeat;
}

/**
 * clutter_timeline_set_step_progress:
 * @timeline: a #ClutterTimeline
 * @n_steps: the number of steps
 * @step_mode: whether the change should happen at the start
 *   or at the end of the step
 *
 * Sets the #ClutterTimeline:progress-mode of the @timeline to %CLUTTER_STEPS
 * and provides the parameters of the step function.
 *
 * Since: 1.12
 */
void
clutter_timeline_set_step_progress (ClutterTimeline *timeline,
                                    gint             n_steps,
                                    ClutterStepMode  step_mode)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (n_steps > 0);

  priv = timeline->priv;

  if (priv->progress_mode == CLUTTER_STEPS &&
      priv->n_steps == n_steps &&
      priv->step_mode == step_mode)
    return;

  priv->n_steps = n_steps;
  priv->step_mode = step_mode;
  clutter_timeline_set_progress_mode (timeline, CLUTTER_STEPS);
}

/**
 * clutter_timeline_get_step_progress:
 * @timeline: a #ClutterTimeline
 * @n_steps: (out): return location for the number of steps, or %NULL
 * @step_mode: (out): return location for the value change policy,
 *   or %NULL
 *
 * Retrieves the parameters of the step progress mode used by @timeline.
 *
 * Return value: %TRUE if the @timeline is using a step progress
 *   mode, and %FALSE otherwise
 *
 * Since: 1.12
 */
gboolean
clutter_timeline_get_step_progress (ClutterTimeline *timeline,
                                    gint            *n_steps,
                                    ClutterStepMode *step_mode)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), FALSE);

  if (timeline->priv->progress_mode != CLUTTER_STEPS ||
      timeline->priv->progress_mode != CLUTTER_STEP_START ||
      timeline->priv->progress_mode != CLUTTER_STEP_END)
    return FALSE;

  if (n_steps != NULL)
    *n_steps = timeline->priv->n_steps;

  if (step_mode != NULL)
    *step_mode = timeline->priv->step_mode;

  return TRUE;
}

/**
 * clutter_timeline_set_cubic_bezier_progress:
 * @timeline: a #ClutterTimeline
 * @c_1: the first control point for the cubic bezier
 * @c_2: the second control point for the cubic bezier
 *
 * Sets the #ClutterTimeline:progress-mode of @timeline
 * to %CLUTTER_CUBIC_BEZIER, and sets the two control
 * points for the cubic bezier.
 *
 * The cubic bezier curve is between (0, 0) and (1, 1). The X coordinate
 * of the two control points must be in the [ 0, 1 ] range, while the
 * Y coordinate of the two control points can exceed this range.
 *
 * Since: 1.12
 */
void
clutter_timeline_set_cubic_bezier_progress (ClutterTimeline    *timeline,
                                            const ClutterPoint *c_1,
                                            const ClutterPoint *c_2)
{
  ClutterTimelinePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TIMELINE (timeline));
  g_return_if_fail (c_1 != NULL && c_2 != NULL);

  priv = timeline->priv;

  priv->cb_1 = *c_1;
  priv->cb_2 = *c_2;

  /* ensure the range on the X coordinate */
  priv->cb_1.x = CLAMP (priv->cb_1.x, 0.f, 1.f);
  priv->cb_2.x = CLAMP (priv->cb_2.x, 0.f, 1.f);

  clutter_timeline_set_progress_mode (timeline, CLUTTER_CUBIC_BEZIER);
}

/**
 * clutter_timeline_get_cubic_bezier_progress:
 * @timeline: a #ClutterTimeline
 * @c_1: (out caller-allocates): return location for the first control
 *   point of the cubic bezier, or %NULL
 * @c_2: (out caller-allocates): return location for the second control
 *   point of the cubic bezier, or %NULL
 *
 * Retrieves the control points for the cubic bezier progress mode.
 *
 * Return value: %TRUE if the @timeline is using a cubic bezier progress
 *   more, and %FALSE otherwise
 *
 * Since: 1.12
 */
gboolean
clutter_timeline_get_cubic_bezier_progress (ClutterTimeline *timeline,
                                            ClutterPoint    *c_1,
                                            ClutterPoint    *c_2)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), FALSE);

  if (timeline->priv->progress_mode != CLUTTER_CUBIC_BEZIER ||
      timeline->priv->progress_mode != CLUTTER_EASE ||
      timeline->priv->progress_mode != CLUTTER_EASE_IN ||
      timeline->priv->progress_mode != CLUTTER_EASE_OUT ||
      timeline->priv->progress_mode != CLUTTER_EASE_IN_OUT)
    return FALSE;

  if (c_1 != NULL)
    *c_1 = timeline->priv->cb_1;

  if (c_2 != NULL)
    *c_2 = timeline->priv->cb_2;

  return TRUE;
}
