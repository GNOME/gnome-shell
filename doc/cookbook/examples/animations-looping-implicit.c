#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };

typedef struct
{
  ClutterActor    *actor;
  ClutterTimeline *timeline;
} State;

static gboolean
key_pressed_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      user_data)
{
  State *state = (State *) user_data;

  /* only start animating if actor isn't animating already */
  if (clutter_actor_get_animation (state->actor) == NULL)
    clutter_actor_animate_with_timeline (state->actor,
                                         CLUTTER_LINEAR,
                                         state->timeline,
                                         "x", 50.0,
                                         NULL);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  State *state = g_new0 (State, 1);

  ClutterActor *stage;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 300, 200);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  state->actor = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (state->actor, 100, 100);
  clutter_actor_set_position (state->actor, 150, 50);

  state->timeline = clutter_timeline_new (1000);
  clutter_timeline_set_repeat_count (state->timeline, -1);
  clutter_timeline_set_auto_reverse (state->timeline, TRUE);

  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_pressed_cb),
                    state);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), state->actor);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (state->timeline);
  g_free (state);

  return EXIT_SUCCESS;
}
