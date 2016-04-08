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
  ClutterColor     rect_color = { 0xff, 0xff, 0xff, 0x99 };
  ClutterTimeline *timeline;
  ClutterAlpha    *alpha;
  ClutterBehaviour *behave;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Scaling");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Black);
  clutter_actor_set_size (stage, 300, 300);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_size (rect, 100, 100);
  clutter_actor_set_position (rect, 100, 100);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  label = clutter_text_new_with_text ("Sans 20px", "");
  clutter_text_set_color (CLUTTER_TEXT (label), CLUTTER_COLOR_White);
  clutter_actor_set_position (label,
                              clutter_actor_get_x (rect),
                              clutter_actor_get_y (rect)
                              + clutter_actor_get_height (rect));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);

  rect_color.alpha = 0xff;
  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_position (rect, 100, 100);
  clutter_actor_set_size (rect, 100, 100);
  set_next_gravity (rect);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  timeline = clutter_timeline_new (750);
  alpha    = clutter_alpha_new_with_func (timeline,
				          my_ramp_func,
				          NULL, NULL);

  behave = clutter_behaviour_scale_new (alpha,
					0.0, 0.0,  /* scale start */
					1.0, 1.0); /* scale end */

  clutter_behaviour_apply (behave, rect);

  clutter_timeline_set_repeat_count (timeline, -1);
  g_signal_connect_swapped (timeline, "completed",
                            G_CALLBACK (set_next_gravity), rect);
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  g_object_unref (timeline);
  g_object_unref (behave);

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_scale_describe (void)
{
  return "Scaling animation and scaling center changes";
}
