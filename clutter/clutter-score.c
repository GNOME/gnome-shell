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

/*
 * IDEAS:
 *  API;
 *  - add()
 *      + an new timeline to beginning of score
 *  - append (timeline_existing, timeline_new, delay)
 *      + appends a new timeline to an existing one
 *
 *  ScoreEntry
 *   {
 *     Timeline *base;
 *     GList    *next_timelines; - to start on completion of base,
 *                                 (points to score entries)
 *     Callback  id;
 *     delay
 *   }
 *
 *  start()/stop(),remove(),remove_all() ?
 */

/**
 * SECTION:clutter-score
 * @short_description: Sequencing multiple #ClutterTimelines in order
 *
 * #ClutterScore is a base class for sequencing multiple timelines in order.
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

G_DEFINE_TYPE (ClutterScore, clutter_score, G_TYPE_OBJECT);

typedef struct ClutterScoreEntry
{
  ClutterTimeline *timeline;
  gulong           handler_id;
  GSList          *child_entries;
  ClutterScore    *score;
}
ClutterScoreEntry;

struct _ClutterScorePrivate
{
  GSList            *entries;
  GHashTable        *running_timelines;
  guint              paused :1;
  guint              loop : 1;
};

enum
{
  PROP_0,
  PROP_LOOP
};

enum
{
  NEW_TIMELINE,
  STARTED,
  PAUSED,
  COMPLETED,

  LAST_SIGNAL
};

static int score_signals[LAST_SIGNAL] = { 0 }; 

static void start_entry (ClutterScoreEntry *entry);

/* Object */

