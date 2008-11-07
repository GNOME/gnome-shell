#include <stdlib.h>
#include <glib.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

#define TEST_TIMELINE_FPS 10
#define TEST_TIMELINE_FRAME_COUNT 5
#define TEST_WATCHDOG_KICK_IN_SECONDS 10

typedef struct _TestState {
    ClutterTimeline *timeline;
    gint rewind_count;
}TestState;


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
  gint current_frame = clutter_timeline_get_current_frame (timeline);

  if (current_frame == TEST_TIMELINE_FRAME_COUNT)
    {
      g_test_message ("new-frame signal recieved (end of timeline)\n");
      g_test_message ("Rewinding timeline\n");
      clutter_timeline_rewind (timeline);
      state->rewind_count++;
    }
  else
    {
      if (current_frame == 0)
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


void
test_timeline_rewind (TestConformSimpleFixture *fixture,
		      gconstpointer data)
{
  TestState state;

  state.timeline = 
    clutter_timeline_new (TEST_TIMELINE_FRAME_COUNT,
                          TEST_TIMELINE_FPS);
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

  clutter_timeline_start (state.timeline);
  
  clutter_main();

  g_object_unref (state.timeline);
}

