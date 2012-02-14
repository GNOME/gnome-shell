#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

#define SIZE    128

static gboolean
on_button_press (ClutterActor *actor,
                 ClutterEvent *event,
                 gpointer      data)
{
  static gboolean toggled = TRUE;
  const ClutterColor *end_color;

  if (toggled)
    end_color = CLUTTER_COLOR_Blue;
  else
    end_color = CLUTTER_COLOR_Red;

  clutter_actor_animate (actor, CLUTTER_LINEAR, 500,
                         "background-color", end_color,
                         NULL);

  toggled = !toggled;

  return CLUTTER_EVENT_STOP;
}

G_MODULE_EXPORT int
test_actor_main (int argc, char *argv[])
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
  clutter_actor_set_margin_top (vase, 18);
  clutter_actor_set_margin_bottom (vase, 18);
  clutter_actor_set_margin_left (vase, 6);
  clutter_actor_set_margin_right (vase, 6);
  clutter_actor_add_constraint (vase, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_add_child (stage, vase);

  flowers[0] = clutter_actor_new ();
  clutter_actor_set_name (flowers[0], "flower.1");
  clutter_actor_set_size (flowers[0], SIZE, SIZE);
  clutter_actor_set_background_color (flowers[0], CLUTTER_COLOR_Red);
  clutter_actor_set_reactive (flowers[0], TRUE);
  g_signal_connect (flowers[0], "button-press-event",
                    G_CALLBACK (on_button_press),
                    GUINT_TO_POINTER (0));
  clutter_actor_add_child (vase, flowers[0]);

  flowers[1] = clutter_actor_new ();
  clutter_actor_set_name (flowers[1], "flower.2");
  clutter_actor_set_size (flowers[1], SIZE, SIZE);
  clutter_actor_set_background_color (flowers[1], CLUTTER_COLOR_Yellow);
  clutter_actor_add_child (vase, flowers[1]);

  /* the third one is green */
  flowers[2] = clutter_actor_new ();
  clutter_actor_set_name (flowers[2], "flower.3");
  clutter_actor_set_size (flowers[2], SIZE, SIZE);
  clutter_actor_set_background_color (flowers[2], CLUTTER_COLOR_Green);
  clutter_actor_add_child (vase, flowers[2]);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_actor_describe (void)
{
  return "Basic example of actor usage.";
}
