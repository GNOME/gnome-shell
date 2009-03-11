#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

G_MODULE_EXPORT gint
test_texture_async_main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *depth_behavior;
  ClutterActor     *stage;
  ClutterActor     *image[4];
  ClutterColor      stage_color = { 0x12, 0x34, 0x56, 0xff };
  GError           *error;
  const gchar      *path = "redhand.png";
  gint i;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_use_fog (CLUTTER_STAGE (stage), TRUE);
  clutter_stage_set_fog (CLUTTER_STAGE (stage), 1.0, 10, -50);

  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  error = NULL;
  path = argv[1]?argv[1]:"redhand.png";

  if (!argv[1])
    g_print ("Hint: the redhand.png isn't a good test image for this test.\n"
             "This test can take any clutter loadable image as an argument\n");

  for (i=0;i<4;i++)
    image[i] = g_object_new (CLUTTER_TYPE_TEXTURE,
                             "filename", path,
                             NULL);
  /*
  image[1] = g_object_new (CLUTTER_TYPE_TEXTURE,
                           "filename", path,
                           "load-async", TRUE,
                           NULL);
  image[2] = g_object_new (CLUTTER_TYPE_TEXTURE,
                           "filename", path,
                           "load-data-async", TRUE,
                           NULL);
  image[3] = g_object_new (CLUTTER_TYPE_TEXTURE,
                           "filename", path,
                           "load-data-async", TRUE,
                           "load-size-async", TRUE,
                           NULL);*/

  /* center the image */

  for (i=0;i<4;i++)
    {
      clutter_actor_set_position (image[i], 50+i*100, 50+i*50);
      clutter_container_add (CLUTTER_CONTAINER (stage), image[i], NULL);
      timeline = clutter_timeline_new (60*5, 60);
      alpha = clutter_alpha_new_full (timeline, CLUTTER_LINEAR);
      depth_behavior = clutter_behaviour_depth_new (alpha, -2500, 0);
      clutter_behaviour_apply (depth_behavior, image[i]);
      clutter_timeline_start (timeline);
    }
 
  clutter_actor_show (stage);

  clutter_main ();

  /*g_object_unref (depth_behavior);
  g_object_unref (timeline);*/

  return EXIT_SUCCESS;
}
