/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *
 * Copyright (C) 2015  Intel Corporation.
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
 * SECTION:clutter-master-clock-gdk
 * @short_description: The GDK master clock for all animations
 *
 * The #ClutterMasterClockDefault class is the GdkFrameClock based implementation
 * of #ClutterMasterClock.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gdk/gdk.h>

#include "clutter-master-clock.h"
#include "clutter-master-clock-gdk.h"
#include "clutter-stage-gdk.h"
#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-stage-manager-private.h"
#include "clutter-stage-private.h"

#ifdef CLUTTER_ENABLE_DEBUG
#define clutter_warn_if_over_budget(master_clock,start_time,section)    G_STMT_START  { \
  gint64 __delta = g_get_monotonic_time () - start_time;                                \
  gint64 __budget = master_clock->remaining_budget;                                     \
  if (__budget > 0 && __delta >= __budget) {                                            \
    _clutter_diagnostic_message ("%s took %" G_GINT64_FORMAT " microseconds "           \
                                 "more than the remaining budget of %" G_GINT64_FORMAT  \
                                 " microseconds",                                       \
                                 section, __delta - __budget, __budget);                \
  }                                                                     } G_STMT_END
#else
#define clutter_warn_if_over_budget(master_clock,start_time,section)
#endif

typedef struct _ClutterClockSource              ClutterClockSource;

struct _ClutterMasterClockGdk
{
  GObject parent_instance;

  /* the list of timelines handled by the clock */
  GSList *timelines;

  /* mapping between ClutterStages and GdkFrameClocks.
   *
   * @stage_to_clock: a direct mapping because each stage has at most one clock
   * @clock_to_stage: each clock can have more than one stage
   */
  GHashTable *stage_to_clock;
  GHashTable *clock_to_stage;

  /* the current state of the clock, in usecs */
  gint64 cur_tick;

  /* the previous state of the clock, in usecs, used to compute the delta */
  gint64 prev_tick;

#ifdef CLUTTER_ENABLE_DEBUG
  gint64 frame_budget;
  gint64 remaining_budget;
#endif
};

struct _ClutterClockSource
{
  GSource source;

  ClutterMasterClock *master_clock;
};

static void clutter_master_clock_iface_init (ClutterMasterClockIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterMasterClockGdk,
                         clutter_master_clock_gdk,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MASTER_CLOCK,
                                                clutter_master_clock_iface_init));

static void
master_clock_schedule_forced_stages_updates (ClutterMasterClockGdk *master_clock)
{
  GHashTableIter iter;
  gpointer stage, frame_clock;

  g_hash_table_iter_init (&iter, master_clock->stage_to_clock);
  while (g_hash_table_iter_next (&iter, &stage, &frame_clock))
    gdk_frame_clock_request_phase (GDK_FRAME_CLOCK (frame_clock),
                                   GDK_FRAME_CLOCK_PHASE_PAINT);
}

static void
master_clock_sync_frame_clock_update (ClutterMasterClockGdk *master_clock)
{
  gboolean updating = master_clock->timelines != NULL;
  gpointer frame_clock, stage_list;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, master_clock->clock_to_stage);
  while (g_hash_table_iter_next (&iter, &frame_clock, &stage_list))
    {
      gboolean clock_updating =
        GPOINTER_TO_UINT (g_object_get_data (frame_clock,
                                             "clutter-master-clock-updating"));
      if (clock_updating != updating)
        {
          if (updating)
            gdk_frame_clock_begin_updating (GDK_FRAME_CLOCK (frame_clock));
          else
            gdk_frame_clock_end_updating (GDK_FRAME_CLOCK (frame_clock));
          g_object_set_data (frame_clock,
                             "clutter-master-clock-updating",
                             GUINT_TO_POINTER (updating));
        }
    }
}

static void
master_clock_schedule_stage_update (ClutterMasterClockGdk *master_clock,
                                    ClutterStage          *stage,
                                    GdkFrameClock         *frame_clock)
{
  /* Clear the old update time */
  _clutter_stage_clear_update_time (stage);

  /* And if there is still work to be done, schedule a new one */
  if (_clutter_stage_has_queued_events (stage) ||
      _clutter_stage_needs_update (stage))
    _clutter_stage_schedule_update (stage);

  /* We can avoid to schedule a new frame if the stage doesn't need
   * anymore redrawing. But in the case we still have timelines alive,
   * we have no choice, we need to advance the timelines for the next
   * frame. */
  if (master_clock->timelines != NULL)
    gdk_frame_clock_request_phase (frame_clock, GDK_FRAME_CLOCK_PHASE_PAINT);
}

