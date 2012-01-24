#include <stdlib.h>
#include <clutter/clutter.h>

typedef struct
{
  ClutterActor    *stage;
  ClutterActor    *group;
  ClutterAnimator *animator;
} State;

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor green_color = { 0x00, 0xff, 0x00, 0xff };
static const ClutterColor blue_color = { 0x00, 0x00, 0xff, 0xff };

/* add keys to the animator such that the actor is moved
 * to a random x position
 */
static void
add_keys_for_actor (ClutterActor    *actor,
                    ClutterAnimator *animator)
{
  gfloat x, end_x;

  x = clutter_actor_get_x (actor);

  end_x = 50.0;
  if (x == 50.0)
    end_x = 225.0 + (100.0 * rand () / (RAND_MAX + 1.0));

  clutter_animator_set (animator,
                        actor, "x", CLUTTER_LINEAR, 0.0, x,
                        actor, "x", CLUTTER_EASE_OUT_CUBIC, 1.0, end_x,
                        NULL);
}

static gboolean
move_actors (ClutterActor *actor,
             ClutterEvent *event,
             gpointer      user_data)
{
  State *state = user_data;
  ClutterActor *child;

  /* do nothing if the animator is already running */
  if (clutter_timeline_is_playing (clutter_animator_get_timeline (state->animator)))
    return TRUE;

  /* remove all keys from the animator */
  clutter_animator_remove_key (state->animator, NULL, NULL, -1);

  /* add keys for all actors in the group */
  for (child = clutter_actor_get_first_child (state->group);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      add_keys_for_actor (child, state->animator);
    }

  /* start the animation */
  clutter_animator_start (state->animator);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *red;
  ClutterActor *green;
  ClutterActor *blue;

  State *state = g_new0 (State, 1);

  /* seed random number generator */
  srand ((unsigned int) time (NULL));

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  state->animator = clutter_animator_new ();
  clutter_animator_set_duration (state->animator, 500);

  state->stage = clutter_stage_new ();
  clutter_actor_set_size (state->stage, 400, 350);
  clutter_stage_set_color (CLUTTER_STAGE (state->stage), &stage_color);
  g_signal_connect (state->stage,
                    "destroy",
                    G_CALLBACK (clutter_main_quit),
                    NULL);

  state->group = clutter_actor_new ();
  clutter_actor_add_child (state->stage, state->group);

  red = clutter_actor_new ();
  clutter_actor_set_background_color (red, &red_color);
  clutter_actor_set_size (red, 50, 50);
  clutter_actor_set_position (red, 50, 50);
  clutter_actor_add_child (state->group, red);

  green = clutter_actor_new ();
  clutter_actor_set_background_color (green, &green_color);
  clutter_actor_set_size (green, 50, 50);
  clutter_actor_set_position (green, 50, 150);
  clutter_actor_add_child (state->group, green);

  blue = clutter_actor_new ();
  clutter_actor_set_background_color (blue, &blue_color);
  clutter_actor_set_size (blue, 50, 50);
  clutter_actor_set_position (blue, 50, 250);
  clutter_actor_add_child (state->group, blue);

  g_signal_connect (state->stage,
                    "key-press-event",
                    G_CALLBACK (move_actors),
                    state);

  clutter_actor_show (state->stage);

  clutter_main ();

  g_object_unref (state->animator);
  g_free (state);

  return EXIT_SUCCESS;
}
