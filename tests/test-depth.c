#include <stdlib.h>
#include <clutter/clutter.h>

static gboolean zoom_in = TRUE;
static ClutterBehaviour *d_behave = NULL;

static void
timeline_completed (ClutterTimeline *timeline,
                    gpointer         user_data)
{
  gint depth_start, depth_end;

  if (zoom_in)
    {
      depth_start = 0;
      depth_end = -500;
      zoom_in = FALSE;
    }
  else
    {
      depth_start = -500;
      depth_end = 0;
      zoom_in = TRUE;
    }

  clutter_behaviour_depth_set_bounds (CLUTTER_BEHAVIOUR_DEPTH (d_behave),
                                      depth_start, depth_end);

  clutter_timeline_rewind (timeline);
  clutter_timeline_start (timeline);
}

int
main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  ClutterActor *stage;
  ClutterActor *hand, *label, *rect;
  ClutterColor stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterColor rect_color = { 0, 0, 0, 0x88 };
  GdkPixbuf *pixbuf;
  GError *error;

  clutter_init (&argc, &argv);

  error = NULL;
  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", &error);
  if (error)
    g_error ("Unable to load redhand.png: %s", error->message);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_use_fog (CLUTTER_STAGE (stage), TRUE);

  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  label = clutter_label_new_with_text ("Mono 26", "Clutter");
  clutter_actor_set_position (label, 40, 140);
  clutter_actor_show (label);

  hand = clutter_texture_new_from_pixbuf (pixbuf);
  clutter_actor_set_position (hand, 240, 140);
  clutter_actor_show (hand);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_position (rect, 440, 140);
  clutter_actor_set_size (rect, 200, 200);
  clutter_actor_show (rect);

  clutter_container_add (CLUTTER_CONTAINER (stage), hand, label, rect, NULL);

  /* 3 seconds, at 60 fps */
  timeline = clutter_timeline_new (180, 60);
  g_signal_connect (timeline,
                    "completed", G_CALLBACK (timeline_completed),
                    NULL);

  d_behave = clutter_behaviour_depth_new (clutter_alpha_new_full (timeline,
                                                                  CLUTTER_ALPHA_RAMP_INC,
                                                                  NULL, NULL),
                                          -500, 0);
  clutter_behaviour_apply (d_behave, hand);
  clutter_behaviour_apply (d_behave, label);
  clutter_behaviour_apply (d_behave, rect);

  clutter_actor_show (stage);

  clutter_timeline_start (timeline);

  clutter_main ();

  g_object_unref (d_behave);
  g_object_unref (timeline);

  return EXIT_SUCCESS;
}
