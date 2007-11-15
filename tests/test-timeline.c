#include <stdio.h>
#include <stdlib.h>
#include <clutter/clutter.h>

static void
timeline_1_complete (ClutterTimeline *timeline)
{
  g_debug ("1: Completed");
}

static void
timeline_2_complete (ClutterTimeline *timeline)
{
  g_debug ("2: Completed");
}

static void
timeline_3_complete (ClutterTimeline *timeline)
{
  g_debug ("3: Completed");
}

static void
timeline_1_new_frame_cb (ClutterTimeline *timeline, gint frame_no)
{
  g_debug ("1: Doing frame %d.", frame_no);
}

static void
timeline_2_new_frame_cb (ClutterTimeline *timeline, gint frame_no)
{
  g_debug ("2: Doing frame %d.", frame_no);
}

static void
timeline_3_new_frame_cb (ClutterTimeline *timeline, gint frame_no)
{
  g_debug ("3: Doing frame %d.", frame_no);
}

int
main (int argc, char **argv)
{
  ClutterTimeline *timeline_1;
  ClutterTimeline *timeline_2;
  ClutterTimeline *timeline_3;

  clutter_init (&argc, &argv);

  timeline_1 = clutter_timeline_new (10, 120);

  timeline_2 = clutter_timeline_clone (timeline_1);

  timeline_3 = clutter_timeline_clone (timeline_1);
  clutter_timeline_set_direction (timeline_3, CLUTTER_TIMELINE_BACKWARD);

  g_signal_connect (timeline_1,
                    "new-frame", G_CALLBACK (timeline_1_new_frame_cb),
                    NULL);
  g_signal_connect (timeline_1,
                    "completed", G_CALLBACK (timeline_1_complete),
                    NULL);

  g_signal_connect (timeline_2,
                    "new-frame", G_CALLBACK (timeline_2_new_frame_cb),
                    NULL);
  g_signal_connect (timeline_2,
                    "completed", G_CALLBACK (timeline_2_complete),
                    NULL);

  g_signal_connect (timeline_3,
                    "new-frame", G_CALLBACK (timeline_3_new_frame_cb),
                    NULL);
  g_signal_connect (timeline_3,
                    "completed", G_CALLBACK (timeline_3_complete),
                    NULL);

  clutter_timeline_start (timeline_1);
  clutter_timeline_start (timeline_2);
  clutter_timeline_start (timeline_3);

  clutter_main ();

  g_object_unref (timeline_1);
  g_object_unref (timeline_2);
  g_object_unref (timeline_3);

  return EXIT_SUCCESS;
}
