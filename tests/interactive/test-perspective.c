#include <gmodule.h>
#include <clutter/clutter.h>

G_MODULE_EXPORT int
test_perspective_main (int argc, char *argv[])
{
  ClutterActor    *rect;
  ClutterActor    *stage;
  ClutterColor     red = {0xff, 0, 0, 0xff}, white = {0xff, 0xff, 0xff, 0xff};

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  g_object_set (stage, "fullscreen", TRUE, NULL);

  clutter_stage_set_color (CLUTTER_STAGE (stage), &red);
  
  rect = clutter_rectangle_new_with_color (&white);
  clutter_actor_set_size (rect, 
			  clutter_actor_get_width(stage), 
			  clutter_actor_get_height(stage));
  clutter_actor_set_position (rect, 0, 0);
  clutter_group_add (CLUTTER_GROUP (stage), rect);

  rect = clutter_rectangle_new_with_color (&red);
  clutter_actor_set_size (rect, 2, 2);
  clutter_actor_set_position (rect, 1, 1);
  clutter_group_add (CLUTTER_GROUP (stage), rect);

  rect = clutter_rectangle_new_with_color (&red);
  clutter_actor_set_size (rect, 2, 2);
  clutter_actor_set_position (rect, clutter_actor_get_width(stage)-3, 1);
  clutter_group_add (CLUTTER_GROUP (stage), rect);

  rect = clutter_rectangle_new_with_color (&red);
  clutter_actor_set_size (rect, 2, 2);
  clutter_actor_set_position (rect, 1, clutter_actor_get_height(stage)-3);
  clutter_group_add (CLUTTER_GROUP (stage), rect);

  rect = clutter_rectangle_new_with_color (&red);
  clutter_actor_set_size (rect, 2, 2);
  clutter_actor_set_position (rect, 
			      clutter_actor_get_width(stage)-3, 
			      clutter_actor_get_height(stage)-3);
  clutter_group_add (CLUTTER_GROUP (stage), rect);

  clutter_actor_show_all (CLUTTER_ACTOR (stage));

  clutter_main ();

  return 0;
}
