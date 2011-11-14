#include <stdlib.h>
#include <clutter/clutter.h>

typedef struct
{
  ClutterState *transitions;
  ClutterActor *actor;
  ClutterActor *props_display;
  guint         scale_gravity;
  gboolean      transitions_running;
} State;

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor yellow_color = { 0xff, 0xff, 0x00, 0xff };

static void
show_scale_properties_cb (ClutterActor *actor,
                          gpointer      user_data)
{
  State *state = (State *) user_data;

  gfloat transformed_x, transformed_y;
  gfloat transformed_width, transformed_height;
  gfloat scale_center_x, scale_center_y;

  gchar *message;

  clutter_actor_get_transformed_position (state->actor,
                                          &transformed_x,
                                          &transformed_y);

  clutter_actor_get_transformed_size (state->actor,
                                      &transformed_width,
                                      &transformed_height);

  g_object_get (G_OBJECT (actor),
                "scale-center-x", &scale_center_x,
                "scale-center-y", &scale_center_y,
                NULL);

  /* draw cross on the scale center */
  cogl_set_source_color4ub (255, 255, 0, 255);

  cogl_path_move_to (scale_center_x, scale_center_y);
  cogl_path_rel_line_to (10, 10);
  cogl_path_rel_line_to (-20, -20);
  cogl_path_move_to (scale_center_x, scale_center_y);
  cogl_path_rel_line_to (10, -10);
  cogl_path_rel_line_to (-20, 20);

  cogl_path_stroke ();

  /* show actor properties */
  message = g_strdup_printf ("Scale center: %.0f, %.0f\n"
                             "Transformed position: %.2f, %.2f\n"
                             "Transformed size: %.2f, %.2f",
                             scale_center_x, scale_center_y,
                             transformed_x, transformed_y,
                             transformed_width, transformed_height);

  clutter_text_set_text (CLUTTER_TEXT (state->props_display), message);

  g_free (message);
}

static void
next_transition_cb (ClutterState *transitions,
                    gpointer      user_data)
{
  State *state = (State *) user_data;

  if (clutter_actor_is_scaled (state->actor))
    clutter_state_set_state (state->transitions, "not-scaled");
  else if (state->scale_gravity > 9)
    {
      /* gravity is at center, so reset ready for next key press */
      state->scale_gravity = CLUTTER_GRAVITY_NORTH;

      state->transitions_running = FALSE;
    }
  else
    {
      g_object_set (G_OBJECT (state->actor),
                    "scale-gravity", state->scale_gravity,
                    NULL);

      state->scale_gravity++;

      clutter_state_set_state (state->transitions, "scaled-down");
    }
}

static gboolean
key_pressed_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      user_data)
{
  State *state = (State *) user_data;

  if (!state->transitions_running)
    {
      state->transitions_running = TRUE;
      next_transition_cb (NULL, state);
    }

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
  clutter_actor_set_size (stage, 350, 350);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  state->scale_gravity = CLUTTER_GRAVITY_NORTH;
  state->transitions_running = FALSE;

  state->props_display = clutter_text_new ();
  clutter_actor_set_size (state->props_display, 340, 80);
  clutter_actor_set_position (state->props_display, 5, 280);
  clutter_text_set_color (CLUTTER_TEXT (state->props_display), &yellow_color);

  state->actor = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (state->actor, 200, 200);
  clutter_actor_set_position (state->actor, 75, 50);

  g_object_set (G_OBJECT (state->actor),
                "scale-gravity", state->scale_gravity,
                NULL);

  state->transitions = clutter_state_new ();
  clutter_state_set_duration (state->transitions, NULL, NULL, 400);

  clutter_state_set (state->transitions, NULL, "not-scaled",
                     state->actor, "scale-x", CLUTTER_LINEAR, 1.0,
                     state->actor, "scale-y", CLUTTER_LINEAR, 1.0,
                     NULL);

  clutter_state_set (state->transitions, NULL, "scaled-down",
                     state->actor, "scale-x", CLUTTER_LINEAR, 0.25,
                     state->actor, "scale-y", CLUTTER_LINEAR, 0.25,
                     NULL);

  clutter_state_warp_to_state (state->transitions, "not-scaled");

  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_pressed_cb),
                    state);

  g_signal_connect (state->transitions,
                    "completed",
                    G_CALLBACK (next_transition_cb),
                    state);

  g_signal_connect_after (state->actor,
                          "paint",
                          G_CALLBACK (show_scale_properties_cb),
                          state);

  clutter_container_add (CLUTTER_CONTAINER (stage),
                         state->actor,
                         state->props_display,
                         NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (state->transitions);
  g_free (state);

  return EXIT_SUCCESS;
}
