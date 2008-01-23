#include <stdlib.h>
#include <clutter/clutter.h>

ClutterGravity gravities[] = {
  CLUTTER_GRAVITY_NORTH_EAST,
  CLUTTER_GRAVITY_NORTH,
  CLUTTER_GRAVITY_NORTH_WEST,
  CLUTTER_GRAVITY_WEST,
  CLUTTER_GRAVITY_SOUTH_WEST,
  CLUTTER_GRAVITY_SOUTH,
  CLUTTER_GRAVITY_SOUTH_EAST,
  CLUTTER_GRAVITY_EAST,
  CLUTTER_GRAVITY_CENTER,
  CLUTTER_GRAVITY_NONE
};

gint gindex = 0;

void
on_timeline_completed (ClutterTimeline *cluttertimeline,
		       gpointer         data)
{
  ClutterBehaviourScale *behave = CLUTTER_BEHAVIOUR_SCALE(data);

  if (++gindex >= G_N_ELEMENTS (gravities))
    gindex = 0;

  g_object_set (behave, "scale-gravity", gravities[gindex], NULL);
}

int
main (int argc, char *argv[])
{
  ClutterActor    *stage, *rect;
  ClutterColor     stage_color = { 0x0, 0x0, 0x0, 0xff };
  ClutterColor     rect_color = { 0xff, 0xff, 0xff, 0x99 };
  ClutterTimeline *timeline;
  ClutterAlpha    *alpha;
  ClutterBehaviour *behave;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage, 300, 300);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_size (rect, 100, 100);
  clutter_actor_set_position (rect, 100, 100);

  clutter_group_add (CLUTTER_GROUP (stage), rect);

  rect_color.alpha = 0xff;
  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_anchor_point_from_gravity (rect, CLUTTER_GRAVITY_CENTER);
  clutter_actor_set_size (rect, 100, 100);
  clutter_actor_set_position (rect, 150, 150);

  clutter_group_add (CLUTTER_GROUP (stage), rect);

  timeline = clutter_timeline_new (20, 30);
  alpha    = clutter_alpha_new_full (timeline,
				     CLUTTER_ALPHA_RAMP,
				     NULL, NULL);

  behave = clutter_behaviour_scale_new (alpha,
					0.0, 0.0, /* scale start */
					1.5, 1.5, /* scale end */
					gravities[gindex]);

  clutter_behaviour_apply (behave, rect); 

  clutter_timeline_set_loop (timeline, TRUE);
  g_signal_connect (timeline, "completed", 
		    G_CALLBACK(on_timeline_completed), behave);
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  return EXIT_SUCCESS;
}
