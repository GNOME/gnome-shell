#include <stdlib.h>
#include <clutter/clutter.h>

typedef struct
{
  gchar *axis;
  gfloat target;
} AnimationSpec;

static gboolean
button_pressed_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  AnimationSpec *animation_spec = user_data;
  ClutterTransition *transition;

  if (clutter_actor_get_transition (actor, animation_spec->axis) != NULL)
    return TRUE;

  clutter_actor_save_easing_state (actor);
  clutter_actor_set_easing_duration (actor, 500);

  g_object_set (actor, animation_spec->axis, animation_spec->target, NULL);
  transition = clutter_actor_get_transition (actor, animation_spec->axis);
  clutter_timeline_set_auto_reverse (CLUTTER_TIMELINE (transition), TRUE);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), 1);

  clutter_actor_restore_easing_state (actor);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *rectangle1;
  ClutterActor *rectangle2;
  ClutterActor *rectangle3;

  AnimationSpec x_move = { "x", 50.0 };
  AnimationSpec y_move = { "y", 400.0 };
  AnimationSpec z_move = { "depth", -1000.0 };

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 500, 500);
  clutter_stage_set_color (CLUTTER_STAGE (stage), CLUTTER_COLOR_Aluminium2);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  rectangle1 = clutter_actor_new ();
  clutter_actor_set_background_color (rectangle1, CLUTTER_COLOR_ScarletRed);
  clutter_actor_set_reactive (rectangle1, TRUE);
  clutter_actor_set_size (rectangle1, 50, 50);
  clutter_actor_set_position (rectangle1, 400, 400);
  clutter_actor_add_child (stage, rectangle1);

  rectangle2 = clutter_actor_new ();
  clutter_actor_set_background_color (rectangle2, CLUTTER_COLOR_Chameleon);
  clutter_actor_set_reactive (rectangle2, TRUE);
  clutter_actor_set_size (rectangle2, 50, 50);
  clutter_actor_set_position (rectangle2, 50, 50);
  clutter_actor_add_child (stage, rectangle2);

  rectangle3 = clutter_actor_new ();
  clutter_actor_set_background_color (rectangle3, CLUTTER_COLOR_SkyBlue);
  clutter_actor_set_reactive (rectangle3, TRUE);
  clutter_actor_set_size (rectangle3, 50, 50);
  clutter_actor_set_position (rectangle3, 225, 225);
  clutter_actor_add_child (stage, rectangle3);

  g_signal_connect (rectangle1,
                    "button-press-event",
                    G_CALLBACK (button_pressed_cb),
                    &x_move);

  g_signal_connect (rectangle2,
                    "button-press-event",
                    G_CALLBACK (button_pressed_cb),
                    &y_move);

  g_signal_connect (rectangle3,
                    "button-press-event",
                    G_CALLBACK (button_pressed_cb),
                    &z_move);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
