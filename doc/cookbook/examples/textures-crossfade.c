#include <stdlib.h>
#include <clutter/clutter.h>

static gchar *source   = NULL;
static gchar *target   = NULL;
static guint  duration = 1000;

static GOptionEntry entries[] = {
  {
    "source", 's',
    0,
    G_OPTION_ARG_FILENAME, &source,
    "The source image of the cross-fade", "FILE"
  },
  {
    "target", 't',
    0,
    G_OPTION_ARG_FILENAME, &target,
    "The target image of the cross-fade", "FILE"
  },
  {
    "duration", 'd',
    0,
    G_OPTION_ARG_INT, &duration,
    "The duration of the cross-fade, in milliseconds", "MSECS"
  },

  { NULL }
};

static gboolean
start_animation (ClutterActor *actor,
                 ClutterEvent *event,
                 gpointer      user_data)
{
  ClutterState *transitions = CLUTTER_STATE (user_data);
  clutter_state_set_state (transitions, "show-top");
  return TRUE;
}

static gboolean
load_image (ClutterTexture *texture,
            gchar          *image_path)
{
  GError *error = NULL;

  gboolean success = clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                                    image_path,
                                                    &error);

  if (error != NULL)
    {
      g_warning ("Error loading %s\n%s", image_path, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  return success;
}

int
main (int argc, char *argv[])
{
  GError *error = NULL;

  /* UI */
  ClutterActor *stage;
  ClutterLayoutManager *layout;
  ClutterActor *box;
  ClutterActor *top, *bottom;
  ClutterState *transitions;

  if (clutter_init_with_args (&argc, &argv,
                              " - cross-fade", entries,
                              NULL,
                              NULL) != CLUTTER_INIT_SUCCESS)
    return 1;

  if (source == NULL || target == NULL)
    {
      g_print ("Usage: %s -s <source> -t <target> [-d <duration>]\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "cross-fade");
  clutter_actor_set_size (stage, 400, 300);
  clutter_actor_show (stage);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  box = clutter_box_new (layout);
  clutter_actor_set_size (box, 400, 300);

  bottom = clutter_texture_new ();
  top = clutter_texture_new ();

  clutter_container_add_actor (CLUTTER_CONTAINER (box), bottom);
  clutter_container_add_actor (CLUTTER_CONTAINER (box), top);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);

  /* load the first image into the bottom */
  load_image (CLUTTER_TEXTURE (bottom), source);

  /* load the second image into the top */
  load_image (CLUTTER_TEXTURE (top), target);

  /* animations */
  transitions = clutter_state_new ();
  clutter_state_set (transitions, NULL, "show-bottom",
                     top, "opacity", CLUTTER_LINEAR, 0,
                     bottom, "opacity", CLUTTER_LINEAR, 255,
                     NULL);
  clutter_state_set (transitions, NULL, "show-top",
                     top, "opacity", CLUTTER_EASE_IN_CUBIC, 255,
                     bottom, "opacity", CLUTTER_EASE_IN_CUBIC, 0,
                     NULL);
  clutter_state_set_duration (transitions, NULL, NULL, duration);

  /* make the bottom opaque and top transparent */
  clutter_state_warp_to_state (transitions, "show-bottom");

  /* on key press, fade in the top texture and fade out the bottom texture */
  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (start_animation),
                    transitions);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (transitions);

  if (error != NULL)
    g_error_free (error);

  return EXIT_SUCCESS;
}
