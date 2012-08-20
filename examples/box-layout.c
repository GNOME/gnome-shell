/*
 * Copyright 2009 Intel Corporation.
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
        "Press v\t\342\236\236\tSwitch horizontal/vertical\n"           \
        "Press h\t\342\236\236\tToggle homogeneous\n"			\
        "Press p\t\342\236\236\tToggle pack start/end\n"                \
        "Press s\t\342\236\236\tIncrement spacing (up to 12px)\n"       \
        "Press +\t\342\236\236\tAdd a new actor\n"                      \
        "Press a\t\342\236\236\tToggle animations\n"                    \
        "Press q\t\342\236\236\tQuit"


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

static gboolean
button_release_cb (ClutterActor *rect,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  ClutterActorAlign x_align, y_align;
  gboolean x_expand, y_expand;

  g_object_get (rect,
                "x-align", &x_align,
                "y-align", &y_align,
                "x-expand", &x_expand,
                "y-expand", &y_expand,
                NULL);

  switch (clutter_event_get_button (event))
    {
    case CLUTTER_BUTTON_PRIMARY:
      if (clutter_event_has_shift_modifier (event))
        {
          if (y_align < 3)
            y_align += 1;
          else
            y_align = 0;
          break;
        }
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
        x_expand = !x_expand;
      break;

    default:
      break;
    }

  g_object_set (rect,
                "x-align", x_align,
                "y-align", y_align,
                "x-expand", x_expand,
                "y-expand", y_expand,
                NULL);
  return TRUE;
}

static void
changed_cb (ClutterActor *actor,
            GParamSpec   *pspec,
            ClutterActor *text)
{
  ClutterActorAlign x_align, y_align;
  gboolean x_expand, y_expand;
  gchar *label;

  g_object_get (actor,
                "x-align", &x_align,
                "y-align", &y_align,
                "x-expand", &x_expand,
                "y-expand", &y_expand,
                NULL);

  label = g_strdup_printf ("%d,%d\n"
                           "%s\n%s",
                           x_expand, y_expand,
                           get_align_name (x_align),
                           get_align_name (y_align));
  clutter_text_set_text (CLUTTER_TEXT (text), label);
  g_free (label);
}

static void
add_actor (ClutterActor *box,
           gint          position)
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
  clutter_actor_set_size (rect, 32, 64);
  clutter_actor_set_x_expand (rect, TRUE);
  clutter_actor_set_y_expand (rect, TRUE);
  clutter_actor_set_x_align (rect, CLUTTER_ACTOR_ALIGN_CENTER);
  clutter_actor_set_y_align (rect, CLUTTER_ACTOR_ALIGN_CENTER);

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
  changed_cb (rect, NULL, text);

  clutter_actor_insert_child_at_index (box, rect, position);
}

static gboolean
key_release_cb (ClutterActor *stage,
                ClutterEvent *event,
                ClutterActor *box)
{
  ClutterBoxLayout *layout;
  gboolean toggle;
  guint spacing;

  layout = CLUTTER_BOX_LAYOUT (clutter_actor_get_layout_manager (box));

  switch (clutter_event_get_key_symbol (event))
    {
    case CLUTTER_KEY_a:
      {
        ClutterActorIter iter;
        ClutterActor *child;

        clutter_actor_iter_init (&iter, box);
        while (clutter_actor_iter_next (&iter, &child))
          {
            guint duration;

            duration = clutter_actor_get_easing_duration (child);
            if (duration != 0)
              duration = 0;
            else
              duration = 250;

            clutter_actor_set_easing_duration (child, duration);
          }
      }
      break;

    case CLUTTER_KEY_v:
      {
        ClutterOrientation orientation;

        orientation = clutter_box_layout_get_orientation (layout);

        if (orientation == CLUTTER_ORIENTATION_HORIZONTAL)
          orientation = CLUTTER_ORIENTATION_VERTICAL;
        else
          orientation = CLUTTER_ORIENTATION_HORIZONTAL;

        clutter_box_layout_set_orientation (layout, orientation);
      }
      break;

    case CLUTTER_KEY_h:
      toggle = clutter_box_layout_get_homogeneous (layout);
      clutter_box_layout_set_homogeneous (layout, !toggle);
      break;

    case CLUTTER_KEY_p:
      toggle = clutter_box_layout_get_pack_start (layout);
      clutter_box_layout_set_pack_start (layout, !toggle);
      break;

    case CLUTTER_KEY_s:
      spacing = clutter_box_layout_get_spacing (layout);

      if (spacing > 12)
        spacing = 0;
      else
        spacing++;

      clutter_box_layout_set_spacing (layout, spacing);
      break;

    case CLUTTER_KEY_plus:
      add_actor (box, g_random_int_range (0, clutter_actor_get_n_children (box)));
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
  ClutterLayoutManager *layout;
  gint i;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Box Layout");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  /* make the stage a vbox */
  layout = clutter_box_layout_new ();
  clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (layout),
                                      CLUTTER_ORIENTATION_VERTICAL);
  clutter_actor_set_layout_manager (stage, layout);

  box = clutter_actor_new ();
  clutter_actor_set_background_color (box, CLUTTER_COLOR_LightGray);
  clutter_actor_set_x_expand (box, TRUE);
  clutter_actor_set_y_expand (box, TRUE);
  layout = clutter_box_layout_new ();
  clutter_actor_set_layout_manager (box, layout);
  clutter_actor_add_child (stage, box);

  instructions = clutter_text_new_with_text ("Sans 12px", INSTRUCTIONS);
  clutter_actor_set_x_expand (instructions, TRUE);
  clutter_actor_set_y_expand (instructions, FALSE);
  clutter_actor_set_x_align (instructions, CLUTTER_ACTOR_ALIGN_START);
  clutter_actor_set_margin_top (instructions, 4);
  clutter_actor_set_margin_left (instructions, 4);
  clutter_actor_set_margin_bottom (instructions, 4);
  clutter_actor_add_child (stage, instructions);

  for (i = 0; i < 5; i++)
    add_actor (box, i);

  g_signal_connect (stage, "destroy",
                    G_CALLBACK (clutter_main_quit), NULL);
  g_signal_connect (stage, "key-release-event",
                    G_CALLBACK (key_release_cb), box);

  clutter_actor_show (stage);
  clutter_main ();

  return EXIT_SUCCESS;
}
