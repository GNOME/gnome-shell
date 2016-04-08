#include <stdio.h>
#include <stdlib.h>

#include <gmodule.h>

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

#define FONT "Sans 12"

static void
set_text (ClutterActor *actor, const gchar *text)
{
  GList *children, *l;

  children = clutter_container_get_children (CLUTTER_CONTAINER (actor));
  for (l = children; l; l = g_list_next (l)) {
    if (CLUTTER_IS_TEXT (l->data)) {
      clutter_text_set_text (CLUTTER_TEXT (l->data), text);
      break;
    }
  }
  g_list_free (children);
}

static void
toggle_expand (ClutterActor *actor, ClutterEvent *event, ClutterBox *box)
{
  gboolean x_expand;
  gchar *label;
  ClutterLayoutManager *layout = clutter_box_get_layout_manager (box);


  clutter_layout_manager_child_get (layout, CLUTTER_CONTAINER (box), actor,
				    "x-expand", &x_expand,
				    NULL);

  x_expand = !x_expand;

  clutter_layout_manager_child_set (layout, CLUTTER_CONTAINER (box), actor,
				    "x-expand", x_expand,
				    "y-expand", x_expand,
				    NULL);

  label = g_strdup_printf ("Expand = %d", x_expand);
  set_text (actor, label);

  g_free (label);
}

static const gchar *
get_alignment_name (ClutterTableAlignment alignment)
{
  switch (alignment)
    {
    case CLUTTER_TABLE_ALIGNMENT_START:
      return "start";

    case CLUTTER_TABLE_ALIGNMENT_CENTER:
      return "center";

    case CLUTTER_TABLE_ALIGNMENT_END:
      return "end";
    }

  return "undefined";
}

static void
randomise_align (ClutterActor *actor, ClutterEvent *event, ClutterBox *box)
{
  ClutterTableAlignment x_align, y_align;
  gchar *label;
  ClutterLayoutManager *layout;

  layout = clutter_box_get_layout_manager (box);

  x_align = (ClutterTableAlignment) g_random_int_range (0, 3);
  y_align = (ClutterTableAlignment) g_random_int_range (0, 3);

  clutter_layout_manager_child_set (layout, CLUTTER_CONTAINER (box), actor,
				    "x-align", x_align,
				    "y-align", y_align,
				    NULL);

  label = g_strdup_printf ("Align (%s, %s)",
                           get_alignment_name (x_align),
                           get_alignment_name (y_align));
  set_text (actor, label);
  g_free (label);
}

static void
toggle_visible (ClutterActor *actor, ClutterEvent *event, gpointer userdata)
{
  clutter_actor_hide (actor);
}

gboolean drag = FALSE;

static ClutterActor *
create_cell (ClutterActor *actor, const gchar *color_str)
{
  ClutterActor *result;
  ClutterActor *rectangle;
  ClutterColor color;

  result =
    clutter_box_new (clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FILL,
                                             CLUTTER_BIN_ALIGNMENT_FILL));

  rectangle = clutter_rectangle_new ();
  clutter_color_from_string (&color, color_str);
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rectangle), (const ClutterColor *) &color);
  clutter_color_from_string (&color, "#000f");
  clutter_rectangle_set_border_color (CLUTTER_RECTANGLE (rectangle), (const ClutterColor *) &color);
  clutter_rectangle_set_border_width (CLUTTER_RECTANGLE (rectangle), 2);

  clutter_actor_show (rectangle);
  clutter_actor_set_reactive (result, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (result), rectangle);
  clutter_box_pack (CLUTTER_BOX (result), actor,
                    "x-align", CLUTTER_BIN_ALIGNMENT_CENTER,
                    "y-align", CLUTTER_BIN_ALIGNMENT_CENTER,
                    NULL);

  return result;
}

static ClutterActor *
create_text (const gchar *label, const gchar *color)
{
  ClutterActor *text;
  ClutterActor *result;

  text = clutter_text_new_with_text (FONT, label);
  clutter_actor_show (text);

  result = create_cell (text, color);
  clutter_actor_show (result);

  return result;
}

static ClutterActor *
create_image (const gchar *file, const gchar *color)
{
  ClutterActor *texture;
  ClutterActor *result;

  texture = clutter_texture_new_from_file (file, NULL);
  g_object_set (G_OBJECT (texture), "keep-aspect-ratio", TRUE, NULL);
  clutter_actor_show (texture);

  result = create_cell (texture, color);
  clutter_actor_show (result);

  return result;
}

