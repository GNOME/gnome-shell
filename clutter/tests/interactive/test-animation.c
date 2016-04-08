#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static gboolean is_expanded = FALSE;

static void
on_rect_transitions_completed (ClutterActor *actor)
{
  is_expanded = !is_expanded;

  g_print ("Animation complete\n");

  clutter_actor_set_reactive (actor, TRUE);
}

static void
on_clicked (ClutterClickAction *action,
            ClutterActor       *actor,
            gpointer            dummy G_GNUC_UNUSED)
{
  ClutterAnimation *animation;
  gfloat old_x, old_y, new_x, new_y;
  gfloat old_width, old_height, new_width, new_height;
  gdouble new_angle;
  const ClutterColor *new_color;
  guint8 new_opacity;

  clutter_actor_get_position (actor, &old_x, &old_y);
  clutter_actor_get_size (actor, &old_width, &old_height);

  /* determine the final state of the animation depending on
   * the state of the actor
   */
  if (!is_expanded)
    {
      new_x = old_x - 100;
      new_y = old_y - 100;
      new_width = old_width + 200;
      new_height = old_height + 200;
      new_angle = 360.0;

      new_color = CLUTTER_COLOR_DarkScarletRed;

      new_opacity = 255;
    }
  else
    {
      new_x = old_x + 100;
      new_y = old_y + 100;
      new_width = old_width - 200;
      new_height = old_height - 200;
      new_angle = 0.0;

      new_color = CLUTTER_COLOR_LightOrange;

      new_opacity = 128;
    }

  clutter_actor_save_easing_state (actor);
  clutter_actor_set_easing_mode (actor, CLUTTER_EASE_IN_EXPO);
  clutter_actor_set_easing_duration (actor, 2000);

  clutter_actor_set_position (actor, new_x, new_y);
  clutter_actor_set_size (actor, new_width, new_height);
  clutter_actor_set_background_color (actor, new_color);
  clutter_actor_set_rotation_angle (actor, CLUTTER_Z_AXIS, new_angle);
  clutter_actor_set_reactive (actor, FALSE);

  /* animate the opacity halfway through, with a different pacing */
  clutter_actor_save_easing_state (actor);
  clutter_actor_set_easing_mode (actor, CLUTTER_LINEAR);
  clutter_actor_set_easing_delay (actor, 1000);
  clutter_actor_set_easing_duration (actor, 1000);
  clutter_actor_set_opacity (actor, new_opacity);
  clutter_actor_restore_easing_state (actor);

  clutter_actor_restore_easing_state (actor);
}

G_MODULE_EXPORT int
test_animation_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect;
  ClutterColor rect_color = { 0x44, 0xdd, 0x44, 0xff };
  ClutterAction *action;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_LightSkyBlue);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Animation");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  rect = clutter_actor_new ();
  clutter_actor_set_background_color (rect, CLUTTER_COLOR_LightOrange);
  clutter_actor_add_child (stage, rect);
  clutter_actor_set_size (rect, 50, 50);
  clutter_actor_set_pivot_point (rect, .5f, .5f);
  clutter_actor_set_translation (rect, -25, -25, 0);
  clutter_actor_set_position (rect,
                              clutter_actor_get_width (stage) / 2,
                              clutter_actor_get_height (stage) / 2);
  clutter_actor_set_opacity (rect, 128);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect (rect, "transitions-completed",
                    G_CALLBACK (on_rect_transitions_completed),
                    NULL);

  action = clutter_click_action_new ();
  g_signal_connect (action, "clicked", G_CALLBACK (on_clicked), NULL);
  clutter_actor_add_action_with_name (rect, "click", action);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_animation_describe (void)
{
  return "Simple animation demo";
}
