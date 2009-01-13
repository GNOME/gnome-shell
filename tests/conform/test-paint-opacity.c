#include <clutter/clutter.h>
#include <stdlib.h>

#include "test-conform-common.h"

void
test_label_opacity (TestConformSimpleFixture *fixture,
                    gpointer                  dummy)
{
  ClutterActor *stage;
  ClutterActor *label;
  ClutterColor label_color = { 255, 0, 0, 128 };
  ClutterColor color_check = { 0, };

  stage = clutter_stage_get_default ();

  label = clutter_text_new_with_text ("Sans 18px", "Label, 50% opacity");
  clutter_text_set_color (CLUTTER_TEXT (label), &label_color);

  if (g_test_verbose ())
    g_print ("label 50%%.get_color()/1\n");
  clutter_text_get_color (CLUTTER_TEXT (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (stage), label, NULL);
  clutter_actor_set_position (label, 10, 10);

  if (g_test_verbose ())
    g_print ("label 50%%.get_color()/2\n");
  clutter_text_get_color (CLUTTER_TEXT (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  if (g_test_verbose ())
    g_print ("label 50%%.get_paint_opacity()/1\n");
  g_assert (clutter_actor_get_paint_opacity (label) == 255);

  if (g_test_verbose ())
    g_print ("label 50%%.get_paint_opacity()/2\n");
  clutter_actor_set_opacity (label, 128);
  g_assert (clutter_actor_get_paint_opacity (label) == 128);

  clutter_actor_destroy (label);
}

void
test_rectangle_opacity (TestConformSimpleFixture *fixture,
                        gpointer                  dummy)
{
  ClutterActor *stage;
  ClutterActor *rect;
  ClutterColor rect_color = { 0, 0, 255, 255 };
  ClutterColor color_check = { 0, };

  stage = clutter_stage_get_default ();

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_size (rect, 128, 128);
  clutter_actor_set_position (rect, 150, 90);

  if (g_test_verbose ())
    g_print ("rect 100%%.get_color()/1\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (stage), rect, NULL);

  if (g_test_verbose ())
    g_print ("rect 100%%.get_color()/2\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  if (g_test_verbose ())
    g_print ("rect 100%%.get_paint_opacity()\n");
  g_assert (clutter_actor_get_paint_opacity (rect) == 255);

  clutter_actor_destroy (rect);
}

void
test_paint_opacity (TestConformSimpleFixture *fixture,
                    gpointer                  dummy)
{
  ClutterActor *stage, *group1, *group2;
  ClutterActor *label, *rect;
  ClutterColor label_color = { 255, 0, 0, 128 };
  ClutterColor rect_color = { 0, 0, 255, 255 };
  ClutterColor color_check = { 0, };

  stage = clutter_stage_get_default ();

  group1 = clutter_group_new ();
  clutter_actor_set_opacity (group1, 128);
  clutter_container_add (CLUTTER_CONTAINER (stage), group1, NULL);
  clutter_actor_set_position (group1, 10, 30);
  clutter_actor_show (group1);

  label = clutter_text_new_with_text ("Sans 18px", "Label+Group, 25% opacity");
  clutter_text_set_color (CLUTTER_TEXT (label), &label_color);

  if (g_test_verbose ())
    g_print ("label 50%% + group 50%%.get_color()/1\n");
  clutter_text_get_color (CLUTTER_TEXT (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (group1), label, NULL);

  if (g_test_verbose ())
    g_print ("label 50%% + group 50%%.get_color()/2\n");
  clutter_text_get_color (CLUTTER_TEXT (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  if (g_test_verbose ())
    g_print ("label 50%% + group 50%%.get_paint_opacity() = 128\n");
  g_assert (clutter_actor_get_paint_opacity (label) == 128);

  clutter_actor_destroy (label);

  group2 = clutter_group_new ();
  clutter_container_add (CLUTTER_CONTAINER (group1), group2, NULL);
  clutter_actor_set_position (group2, 10, 60);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_size (rect, 128, 128);

  if (g_test_verbose ())
    g_print ("rect 100%% + group 100%% + group 50%%.get_color()/1\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (group2), rect, NULL);

  if (g_test_verbose ())
    g_print ("rect 100%% + group 100%% + group 50%%.get_color()/2\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  if (g_test_verbose ())
    g_print ("rect 100%%.get_paint_opacity()\n");
  g_assert (clutter_actor_get_paint_opacity (rect) == 128);

  clutter_actor_destroy (rect);
  clutter_actor_destroy (group2);
  clutter_actor_destroy (group1);
}
