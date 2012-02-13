#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static gboolean is_expanded = FALSE;

static void
on_animation_complete (ClutterAnimation *animation,
                       ClutterActor     *actor)
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
  ClutterVertex vertex = { 0, };
  ClutterColor new_color = { 0, };

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

      new_color.red = 0xdd;
      new_color.green = 0x44;
      new_color.blue = 0xdd;
      new_color.alpha = 0xff;
    }
  else
    {
      new_x = old_x + 100;
      new_y = old_y + 100;
      new_width = old_width - 200;
      new_height = old_height - 200;
      new_angle = 0.0;

      new_color.red = 0x44;
      new_color.green = 0xdd;
      new_color.blue = 0x44;
      new_color.alpha = 0x88;
    }

  vertex.x = new_width / 2;
  vertex.y = new_height / 2;
  vertex.z = 0.0;

  animation =
    clutter_actor_animate (actor, CLUTTER_EASE_IN_EXPO, 2000,
                           "x", new_x,
                           "y", new_y,
                           "width", new_width,
                           "height", new_height,
                           "color", &new_color,
                           "rotation-angle-z", new_angle,
                           "fixed::rotation-center-z", &vertex,
                           "fixed::reactive", FALSE,
                           NULL);
  g_signal_connect (animation,
                    "completed", G_CALLBACK (on_animation_complete),
                    actor);
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

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  clutter_actor_set_size (rect, 50, 50);
  clutter_actor_set_anchor_point (rect, 25, 25);
  clutter_actor_set_position (rect,
                              clutter_actor_get_width (stage) / 2,
                              clutter_actor_get_height (stage) / 2);
  clutter_actor_set_opacity (rect, 0x88);
  clutter_actor_set_reactive (rect, TRUE);

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
  return "Simple clutter_actor_animate() demo";
}
