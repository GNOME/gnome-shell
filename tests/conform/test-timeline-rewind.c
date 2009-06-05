#include <stdlib.h>
#include <glib.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

#define TEST_TIMELINE_DURATION 500
#define TEST_WATCHDOG_KICK_IN_SECONDS 10

typedef struct _TestState
{
  ClutterTimeline *timeline;
  gint rewind_count;
  guint source_id;
  GTimeVal prev_tick;
  gulong msecs_delta;
} TestState;

static gboolean
watchdog_timeout (TestState *state)
{
  g_test_message ("Watchdog timer kicking in\n");
  g_test_message ("rewind_count=%i\n", state->rewind_count);
  if (state->rewind_count <= 3)
    {
      /* The test has hung */
      g_test_message ("Failed (This test shouldn't have hung!)\n");
      exit (EXIT_FAILURE);
    }
  else
    {
      g_test_message ("Passed\n");
      clutter_main_quit ();
    }

  return FALSE;
}

static void
new_frame_cb (ClutterTimeline *timeline,
              gint frame_num,
              TestState *state)
{
  gint elapsed_time = clutter_timeline_get_elapsed_time (timeline);

  if (elapsed_time == TEST_TIMELINE_DURATION)
    {
      g_test_message ("new-frame signal recieved (end of timeline)\n");
      g_test_message ("Rewinding timeline\n");
      clutter_timeline_rewind (timeline);
      state->rewind_count++;
    }
  else
    {
      if (elapsed_time == 0)
        {
          g_test_message ("new-frame signal recieved (start of timeline)\n");
        }
      else
        {
          g_test_message ("new-frame signal recieved (mid frame)\n");
        }

      if (state->rewind_count >= 2)
        {
          g_test_message ("Sleeping for 1 second\n");
          g_usleep (1000000);
        }
    }
}

static gboolean
frame_tick (gpointer data)
{
  TestState *state = data;
  GTimeVal cur_tick = { 0, };
  gulong msecs;

  g_get_current_time (&cur_tick);

  if (state->prev_tick.tv_sec == 0)
    state->prev_tick = cur_tick;

  msecs = (cur_tick.tv_sec - state->prev_tick.tv_sec) * 1000
        + (cur_tick.tv_usec - state->prev_tick.tv_usec) / 1000;

  if (clutter_timeline_is_playing (state->timeline))
   clutter_timeline_advance_delta (state->timeline, msecs);

  state->msecs_delta = msecs;
  state->prev_tick = cur_tick;

  return TRUE;
}

void
test_timeline_rewind (TestConformSimpleFixture *fixture,
		      gconstpointer data)
{
  TestState state;

  state.timeline = 
    clutter_timeline_new (TEST_TIMELINE_DURATION);
  g_signal_connect (G_OBJECT(state.timeline),
                    "new-frame",
                    G_CALLBACK(new_frame_cb),
                    &state);
  g_test_message ("Installing a watchdog timeout "
		  "to determine if this test hangs\n");
  g_timeout_add (TEST_WATCHDOG_KICK_IN_SECONDS*1000,
		 (GSourceFunc)watchdog_timeout,
                 &state);
  state.rewind_count = 0;
  state.prev_tick.tv_sec = 0;
  state.prev_tick.tv_usec = 0;
  state.msecs_delta = 0;

  state.source_id =
    clutter_threads_add_frame_source (60, frame_tick, &state);

  clutter_timeline_start (state.timeline);
  
  clutter_main();

  g_source_remove (state.source_id);
  g_object_unref (state.timeline);
}
