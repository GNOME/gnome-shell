#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };

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
  clutter_init_with_args (&argc, &argv,
                          " - cross-fade", entries,
                          NULL,
                          NULL);

  if (source == NULL || target == NULL)
    {
      g_print ("Usage: %s -s <source> -t <target> [-d <duration>]\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  GError *error = NULL;

  /* UI */
  ClutterActor *stage;
  ClutterLayoutManager *layout;
  ClutterActor *box;
  ClutterActor *front, *back;
  ClutterState *transitions;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "cross-fade");
  clutter_actor_set_size (stage, 600, 600);
  clutter_actor_show (stage);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  box = clutter_box_new (layout);
  clutter_actor_set_size (box, 600, 600);

  back = clutter_texture_new ();
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (back), TRUE);

  front = clutter_texture_new ();
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (front), TRUE);

  clutter_container_add_actor (CLUTTER_CONTAINER (box), back);
  clutter_container_add_actor (CLUTTER_CONTAINER (box), front);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);

  /* load the first image into the back */
  load_image (CLUTTER_TEXTURE (back), source);

  /* load the second image into the front */
  load_image (CLUTTER_TEXTURE (front), target);

  /* animations */
  transitions = clutter_state_new ();
  clutter_state_set (transitions, NULL, "show-back",
                     front, "opacity", CLUTTER_LINEAR, 0,
                     back, "opacity", CLUTTER_LINEAR, 255,
                     NULL);
  clutter_state_set (transitions, NULL, "show-front",
                     front, "opacity", CLUTTER_EASE_IN_CUBIC, 255,
                     back, "opacity", CLUTTER_EASE_IN_CUBIC, 0,
                     NULL);
  clutter_state_set_duration (transitions, NULL, NULL, duration);

  /* make the back opaque and front transparent */
  clutter_state_warp_to_state (transitions, "show-back");

  /* fade in the front texture and fade out the back texture */
  clutter_state_set_state (transitions, "show-front");

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (transitions);

  if (error != NULL)
    g_error_free (error);

  return EXIT_SUCCESS;
}
