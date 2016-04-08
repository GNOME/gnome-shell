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
} TestState;

static gboolean
watchdog_timeout (gpointer data)
{
  TestState *state = data;

  g_test_message ("Watchdog timer kicking in");
  g_test_message ("rewind_count=%i", state->rewind_count);
  if (state->rewind_count <= 3)
    {
      /* The test has hung */
      g_test_message ("Failed (This test shouldn't have hung!)");
      exit (EXIT_FAILURE);
    }
  else
    {
      g_test_message ("Passed");
      clutter_main_quit ();
    }

  return G_SOURCE_REMOVE;
}

static void
new_frame_cb (ClutterTimeline *timeline,
              gint elapsed_time,
              TestState *state)
{
  if (elapsed_time == TEST_TIMELINE_DURATION)
    {
      g_test_message ("new-frame signal received (end of timeline)");
      g_test_message ("Rewinding timeline");
      clutter_timeline_rewind (timeline);
      state->rewind_count++;
    }
  else
    {
      if (elapsed_time == 0)
        {
          g_test_message ("new-frame signal received (start of timeline)");
        }
      else
        {
          g_test_message ("new-frame signal received (mid frame)");
        }

      if (state->rewind_count >= 2)
        {
          g_test_message ("Sleeping for 1 second");
          g_usleep (1000000);
        }
    }
}

void
timeline_rewind (void)
{
  TestState state;

  state.timeline = 
    clutter_timeline_new (TEST_TIMELINE_DURATION);
  g_signal_connect (G_OBJECT(state.timeline),
                    "new-frame",
                    G_CALLBACK(new_frame_cb),
                    &state);
  g_test_message ("Installing a watchdog timeout "
		  "to determine if this test hangs");
  clutter_threads_add_timeout (TEST_WATCHDOG_KICK_IN_SECONDS * 1000,
                               watchdog_timeout,
                               &state);
  state.rewind_count = 0;

  clutter_timeline_start (state.timeline);
  
  clutter_main();

  g_object_unref (state.timeline);
}
