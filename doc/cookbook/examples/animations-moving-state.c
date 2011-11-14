#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor green_color = { 0x00, 0xff, 0x00, 0xff };

static gboolean
button_pressed_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  ClutterState *transitions = CLUTTER_STATE (user_data);

  /* set the state to the one with a name matching the actor's name */
  clutter_state_set_state (transitions, clutter_actor_get_name (actor));

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *red;
  ClutterActor *green;
  ClutterState *transitions;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 650, 500);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* actor names choose the next ClutterState to transition to */
  red = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_reactive (red, TRUE);
  clutter_actor_set_name (red, "red");
  clutter_actor_set_size (red, 100, 100);
  clutter_actor_set_position (red, 50, 50);

  green = clutter_rectangle_new_with_color (&green_color);
  clutter_actor_set_reactive (green, TRUE);
  clutter_actor_set_name (green, "green");
  clutter_actor_set_size (green, 100, 100);
  clutter_actor_set_position (green, 50, 350);

  transitions = clutter_state_new ();
  clutter_state_set_duration (transitions, NULL, NULL, 250);

  /* state names match actor names */
  clutter_state_set (transitions, NULL, "red",
                     red, "x", CLUTTER_EASE_OUT_CUBIC, 200.0,
                     red, "y", CLUTTER_EASE_OUT_CUBIC, 50.0,
                     red, "scale-x", CLUTTER_EASE_OUT_CUBIC, 4.0,
                     red, "scale-y", CLUTTER_EASE_OUT_CUBIC, 4.0,
                     green, "x", CLUTTER_EASE_OUT_CUBIC, 50.0,
                     green, "y", CLUTTER_EASE_OUT_CUBIC, 350.0,
                     green, "scale-x", CLUTTER_EASE_OUT_CUBIC, 1.0,
                     green, "scale-y", CLUTTER_EASE_OUT_CUBIC, 1.0,
                     NULL);

  clutter_state_set (transitions, NULL, "green",
                     green, "x", CLUTTER_EASE_OUT_CUBIC, 200.0,
                     green, "y", CLUTTER_EASE_OUT_CUBIC, 50.0,
                     green, "scale-x", CLUTTER_EASE_OUT_CUBIC, 4.0,
                     green, "scale-y", CLUTTER_EASE_OUT_CUBIC, 4.0,
                     red, "x", CLUTTER_EASE_OUT_CUBIC, 50.0,
                     red, "y", CLUTTER_EASE_OUT_CUBIC, 50.0,
                     red, "scale-x", CLUTTER_EASE_OUT_CUBIC, 1.0,
                     red, "scale-y", CLUTTER_EASE_OUT_CUBIC, 1.0,
                     NULL);

  g_signal_connect (red,
                    "button-press-event",
                    G_CALLBACK (button_pressed_cb),
                    transitions);

  g_signal_connect (green,
                    "button-press-event",
                    G_CALLBACK (button_pressed_cb),
                    transitions);

  clutter_container_add (CLUTTER_CONTAINER (stage),
                         red,
                         green,
                         NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (transitions);

  return EXIT_SUCCESS;
}
