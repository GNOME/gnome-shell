
#ifdef CLUTTER_ENABLE_PROFILE

#include "clutter-profile.h"

#include <stdlib.h>

UProfContext *_clutter_uprof_context;
#define REPORT_COLUMN0_WIDTH 40

static gboolean searched_for_gl_uprof_context = FALSE;
static UProfContext *gl_uprof_context = NULL;

typedef struct _ClutterUProfReportState
{
  gulong n_frames;
} ClutterUProfReportState;

static void
print_counter (UProfCounterResult *counter,
               gpointer            data)
{
  ClutterUProfReportState  *state = data;
  gulong count = uprof_counter_result_get_count (counter);
  if (count == 0)
    return;

  g_print (" %-*s %-5ld %-5ld\n", REPORT_COLUMN0_WIDTH - 2,
           uprof_counter_result_get_name (counter),
           uprof_counter_result_get_count (counter),
           uprof_counter_result_get_count (counter) / state->n_frames);
}

static char *
print_timer_fields (UProfTimerResult *timer,
                    guint            *fields_width,
                    gpointer          data)
{
  ClutterUProfReportState *state = data;
  /* Print the field titles when timer == NULL */
  if (!timer)
    return g_strdup_printf ("Per Frame");

  return g_strdup_printf ("%-10.2f",
                          uprof_timer_result_get_total_msecs (timer) /
                          (float)state->n_frames);
}

static void
print_report (UProfReport *report, UProfContext *context)
{
  GList *root_timers;
  GList *l;
  UProfTimerResult *stage_paint_timer;
  UProfTimerResult *mainloop_timer;
  UProfTimerResult *do_pick_timer;
  float fps;
  ClutterUProfReportState state;

  /* FIXME: We need to fix the way Clutter initializes the uprof library
   * (we don't currently call uprof_init()) and add a mechanism to know
   * if uprof_init hasn't been called so we can simply bail out of report
   * generation and not print spurious warning about missing timers.
   * Probably we can just have uprof_report_print bail out if uprof wasn't
   * initialized, so we don't have to care here.
   */
  mainloop_timer = uprof_context_get_timer_result (context, "Mainloop");
  if (!mainloop_timer)
    return;
  stage_paint_timer = uprof_context_get_timer_result (context, "Redrawing");
  if (!stage_paint_timer)
    return;
  do_pick_timer = uprof_context_get_timer_result (context, "Do pick");
  if (!do_pick_timer)
    return;

  g_print ("\n");

  state.n_frames = uprof_timer_result_get_start_count (stage_paint_timer);
  g_print ("Frame count = %lu\n", state.n_frames);

  fps = (float)state.n_frames / (uprof_timer_result_get_total_msecs (mainloop_timer)
                                 / 1000.0);
  g_print ("Average fps = %5.2f\n", fps);

  if (do_pick_timer)
    {
      int n_picks = uprof_timer_result_get_start_count (do_pick_timer);

      g_print ("Pick Stats:\n");
      g_print ("Pick count = %d\n", n_picks);
      g_print ("Average picks per frame = %3.2f\n",
               (float)n_picks / (float)state.n_frames);
      g_print ("Average Msecs per pick = %3.2f\n",
               (float)uprof_timer_result_get_total_msecs (do_pick_timer)
               / (float)n_picks);

      g_print ("\n");
    }

  /* XXX: UProfs default reporting code now supports dynamic sizing for the Name
   * column, the only thing it's missing is support for adding custom columns but
   * when that's added we should switch away from manual report generation. */
  g_print ("Counters:\n");
  g_print (" %-*s %5s %s\n", REPORT_COLUMN0_WIDTH - 2, "Name", "Total", "Per Frame");
  g_print (" %-*s %5s %s\n", REPORT_COLUMN0_WIDTH - 2, "----", "-----", "---------");
  uprof_context_foreach_counter (context,
                                 UPROF_COUNTER_SORT_COUNT_INC,
                                 print_counter,
                                 &state);

  g_print ("\n");
  g_print ("Timers:\n");
  root_timers = uprof_context_get_root_timer_results (context);
  for (l = root_timers; l != NULL; l = l->next)
    uprof_timer_result_print_and_children ((UProfTimerResult *)l->data,
                                           print_timer_fields,
                                           &state);

  g_print ("\n");
}

/* FIXME: we should be able to deal with creating the uprof context in
 * clutter_init instead. I think the only reason I did it this way originally
 * was as a quick hack.
 */
static void __attribute__ ((constructor))
clutter_uprof_constructor (void)
{
  _clutter_uprof_context = uprof_context_new ("Clutter");
}

#if 0
static void
print_timers (UProfContext *context)
{
  GList *root_timers;
  GList *l;

  root_timers = uprof_context_get_root_timer_results ();

  root_timers =
    g_list_sort_with_data (context->root_timers,
                           (GCompareDataFunc)_uprof_timer_compare_total_times,
                           NULL);
  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimerState *timer = l->data;
      timer->children =
        g_list_sort_with_data (timer->children,
                               (GCompareDataFunc)
                                 _uprof_timer_compare_total_times,
                               NULL);
    }
}
#endif

static void __attribute__ ((destructor))
clutter_uprof_destructor (void)
{
  if (!(clutter_profile_flags & CLUTTER_PROFILE_DISABLE_REPORT))
    {
      UProfReport *report = uprof_report_new ("Clutter report");
      uprof_report_add_context (report, _clutter_uprof_context);
      uprof_report_add_context_callback (report, print_report);
      uprof_report_print (report);
      uprof_report_unref (report);
    }
  uprof_context_unref (_clutter_uprof_context);
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

