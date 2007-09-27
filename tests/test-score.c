#include <stdio.h>
#include <stdlib.h>
#include <clutter/clutter.h>


int
main (int argc, char **argv)
{
  ClutterScore    *score;
  ClutterTimeline *timeline_1;
  ClutterTimeline *timeline_2;
  ClutterTimeline *timeline_3;

  clutter_init (&argc, &argv);

  timeline_1 = clutter_timeline_new (10, 120);
  timeline_2 = clutter_timeline_clone (timeline_1);
  timeline_3 = clutter_timeline_clone (timeline_1);

  score = clutter_score_new();
  clutter_score_add (score, timeline_1);
  clutter_score_append (score, timeline_1, timeline_2);
#if 0
  clutter_score_append (score, timeline_2, timeline_3);
#endif
  clutter_score_start (score);

  clutter_main ();

  g_object_unref (score);
  g_object_unref (timeline_1);
  g_object_unref (timeline_2);
  g_object_unref (timeline_3);

  return EXIT_SUCCESS;
}
