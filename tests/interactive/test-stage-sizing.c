#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static gboolean
fullscreen_clicked_cb (ClutterStage *stage)
{
  clutter_stage_set_fullscreen (stage, !clutter_stage_get_fullscreen (stage));
  return TRUE;
}

static gboolean
resize_clicked_cb (ClutterStage *stage)
{
  clutter_stage_set_user_resizable (stage,
                                    !clutter_stage_get_user_resizable (stage));
  return TRUE;
}

static gboolean
shrink_clicked_cb (ClutterActor *stage)
{
  gfloat width, height;
  clutter_actor_get_size (stage, &width, &height);
  clutter_actor_set_size (stage, MAX (0, width - 10.f), MAX (0, height - 10.f));
  return TRUE;
}

static gboolean
expand_clicked_cb (ClutterActor *stage)
{
  gfloat width, height;
  clutter_actor_get_size (stage, &width, &height);
  clutter_actor_set_size (stage, width + 10.f, height + 10.f);
  return TRUE;
}

G_MODULE_EXPORT int
test_stage_sizing_main (int argc, char *argv[])
{
  gfloat width;
  ClutterColor color;
  ClutterActor *stage, *rect, *label;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  label = clutter_text_new_with_text ("Sans 16", "Toggle fullscreen");
  clutter_color_from_string (&color, "light red");
  rect = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_size (rect,
                          clutter_actor_get_width (label) + 20,
                          clutter_actor_get_height (label) + 20);
  clutter_container_add (CLUTTER_CONTAINER (stage), rect, label, NULL);
  clutter_actor_set_position (label, 10, 10);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (fullscreen_clicked_cb), stage);
  width = clutter_actor_get_width (rect);

  label = clutter_text_new_with_text ("Sans 16", "Toggle resizable");
  clutter_color_from_string (&color, "light green");
  rect = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_size (rect,
                          clutter_actor_get_width (label) + 20,
                          clutter_actor_get_height (label) + 20);
  clutter_container_add (CLUTTER_CONTAINER (stage), rect, label, NULL);
  clutter_actor_set_x (rect, width);
  clutter_actor_set_position (label, 10 + width, 10);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (resize_clicked_cb), stage);
  width += clutter_actor_get_width (rect);

  label = clutter_text_new_with_text ("Sans 16", "Shrink");
  clutter_color_from_string (&color, "light blue");
  rect = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_size (rect,
                          clutter_actor_get_width (label) + 20,
                          clutter_actor_get_height (label) + 20);
  clutter_container_add (CLUTTER_CONTAINER (stage), rect, label, NULL);
  clutter_actor_set_x (rect, width);
  clutter_actor_set_position (label, 10 + width, 10);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (shrink_clicked_cb), stage);
  width += clutter_actor_get_width (rect);

  label = clutter_text_new_with_text ("Sans 16", "Expand");
  clutter_color_from_string (&color, "light yellow");
  rect = clutter_rectangle_new_with_color (&color);
  clutter_actor_set_size (rect,
                          clutter_actor_get_width (label) + 20,
                          clutter_actor_get_height (label) + 20);
  clutter_container_add (CLUTTER_CONTAINER (stage), rect, label, NULL);
  clutter_actor_set_x (rect, width);
  clutter_actor_set_position (label, 10 + width, 10);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (expand_clicked_cb), stage);
  width += clutter_actor_get_width (rect);

  clutter_stage_set_minimum_size (CLUTTER_STAGE (stage),
                                  width,
                                  clutter_actor_get_height (rect));

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
