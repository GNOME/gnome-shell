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

typedef struct _ClutterClockSource              ClutterClockSource;
typedef struct _ClutterMasterClockClass         ClutterMasterClockClass;

struct _ClutterMasterClock
{
  GObject parent_instance;

  /* the list of timelines handled by the clock */
  GSList *timelines;

  /* the current state of the clock
   */
  GTimeVal cur_tick;

  /* the previous state of the clock, used to compute
   * the delta
   */
  GTimeVal prev_tick;

  /* an idle source, used by the Master Clock to queue
   * a redraw on the stage and drive the animations
   */
  GSource *source;

  guint updated_stages : 1;
};

struct _ClutterMasterClockClass
{
  GObjectClass parent_class;
};

struct _ClutterClockSource
{
  GSource source;

  ClutterMasterClock *master_clock;
};

static gboolean clutter_clock_prepare  (GSource     *source,
                                        gint        *timeout);
static gboolean clutter_clock_check    (GSource     *source);
static gboolean clutter_clock_dispatch (GSource     *source,
                                        GSourceFunc  callback,
                                        gpointer     user_data);

static ClutterMasterClock *default_clock = NULL;

static GSourceFuncs clock_funcs = {
  clutter_clock_prepare,
  clutter_clock_check,
  clutter_clock_dispatch,
  NULL
};

G_DEFINE_TYPE (ClutterMasterClock, clutter_master_clock, G_TYPE_OBJECT);

/*
 * master_clock_is_running:
 * @master_clock: a #ClutterMasterClock
 *
 * Checks if we should currently be advancing timelines or redrawing
 * stages.
 *
 * Return value: %TRUE if the #ClutterMasterClock has at least
 *   one running timeline
 */
static gboolean
master_clock_is_running (ClutterMasterClock *master_clock)
{
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();
  const GSList *stages, *l;

  if (master_clock->timelines)
    return TRUE;

  stages = clutter_stage_manager_peek_stages (stage_manager);
  for (l = stages; l; l = l->next)
    if (_clutter_stage_has_queued_events (l->data) ||
        _clutter_stage_needs_update (l->data))
      return TRUE;

  return FALSE;
}

/*
 * master_clock_next_frame_delay:
 * @master_clock: a #ClutterMasterClock
 *
 * Computes the number of delay before we need to draw the next frame.
 *
 * Return value: -1 if there is no next frame pending, otherwise the
 *  number of millseconds before the we need to draw the next frame
 */
static gint
master_clock_next_frame_delay (ClutterMasterClock *master_clock)
{
  GTimeVal now;
  GTimeVal next;

  if (!master_clock_is_running (master_clock))
    return -1;

  if (clutter_feature_available (CLUTTER_FEATURE_SYNC_TO_VBLANK) &&
      master_clock->updated_stages)
    {
      /* When we have sync-to-vblank, we count on that to throttle
       * our frame rate, and otherwise draw frames as fast as possible.
       */
      return 0;
    }

  if (master_clock->prev_tick.tv_sec == 0)
    {
      /* If we weren't previously running, then draw the next frame
       * immediately
       */
      return 0;
    }

  /* Otherwise, wait at least 1/frame_rate seconds since we last started a frame */

  g_source_get_current_time (master_clock->source, &now);

  next = master_clock->prev_tick;
  g_time_val_add (&next, 1000000 / clutter_get_default_frame_rate ());

  if (next.tv_sec < now.tv_sec ||
      (next.tv_sec == now.tv_sec && next.tv_usec < now.tv_usec))
    {
      return 0;
    }
  else
    {
      return ((next.tv_sec - now.tv_sec) * 1000 +
              (next.tv_usec - now.tv_usec) / 1000);
    }
}

/*
 * clutter_clock_source_new:
 * @master_clock: a #ClutterMasterClock for the source
 *
 * The #ClutterClockSource is an idle GSource that will queue a redraw
 * if @master_clock has at least a running #ClutterTimeline. The redraw
 * will cause @master_clock to advance all timelines, thus advancing all
 * animations as well.
 *
 * Return value: the newly created #GSource
 */
static GSource *
clutter_clock_source_new (ClutterMasterClock *master_clock)
{
  GSource *source = g_source_new (&clock_funcs, sizeof (ClutterClockSource));
  ClutterClockSource *clock_source = (ClutterClockSource *) source;

  clock_source->master_clock = master_clock;

  return source;
}

