/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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
 * SECTION:clutter-score
 * @short_description: Controller for multiple timelines
 *
 * #ClutterScore is a base class for sequencing multiple timelines in order.
 * Using #ClutterScore it is possible to start multiple timelines at the
 * same time or launch multiple timelines when a particular timeline has
 * emitted the ClutterTimeline::completed signal.
 *
 * Each time a #ClutterTimeline is started and completed, a signal will be
 * emitted.
 *
 * For example, this code will start two #ClutterTimeline<!-- -->s after
 * a third timeline terminates:
 *
 * |[
 *   ClutterTimeline *timeline_1, *timeline_2, *timeline_3;
 *   ClutterScore *score;
 *
 *   timeline_1 = clutter_timeline_new_for_duration (1000);
 *   timeline_2 = clutter_timeline_new_for_duration (500);
 *   timeline_3 = clutter_timeline_new_for_duration (500);
 *
 *   score = clutter_score_new ();
 *
 *   clutter_score_append (score, NULL,       timeline_1);
 *   clutter_score_append (score, timeline_1, timeline_2);
 *   clutter_score_append (score, timeline_1, timeline_3);
 *
 *   clutter_score_start (score);
 * ]|
 *
 * A #ClutterScore takes a reference on the timelines it manages,
 * so timelines can be safely unreferenced after being appended.
 *
 * New timelines can be appended to the #ClutterScore using
 * clutter_score_append() and removed using clutter_score_remove().
 *
 * Timelines can also be appended to a specific marker on the
 * parent timeline, using clutter_score_append_at_marker().
 *
 * The score can be cleared using clutter_score_remove_all().
 *
 * The list of timelines can be retrieved using
 * clutter_score_list_timelines().
 *
 * The score state is controlled using clutter_score_start(),
 * clutter_score_pause(), clutter_score_stop() and clutter_score_rewind().
 * The state can be queried using clutter_score_is_playing().
 *
 * #ClutterScore is available since Clutter 0.6
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-score.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-debug.h"

typedef struct _ClutterScoreEntry       ClutterScoreEntry;

struct _ClutterScoreEntry
{
  /* the entry unique id */
  gulong id;

  ClutterTimeline *timeline;
  ClutterTimeline *parent;

  /* the optional marker on the parent */
  gchar *marker;

  /* signal handlers id */
  gulong complete_id;
  gulong marker_id;

  ClutterScore *score;

  /* pointer back to the tree structure */
  GNode *node;
};

#define CLUTTER_SCORE_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_SCORE, ClutterScorePrivate))

struct _ClutterScorePrivate
{
  GNode      *root;

  GHashTable *running_timelines;

  gulong      last_id;

  guint       is_paused : 1;
  guint       loop      : 1;
};

enum
{
  PROP_0,

  PROP_LOOP
};

enum
{
  TIMELINE_STARTED,
  TIMELINE_COMPLETED,

  STARTED,
  PAUSED,
  COMPLETED,

  LAST_SIGNAL
};

static inline void clutter_score_clear (ClutterScore *score);

G_DEFINE_TYPE (ClutterScore, clutter_score, G_TYPE_OBJECT);

static int score_signals[LAST_SIGNAL] = { 0 }; 

/* Object */

static void
clutter_score_set_property (GObject      *gobject,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterScorePrivate *priv = CLUTTER_SCORE_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_LOOP:
      priv->loop = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
  }
}

