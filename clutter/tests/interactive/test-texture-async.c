#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

enum
{
  LOAD_SYNC,
  LOAD_DATA_ASYNC,
  LOAD_ASYNC
};

static ClutterActor *stage = NULL;

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
  const gchar *path = user_data;
  ClutterActor *image[3];
  ClutterActor *clone[3];
  gint i;

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
    {
      GError *error = NULL;

      clutter_texture_set_from_file (CLUTTER_TEXTURE (image[i]), path, &error);
      if (error != NULL)
        g_error ("Unable to load image at '%s': %s",
                 path != NULL ? path : "<unknown>",
                 error->message);
    }

  for (i = 0; i < 3; i++)
    clutter_container_add_actor (CLUTTER_CONTAINER (stage), image[i]);

  for (i = 0; i < 3; i++)
    {
      clutter_actor_set_position (image[i], 50 + i * 100, 0 + i * 50);
      clutter_actor_set_depth (image[i], -2500);

      clone[i] = clutter_clone_new (image[i]);
      g_signal_connect (image[i], "size-change",
                        G_CALLBACK (size_change_cb), clone[i]);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), clone[i]);
      clutter_actor_set_position (clone[i], 50 + i * 100, 150 + i * 50 + 100);
    }

  for (i = 0; i < 3; i++)
    {
      clutter_actor_save_easing_state (image[i]);
      clutter_actor_set_easing_duration (image[i], 5000);
      clutter_actor_set_depth (image[i], 0);
      clutter_actor_restore_easing_state (image[i]);
    }

  return FALSE;
}

static void
cleanup_task (gpointer data)
{
}

G_MODULE_EXPORT gint
test_texture_async_main (int argc, char *argv[])
{
  gchar *path;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Asynchronous Texture Loading");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_LightSkyBlue);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  clutter_actor_show (stage);

  path = (argc > 1)
       ? g_strdup (argv[1])
       : g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
 
  clutter_threads_add_timeout_full (G_PRIORITY_DEFAULT, 500,
                                    task, path,
                                    cleanup_task);

  clutter_threads_enter ();
  clutter_main ();
  clutter_threads_leave ();

  g_free (path);

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_texture_async_describe (void)
{
  return "Texture asynchronous loading using threads";
}
