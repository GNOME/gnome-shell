#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };

static void
next_state (ClutterState *transitions,
            gpointer      user_data)
{
  const gchar *state = clutter_state_get_state (transitions);

  if (g_strcmp0 (state, "right") == 0)
    clutter_state_set_state (transitions, "left");
  else
    clutter_state_set_state (transitions, "right");
}

static gboolean
key_pressed_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      user_data)
{
  ClutterState *transitions = CLUTTER_STATE (user_data);

  if (!clutter_timeline_is_playing (clutter_state_get_timeline (transitions)))
    next_state (transitions, NULL);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *actor;
  ClutterState *transitions;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 300, 200);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  actor = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_position (actor, 150, 50);
  clutter_actor_set_size (actor, 100, 100);

  transitions = clutter_state_new ();
  clutter_state_set_duration (transitions, NULL, NULL, 1000);

  clutter_state_set (transitions, NULL, "right",
                     actor, "x", CLUTTER_LINEAR, 150.0,
                     NULL);

  clutter_state_set (transitions, NULL, "left",
                     actor, "x", CLUTTER_LINEAR, 50.0,
                     NULL);

  clutter_state_warp_to_state (transitions, "right");

  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_pressed_cb),
                    transitions);

  g_signal_connect (transitions,
                    "completed",
                    G_CALLBACK (next_state),
                    NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (transitions);

  return EXIT_SUCCESS;
}
