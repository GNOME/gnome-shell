#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include <gmodule.h>

#include <clutter/clutter.h>

G_MODULE_EXPORT int
test_opacity_main (int argc, char *argv[])
{
  ClutterActor *stage, *group1, *group2, *label, *rect;
  ClutterColor label_color = { 255, 0, 0, 128 };
  ClutterColor rect_color = { 0, 0, 255, 255 };
  ClutterColor color_check = { 0, };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  label = clutter_label_new_with_text ("Sans 18px", "Label, 50% opacity");
  clutter_label_set_color (CLUTTER_LABEL (label), &label_color);

  g_print ("label 50%%.get_color()/1\n");
  clutter_label_get_color (CLUTTER_LABEL (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (stage), label, NULL);
  clutter_actor_set_position (label, 10, 10);

  g_print ("label 50%%.get_color()/2\n");
  clutter_label_get_color (CLUTTER_LABEL (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  g_print ("label 50%%.get_paint_opacity() = %d\n",
           clutter_actor_get_paint_opacity (label));
  g_assert (clutter_actor_get_paint_opacity (label) == 128);

  clutter_actor_show (label);

  group1 = clutter_group_new ();
  clutter_actor_set_opacity (group1, 128);
  clutter_container_add (CLUTTER_CONTAINER (stage), group1, NULL);
  clutter_actor_set_position (group1, 10, 30);
  clutter_actor_show (group1);

  label = clutter_label_new_with_text ("Sans 18px", "Label+Group, 25% opacity");

  clutter_label_set_color (CLUTTER_LABEL (label), &label_color);

  g_print ("label 50%% + group 50%%.get_color()/1\n");
  clutter_label_get_color (CLUTTER_LABEL (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (group1), label, NULL);

  g_print ("label 50%% + group 50%%.get_color()/2\n");
  clutter_label_get_color (CLUTTER_LABEL (label), &color_check);
  g_assert (color_check.alpha == label_color.alpha);

  g_print ("label 50%% + group 50%%.get_paint_opacity() = %d\n",
           clutter_actor_get_paint_opacity (label));
  g_assert (clutter_actor_get_paint_opacity (label) == 64);

  clutter_actor_show (label);

  group2 = clutter_group_new ();
  clutter_container_add (CLUTTER_CONTAINER (group1), group2, NULL);
  clutter_actor_set_position (group2, 10, 60);
  clutter_actor_show (group2);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_size (rect, 128, 128);

  g_print ("rect 100%% + group 100%% + group 50%%.get_color()/1\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (group2), rect, NULL);

  g_print ("rect 100%% + group 100%% + group 50%%.get_color()/2\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  g_print ("rect 100%%.get_paint_opacity() = %d\n",
           clutter_actor_get_paint_opacity (rect));
  g_assert (clutter_actor_get_paint_opacity (rect) == 128);

  clutter_actor_show (rect);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_size (rect, 128, 128);
  clutter_actor_set_position (rect, 150, 90);

  g_print ("rect 100%%.get_color()/1\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  clutter_container_add (CLUTTER_CONTAINER (stage), rect, NULL);

  g_print ("rect 100%%.get_color()/2\n");
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color_check);
  g_assert (color_check.alpha == rect_color.alpha);

  g_print ("rect 100%%.get_paint_opacity() = %d\n",
           clutter_actor_get_paint_opacity (rect));
  g_assert (clutter_actor_get_paint_opacity (rect) == 255);

  clutter_actor_show (rect);

  clutter_actor_show_all (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
