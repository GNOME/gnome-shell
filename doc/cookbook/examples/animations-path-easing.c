#include <stdlib.h>
#include <clutter/clutter.h>

typedef struct {
  ClutterActor *red;
  ClutterActor *green;
  ClutterTimeline *timeline;
} State;

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor green_color = { 0x00, 0xff, 0x00, 0xff };

static void
reverse_timeline (ClutterTimeline *timeline)
{
  ClutterTimelineDirection dir = clutter_timeline_get_direction (timeline);

  if (dir == CLUTTER_TIMELINE_FORWARD)
    dir = CLUTTER_TIMELINE_BACKWARD;
  else
    dir = CLUTTER_TIMELINE_FORWARD;

  clutter_timeline_set_direction (timeline, dir);
}

/* a key press either starts the timeline or reverses it */
static gboolean
key_pressed_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      user_data)
{
  State *state = (State *) user_data;

  if (clutter_timeline_is_playing (state->timeline))
    reverse_timeline (state->timeline);
  else
    clutter_timeline_start (state->timeline);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  State *state = g_new0 (State, 1);

  ClutterActor *stage;
  ClutterAnimator *animator;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  state->red = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (state->red, 100, 100);
  clutter_actor_set_position (state->red, 300, 300);

  state->green = clutter_rectangle_new_with_color (&green_color);
  clutter_actor_set_size (state->green, 100, 100);
  clutter_actor_set_position (state->green, 0, 0);

  animator = clutter_animator_new ();
  clutter_animator_set_duration (animator, 1000);

  clutter_animator_set (animator,
                        state->red, "x", CLUTTER_LINEAR, 0.0, 300.0,
                        state->red, "y", CLUTTER_LINEAR, 0.0, 300.0,
                        state->red, "x", CLUTTER_LINEAR, 1.0, 0.0,
                        state->red, "y", CLUTTER_EASE_IN_QUINT, 1.0, 0.0,
                        NULL);

  clutter_animator_set (animator,
                        state->green, "x", CLUTTER_LINEAR, 0.0, 0.0,
                        state->green, "y", CLUTTER_LINEAR, 0.0, 0.0,
                        state->green, "x", CLUTTER_LINEAR, 1.0, 300.0,
                        state->green, "y", CLUTTER_EASE_IN_QUINT, 1.0, 300.0,
                        NULL);

  state->timeline = clutter_animator_get_timeline (animator);

  clutter_timeline_set_auto_reverse (state->timeline, TRUE);

  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_pressed_cb),
                    state);

  clutter_container_add (CLUTTER_CONTAINER (stage), state->red, state->green, NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (animator);

  g_free (state);

  return EXIT_SUCCESS;
}
