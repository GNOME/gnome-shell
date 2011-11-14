#include <stdlib.h>
#include <clutter/clutter.h>

typedef struct
{
  gchar *axis;
  gfloat target;
} AnimationSpec;

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor green = { 0x00, 0xff, 0x00, 0xff };
static const ClutterColor blue = { 0x00, 0x00, 0xff, 0xff };

static gboolean
button_pressed_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  AnimationSpec *animation_spec;

  if (clutter_actor_get_animation (actor) != NULL)
    return TRUE;

  animation_spec = (AnimationSpec *) user_data;

  clutter_actor_animate (actor, CLUTTER_LINEAR, 500,
                         animation_spec->axis, animation_spec->target,
                         NULL);

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
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  rectangle1 = clutter_rectangle_new_with_color (&red);
  clutter_actor_set_reactive (rectangle1, TRUE);
  clutter_actor_set_size (rectangle1, 50, 50);
  clutter_actor_set_position (rectangle1, 400, 400);

  rectangle2 = clutter_rectangle_new_with_color (&green);
  clutter_actor_set_reactive (rectangle2, TRUE);
  clutter_actor_set_size (rectangle2, 50, 50);
  clutter_actor_set_position (rectangle2, 50, 50);

  rectangle3 = clutter_rectangle_new_with_color (&blue);
  clutter_actor_set_reactive (rectangle3, TRUE);
  clutter_actor_set_size (rectangle3, 50, 50);
  clutter_actor_set_position (rectangle3, 225, 225);

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

  clutter_container_add (CLUTTER_CONTAINER (stage),
                         rectangle1,
                         rectangle2,
                         rectangle3,
                         NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
