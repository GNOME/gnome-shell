#include <stdlib.h>
#include <glib.h>
#include <clutter/clutter.h>

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
  g_print ("Watchdog timer kicking in\n");
  g_print ("rewind_count=%i\n", state->rewind_count);
  if (state->rewind_count <= 3)
    {
      /* The test has hung */
      g_print ("Failed (This test shouldn't have hung!)\n");
      exit (EXIT_FAILURE);
    }
  else
    {
      g_print ("Passed\n");
      exit (EXIT_SUCCESS);
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
      g_print ("new-frame signal recieved (end of timeline)\n");
      g_print ("Rewinding timeline\n");
      clutter_timeline_rewind (timeline);
      state->rewind_count++;
    }
  else
    {
      if (current_frame == 0)
        {
          g_print ("new-frame signal recieved (start of timeline)\n");
        }
      else
        {
          g_print ("new-frame signal recieved (mid frame)\n");
        }

      if (state->rewind_count >= 2)
        {
          g_print ("Sleeping for 1 second\n");
          g_usleep (1000000);
        }
    }
}


int
main (int argc, char **argv)
{
  TestState state;

  clutter_init (&argc, &argv);
  
  state.timeline = 
    clutter_timeline_new (TEST_TIMELINE_FRAME_COUNT,
                          TEST_TIMELINE_FPS);
  g_signal_connect (G_OBJECT(state.timeline),
                    "new-frame",
                    G_CALLBACK(new_frame_cb),
                    &state);
  g_print ("Installing a watchdog timeout to determin if this test hangs\n");
  g_timeout_add (TEST_WATCHDOG_KICK_IN_SECONDS*1000,
		 (GSourceFunc)watchdog_timeout,
                 &state);
  state.rewind_count = 0;

  clutter_timeline_start (state.timeline);
  
  clutter_main();
  
  return EXIT_FAILURE;
}

