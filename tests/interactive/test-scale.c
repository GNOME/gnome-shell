#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static const ClutterGravity gravities[] = {
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

static gint gindex = 0;
static ClutterActor *label;

static void
set_next_gravity (ClutterActor *actor)
{
  ClutterGravity gravity = gravities[gindex];
  GEnumClass *eclass;
  GEnumValue *evalue;

  clutter_actor_move_anchor_point_from_gravity (actor, gravities[gindex]);

  eclass = g_type_class_ref (CLUTTER_TYPE_GRAVITY);
  evalue = g_enum_get_value (eclass, gravity);
  clutter_text_set_text (CLUTTER_TEXT (label), evalue->value_nick);
  g_type_class_unref (eclass);

  if (++gindex >= G_N_ELEMENTS (gravities))
    gindex = 0;
}

static gdouble
my_ramp_func (ClutterAlpha *alpha,
              gpointer      unused)
{
  ClutterTimeline *timeline = clutter_alpha_get_timeline (alpha);

  return clutter_timeline_get_progress (timeline);
}

G_MODULE_EXPORT int
test_scale_main (int argc, char *argv[])
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

  label = clutter_text_new_with_text ("Sans 20px", "");
  clutter_text_set_color (CLUTTER_TEXT (label),
                           &(ClutterColor) { 0xff, 0xff, 0xff, 0xff });
  clutter_actor_set_position (label,
                              clutter_actor_get_x (rect),
                              clutter_actor_get_y (rect)
                              + clutter_actor_get_height (rect));

  clutter_group_add (CLUTTER_GROUP (stage), label);

  rect_color.alpha = 0xff;
  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_position (rect, 100, 100);
  clutter_actor_set_size (rect, 100, 100);
  set_next_gravity (rect);

  clutter_group_add (CLUTTER_GROUP (stage), rect);

  timeline = clutter_timeline_new_for_duration (750);
  alpha    = clutter_alpha_new_with_func (timeline,
				          my_ramp_func,
				          NULL, NULL);

  behave = clutter_behaviour_scale_new (alpha,
					0.0, 0.0,  /* scale start */
					1.0, 1.0); /* scale end */

  clutter_behaviour_apply (behave, rect);

  clutter_timeline_set_loop (timeline, TRUE);
  g_signal_connect_swapped (timeline, "completed",
                            G_CALLBACK (set_next_gravity), rect);
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  g_object_unref (timeline);
  g_object_unref (behave);

  return EXIT_SUCCESS;
}
