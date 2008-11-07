#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

/* each time the timeline animating the label completes, swap the direction */
static void
timeline_completed (ClutterTimeline *timeline,
                    gpointer         user_data)
{
  clutter_timeline_set_direction (timeline,
                                  !clutter_timeline_get_direction (timeline));
  clutter_timeline_start (timeline);
}

static gboolean
change_filter (gpointer actor)
{
  ClutterTextureQuality old_quality;

  old_quality = clutter_texture_get_filter_quality (actor);
  switch (old_quality)
    {
      case CLUTTER_TEXTURE_QUALITY_LOW:
        clutter_texture_set_filter_quality (actor,
           CLUTTER_TEXTURE_QUALITY_MEDIUM);
        g_print ("Setting texture rendering quality to medium\n");
        break;
      case CLUTTER_TEXTURE_QUALITY_MEDIUM:
        clutter_texture_set_filter_quality (actor,
           CLUTTER_TEXTURE_QUALITY_HIGH);
        g_print ("Setting texture rendering quality to high\n");
        break;
      case CLUTTER_TEXTURE_QUALITY_HIGH:
        clutter_texture_set_filter_quality (actor,
           CLUTTER_TEXTURE_QUALITY_LOW);
        g_print ("Setting texture rendering quality to low\n");
        break;
    }
  return TRUE;
}

G_MODULE_EXPORT gint
test_texture_quality_main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterBehaviour *depth_behavior;
  ClutterActor     *stage;
  ClutterActor     *image;
  ClutterColor      stage_color = { 0x12, 0x34, 0x56, 0xff };
  GError           *error;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_use_fog (CLUTTER_STAGE (stage), TRUE);
  clutter_stage_set_fog (CLUTTER_STAGE (stage), 1.0, 10, -50);

  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  error = NULL;
  image = clutter_texture_new_from_file (argv[1]?argv[1]:"redhand.png", &error);
  if (error)
    g_error ("Unable to load image: %s", error->message);

  if (!argv[1])
    g_print ("Hint: the redhand.png isn't a good test image for this test.\n"
             "This test can take any clutter loadable image as an argument\n");

  /* center the image */
  clutter_actor_set_position (image, 
    (clutter_actor_get_width (stage) - clutter_actor_get_width (image))/2,
    (clutter_actor_get_height (stage) - clutter_actor_get_height (image))/2);
  clutter_container_add (CLUTTER_CONTAINER (stage), image, NULL);

  timeline = clutter_timeline_new (60*5, 60);
  g_signal_connect (timeline,
                    "completed", G_CALLBACK (timeline_completed),
                    NULL);

  depth_behavior = clutter_behaviour_depth_new (
       clutter_alpha_new_full (timeline, CLUTTER_ALPHA_RAMP_INC, NULL, NULL),
       -2500, 400);

  clutter_behaviour_apply (depth_behavior, image);

  clutter_actor_show (stage);
  clutter_timeline_start (timeline);

  g_timeout_add (10000, change_filter, image);

  clutter_main ();

  g_object_unref (depth_behavior);
  g_object_unref (timeline);

  return EXIT_SUCCESS;
}
