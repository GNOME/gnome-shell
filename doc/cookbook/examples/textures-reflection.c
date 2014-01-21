#include <stdlib.h>
#include <clutter/clutter.h>

/* pixels between the source and its reflection */
#define V_PADDING       4

static void
_clone_paint_cb (ClutterActor *actor)
{
  ClutterActor *source;
  ClutterActorBox box;
  CoglHandle material;
  gfloat width, height;
  guint8 opacity;
  CoglColor color_1, color_2;
  CoglTextureVertex vertices[4];

  /* if we don't have a source actor, don't paint */
  source = clutter_clone_get_source (CLUTTER_CLONE (actor));
  if (source == NULL)
    goto out;

  /* if the source texture does not have any content, don't paint */
  material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (source));
  if (material == NULL)
    goto out;

  /* get the size of the reflection */
  clutter_actor_get_allocation_box (actor, &box);
  clutter_actor_box_get_size (&box, &width, &height);

  /* get the composite opacity of the actor */
  opacity = clutter_actor_get_paint_opacity (actor);

  /* figure out the two colors for the reflection: the first is
   * full color and the second is the same, but at 0 opacity
   */
  cogl_color_init_from_4f (&color_1, 1.0, 1.0, 1.0, opacity / 255.);
  cogl_color_premultiply (&color_1);
  cogl_color_init_from_4f (&color_2, 1.0, 1.0, 1.0, 0.0);
  cogl_color_premultiply (&color_2);

  /* now describe the four vertices of the quad; since it has
   * to be a reflection, we need to invert it as well
   */
  vertices[0].x = 0; vertices[0].y = 0; vertices[0].z = 0;
  vertices[0].tx = 0.0; vertices[0].ty = 1.0;
  vertices[0].color = color_1;

  vertices[1].x = width; vertices[1].y = 0; vertices[1].z = 0;
  vertices[1].tx = 1.0; vertices[1].ty = 1.0;
  vertices[1].color = color_1;

  vertices[2].x = width; vertices[2].y = height; vertices[2].z = 0;
  vertices[2].tx = 1.0; vertices[2].ty = 0.0;
  vertices[2].color = color_2;

  vertices[3].x = 0; vertices[3].y = height; vertices[3].z = 0;
  vertices[3].tx = 0.0; vertices[3].ty = 0.0;
  vertices[3].color = color_2;

  /* paint the same texture but with a different geometry */
  cogl_set_source (material);
  cogl_polygon (vertices, 4, TRUE);

out:
  /* prevent the default clone handler from running */
  g_signal_stop_emission_by_name (actor, "paint");
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *texture;
  GError *error = NULL;
  ClutterActor *clone;
  gfloat y_offset;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Reflection");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  texture = clutter_texture_new ();
  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 "redhand.png",
                                 &error);
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.2));

  y_offset = clutter_actor_get_height (texture) + V_PADDING;

  clone = clutter_clone_new (texture);
  clutter_actor_add_constraint (clone, clutter_bind_constraint_new (texture, CLUTTER_BIND_X, 0.0));
  clutter_actor_add_constraint (clone, clutter_bind_constraint_new (texture, CLUTTER_BIND_Y, y_offset));
  g_signal_connect (clone,
                    "paint",
                    G_CALLBACK (_clone_paint_cb),
                    NULL);

  clutter_container_add (CLUTTER_CONTAINER (stage), texture, clone, NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
