#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor rectangle_color = { 0xaa, 0x99, 0x00, 0xff };

int
main (int argc, char *argv[])
{
  /* the stage is the "source" for constraints on the texture */
  ClutterActor *stage;

  /* the "target" actor which will be bound by the constraints */
  ClutterActor *texture;

  ClutterConstraint *width_binding;
  ClutterConstraint *height_binding;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* make the stage resizable */
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  texture = clutter_texture_new ();
  clutter_actor_set_opacity (texture, 50);
  clutter_texture_set_repeat (CLUTTER_TEXTURE (texture), TRUE, TRUE);
  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture), "smiley.png", NULL);

  /* the texture's width will be 100px less than the stage's */
  width_binding = clutter_bind_constraint_new (stage, CLUTTER_BIND_WIDTH, -100);

  /* the texture's height will be 100px less than the stage's */
  height_binding = clutter_bind_constraint_new (stage, CLUTTER_BIND_HEIGHT, -100);

  /* add the constraints to the texture */
  clutter_actor_add_constraint (texture, width_binding);
  clutter_actor_add_constraint (texture, height_binding);

  /* add some alignment constraints */
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), texture);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