static void
clutter_score_get_property (GObject    *gobject,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterScorePrivate *priv = CLUTTER_SCORE_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_LOOP:
      g_value_set_boolean (value, priv->loop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_score_finalize (GObject *object)
{
  ClutterScore *score = CLUTTER_SCORE (object);

  clutter_score_stop (score);
  clutter_score_clear (score);

  G_OBJECT_CLASS (clutter_score_parent_class)->finalize (object);
}

static void
clutter_score_class_init (ClutterScoreClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_score_set_property;
  gobject_class->get_property = clutter_score_get_property;
  gobject_class->finalize     = clutter_score_finalize;

  g_type_class_add_private (klass, sizeof (ClutterScorePrivate));

  /**
   * ClutterScore:loop:
   *
   * Whether the #ClutterScore should restart once finished.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LOOP,
                                   g_param_spec_boolean ("loop",
                                                         "Loop",
                                                         "Whether the score should restart once finished",
                                                         FALSE,
                                                         CLUTTER_PARAM_READWRITE));

  /**
   * ClutterScore::timeline-started:
   * @score: the score which received the signal
   * @timeline: the current timeline
   *
   * The ::timeline-started signal is emitted each time a new timeline
   * inside a #ClutterScore starts playing.
   *
   * Since: 0.6
   */
  score_signals[TIMELINE_STARTED] =
    g_signal_new ("timeline-started",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, timeline_started),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE,
		  1, CLUTTER_TYPE_TIMELINE);
  /**
   * ClutterScore::timeline-completed:
   * @score: the score which received the signal
   * @timeline: the completed timeline
   *
   * The ::timeline-completed signal is emitted each time a timeline
   * inside a #ClutterScore terminates.
   *
   * Since: 0.6
   */
  score_signals[TIMELINE_COMPLETED] =
   g_signal_new ("timeline-completed",
                 G_TYPE_FROM_CLASS (gobject_class),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET (ClutterScoreClass, timeline_completed),
                 NULL, NULL,
                 clutter_marshal_VOID__OBJECT,
                 G_TYPE_NONE, 1,
                 CLUTTER_TYPE_TIMELINE);
  /**
   * ClutterScore::completed:
   * @score: the score which received the signal
   *
   * The ::completed signal is emitted each time a #ClutterScore terminates.
   *
   * Since: 0.6
   */
  score_signals[COMPLETED] =
    g_signal_new ("completed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, completed),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterScore::started:
   * @score: the score which received the signal
   *
   * The ::started signal is emitted each time a #ClutterScore starts playing.
   *
   * Since: 0.6
   */
  score_signals[STARTED] =
    g_signal_new ("started",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, started),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterScore::paused:
   * @score: the score which received the signal
   *
   * The ::paused signal is emitted each time a #ClutterScore
   * is paused.
   *
   * Since: 0.6
   */
  score_signals[PAUSED] =
    g_signal_new ("paused",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, paused),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
clutter_score_init (ClutterScore *self)
{
  ClutterScorePrivate *priv;

  self->priv = priv = CLUTTER_SCORE_GET_PRIVATE (self);

  /* sentinel */
  priv->root = g_node_new (NULL);

  priv->running_timelines = NULL;

  priv->is_paused = FALSE;
  priv->loop = FALSE;

  priv->last_id = 1;
}

/**
 * clutter_score_new:
 *
 * Creates a new #ClutterScore. A #ClutterScore is an object that can
 * hold multiple #ClutterTimeline<!-- -->s in a sequential order.
 *
 * Return value: the newly created #ClutterScore. Use g_object_unref()
 *   when done.
 *
 * Since: 0.6
 */
ClutterScore *
clutter_score_new (void)
{
  return g_object_new (CLUTTER_TYPE_SCORE,  NULL);
}

/**
 * clutter_score_set_loop:
 * @score: a #ClutterScore
 * @loop: %TRUE for enable looping
 *
 * Sets whether @score should loop. A looping #ClutterScore will start
 * from its initial state after the ::complete signal has been fired.
 *
 * Since: 0.6
 */
void
clutter_score_set_loop (ClutterScore *score,
			   gboolean         loop)
{
  g_return_if_fail (CLUTTER_IS_SCORE (score));

  if (score->priv->loop != loop)
    {
      score->priv->loop = loop;

      g_object_notify (G_OBJECT (score), "loop");
    }
}

/**
 * clutter_score_get_loop:
 * @score: a #ClutterScore
 *
 * Gets whether @score is looping
 *
 * Return value: %TRUE if the score is looping
 *
 * Since: 0.6
 */
gboolean
clutter_score_get_loop (ClutterScore *score)
{
  g_return_val_if_fail (CLUTTER_IS_SCORE (score), FALSE);

  return score->priv->loop;
}

/**
 * clutter_score_is_playing:
 * @score: A #ClutterScore
 *
 * Query state of a #ClutterScore instance.
 *
 * Return Value: %TRUE if score is currently playing
 *
 * Since: 0.6
 */
gboolean
clutter_score_is_playing (ClutterScore *score)
{
  g_return_val_if_fail (CLUTTER_IS_SCORE (score), FALSE);

  if (score->priv->is_paused)
    return FALSE;

  return score->priv->running_timelines
    && g_hash_table_size (score->priv->running_timelines) != 0;
}

/* destroy_entry:
 * @node: a #GNode
 *
 * Frees the #ClutterScoreEntry attached to @node.
 */
static gboolean
destroy_entry (GNode                  *node,
               G_GNUC_UNUSED gpointer  data)
{
  ClutterScoreEntry *entry = node->data;

  if (G_LIKELY (entry != NULL))
    {
      if (entry->marker_id)
        {
          g_signal_handler_disconnect (entry->parent, entry->marker_id);
          entry->marker_id = 0;
        }

      if (entry->complete_id)
        {
          g_signal_handler_disconnect (entry->timeline, entry->complete_id);
          entry->complete_id = 0;
        }

      g_object_unref (entry->timeline);
      g_free (entry->marker);
      g_slice_free (ClutterScoreEntry, entry);

      node->data = NULL;
    }

  /* continue */
  return FALSE;
}

typedef enum {
  FIND_BY_TIMELINE,
  FIND_BY_ID,
  REMOVE_BY_ID,
  LIST_TIMELINES
} TraverseAction;

typedef struct {
  TraverseAction action;

  ClutterScore *score;

  /* parameters */
  union {
    ClutterTimeline *timeline;
    gulong id;
    ClutterScoreEntry *entry;
  } d;

  gpointer result;
} TraverseClosure;

/* multi-purpose traversal function for the N-ary tree used by the score */
static gboolean
traverse_children (GNode    *node,
                   gpointer  data)
{
  TraverseClosure *closure = data;
  ClutterScoreEntry *entry = node->data;
  gboolean retval = FALSE;

  /* root */
  if (!entry)
    return TRUE;

  switch (closure->action)
    {
    case FIND_BY_TIMELINE:
      if (closure->d.timeline == entry->timeline)
        {
          closure->result = node;
          retval = TRUE;
        }
      break;

    case FIND_BY_ID:
      if (closure->d.id == entry->id)
        {
          closure->result = node;
          retval = TRUE;
        }
      break;

    case REMOVE_BY_ID:
      if (closure->d.id == entry->id)
        {
          if (entry->complete_id)
            {
              g_signal_handler_disconnect (entry->timeline, entry->complete_id);
              entry->complete_id = 0;
            }

          if (entry->marker_id)
            {
              g_signal_handler_disconnect (entry->timeline, entry->marker_id);
              entry->marker_id = 0;
            }

          g_object_unref (entry->timeline);

          g_node_traverse (node,
                           G_POST_ORDER,
                           G_TRAVERSE_ALL,
                           -1,
                           destroy_entry, NULL);

          g_free (entry->marker);
          g_slice_free (ClutterScoreEntry, entry);

          closure->result = node;
          retval = TRUE;
        }
      break;

    case LIST_TIMELINES:
      closure->result = g_slist_prepend (closure->result, entry->timeline);
      retval = FALSE;
      break;
    }

  return retval;
}

static GNode *
find_entry_by_timeline (ClutterScore    *score,
                        ClutterTimeline *timeline)
{
  ClutterScorePrivate *priv = score->priv;
  TraverseClosure closure;

  closure.action = FIND_BY_TIMELINE;
  closure.score = score;
  closure.d.timeline = timeline;
  closure.result = NULL;

  g_node_traverse (priv->root,
                   G_POST_ORDER,
                   G_TRAVERSE_ALL,
                   -1,
                   traverse_children, &closure);

  if (closure.result)
    return closure.result;

  return NULL;
}

static GNode *
find_entry_by_id (ClutterScore *score,
                  gulong        id)
{
  ClutterScorePrivate *priv = score->priv;
  TraverseClosure closure;

  closure.action = FIND_BY_ID;
  closure.score = score;
  closure.d.id = id;
  closure.result = NULL;

  g_node_traverse (priv->root,
                   G_POST_ORDER,
                   G_TRAVERSE_ALL,
                   -1,
                   traverse_children, &closure);

  if (closure.result)
    return closure.result;

  return NULL;
}

/* forward declaration */
static void start_entry (ClutterScoreEntry *entry);

static void
start_children_entries (GNode    *node,
                        gpointer  data)
{
  ClutterScoreEntry *entry = node->data;

  /* If data is NULL, start all entries that have no marker, otherwise
     only start entries that have the same marker */
  if (data == NULL ? entry->marker == NULL : !strcmp (data, entry->marker))
    start_entry (entry);
}

static void
on_timeline_marker (ClutterTimeline   *timeline,
                    const gchar       *marker_name,
                    gint               frame_num,
                    ClutterScoreEntry *entry)
{
  GNode *parent;
  CLUTTER_NOTE (SCHEDULER, "timeline [%p] marker ('%s') reached",
		entry->timeline,
                entry->marker);

  parent = find_entry_by_timeline (entry->score, timeline);
  if (!parent)
    return;

  /* start every child */
  if (parent->children)
    {
      g_node_children_foreach (parent,
                               G_TRAVERSE_ALL,
                               start_children_entries,
                               (gpointer) marker_name);
    }
}

static void
on_timeline_completed (ClutterTimeline   *timeline,
                       ClutterScoreEntry *entry)
{
  ClutterScorePrivate *priv = entry->score->priv;

  g_hash_table_remove (priv->running_timelines,
                       GUINT_TO_POINTER (entry->id));

  g_signal_handler_disconnect (timeline, entry->complete_id);
  entry->complete_id = 0;

  CLUTTER_NOTE (SCHEDULER, "timeline [%p] ('%lu') completed", 
		entry->timeline,
                entry->id);

  g_signal_emit (entry->score, score_signals[TIMELINE_COMPLETED], 0,
                 entry->timeline);

  /* start every child */
  if (entry->node->children)
    {
      g_node_children_foreach (entry->node,
                               G_TRAVERSE_ALL,
                               start_children_entries,
                               NULL);
    }

  /* score has finished - fire 'completed' signal */
  if (g_hash_table_size (priv->running_timelines) == 0)
    {
      CLUTTER_NOTE (SCHEDULER, "looks like we finished");
      
      g_signal_emit (entry->score, score_signals[COMPLETED], 0);

      clutter_score_stop (entry->score);
      
      if (priv->loop)
        clutter_score_start (entry->score);
    }
}

static void
start_entry (ClutterScoreEntry *entry)
{
  ClutterScorePrivate *priv = entry->score->priv;

  /* timelines attached to a marker might already be playing when we
   * end up here from the ::completed handler, so we need to perform
   * this check to avoid restarting those timelines
   */
  if (clutter_timeline_is_playing (entry->timeline))
    return;

  entry->complete_id = g_signal_connect (entry->timeline,
                                         "completed",
                                         G_CALLBACK (on_timeline_completed),
                                         entry);

  CLUTTER_NOTE (SCHEDULER, "timeline [%p] ('%lu') started",
                entry->timeline,
                entry->id);

  if (G_UNLIKELY (priv->running_timelines == NULL))
    priv->running_timelines = g_hash_table_new (NULL, NULL);

  g_hash_table_insert (priv->running_timelines,
                       GUINT_TO_POINTER (entry->id),
                       entry);

  clutter_timeline_start (entry->timeline);

  g_signal_emit (entry->score, score_signals[TIMELINE_STARTED], 0,
                 entry->timeline);
}

enum
{
  ACTION_START,
  ACTION_PAUSE,
  ACTION_STOP
};

static void
foreach_running_timeline (gpointer key,
                          gpointer value,
                          gpointer user_data)
{
  ClutterScoreEntry *entry = value;
  gint action = GPOINTER_TO_INT (user_data);

  switch (action)
    {
    case ACTION_START:
      clutter_timeline_start (entry->timeline);
      break;

    case ACTION_PAUSE:
      clutter_timeline_pause (entry->timeline);
      break;

    case ACTION_STOP:
      if (entry->complete_id)
	{
	  g_signal_handler_disconnect (entry->timeline, entry->complete_id);
	  entry->complete_id = 0;
	}
      clutter_timeline_stop (entry->timeline);
      break;
    }
}

/**
 * clutter_score_start:
 * @score: A #ClutterScore
 *
 * Starts the score.
 *
 * Since: 0.6
 */
void
clutter_score_start (ClutterScore *score)
{
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;

  if (priv->is_paused)
    {
      g_hash_table_foreach (priv->running_timelines,
			    foreach_running_timeline,
			    GINT_TO_POINTER (ACTION_START));
      priv->is_paused = FALSE;
    }
  else
    {
      g_signal_emit (score, score_signals[STARTED], 0);
      g_node_children_foreach (priv->root,
                               G_TRAVERSE_ALL,
                               start_children_entries,
                               NULL);
    }
}

/**
 * clutter_score_stop:
 * @score: A #ClutterScore
 *
 * Stops and rewinds a playing #ClutterScore instance.
 *
 * Since: 0.6
 */
void
clutter_score_stop (ClutterScore *score)
{
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;

  if (priv->running_timelines)
    {
      g_hash_table_foreach (priv->running_timelines,
                            foreach_running_timeline,
                            GINT_TO_POINTER (ACTION_STOP));
      g_hash_table_destroy (priv->running_timelines);
      priv->running_timelines = NULL;
    }
}

/**
 * clutter_score_pause:
 * @score: a #ClutterScore
 *
 * Pauses a playing score @score.
 *
 * Since: 0.6
 */
void
clutter_score_pause (ClutterScore *score)
{
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;

  if (!clutter_score_is_playing (score)) 
    return;

  g_hash_table_foreach (priv->running_timelines,
			foreach_running_timeline,
			GINT_TO_POINTER (ACTION_PAUSE));

  priv->is_paused = TRUE;

  g_signal_emit (score, score_signals[PAUSED], 0);
}

/**
 * clutter_score_rewind:
 * @score: A #ClutterScore
 *
 * Rewinds a #ClutterScore to its initial state.
 *
 * Since: 0.6
 */
void
clutter_score_rewind (ClutterScore *score)
{
  gboolean was_playing;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  was_playing = clutter_score_is_playing (score);

  clutter_score_stop (score);

  if (was_playing)
    clutter_score_start (score);
}

static inline void
clutter_score_clear (ClutterScore *score)
{
  ClutterScorePrivate *priv = score->priv;

  g_node_traverse (priv->root,
                   G_POST_ORDER,
                   G_TRAVERSE_ALL,
                   -1,
                   destroy_entry, NULL);
  g_node_destroy (priv->root);
}

/**
 * clutter_score_append:
 * @score: a #ClutterScore
 * @parent: a #ClutterTimeline in the score, or %NULL
 * @timeline: a #ClutterTimeline
 *
 * Appends a timeline to another one existing in the score; the newly
 * appended timeline will be started when @parent is complete.
 *
 * If @parent is %NULL, the new #ClutterTimeline will be started when
 * clutter_score_start() is called.
 *
 * #ClutterScore will take a reference on @timeline.
 *
 * Return value: the id of the #ClutterTimeline inside the score, or
 *   0 on failure. The returned id can be used with clutter_score_remove()
 *   or clutter_score_get_timeline().
 *
 * Since: 0.6
 */
gulong
clutter_score_append (ClutterScore    *score,
		      ClutterTimeline *parent,
		      ClutterTimeline *timeline)
{
  ClutterScorePrivate *priv;
  ClutterScoreEntry *entry;

  g_return_val_if_fail (CLUTTER_IS_SCORE (score), 0);
  g_return_val_if_fail (parent == NULL || CLUTTER_IS_TIMELINE (parent), 0);
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  priv = score->priv;

  if (!parent)
    {
      entry = g_slice_new (ClutterScoreEntry);
      entry->timeline = g_object_ref (timeline);
      entry->parent = NULL;
      entry->id = priv->last_id;
      entry->marker = NULL;
      entry->marker_id = 0;
      entry->complete_id = 0;
      entry->score = score;

      entry->node = g_node_append_data (priv->root, entry);
    }
  else
    {
      GNode *node;

      node = find_entry_by_timeline (score, parent);
      if (G_UNLIKELY (!node))
        {
          g_warning ("Unable to find the parent timeline inside the score.");
          return 0;
        }

      entry = g_slice_new (ClutterScoreEntry);
      entry->timeline = g_object_ref (timeline);
      entry->parent = parent;
      entry->id = priv->last_id;
      entry->marker = NULL;
      entry->marker_id = 0;
      entry->complete_id = 0;
      entry->score = score;

      entry->node = g_node_append_data (node, entry);
    }

  priv->last_id += 1;

  return entry->id;
}

/**
 * clutter_score_append_at_marker:
 * @score: a #ClutterScore
 * @parent: the parent #ClutterTimeline
 * @marker_name: the name of the marker to use
 * @timeline: the #ClutterTimeline to append
 *
 * Appends @timeline at the given @marker_name on the @parent
 * #ClutterTimeline.
 *
 * If you want to append @timeline at the end of @parent, use
 * clutter_score_append().
 *
 * The #ClutterScore will take a reference on @timeline.
 *
 * Return value: the id of the #ClutterTimeline inside the score, or
 *   0 on failure. The returned id can be used with clutter_score_remove()
 *   or clutter_score_get_timeline().
 *
 * Since: 0.8
 */
gulong
clutter_score_append_at_marker (ClutterScore    *score,
                                ClutterTimeline *parent,
                                const gchar     *marker_name,
                                ClutterTimeline *timeline)
{
  ClutterScorePrivate *priv;
  GNode *node;
  ClutterScoreEntry *entry;
  gchar *marker_reached_signal;

  g_return_val_if_fail (CLUTTER_IS_SCORE (score), 0);
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (parent), 0);
  g_return_val_if_fail (marker_name != NULL, 0);
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), 0);

  if (!clutter_timeline_has_marker (parent, marker_name))
    {
      g_warning ("The parent timeline has no marker `%s'", marker_name);
      return 0;
    }

  priv = score->priv;

  node = find_entry_by_timeline (score, parent);
  if (G_UNLIKELY (!node))
    {
      g_warning ("Unable to find the parent timeline inside the score.");
      return 0;
    }

  entry = g_slice_new (ClutterScoreEntry);
  entry->timeline = g_object_ref (timeline);
  entry->parent = parent;
  entry->marker = g_strdup (marker_name);
  entry->id = priv->last_id;
  entry->score = score;

  marker_reached_signal = g_strdup_printf ("marker-reached::%s", marker_name);
  entry->marker_id = g_signal_connect (entry->parent,
                                       marker_reached_signal,
                                       G_CALLBACK (on_timeline_marker),
                                       entry);

  entry->node = g_node_append_data (node, entry);

  g_free (marker_reached_signal);

  priv->last_id += 1;

  return entry->id;
}

