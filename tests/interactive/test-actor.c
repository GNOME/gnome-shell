#include <stdlib.h>
#include <gmodule.h>
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

  clutter_actor_animate (actor, CLUTTER_LINEAR, 500,
                         "background-color", end_color,
                         NULL);

  toggled = !toggled;

  return CLUTTER_EVENT_STOP;
}

static gboolean
on_crossing (ClutterActor *actor,
             ClutterEvent *event,
             gpointer      data)
{
  gboolean is_enter = GPOINTER_TO_UINT (data);
  float depth;

  if (is_enter)
    depth = -250.0;
  else
    depth = 0.0;

  clutter_actor_animate (actor, CLUTTER_EASE_OUT_BOUNCE, 500,
                         "depth", depth,
                         NULL);

  return CLUTTER_EVENT_STOP;
}

static void
restore_actor (ClutterActor *actor)
{
  clutter_actor_set_rotation (actor, CLUTTER_Y_AXIS, 0.0,
                              SIZE / 2.0,
                              0.f,
                              0.f);
  clutter_actor_set_reactive (actor, TRUE);
}

static gboolean
animate_rotation (ClutterActor *actor,
                  ClutterEvent *event)
{
  ClutterVertex center;

  center.x = SIZE / 2.0;
  center.y = 0.f;
  center.z = 0.f;

  clutter_actor_animate (actor, CLUTTER_EASE_OUT_EXPO, 500,
                         "fixed::reactive", FALSE,
                         "fixed::rotation-center-y", &center,
                         "rotation-angle-y", 360.0,
                         "signal-swapped-after::completed",
                           G_CALLBACK (restore_actor),
                           actor,
                         NULL);

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
                    GUINT_TO_POINTER (TRUE));
  g_signal_connect (flowers[1], "leave-event",
                    G_CALLBACK (on_crossing),
                    GUINT_TO_POINTER (FALSE));

  /* the third one is green */
  flowers[2] = clutter_actor_new ();
  clutter_actor_set_name (flowers[2], "flower.3");
  clutter_actor_set_size (flowers[2], SIZE, SIZE);
  clutter_actor_set_margin_right (flowers[2], 12);
  clutter_actor_set_background_color (flowers[2], CLUTTER_COLOR_Green);
  clutter_actor_set_reactive (flowers[2], TRUE);
  clutter_actor_add_child (vase, flowers[2]);
  g_signal_connect (flowers[2], "button-press-event",
                    G_CALLBACK (animate_rotation),
                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_actor_describe (void)
{
  return "Basic example of actor usage.";
}
