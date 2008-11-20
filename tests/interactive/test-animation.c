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

static gboolean
on_button_press (ClutterActor       *actor,
                 ClutterButtonEvent *event,
                 gpointer            dummy)
{
  ClutterAnimation *animation;
  gint old_x, old_y, new_x, new_y;
  guint old_width, old_height, new_width, new_height;
  guint8 old_op, new_op;
  gdouble new_angle;
  ClutterVertex vertex = { 0, };

  clutter_actor_get_position (actor, &old_x, &old_y);
  clutter_actor_get_size (actor, &old_width, &old_height);
  old_op = clutter_actor_get_opacity (actor);

  /* determine the final state of the animation depending on
   * the state of the actor
   */
  if (!is_expanded)
    {
      new_x = old_x - 100;
      new_y = old_y - 100;
      new_width = old_width + 200;
      new_height = old_height + 200;
      new_op = 255;
      new_angle = 360.0;
    }
  else
    {
      new_x = old_x + 100;
      new_y = old_y + 100;
      new_width = old_width - 200;
      new_height = old_height - 200;
      new_op = 128;
      new_angle = 0.0;
    }

  vertex.x = CLUTTER_UNITS_FROM_FLOAT ((float) new_width / 2);
  vertex.y = CLUTTER_UNITS_FROM_FLOAT ((float) new_height / 2);

  animation =
    clutter_actor_animate (actor, CLUTTER_EASE_IN, 2000,
                           "x", new_x,
                           "y", new_y,
                           "width", new_width,
                           "height", new_height,
                           "opacity", new_op,
                           "rotation-angle-z", new_angle,
                           "fixed::rotation-center-z", &vertex,
                           "fixed::reactive", FALSE,
                           NULL);
  g_signal_connect (animation,
                    "completed", G_CALLBACK (on_animation_complete),
                    actor);

  return TRUE;
}

G_MODULE_EXPORT int
test_animation_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect;
  ClutterColor stage_color = { 0x66, 0x66, 0xdd, 0xff };
  ClutterColor rect_color = { 0x44, 0xdd, 0x44, 0xff };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  clutter_actor_set_size (rect, 50, 50);
  clutter_actor_set_anchor_point (rect, 25, 25);
  clutter_actor_set_position (rect,
                              clutter_actor_get_width (stage) / 2,
                              clutter_actor_get_height (stage) / 2);
  clutter_actor_set_opacity (rect, 0x88);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect (rect,
                    "button-press-event", G_CALLBACK (on_button_press),
                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
