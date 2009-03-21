/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Copyright (C) 2009  Intel Corporation.
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

/*
 * SECTION:clutter-master-clock
 * @short_description: The master clock for all animations
 *
 * The #ClutterMasterClock class is responsible for advancing all
 * #ClutterTimelines when a stage is being redrawn. The master clock
 * makes sure that the scenegraph is always integrally updated before
 * painting it.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-master-clock.h"
#include "clutter-debug.h"
#include "clutter-private.h"

#define CLUTTER_MASTER_CLOCK_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_MASTER_CLOCK, ClutterMasterClockClass))
#define CLUTTER_IS_MASTER_CLOCK_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_MASTER_CLOCK))
#define CLUTTER_MASTER_CLASS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_MASTER_CLOCK, ClutterMasterClockClass))

typedef struct _ClutterMasterClockClass         ClutterMasterClockClass;

struct _ClutterMasterClock
{
  GObject parent_instance;

  GSList *timelines;

  GTimeVal prev_tick;
  gulong msecs_delta;

  gulong tick_id;
};

struct _ClutterMasterClockClass
{
  GObjectClass parent_class;
};

static ClutterMasterClock *default_clock = NULL;

static void on_timeline_start     (ClutterTimeline    *timeline,
                                   ClutterMasterClock *master_clock);
static void on_timeline_completed (ClutterTimeline    *timeline,
                                   ClutterMasterClock *master_clock);
static void on_timeline_paused    (ClutterTimeline    *timeline,
                                   ClutterMasterClock *master_clock);

G_DEFINE_TYPE (ClutterMasterClock, clutter_master_clock, G_TYPE_OBJECT);

static void
timeline_weak_ref (gpointer  data,
                   GObject  *object_pointer)
{
  ClutterMasterClock *master_clock = data;

  master_clock->timelines = g_slist_remove (master_clock->timelines,
                                            object_pointer);

  if (master_clock->timelines == NULL)
    {
      if (master_clock->tick_id != 0)
        {
          g_source_remove (master_clock->tick_id);
          master_clock->tick_id = 0;
        }

      master_clock->prev_tick.tv_sec = 0;
    }
}

static void
clutter_master_clock_finalize (GObject *gobject)
{
  ClutterMasterClock *master_clock = CLUTTER_MASTER_CLOCK (gobject);
  GSList *l;

  if (master_clock->tick_id != 0)
    {
      g_source_remove (master_clock->tick_id);
      master_clock->tick_id = 0;
    }

  for (l = master_clock->timelines; l != NULL; l = l->next)
    {
      ClutterTimeline *timeline = l->data;

      g_object_weak_unref (G_OBJECT (timeline),
                           timeline_weak_ref,
                           master_clock);

      g_signal_handlers_disconnect_by_func (timeline,
                                            G_CALLBACK (on_timeline_start),
                                            master_clock);
      g_signal_handlers_disconnect_by_func (timeline,
                                            G_CALLBACK (on_timeline_completed),
                                            master_clock);
      g_signal_handlers_disconnect_by_func (timeline,
                                            G_CALLBACK (on_timeline_paused),
                                            master_clock);
    }

  g_slist_free (master_clock->timelines);

  G_OBJECT_CLASS (clutter_master_clock_parent_class)->finalize (gobject);
}

static void
clutter_master_clock_class_init (ClutterMasterClockClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = clutter_master_clock_finalize;
}

static void
clutter_master_clock_init (ClutterMasterClock *self)
{
}

/*
 * _clutter_master_clock_get_default:
 *
 * Retrieves the default master clock. If this function has never
 * been called before, the default master clock is created.
 *
 * Return value: the default master clock. The returned object is
 *   owned by Clutter and should not be modified or freed
 */
ClutterMasterClock *
_clutter_master_clock_get_default (void)
{
  if (G_LIKELY (default_clock != NULL))
    return default_clock;

  default_clock = g_object_new (CLUTTER_TYPE_MASTER_CLOCK, NULL);

  return default_clock;
}

static gboolean
master_clock_tick (gpointer data)
{
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();
  GSList *stages, *l;

  CLUTTER_NOTE (SCHEDULER, "Master clock [tick]");

  stages = clutter_stage_manager_list_stages (stage_manager);

  for (l = stages; l != NULL; l = l->next)
    clutter_actor_queue_redraw (l->data);

  g_slist_free (stages);

  return TRUE;
}

static gboolean
has_running_timeline (ClutterMasterClock *master_clock)
{
  GSList *l;

  for (l = master_clock->timelines; l != NULL; l = l->next)
    {
      if (clutter_timeline_is_playing (l->data))
        return TRUE;
    }

  return FALSE;
}