/**
 * clutter_score_remove:
 * @score: a #ClutterScore
 * @id: the id of the timeline to remove
 *
 * Removes the #ClutterTimeline with the given id inside @score. If
 * the timeline has other timelines attached to it, those are removed
 * as well.
 *
 * Since: 0.6
 */
void
clutter_score_remove (ClutterScore *score,
                      gulong        id)
{
  ClutterScorePrivate *priv;
  TraverseClosure closure;

  g_return_if_fail (CLUTTER_IS_SCORE (score));
  g_return_if_fail (id > 0);

  priv = score->priv;

  closure.action = REMOVE_BY_ID;
  closure.score = score;
  closure.d.id = id;
  closure.result = NULL;

  g_node_traverse (priv->root,
                   G_POST_ORDER,
                   G_TRAVERSE_ALL,
                   -1,
                   traverse_children, &closure);

  if (closure.result)
    g_node_destroy (closure.result);
}

/**
 * clutter_score_remove_all:
 * @score: a #ClutterScore
 *
 * Removes all the timelines inside @score.
 *
 * Since: 0.6
 */
void
clutter_score_remove_all (ClutterScore *score)
{
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;

  /* this will take care of the running timelines */
  clutter_score_stop (score);

  /* destroy all the contents of the tree */
  clutter_score_clear (score);

  /* recreate the sentinel */
  priv->root = g_node_new (NULL);
}

