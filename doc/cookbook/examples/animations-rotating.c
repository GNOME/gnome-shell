#include <clutter/clutter.h>

#define ROTATION_ANGLE 75.0
#define DURATION       2000

static void
_set_next_state (ClutterState *transitions,
                 gpointer      user_data)
{
  const gchar *current = clutter_state_get_state (transitions);
  gchar *next_state = "start";

  if (g_strcmp0 (current, "start") == 0)
    next_state = "x-cw";
  else if (g_strcmp0 (current, "x-cw") == 0)
    next_state = "x-ccw";
  else if (g_strcmp0 (current, "x-ccw") == 0)
    next_state = "x-after";
  else if (g_strcmp0 (current, "x-after") == 0)
    next_state = "y-cw";
  else if (g_strcmp0 (current, "y-cw") == 0)
    next_state = "y-ccw";
  else if (g_strcmp0 (current, "y-ccw") == 0)
    next_state = "y-after";
  else if (g_strcmp0 (current, "y-after") == 0)
    next_state = "z-cw";
  else if (g_strcmp0 (current, "z-cw") == 0)
    next_state = "z-ccw";

  clutter_state_set_state (transitions, next_state);
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *texture;
  ClutterState *transitions;
  GError *error = NULL;
  gfloat texture_width, texture_height;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  texture = clutter_texture_new ();
  clutter_actor_add_constraint (texture,
                                clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (texture,
                                clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  clutter_texture_set_sync_size (CLUTTER_TEXTURE (texture), TRUE);
  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 "redhand.png",
                                 &error);

  if (error != NULL)
    {
      g_error ("Problem loading image into texture - %s", error->message);
      g_error_free (error);
      return 1;
    }

  clutter_actor_get_size (texture, &texture_width, &texture_height);
  clutter_actor_set_size (stage, texture_width * 2, texture_height * 2);

  /* set all centres of rotation to the centre of the texture */
  clutter_actor_set_rotation (texture,
                              CLUTTER_X_AXIS,
                              0.0,
                              texture_width * 0.5,
                              texture_height * 0.5,
                              0.0);
  clutter_actor_set_rotation (texture,
                              CLUTTER_Y_AXIS,
                              0.0,
                              texture_width * 0.5,
                              texture_height * 0.5,
                              0.0);
  clutter_actor_set_z_rotation_from_gravity (texture, 0.0, CLUTTER_GRAVITY_CENTER);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), texture);

  /* set up the animations */
  transitions = clutter_state_new ();

  clutter_state_set (transitions, NULL, "start",
                     texture, "rotation-angle-x", CLUTTER_LINEAR, 0.0,
                     texture, "rotation-angle-y", CLUTTER_LINEAR, 0.0,
                     texture, "rotation-angle-z", CLUTTER_LINEAR, 0.0,
                     NULL);
  clutter_state_set (transitions, NULL, "x-cw",
                     texture, "rotation-angle-x", CLUTTER_LINEAR, ROTATION_ANGLE,
                     NULL);
  clutter_state_set (transitions, NULL, "x-ccw",
                     texture, "rotation-angle-x", CLUTTER_LINEAR, -ROTATION_ANGLE,
                     NULL);
  clutter_state_set (transitions, NULL, "x-after",
                     texture, "rotation-angle-x", CLUTTER_LINEAR, 0.0,
                     NULL);
  clutter_state_set (transitions, NULL, "y-cw",
                     texture, "rotation-angle-y", CLUTTER_LINEAR, ROTATION_ANGLE,
                     NULL);
  clutter_state_set (transitions, NULL, "y-ccw",
                     texture, "rotation-angle-y", CLUTTER_LINEAR, -ROTATION_ANGLE,
                     NULL);
  clutter_state_set (transitions, NULL, "y-after",
                     texture, "rotation-angle-y", CLUTTER_LINEAR, 0.0,
                     NULL);
  clutter_state_set (transitions, NULL, "z-cw",
                     texture, "rotation-angle-z", CLUTTER_LINEAR, ROTATION_ANGLE,
                     NULL);
  clutter_state_set (transitions, NULL, "z-ccw",
                     texture, "rotation-angle-z", CLUTTER_LINEAR, -ROTATION_ANGLE,
                     NULL);
  clutter_state_set_duration (transitions, NULL, NULL, DURATION);
  clutter_state_set_duration (transitions, "start", NULL, DURATION * 0.5);
  clutter_state_set_duration (transitions, NULL, "start", DURATION * 0.5);
  clutter_state_set_duration (transitions, NULL, "x-after", DURATION * 0.5);
  clutter_state_set_duration (transitions, NULL, "y-after", DURATION * 0.5);

  clutter_state_warp_to_state (transitions, "start");

  g_signal_connect (transitions,
                    "completed",
                    G_CALLBACK (_set_next_state),
                    NULL);

  clutter_state_set_state (transitions, "x-cw");

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
