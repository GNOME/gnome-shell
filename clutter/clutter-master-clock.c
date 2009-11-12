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
#include "clutter-profile.h"

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

  /* If the master clock is idle that means it's
   * fallen back to idle polling for timeline
   * progressions and it may have been some time since
   * the last real stage update.
   */
  guint idle : 1;
  guint ensure_next_iteration : 1;
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
  gboolean stage_free = FALSE;

  stages = clutter_stage_manager_peek_stages (stage_manager);

  /* If all of the stages are busy waiting for a swap-buffers to complete
   * then we stop the master clock... */
  for (l = stages; l != NULL; l = l->next)
    if (_clutter_stage_get_pending_swaps (l->data) == 0)
      {
        stage_free = TRUE;
        break;
      }
  if (!stage_free)
    return FALSE;

  if (master_clock->timelines)
    return TRUE;

  for (l = stages; l; l = l->next)
    if (_clutter_stage_has_queued_events (l->data) ||
        _clutter_stage_needs_update (l->data))
      return TRUE;

  if (master_clock->ensure_next_iteration)
    {
      master_clock->ensure_next_iteration = FALSE;
      return TRUE;
    }

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

  /* When we have sync-to-vblank, we count on swap-buffer requests (or
   * swap-buffer-complete events if supported in the backend) to throttle our
   * frame rate so no additional delay is needed to start the next frame.
   *
   * If the master-clock has become idle due to no timeline progression causing
   * redraws then we can no longer rely on vblank synchronization because the
   * last real stage update/redraw may have happened a long time ago and so we
   * fallback to polling for timeline progressions every 1/frame_rate seconds.
   *
   * (NB: if there aren't even any timelines running then the master clock will
   * be completely stopped in master_clock_is_running())
   */
  if (clutter_feature_available (CLUTTER_FEATURE_SYNC_TO_VBLANK) &&
      !master_clock->idle)
    {
      CLUTTER_NOTE (SCHEDULER, "vblank available and updated stages");
      return 0;
    }

  if (master_clock->prev_tick.tv_sec == 0)
    {
      /* If we weren't previously running, then draw the next frame
       * immediately
       */
      CLUTTER_NOTE (SCHEDULER, "draw the first frame immediately");
      return 0;
    }

  /* Otherwise, wait at least 1/frame_rate seconds since we last
   * started a frame
   */
  g_source_get_current_time (master_clock->source, &now);

  next = master_clock->prev_tick;

  /* If time has gone backwards then there's no way of knowing how
     long we should wait so let's just dispatch immediately */
  if (now.tv_sec < next.tv_sec ||
      (now.tv_sec == next.tv_sec && now.tv_usec <= next.tv_usec))
    {
      CLUTTER_NOTE (SCHEDULER, "Time has gone backwards");

      return 0;
    }

  g_time_val_add (&next, 1000000L / (gulong) clutter_get_default_frame_rate ());

  if (next.tv_sec < now.tv_sec ||
      (next.tv_sec == now.tv_sec && next.tv_usec <= now.tv_usec))
    {
      CLUTTER_NOTE (SCHEDULER, "Less than %lu microsecs",
                    1000000L / (gulong) clutter_get_default_frame_rate ());

      return 0;
    }
  else
    {
      CLUTTER_NOTE (SCHEDULER, "Waiting %lu msecs",
                    (next.tv_sec - now.tv_sec) * 1000 +
                    (next.tv_usec - now.tv_usec) / 1000);

      return ((next.tv_sec  - now.tv_sec)  * 1000 +
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
  gboolean stages_updated = FALSE;

  CLUTTER_STATIC_TIMER (master_dispatch_timer,
                        "Mainloop",
                        "Master Clock",
                        "Master clock dispatch",
                        0);
  CLUTTER_STATIC_TIMER (master_event_process,
                        "Master Clock",
                        "Event Processing",
                        "The time spent processing events on all stages",
                        0);

  CLUTTER_TIMER_START (_clutter_uprof_context, master_dispatch_timer);

  CLUTTER_NOTE (SCHEDULER, "Master clock [tick]");

  clutter_threads_enter ();

  /* Get the time to use for this frame.
   */
  g_source_get_current_time (source, &master_clock->cur_tick);

  /* We need to protect ourselves against stages being destroyed during
   * event handling
   */
  stages = clutter_stage_manager_list_stages (stage_manager);
  g_slist_foreach (stages, (GFunc) g_object_ref, NULL);

  CLUTTER_TIMER_START (_clutter_uprof_context, master_event_process);

  master_clock->idle = FALSE;

  /* Process queued events */
  for (l = stages; l != NULL; l = l->next)
    {
      /* NB: If a stage is busy waiting for a swap-buffers completion then
       * we don't process its events so we can maximize the benefits of
       * motion compression, and avoid multiple picks per frame.
       */
      if (_clutter_stage_get_pending_swaps (l->data) == 0)
        _clutter_stage_process_queued_events (l->data);
    }

  CLUTTER_TIMER_STOP (_clutter_uprof_context, master_event_process);

  _clutter_master_clock_advance (master_clock);

  _clutter_run_repaint_functions ();

  /* Update any stage that needs redraw/relayout after the clock
   * is advanced.
   */
  for (l = stages; l != NULL; l = l->next)
    {
      /* If a stage has a swap-buffers pending we don't want to draw to it
       * in case the driver may block the CPU while it waits for the next
       * backbuffer to become available.
       *
       * TODO: We should be able to identify if we are running triple or N
       * buffered and in these cases we can still draw if there is 1 swap
       * pending so we can hopefully always be ready to swap for the next
       * vblank and really match the vsync frequency.
       */
      if (_clutter_stage_get_pending_swaps (l->data) == 0)
        stages_updated |= _clutter_stage_do_update (l->data);
    }

  /* The master clock goes idle if no stages were updated and falls back
   * to polling for timeline progressions... */
  if (!stages_updated)
    master_clock->idle = TRUE;

  g_slist_foreach (stages, (GFunc) g_object_unref, NULL);
  g_slist_free (stages);

  master_clock->prev_tick = master_clock->cur_tick;

  clutter_threads_leave ();

  CLUTTER_TIMER_STOP (_clutter_uprof_context, master_dispatch_timer);

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

  self->idle = FALSE;
  self->ensure_next_iteration = FALSE;

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
  GSList *timelines, *l;

  CLUTTER_STATIC_TIMER (master_timeline_advance,
                        "Master Clock",
                        "Timelines Advancement",
                        "The time spent advancing all timelines",
                        0);

  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  CLUTTER_TIMER_START (_clutter_uprof_context, master_timeline_advance);

  /* we protect ourselves from timelines being removed during
   * the advancement by other timelines by copying the list of
   * timelines, taking a reference on them, iterating over the
   * copied list and then releasing the reference.
   *
   * we cannot simply take a reference on the timelines and still
   * use the list held by the master clock because the do_tick()
   * might result in the creation of a new timeline, which gets
   * added at the end of the list with no reference increase and
   * thus gets disposed at the end of the iteration.
   *
   * this implies that a newly added timeline will not be advanced
   * by this clock iteration, which is perfectly fine since we're
   * in its first cycle.
   *
   * we also cannot steal the master clock timelines list because
   * a timeline might be removed as the direct result of do_tick()
   * and remove_timeline() would not find the timeline, failing
   * and leaving a dangling pointer behind.
   */
  timelines = g_slist_copy (master_clock->timelines);
  g_slist_foreach (timelines, (GFunc) g_object_ref, NULL);

  for (l = timelines; l != NULL; l = l->next)
    clutter_timeline_do_tick (l->data, &master_clock->cur_tick);

  g_slist_foreach (timelines, (GFunc) g_object_unref, NULL);
  g_slist_free (timelines);

  CLUTTER_TIMER_STOP (_clutter_uprof_context, master_timeline_advance);
}

/**
 * _clutter_master_clock_ensure_next_iteration:
 * @master_clock: a #ClutterMasterClock
 *
 * Ensures that the master clock will run at least one iteration
 */
void
_clutter_master_clock_ensure_next_iteration (ClutterMasterClock *master_clock)
{
  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  master_clock->ensure_next_iteration = TRUE;
}