static void
clutter_score_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterScore        *score;
  ClutterScorePrivate *priv;

  score = CLUTTER_SCORE(object);
  priv = score->priv;

  switch (prop_id)
    {
    case PROP_LOOP:
      priv->loop = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_score_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  ClutterScore        *score;
  ClutterScorePrivate *priv;

  score = CLUTTER_SCORE(object);
  priv = score->priv;

  switch (prop_id)
    {
    case PROP_LOOP:
      g_value_set_boolean (value, priv->loop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_score_finalize (GObject *object)
{
  ClutterScorePrivate *priv = CLUTTER_SCORE (object)->priv;

  g_hash_table_destroy (priv->running_timelines);
  G_OBJECT_CLASS (clutter_score_parent_class)->finalize (object);
}

static void
clutter_score_dispose (GObject *object)
{
  ClutterScore *self = CLUTTER_SCORE(object);
  ClutterScorePrivate *priv;

  priv = self->priv;

  G_OBJECT_CLASS (clutter_score_parent_class)->dispose (object);
}


static void
clutter_score_class_init (ClutterScoreClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) klass;

  object_class->set_property = clutter_score_set_property;
  object_class->get_property = clutter_score_get_property;
  object_class->finalize     = clutter_score_finalize;
  object_class->dispose      = clutter_score_dispose;

  g_type_class_add_private (klass, sizeof (ClutterScorePrivate));


  /**
   * ClutterScore::new-timeline:
   * @score: the score which received the signal
   * @timeline: the current timeline
   *
   * The ::new-timeline signal is emitted each time a new timeline in the
   * score is reached.
   */
  score_signals[NEW_TIMELINE] =
    g_signal_new ("new-timeline",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, new_timeline),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE,
		  1, CLUTTER_TYPE_TIMELINE);
  score_signals[COMPLETED] =
    g_signal_new ("completed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, completed),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  score_signals[STARTED] =
    g_signal_new ("started",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, started),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  score_signals[PAUSED] =
    g_signal_new ("paused",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, paused),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

}

static void
clutter_score_init (ClutterScore *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
					   CLUTTER_TYPE_SCORE,
					   ClutterScorePrivate);

  self->priv->running_timelines = g_hash_table_new(NULL, NULL);
}

/**
 * clutter_score_set_loop:
 * @score: a #ClutterScore
 * @loop: %TRUE for enable looping
 *
 * Sets whether @score should loop.
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
 * Return Value: TRUE if score is currently playing, FALSE if not.
 */
gboolean
clutter_score_is_playing (ClutterScore *score)
{
  g_return_val_if_fail (CLUTTER_IS_SCORE (score), FALSE);

  /* FIXME: paused state currently counts as playing */

  return !!g_hash_table_size (score->priv->running_timelines);
}

static void
on_timeline_finish (ClutterTimeline   *timeline,
		    ClutterScoreEntry *entry)
{
  GSList  *item;

  g_hash_table_remove (entry->score->priv->running_timelines,
		       GINT_TO_POINTER(entry->handler_id));

  g_signal_handler_disconnect (timeline, entry->handler_id);

  CLUTTER_NOTE (SCHEDULER,
		"completed %p %li\n", 
		entry->timeline, entry->handler_id);

  for (item = entry->child_entries; item != NULL; item = item->next)
    {
      ClutterScoreEntry *child_entry = item->data;
      start_entry (child_entry);
    }

  if (clutter_score_is_playing (entry->score) == FALSE)
    {
      /* Score has finished - fire 'completed' signal */
      /* Also check if looped etc */
      CLUTTER_NOTE (SCHEDULER, "looks like we finished\n");
      
      g_signal_emit (entry->score, score_signals[COMPLETED], 0);
    }
}

static void
start_entry (ClutterScoreEntry *entry)
{
  entry->handler_id = g_signal_connect (entry->timeline,
					"completed",
					G_CALLBACK (on_timeline_finish),
					entry);

    CLUTTER_NOTE (SCHEDULER, 
		  "started %p %li\n", entry->timeline, entry->handler_id);

  g_hash_table_insert (entry->score->priv->running_timelines,
		       GINT_TO_POINTER(entry->handler_id),
		       entry);

  clutter_timeline_start (entry->timeline);

  g_signal_emit (entry->score, score_signals[NEW_TIMELINE], 
		 0, entry->timeline);
}

void
on_foreach_running_timeline_start (gpointer key,
				   gpointer value,
				   gpointer user_data)
{
  clutter_timeline_start (CLUTTER_TIMELINE(value));
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
  GSList              *item;
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;

  if (priv->paused)
    {
      g_hash_table_foreach (priv->running_timelines,
			    (GHFunc)on_foreach_running_timeline_start,
			    NULL);
      priv->paused = 0;
    }
  else
    {
      for (item = priv->entries; item != NULL; item = item->next)
	{
	  ClutterScoreEntry *entry = item->data;
	  start_entry (entry);
	}
    }
}

gboolean            
on_foreach_running_timeline_stop (gpointer key,
				  gpointer value,
				  gpointer user_data)
{
  clutter_timeline_stop (((ClutterScoreEntry*)value)->timeline);
  return TRUE; 
}

/**
 * clutter_score_stop:
 * @score: A #ClutterScore
 *
 * Stops and rewinds a playing #ClutterScore instance.
 *
 */
void
clutter_score_stop (ClutterScore *score)
{
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;

  g_hash_table_foreach_remove (priv->running_timelines,
			       (GHRFunc)on_foreach_running_timeline_stop,
			       NULL);
}

/**
 * clutter_score_rewind:
 * @score: A #ClutterScore
 *
 * Rewinds a #ClutterScore to inital timeline.
 **/
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

void
on_foreach_running_timeline_pause (gpointer key,
				   gpointer value,
				  gpointer user_data)
{
  clutter_timeline_pause (((ClutterScoreEntry*)value)->timeline);
}

void
clutter_score_pause (ClutterScore *score)
{
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;

  if (priv->paused || !clutter_score_is_playing (score)) 
    return;

  g_hash_table_foreach (priv->running_timelines,
			(GHFunc)on_foreach_running_timeline_pause,
			NULL);

  priv->paused = 1;

  g_signal_emit (score, score_signals[PAUSED], 0);
}

static ClutterScoreEntry*
find_entry (GSList *list, ClutterTimeline *timeline)
{
  GSList             *item;
  ClutterScoreEntry  *res = NULL;

  if (list == NULL)
    return NULL;

  for (item = list; item != NULL && res == NULL; item = item->next)
    {
      ClutterScoreEntry *entry = item->data;

      g_assert (entry != NULL);

      if (entry->timeline == timeline)
	return entry;

      if (entry->child_entries)
	res = find_entry (entry->child_entries, timeline);
    }

  return res;
}

/**
 * clutter_score_append:
 * @score: A #ClutterScore
 * @timeline_existing: A #ClutterTimeline in the score
 * @timeline_new: A new #ClutterTimeline to start when #timeline_existing has
 * completed,
 *
 * Appends a new timeline to an one existing in the score.
 *
 */
void
clutter_score_append (ClutterScore    *score,
		      ClutterTimeline *timeline_existing,
		      ClutterTimeline *timeline_new)
{
  ClutterScorePrivate *priv;
  ClutterScoreEntry   *entry, *entry_new;

  priv = score->priv;

  /* Appends a timeline to the end of another */
  if ((entry = find_entry (priv->entries, timeline_existing)) != NULL)
    {
      entry_new =  g_new0(ClutterScoreEntry, 1);
      entry_new->timeline = g_object_ref (timeline_new);
      entry_new->score    = score;

      entry->child_entries = g_slist_append (entry->child_entries, entry_new);

      clutter_timeline_stop (timeline_new); /* stop it */
    }
}

/**
 * clutter_score_add:
 * @score: A #ClutterScore
 * @timeline: A #ClutterTimeline
 *
 * Adds a new initial timeline to start when the score is started.
 */
void
clutter_score_add (ClutterScore    *score,
		   ClutterTimeline *timeline)
{
  ClutterScorePrivate  *priv;
  ClutterScoreEntry    *entry;

  priv = score->priv;

  /* Added timelines are always started first */
  entry = g_new0(ClutterScoreEntry, 1);
  entry->timeline = g_object_ref (timeline);
  entry->score = score;
  score->priv->entries = g_slist_append (score->priv->entries, entry);

  clutter_timeline_stop (timeline); /* stop it */

  CLUTTER_NOTE (SCHEDULER, "added timeline %p\n", entry->timeline);
}

static void
remove_entries (GSList *list)
{
  GSList *item;

  if (!list)
    return;

  for (item = list; item != NULL; item = item->next)
    {
      ClutterScoreEntry *entry = item->data;

      g_object_unref (entry->timeline);

      if (entry->child_entries)
	remove_entries (entry->child_entries);

      g_slist_free (entry->child_entries);
      g_free(entry);
    }
}

void
clutter_score_remove (ClutterScore    *score,
		      ClutterTimeline *timeline_parent,
		      ClutterTimeline *timeline)
{

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
  clutter_score_stop (score);
  remove_entries (score->priv->entries);
}

/**
 * clutter_score_new:
 *
 * Creates a new #ClutterScore.
 *
 * Return value: the newly created #ClutterScore
 *
 * Since: 0.6
 */
ClutterScore *
clutter_score_new (void)
{
  return g_object_new (CLUTTER_TYPE_SCORE,  NULL);
}
