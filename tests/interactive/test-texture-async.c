#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

enum
{
  LOAD_SYNC,
  LOAD_DATA_ASYNC,
  LOAD_ASYNC
};

static void
on_load_finished (ClutterTexture *texture,
                  const GError   *error,
                  gpointer        user_data)
{
  gint load_type = GPOINTER_TO_INT (user_data);
  const gchar *load_str = NULL;

  switch (load_type)
    {
    case LOAD_SYNC:
      load_str = "synchronous loading";
      break;

    case LOAD_DATA_ASYNC:
      load_str = "asynchronous data loading";
      break;

    case LOAD_ASYNC:
      load_str = "asynchronous loading";
      break;
    }

  if (error != NULL)
    g_print ("%s failed: %s\n", load_str, error->message);
  else
    g_print ("%s successful\n", load_str);
}

static void
size_change_cb (ClutterTexture *texture,
                gint            width,
                gint            height,
                gpointer        user_data)
{
  clutter_actor_set_size (user_data, width, height);
}

static
gboolean task (gpointer user_data)
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *depth_behavior;
  ClutterActor     *image[4];
  ClutterActor     *clone[4];
  ClutterActor     *stage;
  gchar            *path = user_data;
  gint i;

  stage = clutter_stage_get_default ();

  image[0] = g_object_new (CLUTTER_TYPE_TEXTURE, NULL);
  g_signal_connect (image[0], "load-finished",
                    G_CALLBACK (on_load_finished),
                    GINT_TO_POINTER (LOAD_SYNC));

  image[1] = g_object_new (CLUTTER_TYPE_TEXTURE,
                           "load-data-async", TRUE,
                           NULL);
  g_signal_connect (image[1], "load-finished",
                    G_CALLBACK (on_load_finished),
                    GINT_TO_POINTER (LOAD_DATA_ASYNC));
  image[2] = g_object_new (CLUTTER_TYPE_TEXTURE,
                           "load-async", TRUE,
                           NULL);
  g_signal_connect (image[2], "load-finished",
                    G_CALLBACK (on_load_finished),
                    GINT_TO_POINTER (LOAD_ASYNC));

  for (i = 0; i < 3; i++)
    clutter_texture_set_from_file (CLUTTER_TEXTURE (image[i]), path, NULL);

  for (i = 0; i < 3; i++)
    clutter_container_add (CLUTTER_CONTAINER (stage), image[i], NULL);

  for (i = 0; i < 3; i++)
    {
      clutter_actor_set_position (image[i], 50+i*100, 0+i*50);
      clone[i]=clutter_clone_new (image[i]);
      g_signal_connect (image[i], "size-change",
                        G_CALLBACK (size_change_cb), clone[i]);
      clutter_container_add (CLUTTER_CONTAINER (stage), clone[i], NULL);
      clutter_actor_set_position (clone[i], 50+i*100, 150+i*50+100);
    }

  for (i = 0; i < 3; i++)
    {
      timeline = clutter_timeline_new (5000);
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
  ClutterActor *stage;
  ClutterColor  stage_color = { 0x12, 0x34, 0x56, 0xff };
  GError       *error;
  gchar        *path;

  clutter_init (&argc, &argv);

  g_thread_init (NULL);
  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  clutter_actor_show (stage);
  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  error = NULL;

  path = (argc > 0)
       ? g_strdup (argv[1])
       : g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
 
  g_timeout_add (500, task, path);

  clutter_main ();

  g_free (path);

  /*g_object_unref (depth_behavior);
  g_object_unref (timeline);*/

  return EXIT_SUCCESS;
}
