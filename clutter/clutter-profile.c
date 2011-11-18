#ifdef CLUTTER_ENABLE_PROFILE

#include <stdlib.h>

/* XXX - we need this for g_atexit() */
#define G_DISABLE_DEPRECATION_WARNINGS
#include "clutter-profile.h"

UProfContext *_clutter_uprof_context;

static UProfReport *clutter_uprof_report;

static gboolean searched_for_gl_uprof_context = FALSE;
static UProfContext *gl_uprof_context = NULL;

typedef struct _ClutterUProfReportState
{
  gulong n_frames;
  float fps;
  gulong n_picks;
  float msecs_picking;
} ClutterUProfReportState;

static char *
timer_per_frame_cb (UProfReport *report,
                    UProfTimerResult *timer,
                    void *user_data)
{
  ClutterUProfReportState *state = user_data;
  int n_frames = state->n_frames ? state->n_frames : 1;

  return g_strdup_printf ("%-10.2f",
                          uprof_timer_result_get_total_msecs (timer) /
                          (float)n_frames);
}

static char *
counter_per_frame_cb (UProfReport *report,
                      UProfCounterResult *counter,
                      void *user_data)
{
  ClutterUProfReportState *state = user_data;
  int n_frames = state->n_frames ? state->n_frames : 1;

  return g_strdup_printf ("%-5ld",
                          uprof_counter_result_get_count (counter) /
                          n_frames);
}

static char *
get_n_frames_cb (UProfReport *report,
                 const char *statistic,
                 const char *attribute,
                 void *user_data)
{
  ClutterUProfReportState *state = user_data;

  return g_strdup_printf ("%lu", state->n_frames);
}

static char *
get_fps_cb (UProfReport *report,
            const char *statistic,
            const char *attribute,
            void *user_data)
{
  ClutterUProfReportState *state = user_data;

  return g_strdup_printf ("%5.2f\n", state->fps);
}

static char *
get_n_picks_cb (UProfReport *report,
                const char *statistic,
                const char *attribute,
                void *user_data)
{
  ClutterUProfReportState *state = user_data;

  return g_strdup_printf ("%lu", state->n_picks);
}

static char *
get_picks_per_frame_cb (UProfReport *report,
                        const char *statistic,
                        const char *attribute,
                        void *user_data)
{
  ClutterUProfReportState *state = user_data;
  int n_frames = state->n_frames ? state->n_frames : 1;

  return g_strdup_printf ("%3.2f",
                          (float)state->n_picks / (float)n_frames);
}

static char *
get_msecs_per_pick_cb (UProfReport *report,
                       const char *statistic,
                       const char *attribute,
                       void *user_data)
{
  ClutterUProfReportState *state = user_data;
  int n_picks = state->n_picks ? state->n_picks : 1;

  return g_strdup_printf ("%3.2f", state->msecs_picking / (float)n_picks);
}