static gboolean
clutter_clock_prepare (GSource *source,
                       gint    *timeout)
{
  ClutterClockSource *clock_source = (ClutterClockSource *) source;
  ClutterMasterClock *master_clock = clock_source->master_clock;
  int delay;

  clutter_threads_enter ();
  delay = master_clock_next_frame_delay (master_clock);
  clutter_threads_leave ();

  *timeout = delay;

  return delay == 0;
}

static gboolean
clutter_clock_check (GSource *source)
{
  ClutterClockSource *clock_source = (ClutterClockSource *) source;
  ClutterMasterClock *master_clock = clock_source->master_clock;
  int delay;

  clutter_threads_enter ();
  delay = master_clock_next_frame_delay (master_clock);
  clutter_threads_leave ();

  return delay == 0;
}

static gboolean
clutter_clock_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterClockSource *clock_source = (ClutterClockSource *) source;
  ClutterMasterClock *master_clock = clock_source->master_clock;
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();
  GSList *stages, *l;

  CLUTTER_NOTE (SCHEDULER, "Master clock [tick]");

  clutter_threads_enter ();

  /* Get the time to use for this frame.
   */
  g_source_get_current_time (source, &master_clock->cur_tick);

  /* We need to protect ourselves against stages being destroyed during
   * event handling
   */
  stages = clutter_stage_manager_list_stages (stage_manager);
  g_slist_foreach (stages, (GFunc)g_object_ref, NULL);

  master_clock->updated_stages = FALSE;

  /* Process queued events
   */
  for (l = stages; l != NULL; l = l->next)
    _clutter_stage_process_queued_events (l->data);

  _clutter_master_clock_advance (master_clock);
  _clutter_run_repaint_functions ();

  /* Update any stage that needs redraw/relayout after the clock
   * is advanced.
   */
  for (l = stages; l != NULL; l = l->next)
    master_clock->updated_stages |= _clutter_stage_do_update (l->data);

  g_slist_foreach (stages, (GFunc)g_object_unref, NULL);
  g_slist_free (stages);

  master_clock->prev_tick = master_clock->cur_tick;

  clutter_threads_leave ();

  return TRUE;
}

static void
clutter_master_clock_finalize (GObject *gobject)
{
  ClutterMasterClock *master_clock = CLUTTER_MASTER_CLOCK (gobject);

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
  GSource *source;

  source = clutter_clock_source_new (self);
  self->source = source;

  self->updated_stages = TRUE;

  g_source_set_priority (source, CLUTTER_PRIORITY_REDRAW);
  g_source_set_can_recurse (source, FALSE);
  g_source_attach (source, NULL);
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

/*
 * _clutter_master_clock_add_timeline:
 * @master_clock: a #ClutterMasterClock
 * @timeline: a #ClutterTimeline
 *
 * Adds @timeline to the list of playing timelines held by the master
 * clock.
 */
void
_clutter_master_clock_add_timeline (ClutterMasterClock *master_clock,
                                    ClutterTimeline    *timeline)
{
  gboolean is_first;

  if (g_slist_find (master_clock->timelines, timeline))
    return;

  is_first = master_clock->timelines == NULL;

  master_clock->timelines = g_slist_prepend (master_clock->timelines,
                                             timeline);

  if (is_first)
    _clutter_master_clock_start_running (master_clock);
}

/*
 * _clutter_master_clock_remove_timeline:
 * @master_clock: a #ClutterMasterClock
 * @timeline: a #ClutterTimeline
 *
 * Removes @timeline from the list of playing timelines held by the
 * master clock.
 */
void
_clutter_master_clock_remove_timeline (ClutterMasterClock *master_clock,
                                       ClutterTimeline    *timeline)
{
  master_clock->timelines = g_slist_remove (master_clock->timelines,
                                            timeline);
}

/*
 * _clutter_master_clock_start_running:
 * @master_clock: a #ClutterMasterClock
 *
 * Called when we have events or redraws to process; if the clock
 * is stopped, does the processing necessary to wake it up again.
 */
void
_clutter_master_clock_start_running (ClutterMasterClock *master_clock)
{
  /* If called from a different thread, we need to wake up the
   * main loop to start running the timelines
   */
  g_main_context_wakeup (NULL);
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
  GSList *l, *next;

  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  for (l = master_clock->timelines; l != NULL; l = next)
    {
      next = l->next;

      clutter_timeline_do_tick (l->data, &master_clock->cur_tick);
    }
}
