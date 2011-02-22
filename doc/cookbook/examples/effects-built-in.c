#include <clutter/clutter.h>

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *texture;
  ClutterConstraint *constraint_x;
  ClutterConstraint *constraint_y;
  ClutterColor *pink;
  ClutterEffect *effect;
  gchar *filename;

  if (argc < 2)
    {
      g_print ("Usage: %s <path to image file>\n", argv[0]);
      return 1;
    }

  filename = argv[1];

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  texture = clutter_texture_new ();
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture), TRUE);
  clutter_actor_set_width (texture, 300);

  /* NB ignoring missing file errors here for brevity */
  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 filename,
                                 NULL);

  /* align the texture on the x and y axes */
  constraint_x = clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5);
  constraint_y = clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5);
  clutter_actor_add_constraint (texture, constraint_x);
  clutter_actor_add_constraint (texture, constraint_y);

  /* create a colorize effect with pink tint */
  pink = clutter_color_new (230, 187, 210, 255);
  effect = clutter_colorize_effect_new (pink);

  /* apply the effect to the texture */
  clutter_actor_add_effect (texture, effect);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), texture);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
