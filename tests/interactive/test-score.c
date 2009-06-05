#include <stdio.h>
#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static gint level = 1;

static void
on_score_started (ClutterScore *score)
{
  g_print ("Score started\n");
}

static void
on_score_completed (ClutterScore *score)
{
  g_print ("Score completed\n");
  clutter_main_quit ();
}

static void
on_timeline_started (ClutterScore    *score,
                     ClutterTimeline *timeline)
{
  gint i;

  for (i = 0; i < level; i++)
    g_print (" ");

  g_print ("Started timeline: '%s'\n",
           (gchar *) g_object_get_data (G_OBJECT (timeline), "timeline-name"));

  level += 1;
}

static void
on_timeline_completed (ClutterScore    *score,
                       ClutterTimeline *timeline)
{
  gint i;

  level -= 1;

  for (i = 0; i < level; i++)
    g_print (" ");

  g_print ("Completed timeline: '%s'\n",
           (gchar *) g_object_get_data (G_OBJECT (timeline), "timeline-name"));
}

G_MODULE_EXPORT int
test_score_main (int argc, char **argv)
{
  ClutterScore    *score;
  ClutterTimeline *timeline_1;
  ClutterTimeline *timeline_2;
  ClutterTimeline *timeline_3;
  ClutterTimeline *timeline_4;
  ClutterTimeline *timeline_5;
  GSList *timelines;

  clutter_init (&argc, &argv);

  timeline_1 = clutter_timeline_new (1000);
  g_object_set_data_full (G_OBJECT (timeline_1),
                          "timeline-name", g_strdup ("Timeline 1"),
                          g_free);

  timeline_2 = clutter_timeline_new (1000);
  clutter_timeline_add_marker_at_time (timeline_2, "foo", 500);
  g_object_set_data_full (G_OBJECT (timeline_2),
                          "timeline-name", g_strdup ("Timeline 2"),
                          g_free);

  timeline_3 = clutter_timeline_new (1000);
  g_object_set_data_full (G_OBJECT (timeline_3),
                          "timeline-name", g_strdup ("Timeline 3"),
                          g_free);

  timeline_4 = clutter_timeline_new (1000);
  g_object_set_data_full (G_OBJECT (timeline_4),
                          "timeline-name", g_strdup ("Timeline 4"),
                          g_free);

  timeline_5 = clutter_timeline_new (1000);
  g_object_set_data_full (G_OBJECT (timeline_5),
                          "timeline-name", g_strdup ("Timeline 5"),
                          g_free);

  score = clutter_score_new();
  g_signal_connect (score, "started",
                    G_CALLBACK (on_score_started),
                    NULL);
  g_signal_connect (score, "timeline-started",
                    G_CALLBACK (on_timeline_started),
                    NULL);
  g_signal_connect (score, "timeline-completed",
                    G_CALLBACK (on_timeline_completed),
                    NULL);
  g_signal_connect (score, "completed",
                    G_CALLBACK (on_score_completed),
                    NULL);

  clutter_score_append (score, NULL,       timeline_1);
  clutter_score_append (score, timeline_1, timeline_2);
  clutter_score_append (score, timeline_1, timeline_3);
  clutter_score_append (score, timeline_3, timeline_4);

  clutter_score_append_at_marker (score, timeline_2, "foo", timeline_5);

  timelines = clutter_score_list_timelines (score);
  g_assert (5 == g_slist_length (timelines));
  g_slist_free (timelines);

  clutter_score_start (score);

  clutter_main ();

  g_object_unref (timeline_1);
  g_object_unref (timeline_2);
  g_object_unref (timeline_3);
  g_object_unref (timeline_4);
  g_object_unref (timeline_5);
  g_object_unref (score);

  return EXIT_SUCCESS;
}
