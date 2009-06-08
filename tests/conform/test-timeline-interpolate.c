#include <stdlib.h>
#include <glib.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

/* We ask for 1 frame per millisecond.
 * Whenever this rate can't be achieved then the timeline
 * will interpolate the number frames that should have
 * passed between timeouts. */
#define TEST_TIMELINE_FPS 1000
#define TEST_TIMELINE_DURATION 5000

/* We are at the mercy of the system scheduler so this
 * may not be a very reliable tolerance. */
#define TEST_ERROR_TOLERANCE 20

typedef struct _TestState
{
  ClutterTimeline *timeline;
  GTimeVal start_time;
  guint new_frame_counter;
  gint expected_frame;
  gint completion_count;
  gboolean passed;
} TestState;


static void
new_frame_cb (ClutterTimeline *timeline,
	      gint frame_num,
	      TestState *state)
{
  GTimeVal current_time;
  gint current_frame;
  glong msec_diff;
  gint loop_overflow = 0;
  static gint step = 1;

  g_get_current_time (&current_time);

  current_frame = clutter_timeline_get_elapsed_time (state->timeline);

  msec_diff = (current_time.tv_sec - state->start_time.tv_sec) * 1000;
  msec_diff += (current_time.tv_usec - state->start_time.tv_usec)/1000;

  /* If we expect to have interpolated past the end of the timeline
   * we keep track of the overflow so we can determine when
   * the next timeout will happen. We then clip expected_frames
   * to TEST_TIMELINE_DURATION since clutter-timeline
   * semantics guaranty this frame is always signaled before
   * looping */
  if (state->expected_frame > TEST_TIMELINE_DURATION)
    {
      loop_overflow = state->expected_frame - TEST_TIMELINE_DURATION;
      state->expected_frame = TEST_TIMELINE_DURATION;
    }

  if (current_frame >= (state->expected_frame-TEST_ERROR_TOLERANCE)
      && current_frame <= (state->expected_frame+TEST_ERROR_TOLERANCE))
    {
      g_test_message ("\nelapsed milliseconds=%-5li "
		      "expected frame=%-4i actual frame=%-4i (OK)\n",
		      msec_diff,
		      state->expected_frame,
		      current_frame);
    }
  else
    {
      g_test_message ("\nelapsed milliseconds=%-5li "
		      "expected frame=%-4i actual frame=%-4i (FAILED)\n",
		      msec_diff,
		      state->expected_frame,
		      current_frame);
      state->passed = FALSE;
    }

  if (step>0)
    {
      state->expected_frame = current_frame + (TEST_TIMELINE_FPS / 4);
      g_test_message ("Sleeping for 250ms "
		      "so next frame should be (%i + %i) = %i\n",
		      current_frame,
		      (TEST_TIMELINE_FPS / 4),
		      state->expected_frame);
      g_usleep (250000);
    }
  else
    {
      state->expected_frame = current_frame + TEST_TIMELINE_FPS;
      g_test_message ("Sleeping for 1sec "
		      "so next frame should be (%i + %i) = %i\n",
		      current_frame,
		      TEST_TIMELINE_FPS,
		      state->expected_frame);
      g_usleep (1000000);
    }

  if (current_frame >= TEST_TIMELINE_DURATION)
    {
      state->expected_frame += loop_overflow;
      state->expected_frame -= TEST_TIMELINE_DURATION;
      g_test_message ("End of timeline reached: "
		      "Wrapping expected frame too %i\n",
		      state->expected_frame);
    }

  state->new_frame_counter++;
  step = -step;
}


static void
completed_cb (ClutterTimeline *timeline,
	      TestState *state)
{
  state->completion_count++;

  if (state->completion_count == 2)
    {
      if (state->passed)
	{
	  g_test_message ("Passed\n");
	  clutter_main_quit ();
	}
      else
	{
	  g_test_message ("Failed\n");
	  exit (EXIT_FAILURE);
	}
    }
}

void
test_timeline_interpolate (TestConformSimpleFixture *fixture, 
			   gconstpointer data)
{
  TestState state;

  state.timeline = 
    clutter_timeline_new (TEST_TIMELINE_DURATION);
  clutter_timeline_set_loop (state.timeline, TRUE);
  g_signal_connect (G_OBJECT(state.timeline),
		    "new-frame",
		    G_CALLBACK(new_frame_cb),
		    &state);
  g_signal_connect (G_OBJECT(state.timeline),
		    "completed",
		    G_CALLBACK(completed_cb),
		    &state);

  state.completion_count = 0;
  state.new_frame_counter = 0;
  state.passed = TRUE;
  state.expected_frame = 0;

  g_get_current_time (&state.start_time);
  clutter_timeline_start (state.timeline);
  
  clutter_main();

  g_object_unref (state.timeline);
}