static void
master_clock_process_stage_events (ClutterMasterClockGdk *master_clock,
                                   ClutterStage          *stage)
{
#ifdef CLUTTER_ENABLE_DEBUG
  gint64 start = g_get_monotonic_time ();
#endif

  /* Process queued events */
  _clutter_stage_process_queued_events (stage);

#ifdef CLUTTER_ENABLE_DEBUG
  if (_clutter_diagnostic_enabled ())
    clutter_warn_if_over_budget (master_clock, start, "Event processing");

  master_clock->remaining_budget -= (g_get_monotonic_time () - start);
#endif
}

/*
 * master_clock_advance_timelines:
 * @master_clock: a #ClutterMasterClock
 *
 * Advances all the timelines held by the master clock. This function
 * should be called before calling _clutter_stage_do_update() to
 * make sure that all the timelines are advanced and the scene is updated.
 */
static void
master_clock_advance_timelines (ClutterMasterClockGdk *master_clock)
{
  GSList *timelines, *l;
#ifdef CLUTTER_ENABLE_DEBUG
  gint64 start = g_get_monotonic_time ();
#endif

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
    _clutter_timeline_do_tick (l->data, master_clock->cur_tick / 1000);

  g_slist_foreach (timelines, (GFunc) g_object_unref, NULL);
  g_slist_free (timelines);

#ifdef CLUTTER_ENABLE_DEBUG
  if (_clutter_diagnostic_enabled ())
    clutter_warn_if_over_budget (master_clock, start, "Animations");

  master_clock->remaining_budget -= (g_get_monotonic_time () - start);
#endif
}

static gboolean
master_clock_update_stage (ClutterMasterClockGdk *master_clock,
                           ClutterStage          *stage)
{
  gboolean stage_updated = FALSE;
#ifdef CLUTTER_ENABLE_DEBUG
  gint64 start = g_get_monotonic_time ();
#endif

  _clutter_run_repaint_functions (CLUTTER_REPAINT_FLAGS_PRE_PAINT);

  /* Update any stage that needs redraw/relayout after the clock
   * is advanced.
   */
  stage_updated |= _clutter_stage_do_update (stage);

  _clutter_run_repaint_functions (CLUTTER_REPAINT_FLAGS_POST_PAINT);

#ifdef CLUTTER_ENABLE_DEBUG
  if (_clutter_diagnostic_enabled ())
    clutter_warn_if_over_budget (master_clock, start, "Updating the stage");

  master_clock->remaining_budget -= (g_get_monotonic_time () - start);
#endif

  return stage_updated;
}

static void
clutter_master_clock_gdk_update (GdkFrameClock         *frame_clock,
                                 ClutterMasterClockGdk *master_clock)
{
  GList *stages, *l;

  _clutter_threads_acquire_lock ();

  /* Get the time to use for this frame */
  master_clock->cur_tick = gdk_frame_clock_get_frame_time (frame_clock);

#ifdef CLUTTER_ENABLE_DEBUG
  /* Update the remaining budget */
  master_clock->remaining_budget = master_clock->frame_budget;
#endif

  stages = g_hash_table_lookup (master_clock->clock_to_stage, frame_clock);
  CLUTTER_NOTE (SCHEDULER, "Updating %d stages tied to frame clock %p",
                g_list_length (stages), frame_clock);
  for (l = stages; l != NULL; l = l->next)
    {
      ClutterStage *stage = l->data;

      CLUTTER_NOTE (SCHEDULER, "Master clock (stage:%p, clock:%p) [tick]", stage, frame_clock);

      /* Each frame is split into three separate phases: */

      /* 1. process all the events; goes through the stage's event queue
       *    and processes each event according to its type, then emits the
       *    various signals that are associated with the event
       */
      master_clock_process_stage_events (master_clock, stage);

      /* 2. advance the timelines */
      master_clock_advance_timelines (master_clock);

      /* 3. relayout and redraw the stage; the stage might have been
       *    destroyed in 1. when processing events, check whether it's
       *    still alive.
       */

      if (g_hash_table_lookup (master_clock->stage_to_clock, stage) != NULL)
        {
          master_clock_update_stage (master_clock, stage);
          master_clock_schedule_stage_update (master_clock, stage, frame_clock);
        }
    }

  master_clock->prev_tick = master_clock->cur_tick;

  _clutter_threads_release_lock ();
}

