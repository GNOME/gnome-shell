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
 *                                 (points to score entrys)
 *     Callback  id;
 *     delay
 *   }
 *
 *  start()/stop(),remove(),remove_all() ? 
 */

/**
 * SECTION:clutter-score @short_description: A class for sequencing
 * multiple #ClutterTimelines in order
 *
 * #ClutterScore is a base class for sequencing multiple timelines in order.
 */

#ifndef HAVE_CONFIG_H
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
  G_OBJECT_CLASS (clutter_score_parent_class)->finalize (object);
}

static void 
clutter_score_dispose (GObject *object)
{
  ClutterScore *self = CLUTTER_SCORE(object);
  ClutterScorePrivate *priv;

  priv = self->priv;

  if (priv != NULL)
    {

    }

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


#if 0
  /**
   * ClutterScore::new-frame:
   * @score: the score which received the signal
   * @timeline: the number of the new frame
   *
   * The ::new-timeline signal is emitted each time a new timeline in the
   * score is reached.
   */
  score_signals[NEW_TIMELINE] =
    g_signal_new ("new-timeline",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterScoreClass, new_frame),
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
#endif
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
      g_object_ref (score);

      score->priv->loop = loop;

      g_object_notify (G_OBJECT (score), "loop");
      g_object_unref (score);
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
 * clutter_score_rewind:
 * @score: A #ClutterScore
 *
 * Rewinds #ClutterScore to frame 0.
 **/
void
clutter_score_rewind (ClutterScore *score)
{
  g_return_if_fail (CLUTTER_IS_SCORE (score));
  
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
  
  return !!g_hash_table_size(score->priv->running_timelines);
}

static void
on_timeline_finish (ClutterTimeline   *timeline, 
		    ClutterScoreEntry *entry)
{
  GSList  *item;

  g_hash_table_remove (entry->score->priv->running_timelines, 
		       GINT_TO_POINTER(entry->handler_id));
  g_signal_handler_disconnect (timeline, entry->handler_id);

  printf("completed %li\n", entry->handler_id); 

  for (item = entry->child_entries; item != NULL; item = item->next)
    {
      ClutterScoreEntry *child_entry = item->data;
      start_entry (child_entry);
    }

  if (clutter_score_is_playing (entry->score) == FALSE)
    {
      /* Score has finished - fire 'completed' signal */
      /* Also check if looped etc */
      printf("looks like we finished\n");
    }
}

static void
start_entry (ClutterScoreEntry *entry)
{
  entry->handler_id = g_signal_connect (entry->timeline,
					"completed", 
					G_CALLBACK (on_timeline_finish),
					entry);

  printf("started %li\n", entry->handler_id); 

  g_hash_table_insert (entry->score->priv->running_timelines,
		       GINT_TO_POINTER(entry->handler_id), 
		       entry);

  clutter_timeline_start (entry->timeline);
}

/**
 * clutter_score_start:
 * @score: A #ClutterScore
 *
 * Query state of a #ClutterScore instance.
 *
 * Return Value: TRUE if score is currently playing, FALSE if not.
 */
void
clutter_score_start (ClutterScore *score)
{
  GSList              *item;
  ClutterScorePrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCORE (score));

  priv = score->priv;  

  for (item = priv->entries; item != NULL; item = item->next)
    {
      ClutterScoreEntry *entry = item->data;

      start_entry (entry);
    }
}

/**
 * clutter_score_start:
 * @score: A #ClutterScore
 *
 * Query state of a #ClutterScore instance.
 *
 * Return Value: TRUE if score is currently playing, FALSE if not.
 */
void
clutter_score_stop (ClutterScore *score)
{
  g_return_if_fail (CLUTTER_IS_SCORE (score));

  /* foreach hash / pause */
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
      entry->timeline = g_object_ref (timeline_new);
      entry->score    = score;
      entry->child_entries = g_slist_append (entry->child_entries, entry);
    }
}

/**
 * clutter_score_add:
 * @score: A #ClutterScore
 * @timeline: A #ClutterTimeline
 *
 * Adds a new initial timeline to start when the score is started.
 *
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
}

void
clutter_score_remove (ClutterScore    *score,
		      ClutterTimeline *timeline_parent,
		      ClutterTimeline *timeline)
{

}

void
clutter_score_remove_all (ClutterScore *score)
{

}


/**
 * clutter_score_new:
 *
 * Create a new #ClutterScore instance.
 *
 * Return Value: a new #ClutterScore
 */
ClutterScore*
clutter_score_new ()
{
  return g_object_new (CLUTTER_TYPE_SCORE,  NULL);
}
