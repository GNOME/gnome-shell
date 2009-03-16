#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>


static void size_change_cb (ClutterTexture *texture,
                            gint            width,
                            gint            height,
                            gpointer        user_data)
{
  guint w,h;
  clutter_actor_set_size (user_data, width, height);
}

  const gchar      *path = "redhand.png";

static gboolean task (gpointer foo)
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *depth_behavior;
  ClutterActor     *image[4];
  ClutterActor     *clone[4];
  ClutterActor     *stage;
  gint i;

  stage = clutter_stage_get_default ();
#if 0
  for (i=0;i<4;i++)
    image[i] = g_object_new (CLUTTER_TYPE_TEXTURE,
                             "filename", path,
                           "load-async", TRUE,
                             NULL);
#else
  /*for (i=0;i<4;i++)*/
    image[0] = g_object_new (CLUTTER_TYPE_TEXTURE,
                             "filename", path,
                             NULL);
  image[1] = g_object_new (CLUTTER_TYPE_TEXTURE,
                           "filename", path,
                           "load-data-async", TRUE,
                           NULL);
  image[2] = g_object_new (CLUTTER_TYPE_TEXTURE,
                           "filename", path,
                           "load-async", TRUE,
                           NULL);

#endif
  for (i=0;i<3;i++)
    {
      clutter_container_add (CLUTTER_CONTAINER (stage), image[i], NULL);
    }
  for (i=0;i<3;i++)
    {
      clutter_actor_set_position (image[i], 50+i*100, 0+i*50);
      clone[i]=clutter_clone_new (image[i]);
      g_signal_connect (image[i], "size-change",
                        G_CALLBACK (size_change_cb), clone[i]);
      clutter_container_add (CLUTTER_CONTAINER (stage), clone[i], NULL);
      clutter_actor_set_position (clone[i], 50+i*100, 150+i*50+100);
    }

  for (i=0; i<3; i++)
    {
      timeline = clutter_timeline_new (60*5, 60);
      alpha = clutter_alpha_new_full (timeline, CLUTTER_LINEAR);
      depth_behavior = clutter_behaviour_depth_new (alpha, -2500, 0);
      clutter_behaviour_apply (depth_behavior, image[i]);
      clutter_timeline_start (timeline);
    }
  return FALSE;
}


G_MODULE_EXPORT gint
test_texture_async_main (int argc, char *argv[])
{
  ClutterActor     *stage;
  ClutterColor      stage_color = { 0x12, 0x34, 0x56, 0xff };
  GError           *error;

  clutter_init (&argc, &argv);

  g_thread_init (NULL);
  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  clutter_actor_show (stage);
  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  error = NULL;

  path = argv[1]?argv[1]:"redhand.png";

 
  g_timeout_add (500, task, NULL);

  clutter_main ();

  /*g_object_unref (depth_behavior);
  g_object_unref (timeline);*/

  return EXIT_SUCCESS;
}
