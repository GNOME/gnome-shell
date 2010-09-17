#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

#define RECT_SIZE       128.0

#define N_COLORS        3

static ClutterActor *rects[N_COLORS] = { NULL, };
static const gchar *colors[N_COLORS] = {
  "#cc0000", "#73d216", "#3465a4"
};
static gboolean is_expanded = FALSE;

static void
on_click (ClutterClickAction *action,
          ClutterActor       *actor)
{
  if (!is_expanded)
    {
      clutter_actor_animate (rects[1], CLUTTER_EASE_OUT_CUBIC, 250,
                             "@constraints.green-x.offset", RECT_SIZE,
                             "opacity", 255,
                             NULL);
      clutter_actor_animate (rects[2], CLUTTER_EASE_OUT_CUBIC, 500,
                             "@constraints.blue-x.offset", (RECT_SIZE * 2.0 + 0.5),
                             "opacity", 255,
                             NULL);
    }
  else
    {
      clutter_actor_animate (rects[1], CLUTTER_EASE_OUT_CUBIC, 250,
                             "@constraints.green-x.offset", 0.0,
                             "opacity", 0,
                             NULL);
      clutter_actor_animate (rects[2], CLUTTER_EASE_OUT_CUBIC, 250,
                             "@constraints.blue-x.offset", 0.0,
                             "opacity", 0,
                             NULL);
    }

  is_expanded = !is_expanded;
}

G_MODULE_EXPORT int
test_bind_main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterConstraint *constraint;
  ClutterAction *action;
  ClutterColor color;

  clutter_init (&argc, &argv);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Bind Constraint");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, RECT_SIZE * 4.0, RECT_SIZE * 3.0);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* main rectangle */
  clutter_color_from_string (&color, colors[0]);
  rects[0] = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_size (rects[0], RECT_SIZE, RECT_SIZE);

  /* center it on the stage */
  clutter_actor_add_constraint (rects[0], clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.1));
  clutter_actor_add_constraint (rects[0], clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));

  /* make it clickable */
  action = clutter_click_action_new ();
  clutter_actor_add_action (rects[0], action);
  clutter_actor_set_reactive (rects[0], TRUE);
  g_signal_connect (action, "clicked", G_CALLBACK (on_click), NULL);

  /* second rectangle */
  clutter_color_from_string (&color, colors[1]);
  rects[1] = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_opacity (rects[1], 0);
  clutter_actor_add_constraint (rects[1], clutter_bind_constraint_new (rects[0], CLUTTER_BIND_WIDTH, 0.0));
  clutter_actor_add_constraint (rects[1], clutter_bind_constraint_new (rects[0], CLUTTER_BIND_HEIGHT, 0.0));
  constraint = clutter_bind_constraint_new (rects[0], CLUTTER_BIND_X, 0.0);
  clutter_actor_add_constraint_with_name (rects[1], "green-x", constraint);
  constraint = clutter_bind_constraint_new (rects[0], CLUTTER_BIND_Y, 0.0);
  clutter_actor_add_constraint_with_name (rects[1], "green-y", constraint);
  clutter_actor_set_name (rects[1], "green rect");

  /* third rectangle */
  clutter_color_from_string (&color, colors[2]);
  rects[2] = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_opacity (rects[2], 0);
  clutter_actor_add_constraint (rects[2], clutter_bind_constraint_new (rects[0], CLUTTER_BIND_WIDTH, 0.0));
  clutter_actor_add_constraint (rects[2], clutter_bind_constraint_new (rects[0], CLUTTER_BIND_HEIGHT, 0.0));
  constraint = clutter_bind_constraint_new (rects[0], CLUTTER_BIND_X, 0.0);
  clutter_actor_add_constraint_with_name (rects[2], "blue-x", constraint);
  constraint = clutter_bind_constraint_new (rects[0], CLUTTER_BIND_Y, 0.0);
  clutter_actor_add_constraint_with_name (rects[2], "blue-y", constraint);
  clutter_actor_set_name (rects[2], "blue rect");

  /* add everything to the stage */
  clutter_container_add (CLUTTER_CONTAINER (stage),
                         rects[2],
                         rects[1],
                         rects[0],
                         NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