static void
clutter_master_clock_gdk_remove_stage_clock (ClutterMasterClockGdk *master_clock,
                                             ClutterStage          *stage)
{
  gpointer frame_clock = g_hash_table_lookup (master_clock->stage_to_clock, stage);
  GList *stages;

  if (frame_clock == NULL)
    return;

  CLUTTER_NOTE (SCHEDULER, "Removing stage %p with clock %p", stage, frame_clock);

  g_hash_table_remove (master_clock->stage_to_clock, stage);

  stages = g_hash_table_lookup (master_clock->clock_to_stage, frame_clock);
  if (stages != NULL)
    {
      if (stages->next == NULL)
        {
          /* Deleting the last stage linked to a given clock. We can stop
             listening to that clock and also tell the clock we're finish
             updating it. */
          if (GPOINTER_TO_UINT (g_object_get_data (frame_clock,
                                                   "clutter-master-clock-updating")))
            {
              gdk_frame_clock_end_updating (GDK_FRAME_CLOCK (frame_clock));
              g_object_set_data (frame_clock, "clutter-master-clock-updating", NULL);
            }
          g_signal_handlers_disconnect_by_func (frame_clock,
                                                clutter_master_clock_gdk_update,
                                                master_clock);

          g_hash_table_remove (master_clock->clock_to_stage, frame_clock);
          g_list_free (stages);
        }
      else
        {
          stages = g_list_remove (stages, stage);
          g_hash_table_replace (master_clock->clock_to_stage,
                                g_object_ref (frame_clock),
                                stages);
        }
    }
}

static void
clutter_master_clock_gdk_add_stage_clock (ClutterMasterClockGdk *master_clock,
                                          ClutterStage          *stage,
                                          GdkFrameClock         *frame_clock)
{
  GList *stages;

  clutter_master_clock_gdk_remove_stage_clock (master_clock, stage);

  CLUTTER_NOTE (SCHEDULER, "Adding stage %p with clock %p", stage, frame_clock);

  g_hash_table_insert (master_clock->stage_to_clock, stage, g_object_ref (frame_clock));

  stages = g_hash_table_lookup (master_clock->clock_to_stage, frame_clock);
  if (stages == NULL)
    {
      g_hash_table_insert (master_clock->clock_to_stage, g_object_ref (frame_clock),
                           g_list_append (NULL, stage));

      g_signal_connect (frame_clock, "paint",
                        G_CALLBACK (clutter_master_clock_gdk_update),
                        master_clock);
    }
  else
    stages = g_list_append (stages, stage);

  if (master_clock->timelines != NULL)
    {
      _clutter_master_clock_start_running ((ClutterMasterClock *) master_clock);
      /* We only need to synchronize the frame clock state if we have
         timelines running. */
      master_clock_sync_frame_clock_update (master_clock);
    }
}

static void
clutter_master_clock_gdk_listen_to_stage (ClutterMasterClockGdk *master_clock,
                                          ClutterStage          *stage)
{
  ClutterStageWindow *stage_window;
  ClutterStageGdk *stage_window_gdk;
  GdkFrameClock *frame_clock;

  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    {
      clutter_master_clock_gdk_remove_stage_clock (master_clock, stage);
      return;
    }

  stage_window_gdk = CLUTTER_STAGE_GDK (stage_window);
  if (stage_window_gdk->window == NULL)
    {
      clutter_master_clock_gdk_remove_stage_clock (master_clock, stage);
      return;
    }

  frame_clock = gdk_window_get_frame_clock (stage_window_gdk->window);
  if (frame_clock == NULL)
    {
      clutter_master_clock_gdk_remove_stage_clock (master_clock, stage);
      return;
    }

  clutter_master_clock_gdk_add_stage_clock (master_clock, stage, frame_clock);
}

static void
clutter_master_clock_gdk_stage_visibility (ClutterStage          *stage,
                                           GParamSpec            *spec,
                                           ClutterMasterClockGdk *master_clock)
{
  ClutterActor *actor = CLUTTER_ACTOR (stage);
  if (clutter_actor_is_mapped (actor))
    clutter_master_clock_gdk_listen_to_stage (master_clock, stage);
  else
    clutter_master_clock_gdk_remove_stage_clock (master_clock, stage);
}

static void
clutter_master_clock_gdk_stage_added (ClutterStageManager   *manager,
                                      ClutterStage          *stage,
                                      ClutterMasterClockGdk *master_clock)
{
  g_signal_connect (stage, "notify::mapped",
                    G_CALLBACK (clutter_master_clock_gdk_stage_visibility),
                    master_clock);

  clutter_master_clock_gdk_stage_visibility (stage, NULL, master_clock);
}

static void
clutter_master_clock_gdk_stage_removed (ClutterStageManager   *manager,
                                        ClutterStage          *stage,
                                        ClutterMasterClockGdk *master_clock)
{
  clutter_master_clock_gdk_remove_stage_clock (master_clock, stage);

  g_signal_handlers_disconnect_by_func (stage,
                                        clutter_master_clock_gdk_stage_visibility,
                                        master_clock);
}

