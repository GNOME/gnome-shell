/*
 * Copyright 2012 Bastian Winkler <buz@netbuz.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 */
#include <stdlib.h>
#include <clutter/clutter.h>

#define INSTRUCTIONS \
        "Press r\t\342\236\236\tSwitch row homogeneous\n"               \
        "Press c\t\342\236\236\tSwitch column homogeneous\n"            \
        "Press s\t\342\236\236\tIncrement spacing (up to 12px)\n"       \
        "Press q\t\342\236\236\tQuit\n\n"                               \
        "Left/right click\t\t\342\236\236\tChange actor align\n"        \
        "Shift left/right click\t\342\236\236\tChange actor expand"

static gboolean random_size = FALSE;
static gboolean random_align = FALSE;
static gboolean default_expand = TRUE;
static gboolean use_box = FALSE;
static gboolean is_vertical = FALSE;

static GOptionEntry entries[] = {
  {
    "random-size", 'r',
    0,
    G_OPTION_ARG_NONE,
    &random_size,
    "Randomly size the rectangles", NULL
  },
  {
    "random-align", 'f',
    0,
    G_OPTION_ARG_NONE,
    &random_align,
    "Randomly set the align values", NULL
  },
  {
    "no-expand", 'e',
    G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE,
    &default_expand,
    "Don't expand all actors by default", NULL,
  },
  {
    "box", 'b',
    0,
    G_OPTION_ARG_NONE,
    &use_box,
    "Use the layout in a ClutterBoxLayout style", NULL
  },
  {
    "vertical", 'v',
    0,
    G_OPTION_ARG_NONE,
    &is_vertical,
    "Use a vertical orientation when used with --box", NULL
  },
  { NULL }
};

static gboolean
button_release_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      data)
{
  ClutterActorAlign x_align, y_align;
  gboolean x_expand, y_expand;

  g_object_get (actor,
                "x-align", &x_align,
                "y-align", &y_align,
                "x-expand", &x_expand,
                "y-expand", &y_expand,
                NULL);

  switch (clutter_event_get_button (event))
    {
    case CLUTTER_BUTTON_PRIMARY:
      if (clutter_event_has_shift_modifier (event))
        x_expand = !x_expand;
      else
        {
          if (x_align < 3)
            x_align += 1;
          else
            x_align = 0;
        }
      break;

    case CLUTTER_BUTTON_SECONDARY:
      if (clutter_event_has_shift_modifier (event))
        y_expand = !y_expand;
      else
        {
          if (y_align < 3)
            y_align += 1;
          else
            y_align = 0;
        }
      break;

    default:
      return FALSE;
    }

  g_object_set (actor,
                "x-align", x_align,
                "y-align", y_align,
                "x-expand", x_expand,
                "y-expand", y_expand,
                NULL);
  return TRUE;
}

static const gchar *
get_align_name (ClutterActorAlign align)
{
  switch (align)
    {
    case CLUTTER_ACTOR_ALIGN_FILL:
      return "fill";

    case CLUTTER_ACTOR_ALIGN_START:
      return "start";

    case CLUTTER_ACTOR_ALIGN_CENTER:
      return "center";

    case CLUTTER_ACTOR_ALIGN_END:
      return "end";

    default:
      g_assert_not_reached ();
    }
}
static void
changed_cb (ClutterActor *actor,
            GParamSpec   *pspec,
            ClutterActor *text)
{
  ClutterActorAlign x_align, y_align;
  ClutterActor *box;
  ClutterLayoutManager *layout;
  ClutterLayoutMeta *meta;
  gboolean x_expand, y_expand;
  gchar *label;
  gint left, top, width, height;

  box = clutter_actor_get_parent (actor);
  layout = clutter_actor_get_layout_manager (box);
  meta = clutter_layout_manager_get_child_meta (layout,
                                                CLUTTER_CONTAINER (box),
                                                actor);

  g_object_get (actor,
                "x-align", &x_align,
                "y-align", &y_align,
                "x-expand", &x_expand,
                "y-expand", &y_expand,
                NULL);

  g_object_get (meta,
                "left-attach", &left,
                "top-attach", &top,
                "width", &width,
                "height", &height,
                NULL);

  label = g_strdup_printf ("attach: %d,%d\n"
                           "span: %d,%d\n"
                           "expand: %d,%d\n"
                           "align: %s,%s",
                           left, top, width, height,
                           x_expand, y_expand,
                           get_align_name (x_align),
                           get_align_name (y_align));
  clutter_text_set_text (CLUTTER_TEXT (text), label);
  g_free (label);
}

