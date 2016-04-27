#include <stdlib.h>
#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *layer_a, *layer_b, *layer_c;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  /* the main container */
  stage = clutter_stage_new ();
  clutter_actor_set_name (stage, "stage");
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Snap Constraint");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Aluminium1);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* first layer, with a fixed (100, 25) size */
  layer_a = clutter_actor_new ();
  clutter_actor_set_background_color (layer_a, CLUTTER_COLOR_ScarletRed);
  clutter_actor_set_name (layer_a, "layerA");
  clutter_actor_set_size (layer_a, 100.0, 25.0);
  clutter_actor_add_child (stage, layer_a);

  /* the first layer is anchored to the middle of the stage */
  clutter_actor_add_constraint (layer_a, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));

  /* second layer, with no implicit size */
  layer_b = clutter_actor_new ();
  clutter_actor_set_background_color (layer_b, CLUTTER_COLOR_DarkButter);
  clutter_actor_set_name (layer_b, "layerB");
  clutter_actor_add_child (stage, layer_b);

  /* the second layer tracks the X coordinate and the width of
   * the first layer
   */
  clutter_actor_add_constraint (layer_b, clutter_bind_constraint_new (layer_a, CLUTTER_BIND_X, 0.0));
  clutter_actor_add_constraint (layer_b, clutter_bind_constraint_new (layer_a, CLUTTER_BIND_WIDTH, 0.0));

  /* the second layer is snapped between the bottom edge of
   * the first layer, and the bottom edge of the stage; a
   * spacing of 10 pixels in each direction is added for padding
   */
  clutter_actor_add_constraint (layer_b,
                                clutter_snap_constraint_new (layer_a,
                                                             CLUTTER_SNAP_EDGE_TOP,
                                                             CLUTTER_SNAP_EDGE_BOTTOM,
                                                             10.0));

  clutter_actor_add_constraint (layer_b,
                                clutter_snap_constraint_new (stage,
                                                             CLUTTER_SNAP_EDGE_BOTTOM,
                                                             CLUTTER_SNAP_EDGE_BOTTOM,
                                                             -10.0));

  /* the third layer, with no implicit size */
  layer_c = clutter_actor_new ();
  clutter_actor_set_background_color (layer_c, CLUTTER_COLOR_LightChameleon);
  clutter_actor_set_name (layer_c, "layerC");
  clutter_actor_add_child (stage, layer_c);

  /* as for the second layer, the third layer tracks the X
   * coordinate and width of the first layer
   */
  clutter_actor_add_constraint (layer_c, clutter_bind_constraint_new (layer_a, CLUTTER_BIND_X, 0.0));
  clutter_actor_add_constraint (layer_c, clutter_bind_constraint_new (layer_a, CLUTTER_BIND_WIDTH, 0.0));

  /* the third layer is snapped between the top edge of the stage
   * and the top edge of the first layer; again, a spacing of
   * 10 pixels in each direction is added for padding
   */
  clutter_actor_add_constraint (layer_c,
                                clutter_snap_constraint_new (layer_a,
                                                             CLUTTER_SNAP_EDGE_BOTTOM,
                                                             CLUTTER_SNAP_EDGE_TOP,
                                                             -10.0));
  clutter_actor_add_constraint (layer_c,
                                clutter_snap_constraint_new (stage,
                                                             CLUTTER_SNAP_EDGE_TOP,
                                                             CLUTTER_SNAP_EDGE_TOP,
                                                             10.0));

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