static void
clutter_master_clock_gdk_dispose (GObject *gobject)
{
  ClutterStageManager *manager = clutter_stage_manager_get_default ();

  g_signal_handlers_disconnect_by_func (manager,
                                        clutter_master_clock_gdk_stage_added,
                                        gobject);
  g_signal_handlers_disconnect_by_func (manager,
                                        clutter_master_clock_gdk_stage_removed,
                                        gobject);

  G_OBJECT_CLASS (clutter_master_clock_gdk_parent_class)->dispose (gobject);
}

static void
clutter_master_clock_gdk_finalize (GObject *gobject)
{
  ClutterMasterClockGdk *master_clock = CLUTTER_MASTER_CLOCK_GDK (gobject);

  g_hash_table_unref (master_clock->clock_to_stage);
  g_hash_table_unref (master_clock->stage_to_clock);
  g_slist_free (master_clock->timelines);

  G_OBJECT_CLASS (clutter_master_clock_gdk_parent_class)->finalize (gobject);
}

static void
clutter_master_clock_gdk_class_init (ClutterMasterClockGdkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_master_clock_gdk_dispose;
  gobject_class->finalize = clutter_master_clock_gdk_finalize;
}

static void
clutter_master_clock_gdk_init (ClutterMasterClockGdk *self)
{
  ClutterStageManager *manager;
  const GSList *stages, *l;

#ifdef CLUTTER_ENABLE_DEBUG
  self->frame_budget = G_USEC_PER_SEC / 60;
#endif

  self->clock_to_stage = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                g_object_unref, NULL);
  self->stage_to_clock = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                NULL, g_object_unref);

  manager = clutter_stage_manager_get_default ();
  g_signal_connect (manager, "stage-added",
                    G_CALLBACK (clutter_master_clock_gdk_stage_added), self);
  g_signal_connect (manager, "stage-removed",
                    G_CALLBACK (clutter_master_clock_gdk_stage_removed), self);

  stages = clutter_stage_manager_peek_stages (manager);
  for (l = stages; l; l = l->next)
    clutter_master_clock_gdk_stage_added (manager, l->data, self);

  if (G_UNLIKELY (clutter_paint_debug_flags & CLUTTER_DEBUG_CONTINUOUS_REDRAW))
    g_warning ("Continuous redraw is not supported with the GDK backend.");
}

static void
clutter_master_clock_gdk_add_timeline (ClutterMasterClock *clock,
                                       ClutterTimeline    *timeline)
{
  ClutterMasterClockGdk *master_clock = (ClutterMasterClockGdk *) clock;
  gboolean is_first;

  if (g_slist_find (master_clock->timelines, timeline))
    return;

  is_first = master_clock->timelines == NULL;

  master_clock->timelines = g_slist_prepend (master_clock->timelines,
                                             timeline);

  if (is_first)
    {
      _clutter_master_clock_start_running (clock);
      /* Sync frame clock update state if needed. */
      master_clock_sync_frame_clock_update (master_clock);
    }
}

static void
clutter_master_clock_gdk_remove_timeline (ClutterMasterClock *clock,
                                          ClutterTimeline    *timeline)
{
  ClutterMasterClockGdk *master_clock = (ClutterMasterClockGdk *) clock;

  master_clock->timelines = g_slist_remove (master_clock->timelines,
                                            timeline);

  /* Sync frame clock update state if we have no more timelines running. */
  if (master_clock->timelines == NULL)
    master_clock_sync_frame_clock_update (master_clock);
}

static void
clutter_master_clock_gdk_start_running (ClutterMasterClock *clock)
{
  master_clock_schedule_forced_stages_updates ((ClutterMasterClockGdk *) clock);
}

static void
clutter_master_clock_gdk_ensure_next_iteration (ClutterMasterClock *clock)
{
  master_clock_schedule_forced_stages_updates ((ClutterMasterClockGdk *) clock);
}

static void
clutter_master_clock_gdk_set_paused (ClutterMasterClock *clock,
                                     gboolean            paused)
{
  /* GdkFrameClock runs the show here. We do not decide whether the
     clock is paused or not. */
}

static void
clutter_master_clock_iface_init (ClutterMasterClockIface *iface)
{
  iface->add_timeline = clutter_master_clock_gdk_add_timeline;
  iface->remove_timeline = clutter_master_clock_gdk_remove_timeline;
  iface->start_running = clutter_master_clock_gdk_start_running;
  iface->ensure_next_iteration = clutter_master_clock_gdk_ensure_next_iteration;
  iface->set_paused = clutter_master_clock_gdk_set_paused;
}
