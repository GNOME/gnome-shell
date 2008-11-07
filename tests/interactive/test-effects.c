#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static ClutterEffectTemplate *tmpl = NULL;
static ClutterTimeline *timeline = NULL;

G_MODULE_EXPORT int
test_effects_main (int argc, char *argv[])
{
  ClutterActor *stage, *actor;
  ClutterContainer *container;
  ClutterColor stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterColor rect_color = { 0, 0, 0, 0xdd };

  clutter_init (&argc, &argv);

  timeline = clutter_timeline_new_for_duration (5000);
  clutter_timeline_set_loop (timeline, TRUE);
  tmpl =
    clutter_effect_template_new (timeline, CLUTTER_ALPHA_RAMP_INC);

  stage = clutter_stage_get_default ();
  container = CLUTTER_CONTAINER (stage);
  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit), 
                    NULL);

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_use_fog (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, 800, 600);
  clutter_actor_show_all (stage);

  actor = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (container, actor);
  clutter_actor_set_size (actor, 50, 50);
  clutter_actor_set_position (actor, 50, 10);
  clutter_effect_fade (tmpl, actor, 0x22, NULL, NULL);
  clutter_actor_show (actor);

  actor = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (container, actor);
  clutter_actor_set_size (actor, 50, 50);
  clutter_actor_set_position (actor, 750, 70);
  clutter_effect_depth (tmpl, actor, -500, NULL, NULL);
  clutter_actor_show (actor);

  actor = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (container, actor);
  clutter_actor_set_size (actor, 50, 50);
  clutter_actor_set_position (actor, 50, 140);
  clutter_effect_move (tmpl, actor, 750, 140, NULL, NULL); 
  clutter_actor_show (actor);

  actor = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (container, actor);
  clutter_actor_set_size (actor, 50, 50);
  clutter_actor_set_position (actor, 750, 210);
  {
    ClutterKnot knots[2];

    knots[0].x = 750; knots[0].y = 210;
    knots[1].x = 350; knots[1].y = 210;

    clutter_effect_path (tmpl, actor, knots, 2, NULL, NULL);
  }
  clutter_actor_show (actor);

  actor = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (container, actor);
  clutter_actor_set_size (actor, 50, 50);
  clutter_actor_set_position (actor, 50, 280);
  clutter_actor_set_anchor_point_from_gravity (actor, CLUTTER_GRAVITY_CENTER);
  
  clutter_effect_scale (tmpl, actor, 2.0, 2.0, NULL, NULL);
  clutter_actor_show (actor);

  actor = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (container, actor);
  clutter_actor_set_size (actor, 50, 50);
  clutter_actor_set_position (actor, 750, 350);
  clutter_effect_rotate (tmpl, actor,
                         CLUTTER_Z_AXIS, 180.0,
                         25, 25, 0,
                         CLUTTER_ROTATE_CW,
                         NULL, NULL);
  clutter_actor_show (actor);

  clutter_main ();

  g_object_unref (tmpl);
  g_object_unref (timeline);

  return EXIT_SUCCESS;
}
