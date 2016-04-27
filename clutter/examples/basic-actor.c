#include <stdlib.h>
#include <clutter/clutter.h>

#define SIZE    128

static gboolean
animate_color (ClutterActor *actor,
               ClutterEvent *event)
{
  static gboolean toggled = TRUE;
  const ClutterColor *end_color;

  if (toggled)
    end_color = CLUTTER_COLOR_Blue;
  else
    end_color = CLUTTER_COLOR_Red;

  clutter_actor_save_easing_state (actor);
  clutter_actor_set_easing_duration (actor, 500);
  clutter_actor_set_easing_mode (actor, CLUTTER_LINEAR);
  clutter_actor_set_background_color (actor, end_color);
  clutter_actor_restore_easing_state (actor);

  toggled = !toggled;

  return CLUTTER_EVENT_STOP;
}

static gboolean
on_crossing (ClutterActor *actor,
             ClutterEvent *event)
{
  gboolean is_enter = clutter_event_type (event) == CLUTTER_ENTER;
  float zpos;

  if (is_enter)
    zpos = -250.0;
  else
    zpos = 0.0;

  clutter_actor_save_easing_state (actor);
  clutter_actor_set_easing_duration (actor, 500);
  clutter_actor_set_easing_mode (actor, CLUTTER_EASE_OUT_BOUNCE);
  clutter_actor_set_z_position (actor, zpos);
  clutter_actor_restore_easing_state (actor);

  return CLUTTER_EVENT_STOP;
}

static void
on_transition_stopped (ClutterActor *actor,
                       const gchar  *transition_name,
                       gboolean      is_finished)
{
  clutter_actor_save_easing_state (actor);
  clutter_actor_set_rotation_angle (actor, CLUTTER_Y_AXIS, 0.0f);
  clutter_actor_restore_easing_state (actor);

  /* disconnect so we don't get multiple notifications */
  g_signal_handlers_disconnect_by_func (actor,
                                        on_transition_stopped,
                                        NULL);
}

static gboolean
animate_rotation (ClutterActor *actor,
                  ClutterEvent *event)
{
  clutter_actor_save_easing_state (actor);
  clutter_actor_set_easing_duration (actor, 1000);

  clutter_actor_set_rotation_angle (actor, CLUTTER_Y_AXIS, 360.0);

  clutter_actor_restore_easing_state (actor);

  /* get a notification when the rotation-angle-y transition ends */
  g_signal_connect (actor,
                    "transition-stopped::rotation-angle-y",
                    G_CALLBACK (on_transition_stopped),
                    NULL);

  return CLUTTER_EVENT_STOP;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *vase;
  ClutterActor *flowers[3];

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Three Flowers in a Vase");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  /* there are three flowers in a vase */
  vase = clutter_actor_new ();
  clutter_actor_set_name (vase, "vase");
  clutter_actor_set_layout_manager (vase, clutter_box_layout_new ());
  clutter_actor_set_background_color (vase, CLUTTER_COLOR_LightSkyBlue);
  clutter_actor_add_constraint (vase, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_add_child (stage, vase);

  flowers[0] = clutter_actor_new ();
  clutter_actor_set_name (flowers[0], "flower.1");
  clutter_actor_set_size (flowers[0], SIZE, SIZE);
  clutter_actor_set_margin_left (flowers[0], 12);
  clutter_actor_set_background_color (flowers[0], CLUTTER_COLOR_Red);
  clutter_actor_set_reactive (flowers[0], TRUE);
  clutter_actor_add_child (vase, flowers[0]);
  g_signal_connect (flowers[0], "button-press-event",
                    G_CALLBACK (animate_color),
                    NULL);

  flowers[1] = clutter_actor_new ();
  clutter_actor_set_name (flowers[1], "flower.2");
  clutter_actor_set_size (flowers[1], SIZE, SIZE);
  clutter_actor_set_margin_top (flowers[1], 12);
  clutter_actor_set_margin_left (flowers[1], 6);
  clutter_actor_set_margin_right (flowers[1], 6);
  clutter_actor_set_margin_bottom (flowers[1], 12);
  clutter_actor_set_background_color (flowers[1], CLUTTER_COLOR_Yellow);
  clutter_actor_set_reactive (flowers[1], TRUE);
  clutter_actor_add_child (vase, flowers[1]);
  g_signal_connect (flowers[1], "enter-event",
                    G_CALLBACK (on_crossing),
                    NULL);
  g_signal_connect (flowers[1], "leave-event",
                    G_CALLBACK (on_crossing),
                    NULL);

  /* the third one is green */
  flowers[2] = clutter_actor_new ();
  clutter_actor_set_name (flowers[2], "flower.3");
  clutter_actor_set_size (flowers[2], SIZE, SIZE);
  clutter_actor_set_margin_right (flowers[2], 12);
  clutter_actor_set_background_color (flowers[2], CLUTTER_COLOR_Green);
  clutter_actor_set_pivot_point (flowers[2], 0.5f, 0.0f);
  clutter_actor_set_reactive (flowers[2], TRUE);
  clutter_actor_add_child (vase, flowers[2]);
  g_signal_connect (flowers[2], "button-press-event",
                    G_CALLBACK (animate_rotation),
                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
