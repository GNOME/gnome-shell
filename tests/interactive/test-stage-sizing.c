#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static gboolean
fullscreen_clicked_cb (ClutterStage *stage)
{
  clutter_stage_set_fullscreen (stage, !clutter_stage_get_fullscreen (stage));
  return CLUTTER_EVENT_STOP;
}

static gboolean
resize_clicked_cb (ClutterStage *stage)
{
  clutter_stage_set_user_resizable (stage,
                                    !clutter_stage_get_user_resizable (stage));
  return CLUTTER_EVENT_STOP;
}

static gboolean
shrink_clicked_cb (ClutterActor *stage)
{
  gfloat width, height;
  clutter_actor_get_size (stage, &width, &height);
  clutter_actor_set_size (stage, MAX (0, width - 10.f), MAX (0, height - 10.f));
  return CLUTTER_EVENT_STOP;
}

static gboolean
expand_clicked_cb (ClutterActor *stage)
{
  gfloat width, height;
  clutter_actor_get_size (stage, &width, &height);
  clutter_actor_set_size (stage, width + 10.f, height + 10.f);
  return CLUTTER_EVENT_STOP;
}

static void
on_fullscreen (ClutterStage *stage)
{
  float width, height;
  gboolean is_fullscreen;

  is_fullscreen = clutter_stage_get_fullscreen (stage);

  clutter_actor_get_size (CLUTTER_ACTOR (stage), &width, &height);

  g_print ("Stage size [%s]: %d x %d\n",
           is_fullscreen ? "fullscreen" : "not fullscreen",
           (int) width,
           (int) height);
}

G_MODULE_EXPORT int
test_stage_sizing_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect, *label, *box;
  ClutterMargin margin = { 12.f, 12.f, 6.f, 6.f };

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Stage Sizing");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  g_signal_connect_after (stage, "notify::fullscreen-set", G_CALLBACK (on_fullscreen), NULL);

  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, clutter_box_layout_new ());
  clutter_actor_add_constraint (box, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_add_child (stage, box);

  rect = clutter_actor_new ();
  clutter_actor_set_layout_manager (rect,
                                    clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                            CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_actor_set_background_color (rect, CLUTTER_COLOR_LightScarletRed);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (fullscreen_clicked_cb),
                            stage);
  label = clutter_text_new_with_text ("Sans 16", "Toggle fullscreen");
  clutter_actor_set_margin (label, &margin);
  clutter_actor_add_child (rect, label);
  clutter_actor_add_child (box, rect);

  rect = clutter_actor_new ();
  clutter_actor_set_layout_manager (rect,
                                    clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                            CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_actor_set_background_color (rect, CLUTTER_COLOR_Chameleon);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (resize_clicked_cb), stage);
  label = clutter_text_new_with_text ("Sans 16", "Toggle resizable");
  clutter_actor_set_margin (label, &margin);
  clutter_actor_add_child (rect, label);
  clutter_actor_add_child (box, rect);

  rect = clutter_actor_new ();
  clutter_actor_set_layout_manager (rect,
                                    clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                            CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_actor_set_background_color (rect, CLUTTER_COLOR_SkyBlue);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (shrink_clicked_cb), stage);
  label = clutter_text_new_with_text ("Sans 16", "Shrink");
  clutter_actor_set_margin (label, &margin);
  clutter_actor_add_child (rect, label);
  clutter_actor_add_child (box, rect);

  rect = clutter_actor_new ();
  clutter_actor_set_layout_manager (rect,
                                    clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                            CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_actor_set_background_color (rect, CLUTTER_COLOR_Butter);
  clutter_actor_set_reactive (rect, TRUE);
  g_signal_connect_swapped (rect, "button-press-event",
                            G_CALLBACK (expand_clicked_cb), stage);
  label = clutter_text_new_with_text ("Sans 16", "Expand");
  clutter_actor_set_margin (label, &margin);
  clutter_actor_add_child (rect, label);
  clutter_actor_add_child (box, rect);

  clutter_stage_set_minimum_size (CLUTTER_STAGE (stage),
                                  clutter_actor_get_width (box),
                                  clutter_actor_get_height (box));

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_stage_sizing_describe (void)
{
  return "Check stage sizing policies.";
}
