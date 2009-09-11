#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

G_MODULE_EXPORT int
test_box_main (int argc, char *argv[])
{
  ClutterActor *stage, *box, *rect;
  ClutterLayoutManager *layout;
  ClutterColor bg_color = { 0xcc, 0xcc, 0xcc, 0x99 };
  ClutterColor *color;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Box test");
  clutter_actor_set_size (stage, 320, 200);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  box = clutter_box_new (layout);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);
  clutter_actor_set_anchor_point_from_gravity (box, CLUTTER_GRAVITY_CENTER);
  clutter_actor_set_position (box, 160, 100);

  rect = clutter_rectangle_new_with_color (&bg_color);
  clutter_container_add_actor (CLUTTER_CONTAINER (box), rect);
  clutter_actor_set_size (rect, 100, 100);

  color = clutter_color_new (g_random_int_range (0, 255),
                             g_random_int_range (0, 255),
                             g_random_int_range (0, 255),
                             255);

  rect = clutter_rectangle_new_with_color (color);
  clutter_container_add_actor (CLUTTER_CONTAINER (box), rect);
  clutter_actor_set_size (rect, 50, 50);

  clutter_actor_show_all (stage);

  clutter_main ();

  clutter_color_free (color);

  return EXIT_SUCCESS;
}