static void
add_actor (ClutterActor *box,
           gint          left,
           gint          top,
           gint          width,
           gint          height)
{
  ClutterActor *rect, *text;
  ClutterColor color;
  ClutterLayoutManager *layout;

  clutter_color_from_hls (&color,
                          g_random_double_range (0.0, 360.0),
                          0.5,
                          0.5);
  color.alpha = 255;

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);
  rect = clutter_actor_new ();
  clutter_actor_set_layout_manager (rect, layout);
  clutter_actor_set_background_color (rect, &color);
  clutter_actor_set_reactive (rect, TRUE);

  if (random_size)
    clutter_actor_set_size (rect,
                            g_random_int_range (40, 80),
                            g_random_int_range (40, 80));
  else
    clutter_actor_set_size (rect, 60, 60);

  clutter_actor_set_x_expand (rect, default_expand);
  clutter_actor_set_y_expand (rect, default_expand);

  if (!default_expand)
    {
      clutter_actor_set_x_align (rect, CLUTTER_ACTOR_ALIGN_CENTER);
      clutter_actor_set_y_align (rect, CLUTTER_ACTOR_ALIGN_CENTER);
    }

  if (random_align)
    {
      clutter_actor_set_x_align (rect, g_random_int_range (0, 3));
      clutter_actor_set_y_align (rect, g_random_int_range (0, 3));
    }

  text = clutter_text_new_with_text ("Sans 8px", NULL);
  clutter_text_set_line_alignment (CLUTTER_TEXT (text),
                                   PANGO_ALIGN_CENTER);
  clutter_actor_add_child (rect, text);

  g_signal_connect (rect, "button-release-event",
                    G_CALLBACK (button_release_cb), NULL);
  g_signal_connect (rect, "notify::x-expand",
                    G_CALLBACK (changed_cb), text);
  g_signal_connect (rect, "notify::y-expand",
                    G_CALLBACK (changed_cb), text);
  g_signal_connect (rect, "notify::x-align",
                    G_CALLBACK (changed_cb), text);
  g_signal_connect (rect, "notify::y-align",
                    G_CALLBACK (changed_cb), text);

  layout = clutter_actor_get_layout_manager (box);
  if (use_box)
    clutter_actor_add_child (box, rect);
  else
    clutter_grid_layout_attach (CLUTTER_GRID_LAYOUT (layout), rect,
                                left, top, width, height);
  changed_cb (rect, NULL, text);
}

static gboolean
key_release_cb (ClutterActor *stage,
                ClutterEvent *event,
                ClutterActor *box)
{
  ClutterGridLayout *layout;
  gboolean toggle;
  guint spacing;

  layout = CLUTTER_GRID_LAYOUT (clutter_actor_get_layout_manager (box));

  switch (clutter_event_get_key_symbol (event))
    {
    case CLUTTER_KEY_c:
      toggle = clutter_grid_layout_get_column_homogeneous (layout);
      clutter_grid_layout_set_column_homogeneous (layout, !toggle);
      break;

    case CLUTTER_KEY_r:
      toggle = clutter_grid_layout_get_row_homogeneous (layout);
      clutter_grid_layout_set_row_homogeneous (layout, !toggle);
      break;

    case CLUTTER_KEY_s:
      spacing = clutter_grid_layout_get_column_spacing (layout);
      if (spacing < 12)
        spacing += 1;
      else
        spacing = 0;
      clutter_grid_layout_set_column_spacing (layout, spacing);
      clutter_grid_layout_set_row_spacing (layout, spacing);
      break;

    case CLUTTER_KEY_q:
      clutter_main_quit ();
      break;

    default:
      return FALSE;
    }

  return TRUE;
}
int
main (int argc, char *argv[])
{
  ClutterActor *stage, *box, *instructions;
  ClutterLayoutManager *stage_layout, *grid_layout;
  GError *error = NULL;

  if (clutter_init_with_args (&argc, &argv,
                              NULL,
                              entries,
                              NULL,
                              &error) != CLUTTER_INIT_SUCCESS)
    {
      g_print ("Unable to run grid-layout: %s", error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  stage = clutter_stage_new ();
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  stage_layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (stage_layout),
                                      CLUTTER_ORIENTATION_VERTICAL);
  clutter_actor_set_layout_manager (stage, stage_layout);

  grid_layout = clutter_grid_layout_new ();
  if (is_vertical)
    clutter_grid_layout_set_orientation (CLUTTER_GRID_LAYOUT (grid_layout),
                                         CLUTTER_ORIENTATION_VERTICAL);
  box = clutter_actor_new ();
  clutter_actor_set_background_color (box, CLUTTER_COLOR_LightGray);
  clutter_actor_set_x_expand (box, TRUE);
  clutter_actor_set_y_expand (box, TRUE);
  clutter_actor_set_layout_manager (box, grid_layout);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (stage_layout), box,
                           TRUE, TRUE, TRUE,
                           CLUTTER_BOX_ALIGNMENT_CENTER,
                           CLUTTER_BOX_ALIGNMENT_CENTER);

  add_actor (box, 0, 0, 1, 1);
  add_actor (box, 1, 0, 1, 1);
  add_actor (box, 2, 0, 1, 1);
  add_actor (box, 0, 1, 1, 1);
  add_actor (box, 1, 1, 2, 1);
  add_actor (box, 0, 2, 3, 1);
  add_actor (box, 0, 3, 2, 2);
  add_actor (box, 2, 3, 1, 1);
  add_actor (box, 2, 4, 1, 1);

  instructions = clutter_text_new_with_text ("Sans 12px", INSTRUCTIONS);
  clutter_actor_set_margin_top (instructions, 4);
  clutter_actor_set_margin_left (instructions, 4);
  clutter_actor_set_margin_bottom (instructions, 4);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (stage_layout), instructions,
                           FALSE, TRUE, FALSE,
                           CLUTTER_BOX_ALIGNMENT_START,
                           CLUTTER_BOX_ALIGNMENT_CENTER);

  g_signal_connect (stage, "destroy",
                    G_CALLBACK (clutter_main_quit), NULL);
  g_signal_connect (stage, "key-release-event",
                    G_CALLBACK (key_release_cb), box);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