static void
on_timeline_start (ClutterTimeline *timeline,
                   ClutterMasterClock *master_clock)
{
  if (has_running_timeline (master_clock) &&
      master_clock->tick_id == 0)
    {
      master_clock->prev_tick.tv_sec = 0;

      master_clock->tick_id =
        clutter_threads_add_frame_source (clutter_get_default_frame_rate (),
                                          master_clock_tick,
                                          master_clock);
    }
}

static void
on_timeline_completed (ClutterTimeline *timeline,
                       ClutterMasterClock *master_clock)
{
  if (!has_running_timeline (master_clock) &&
      master_clock->tick_id != 0)
    {
      master_clock->prev_tick.tv_sec = 0;

      g_source_remove (master_clock->tick_id);
      master_clock->tick_id = 0;
    }
}

static void
on_timeline_paused (ClutterTimeline *timeline,
                    ClutterMasterClock *master_clock)
{
  if (!has_running_timeline (master_clock) &&
      master_clock->tick_id != 0)
    {
      master_clock->prev_tick.tv_sec = 0;

      g_source_remove (master_clock->tick_id);
      master_clock->tick_id = 0;
    }
}

/*
 * _clutter_master_clock_add_timeline:
 * @master_clock: a #ClutterMasterClock
 * @timeline: a #ClutterTimeline
 *
 * Adds @timeline to the list of timelines held by the master
 * clock. This function should be called during the instance
 * creation phase of the timeline.
 */
void
_clutter_master_clock_add_timeline (ClutterMasterClock *master_clock,
                                    ClutterTimeline    *timeline)
{
  gboolean is_first = FALSE;

  if (g_slist_find (master_clock->timelines, timeline))
    return;

  is_first = (master_clock->timelines == NULL) ? TRUE : FALSE;

  master_clock->timelines = g_slist_prepend (master_clock->timelines,
                                             timeline);

  g_object_weak_ref (G_OBJECT (timeline),
                     timeline_weak_ref,
                     master_clock);

  g_signal_connect (timeline, "started",
                    G_CALLBACK (on_timeline_start),
                    master_clock);
  g_signal_connect (timeline, "completed",
                    G_CALLBACK (on_timeline_completed),
                    master_clock);
  g_signal_connect (timeline, "paused",
                    G_CALLBACK (on_timeline_paused),
                    master_clock);
}

/*
 * _clutter_master_clock_remove_timeline:
 * @master_clock: a #ClutterMasterClock
 * @timeline: a #ClutterTimeline
 *
 * Removes @timeline from the list of timelines held by the
 * master clock. This function should be called during the
 * #ClutterTimeline finalization.
 */
void
_clutter_master_clock_remove_timeline (ClutterMasterClock *master_clock,
                                       ClutterTimeline    *timeline)
{
  if (!g_slist_find (master_clock->timelines, timeline))
    return;

  master_clock->timelines = g_slist_remove (master_clock->timelines,
                                            timeline);

  g_object_weak_unref (G_OBJECT (timeline),
                       timeline_weak_ref,
                       master_clock);

  g_signal_handlers_disconnect_by_func (timeline,
                                        G_CALLBACK (on_timeline_start),
                                        master_clock);
  g_signal_handlers_disconnect_by_func (timeline,
                                        G_CALLBACK (on_timeline_completed),
                                        master_clock);
  g_signal_handlers_disconnect_by_func (timeline,
                                        G_CALLBACK (on_timeline_paused),
                                        master_clock);

  if (master_clock->timelines == NULL)
    {
      if (master_clock->tick_id != 0)
        {
          g_source_remove (master_clock->tick_id);
          master_clock->tick_id = 0;
        }

      master_clock->prev_tick.tv_sec = 0;
    }
}

/*
 * _clutter_master_clock_advance:
 * @master_clock: a #ClutterMasterClock
 *
 * Advances all the timelines held by the master clock. This function
 * should be called before calling clutter_redraw() to make sure that
 * all the timelines are advanced and the scene is updated.
 */
void
_clutter_master_clock_advance (ClutterMasterClock *master_clock)
{
  GTimeVal cur_tick = { 0, };
  gulong msecs;
  GSList *l;

  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  g_get_current_time (&cur_tick);

  if (master_clock->prev_tick.tv_sec == 0)
    master_clock->prev_tick = cur_tick;

  msecs = (cur_tick.tv_sec - master_clock->prev_tick.tv_sec) * 1000
        + (cur_tick.tv_usec - master_clock->prev_tick.tv_usec) / 1000;

  CLUTTER_NOTE (SCHEDULER, "Advancing %d timelines by %lu milliseconds",
                g_slist_length (master_clock->timelines),
                msecs);

  for (l = master_clock->timelines; l != NULL; l = l->next)
    {
      ClutterTimeline *timeline = l->data;

      if (clutter_timeline_is_playing (timeline))
        _clutter_timeline_set_delta (timeline, msecs);
    }

  master_clock->msecs_delta = msecs;
  master_clock->prev_tick = cur_tick;
}