/**
 * clutter_score_get_timeline:
 * @score: a #ClutterScore
 * @id: the id of the timeline
 *
 * Retrieves the #ClutterTimeline for @id inside @score.
 *
 * Return value: the requested timeline, or %NULL. This function does
 *   not increase the reference count on the returned #ClutterTimeline
 *
 * Since: 0.6
 */
ClutterTimeline *
clutter_score_get_timeline (ClutterScore *score,
                            gulong        id)
{
  GNode *node;
  ClutterScoreEntry *entry;

  g_return_val_if_fail (CLUTTER_IS_SCORE (score), NULL);
  g_return_val_if_fail (id > 0, NULL);

  node = find_entry_by_id (score, id);
  if (G_UNLIKELY (!node))
    return NULL;

  entry = node->data;

  return entry->timeline;
}

/**
 * clutter_score_list_timelines:
 * @score: a #ClutterScore
 *
 * Retrieves a list of all the #ClutterTimelines managed by @score.
 *
 * Return value: a #GSList containing all the timelines in the score.
 *   This function does not increase the reference count of the
 *   returned timelines. Use g_slist_free() on the returned list to
 *   deallocate its resources.
 *
 * Since: 0.6
 */
GSList *
clutter_score_list_timelines (ClutterScore *score)
{
  ClutterScorePrivate *priv;
  TraverseClosure closure;
  GSList *retval;

  g_return_val_if_fail (CLUTTER_IS_SCORE (score), NULL);

  priv = score->priv;

  closure.action = LIST_TIMELINES;
  closure.result = NULL;

  g_node_traverse (priv->root,
                   G_POST_ORDER,
                   G_TRAVERSE_ALL,
                   -1,
                   traverse_children, &closure);

  retval = closure.result;

  return retval;
}