G_MODULE_EXPORT int
test_table_layout_main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterLayoutManager *layout;
  ClutterActor *actor1, *actor2, *actor3, *actor4, *actor5, *actor6, *actor7, *actor8, *actor9, *actor10;
  ClutterActor *box;
  gchar *file;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Table Layout");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_size (stage, 640, 480);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  layout = clutter_table_layout_new ();
  clutter_table_layout_set_column_spacing (CLUTTER_TABLE_LAYOUT (layout), 10);
  clutter_table_layout_set_row_spacing (CLUTTER_TABLE_LAYOUT (layout), 10);
  clutter_table_layout_set_use_animations (CLUTTER_TABLE_LAYOUT (layout), TRUE);

  box = clutter_box_new (layout);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);
  clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage, CLUTTER_BIND_WIDTH, -10.0));
  clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage, CLUTTER_BIND_HEIGHT, -10.0));

  actor1 = create_text ("label 1", "#f66f");
  file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  actor2 = create_image (file, "#bbcf");
  g_free (file);
  actor3 = create_text ("label 3", "#6f6f");
  actor4 = create_text ("Expand = 1", "#66ff");
  actor5 = create_text ("label 5", "#f6ff");
  actor6 = create_text ("label 6", "#6fff");
  actor7 = create_text ("Align (center, center)", "#66ff");
  actor8 = create_text ("label 8", "#ffff");
  actor9 = create_text ("label 9", "#666f");
  actor10 = create_text ("label 10", "#aaaf");

  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor1, 0, 0);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor2, 1, 0);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor3, 1, 1);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor4, 0, 2);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor5, 0, 3);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor6, 1, 3);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor7, 1, 4);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor8, 0, 4);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor9, 0, 5);
  clutter_table_layout_pack (CLUTTER_TABLE_LAYOUT (layout), actor10, 0, -1);
  clutter_table_layout_set_span (CLUTTER_TABLE_LAYOUT (layout), actor1, 1, 2);
  clutter_table_layout_set_span (CLUTTER_TABLE_LAYOUT (layout), actor7, 1, 2);
  clutter_table_layout_set_span (CLUTTER_TABLE_LAYOUT (layout), actor4, 2, 1);

  clutter_actor_set_size (actor1, 100, 100);
  clutter_actor_set_width (actor4, 250);

  clutter_layout_manager_child_set (CLUTTER_LAYOUT_MANAGER (layout),
				    CLUTTER_CONTAINER (box),
				    actor1,
				    "x-expand", FALSE, "y-expand", FALSE,
				    NULL);
  clutter_layout_manager_child_set (CLUTTER_LAYOUT_MANAGER (layout),
				    CLUTTER_CONTAINER (box),
				    actor4,
				    "x-expand", TRUE, "y-expand", TRUE,
				    "x-fill", TRUE, "y-fill", TRUE,
				    NULL);
  clutter_layout_manager_child_set (CLUTTER_LAYOUT_MANAGER (layout),
				    CLUTTER_CONTAINER (box),
				    actor7,
				    "x-expand", TRUE, "y-expand", TRUE,
				    "x-fill", FALSE, "y-fill", FALSE,
				    NULL);
  clutter_layout_manager_child_set (CLUTTER_LAYOUT_MANAGER (layout),
				    CLUTTER_CONTAINER (box),
				    actor8,
				    "x-expand", FALSE, "y-expand", FALSE,
				    NULL);
  clutter_layout_manager_child_set (CLUTTER_LAYOUT_MANAGER (layout),
				    CLUTTER_CONTAINER (box),
				    actor9,
				    "x-expand", FALSE, "y-expand", FALSE,
				    NULL);

  clutter_layout_manager_child_set (CLUTTER_LAYOUT_MANAGER (layout),
				    CLUTTER_CONTAINER (box),
				    actor2,
				    "y-fill", FALSE,
				    "x-fill", FALSE,
				    NULL);

  clutter_actor_set_position (box, 5, 5);

  g_signal_connect (actor4, "button-release-event", G_CALLBACK (toggle_expand), box);
  g_signal_connect (actor7, "button-release-event", G_CALLBACK (randomise_align), box);
  g_signal_connect (actor10, "button-release-event", G_CALLBACK (toggle_visible), NULL);

  /* g_signal_connect (stage, "button-press-event", G_CALLBACK (button_press), */
  /*                   box); */
  /* g_signal_connect (stage, "motion-event", G_CALLBACK (motion_event), */
  /*                   box); */
  /* g_signal_connect (stage, "button-release-event", G_CALLBACK (button_release), */
  /*                   box); */

  clutter_actor_show (stage);

  g_debug ("table row count = %d",
           clutter_table_layout_get_row_count (CLUTTER_TABLE_LAYOUT (layout)));
  g_debug ("table column count = %d",
           clutter_table_layout_get_column_count (CLUTTER_TABLE_LAYOUT (layout)));

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_table_layout_describe (void)
{
  return "TableLayout layout manager example.";
}
