/*
 * Display multiple rotated copies of an image on top of each other
 *
 * Invoke with the path to a file to load a custom image
 */
#include <clutter/clutter.h>

#define STAGE_SIDE 512

static const ClutterColor box_color = { 0x33, 0x33, 0x55, 0xff };

int
main (int argc, char *argv[])
{
  ClutterLayoutManager *layout;
  ClutterActor *box;
  ClutterActor *stage;
  ClutterActor *texture;
  CoglHandle *cogl_texture;
  GError *error = NULL;
  gfloat width;

  const gchar *filename = "redhand.png";

  if (argc > 1)
    filename = argv[1];

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_SIDE, STAGE_SIDE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, layout);
  clutter_actor_set_background_color (box, &box_color);

  texture = clutter_texture_new_from_file (filename, &error);

  if (error != NULL)
    g_error ("Error loading file %s; message was:\n%s",
             filename,
             error->message);

  /*
   * get a reference to the underlying Cogl texture
   * for copying onto each Clutter texture placed into the layout
   */
  cogl_texture = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (texture));

  /*
   * add gradually turning and shrinking textures,
   * smallest one last; each actor ends up on top
   * of the one added just before it
   */
  for (width = STAGE_SIDE * 0.75; width >= STAGE_SIDE * 0.0625; width -= STAGE_SIDE * 0.0625)
    {
      ClutterActor *texture_copy = clutter_texture_new ();
      clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (texture_copy),
                                        cogl_texture);
      clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture_copy),
                                             TRUE);
      clutter_actor_set_z_rotation_from_gravity (texture_copy,
                                                 (gfloat)(width * 0.5) - (STAGE_SIDE * 0.03125),
                                                 CLUTTER_GRAVITY_CENTER);
      clutter_actor_set_width (texture_copy, width);
      clutter_actor_add_child (box, texture_copy);
    }

  clutter_actor_add_constraint (box, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_add_child (stage, box);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