static gboolean
_clutter_uprof_report_prepare (UProfReport *report,
                               void **closure_ret,
                               void *user_data)
{
  UProfContext            *mainloop_context;
  UProfTimerResult        *mainloop_timer;
  UProfTimerResult        *stage_paint_timer;
  UProfTimerResult        *do_pick_timer;
  ClutterUProfReportState *state;

  /* NB: uprof provides a shared context for mainloop statistics which allows
   * this to work even if the application and not Clutter owns the mainloop.
   *
   * This is the case when running Mutter for example but because Mutter will
   * follow the same convention of using the shared context then we can always
   * be sure of where to look for the mainloop results. */
  mainloop_context = uprof_get_mainloop_context ();
  mainloop_timer = uprof_context_get_timer_result (mainloop_context,
                                                   "Mainloop");
  /* just bail out if the mainloop timer wasn't hit */
  if (!mainloop_timer)
    return FALSE;

  state = g_new0 (ClutterUProfReportState, 1);
  *closure_ret = state;

  stage_paint_timer = uprof_context_get_timer_result (_clutter_uprof_context,
                                                      "Redrawing");
  if (stage_paint_timer)
    {
      state->n_frames = uprof_timer_result_get_start_count (stage_paint_timer);

      uprof_report_add_statistic (report,
                                  "Frames",
                                  "Frame count information");
      uprof_report_add_statistic_attribute (report, "Frames",
                                            "Count", "Count",
                                            "The total number of frames",
                                            UPROF_ATTRIBUTE_TYPE_INT,
                                            get_n_frames_cb,
                                            state);


      state->fps = (float) state->n_frames
        / (uprof_timer_result_get_total_msecs (mainloop_timer)
           / 1000.0);
      uprof_report_add_statistic_attribute (report, "Frames",
                                            "Average FPS", "Average\nFPS",
                                            "The average frames per second",
                                            UPROF_ATTRIBUTE_TYPE_FLOAT,
                                            get_fps_cb,
                                            state);
    }

  do_pick_timer = uprof_context_get_timer_result (_clutter_uprof_context,
                                                  "Picking");
  if (do_pick_timer)
    {
      state->n_picks = uprof_timer_result_get_start_count (do_pick_timer);
      state->msecs_picking =
        uprof_timer_result_get_total_msecs (do_pick_timer);

      uprof_report_add_statistic (report,
                                  "Picks",
                                  "Picking information");
      uprof_report_add_statistic_attribute (report, "Picks",
                                            "Count", "Count",
                                            "The total number of picks",
                                            UPROF_ATTRIBUTE_TYPE_INT,
                                            get_n_picks_cb,
                                            state);

      uprof_report_add_statistic_attribute (report, "Picks",
                                            "Picks Per Frame",
                                            "Picks\nPer Frame",
                                            "The average number of picks "
                                            "per frame",
                                            UPROF_ATTRIBUTE_TYPE_FLOAT,
                                            get_picks_per_frame_cb,
                                            state);

      uprof_report_add_statistic_attribute (report, "Picks",
                                            "Msecs Per Pick",
                                            "Msecs\nPer Pick",
                                            "The average number of "
                                            "milliseconds per pick",
                                            UPROF_ATTRIBUTE_TYPE_FLOAT,
                                            get_msecs_per_pick_cb,
                                            state);
    }

  uprof_report_add_counters_attribute (clutter_uprof_report,
                                       "Per Frame",
                                       "Per Frame",
                                       "The number of counts per frame",
                                       UPROF_ATTRIBUTE_TYPE_INT,
                                       counter_per_frame_cb,
                                       state);
  uprof_report_add_timers_attribute (clutter_uprof_report,
                                     "Per Frame\nmsecs",
                                     "Per Frame",
                                     "The time spent in the timer per frame",
                                     UPROF_ATTRIBUTE_TYPE_FLOAT,
                                     timer_per_frame_cb,
                                     state);

  return TRUE;
}

static void
_clutter_uprof_report_done (UProfReport *report, void *closure, void *user_data)
{
  g_free (closure);
}

static void
print_exit_report (void)
{
  if (!(clutter_profile_flags & CLUTTER_PROFILE_DISABLE_REPORT))
    uprof_report_print (clutter_uprof_report);

  uprof_report_unref (clutter_uprof_report);

  uprof_context_unref (_clutter_uprof_context);
}

void
_clutter_uprof_init (void)
{
  UProfContext *cogl_context;

  _clutter_uprof_context = uprof_context_new ("Clutter");
  uprof_context_link (_clutter_uprof_context, uprof_get_mainloop_context ());
  g_atexit (print_exit_report);

  cogl_context = uprof_find_context ("Cogl");
  if (cogl_context)
    uprof_context_link (_clutter_uprof_context, cogl_context);

  /* We make the report object up-front so we can use uprof-tool
   * to fetch reports at runtime via dbus... */
  clutter_uprof_report = uprof_report_new ("Clutter report");
  uprof_report_add_context (clutter_uprof_report, _clutter_uprof_context);
  uprof_report_set_init_fini_callbacks (clutter_uprof_report,
                                        _clutter_uprof_report_prepare,
                                        _clutter_uprof_report_done,
                                        NULL);
}

void
_clutter_profile_suspend (void)
{
  if (G_UNLIKELY (!searched_for_gl_uprof_context))
    {
      gl_uprof_context = uprof_find_context ("OpenGL");
      searched_for_gl_uprof_context = TRUE;
    }

  if (gl_uprof_context)
    uprof_context_suspend (gl_uprof_context);

  /* NB: The Cogl context is linked to this so it will also be suspended... */
  uprof_context_suspend (_clutter_uprof_context);
}

void
_clutter_profile_resume (void)
{
  if (gl_uprof_context)
    uprof_context_resume (gl_uprof_context);

  /* NB: The Cogl context is linked to this so it will also be resumed... */
  uprof_context_resume (_clutter_uprof_context);
}
#endif
