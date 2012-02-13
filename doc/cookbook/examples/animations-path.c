#include <stdlib.h>
#include <clutter/clutter.h>

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterPath *path;
  ClutterConstraint *constraint;
  ClutterActor *rectangle;
  ClutterTimeline *timeline;

  const ClutterColor *stage_color = clutter_color_new (51, 51, 85, 255);
  const ClutterColor *red_color = clutter_color_new (255, 0, 0, 255);

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 360, 300);
  clutter_stage_set_color (CLUTTER_STAGE (stage), stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* create the path */
  path = clutter_path_new ();
  clutter_path_add_move_to (path, 30, 60);

  /* add a curve round to the top-right of the stage */
  clutter_path_add_rel_curve_to (path,
                                 120, 180,
                                 180, 120,
                                 240, 0);

  /* create a constraint based on the path */
  constraint = clutter_path_constraint_new (path, 0.0);

  /* put a rectangle at the start of the path */
  rectangle = clutter_rectangle_new_with_color (red_color);
  clutter_actor_set_size (rectangle, 60, 60);

  /* add the constraint to the rectangle */
  clutter_actor_add_constraint_with_name (rectangle, "path", constraint);

  /* add the rectangle to the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rectangle);

  /* set up the timeline */
  timeline = clutter_timeline_new (1000);
  clutter_timeline_set_repeat_count (timeline, -1);
  clutter_timeline_set_auto_reverse (timeline, TRUE);

  clutter_actor_animate_with_timeline (rectangle, CLUTTER_LINEAR, timeline,
                                       "@constraints.path.offset", 1.0,
                                       NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
