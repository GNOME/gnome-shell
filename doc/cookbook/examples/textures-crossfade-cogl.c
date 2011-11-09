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

static void
_update_progress_cb (ClutterTimeline *timeline,
                     guint            elapsed_msecs,
                     ClutterTexture  *texture)
{
  CoglHandle copy;
  gdouble progress;
  CoglColor constant;

  CoglHandle material = clutter_texture_get_cogl_material (texture);

  if (material == COGL_INVALID_HANDLE)
    return;

  /* You should assume that a material can only be modified once, after
   * its creation; if you need to modify it later you should use a copy
   * instead. Cogl makes copying materials reasonably cheap
   */
  copy = cogl_material_copy (material);

  progress = clutter_timeline_get_progress (timeline);

  /* Create the constant color to be used when combining the two
   * material layers; we use a black color with an alpha component
   * depending on the current progress of the timeline
   */
  cogl_color_init_from_4ub (&constant, 0x00, 0x00, 0x00, 0xff * progress);

  /* This sets the value of the constant color we use when combining
   * the two layers
   */
  cogl_material_set_layer_combine_constant (copy, 1, &constant);

  /* The Texture now owns the material */
  clutter_texture_set_cogl_material (texture, copy);
  cogl_handle_unref (copy);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));
}

static CoglHandle
load_cogl_texture (const char *type,
                   const char *file)
{
  GError *error = NULL;

  CoglHandle retval = cogl_texture_new_from_file (file,
                                                  COGL_TEXTURE_NO_SLICING,
                                                  COGL_PIXEL_FORMAT_ANY,
                                                  &error);
  if (error != NULL)
    {
      g_print ("Unable to load %s image: %s\n", type, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  return retval;
}

static int
print_usage_and_exit (const char *exec_name,
                      int         exit_code)
{
  g_print ("Usage: %s -s <source> -t <target> [-d <duration>]\n", exec_name);
  return exit_code;
}

int
main (int argc, char *argv[])
{
  CoglHandle texture_1;
  CoglHandle texture_2;
  CoglHandle material;
  ClutterActor *stage;
  ClutterActor *texture;
  ClutterTimeline *timeline;

  if (clutter_init_with_args (&argc, &argv,
                              " - Crossfade", entries,
                              NULL,
                              NULL) != CLUTTER_INIT_SUCCESS)
    return 1;

  if (source == NULL || target == NULL)
    return print_usage_and_exit (argv[0], EXIT_FAILURE);

  /* Load the source and target images using Cogl, because we need
   * to combine them into the same ClutterTexture.
   */
  texture_1 = load_cogl_texture ("source", source);
  texture_2 = load_cogl_texture ("target", target);

  /* Create a new Cogl material holding the two textures inside two
   * separate layers.
   */
  material = cogl_material_new ();
  cogl_material_set_layer (material, 1, texture_1);
  cogl_material_set_layer (material, 0, texture_2);

  /* Set the layer combination description for the second layer; the
   * default for Cogl is to simply multiply the layer with the
   * precendent one. In this case we interpolate the color for each
   * pixel between the pixel value of the previous layer and the
   * current one, using the alpha component of a constant color as
   * the interpolation factor.
   */
  cogl_material_set_layer_combine (material, 1,
                                   "RGBA = INTERPOLATE (PREVIOUS, "
                                                       "TEXTURE, "
                                                       "CONSTANT[A])",
                                   NULL);

  /* The material now owns the two textures */
  cogl_handle_unref (texture_1);
  cogl_handle_unref (texture_2);

  /* Create a Texture and place it in the middle of the stage; then
   * assign the material we created earlier to the Texture for painting
   * it
   */
  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "cross-fade");
  clutter_actor_set_size (stage, 400, 300);
  clutter_actor_show (stage);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  texture = clutter_texture_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), texture);
  clutter_texture_set_cogl_material (CLUTTER_TEXTURE (texture), material);
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  cogl_handle_unref (material);

  /* The timeline will drive the cross-fading */
  timeline = clutter_timeline_new (duration);
  g_signal_connect (timeline, "new-frame", G_CALLBACK (_update_progress_cb), texture);
  clutter_timeline_start (timeline);

  clutter_main ();

  g_object_unref (timeline);

  return EXIT_SUCCESS;
}
