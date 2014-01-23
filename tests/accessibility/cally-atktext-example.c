/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2009 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <atk/atk.h>
#include <clutter/clutter.h>

#include "cally-examples-util.h"

#define WIDTH 800
#define HEIGHT 600

static ClutterActor *text_actor          = NULL;
static ClutterActor *text_editable_actor = NULL;

/*
 * Test AtkText interface
 */
static void
test_atk_text (ClutterActor *actor)
{
  gchar           *text       = NULL;
  AtkObject       *object     = NULL;
  AtkText         *cally_text = NULL;
  gboolean         bool       = FALSE;
  gunichar         unichar;
  gint             count      = -1;
  gint             start      = -1;
  gint             end        = -1;
  gint             pos        = -1;
  AtkAttributeSet *at_set     = NULL;
  GSList          *attrs;
  gchar           *buf;
  gint             x, y, width, height;

  object = atk_gobject_accessible_for_object (G_OBJECT (actor));
  cally_text = ATK_TEXT (object);

  if (!cally_text)
      return;

  text = atk_text_get_text (cally_text, 0, -1);
  g_print ("atk_text_get_text output: %s\n", text);

  unichar = atk_text_get_character_at_offset (cally_text, 5);
  buf = g_ucs4_to_utf8 (&unichar, 1, NULL, NULL, NULL);
  g_print ("atk_text_get_character_at_offset(5): '%s' vs '%c'\n", buf, text[5]);
  g_free (text); text = NULL;
  g_free (buf); buf = NULL;

  text = atk_text_get_text_before_offset (cally_text,
                                          5, ATK_TEXT_BOUNDARY_WORD_END,
                                          &start, &end);
  g_print ("atk_text_get_text_before_offset: %s, %i, %i\n",
           text, start, end);
  g_free (text); text = NULL;

  text = atk_text_get_text_at_offset (cally_text,
                                      5, ATK_TEXT_BOUNDARY_WORD_END,
                                      &start, &end);
  g_print ("atk_text_get_text_at_offset: %s, %i, %i\n",
           text, start, end);
  g_free (text); text = NULL;

  text = atk_text_get_text_after_offset (cally_text,
                                         5, ATK_TEXT_BOUNDARY_WORD_END,
                                         &start, &end);
  g_print ("atk_text_get_text_after_offset: %s, %i, %i\n",
           text, start, end);
  g_free (text); text = NULL;

  pos = atk_text_get_caret_offset (cally_text);
  g_print ("atk_text_get_caret_offset: %i\n", pos);

  atk_text_set_caret_offset (cally_text, 5);

  count = atk_text_get_character_count (cally_text);
  g_print ("atk_text_get_character_count: %i\n", count);

  count = atk_text_get_n_selections (cally_text);
  g_print ("atk_text_get_n_selections: %i\n", count);

  text = atk_text_get_selection (cally_text, 0, &start, &end);
  g_print ("atk_text_get_selection: %s, %i, %i\n", text, start, end);
  g_free(text); text = NULL;

  bool = atk_text_remove_selection (cally_text, 0);
  g_print ("atk_text_remove_selection (0): %i\n", bool);

  bool = atk_text_remove_selection (cally_text, 1);
  g_print ("atk_text_remove_selection (1): %i\n", bool);

  bool = atk_text_add_selection (cally_text, 5, 10);
  g_print ("atk_text_add_selection: %i\n", bool);

  bool = atk_text_set_selection (cally_text, 0, 6, 10);
  g_print ("atk_text_set_selection: %i\n", bool);

  at_set = atk_text_get_run_attributes (cally_text, 0,
                                        &start, &end);
  g_print ("atk_text_get_run_attributes: %i, %i\n", start, end);
  attrs = (GSList*) at_set;
  while (attrs)
    {
        AtkAttribute *at = (AtkAttribute *) attrs->data;
        g_print ("text run %s = %s\n", at->name, at->value);
        attrs = g_slist_next (attrs);
    }

  atk_text_get_character_extents (cally_text, 0, &x, &y, &width, &height,
                                  ATK_XY_WINDOW);
  g_print ("atk_text_get_character_extents (0, window): x=%i y=%i width=%i height=%i\n",
           x, y, width, height);

  atk_text_get_character_extents (cally_text, 0, &x, &y, &width, &height,
                                  ATK_XY_SCREEN);
  g_print ("atk_text_get_character_extents (0, screen): x=%i y=%i width=%i height=%i\n",
           x, y, width, height);

  pos = atk_text_get_offset_at_point (cally_text, 200, 10, ATK_XY_WINDOW);
  g_print ("atk_text_get_offset_at_point (200, 10, window): %i\n", pos);

  pos = atk_text_get_offset_at_point (cally_text, 200, 100, ATK_XY_SCREEN);
  g_print ("atk_text_get_offset_at_point (200, 100, screen): %i\n", pos);

}

static void
dump_actor_default_atk_attributes (ClutterActor *actor)
{
  AtkObject       *object     = NULL;
  AtkText         *cally_text = NULL;
  AtkAttributeSet *at_set     = NULL;
  GSList          *attrs;
  const gchar     *text_value = NULL;

  object = atk_gobject_accessible_for_object (G_OBJECT (actor));
  cally_text = ATK_TEXT (object);

  if (!cally_text)
      return;

  text_value = clutter_text_get_text (CLUTTER_TEXT (actor));
  g_print ("text value = %s\n", text_value);

  at_set = atk_text_get_default_attributes (cally_text);
  attrs = (GSList*) at_set;
  while (attrs) {
      AtkAttribute *at = (AtkAttribute *) attrs->data;
      g_print ("text default %s = %s\n", at->name, at->value);
      attrs = g_slist_next (attrs);
  }
}

static gboolean
button_press_cb (ClutterActor *actor,
                 ClutterButtonEvent *event,
                 gpointer data)
{
  test_atk_text (text_actor);
  test_atk_text (text_editable_actor);

  return TRUE;
}

static void
make_ui (ClutterActor *stage)
{
  ClutterColor  color_stage = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor  color_text  = { 0xff, 0x00, 0x00, 0xff };
  ClutterColor  color_sel   = { 0x00, 0xff, 0x00, 0x55 };
  ClutterColor  color_rect  = { 0x00, 0xff, 0xff, 0xff };
  ClutterColor  color_label = { 0x00, 0x00, 0x00, 0xff };
  ClutterActor *button      = NULL;
  ClutterActor *rectangle   = NULL;
  ClutterActor *label       = NULL;

  clutter_stage_set_color (CLUTTER_STAGE (stage), &color_stage);
  clutter_actor_set_size (stage, WIDTH, HEIGHT);

  /* text */
  text_actor = clutter_text_new_full ("Sans Bold 32px",
                                      "",
                                      &color_text);
  clutter_text_set_markup (CLUTTER_TEXT(text_actor),
                           "<span fgcolor=\"#FFFF00\" bgcolor=\"#00FF00\"><s>Lorem ipsum dolor sit amet</s></span>");
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), text_actor);
  dump_actor_default_atk_attributes (text_actor);

  /* text_editable */
  text_editable_actor = clutter_text_new_full ("Sans Bold 32px",
                                               "consectetur adipisicing elit",
                                               &color_text);
  clutter_actor_set_position (text_editable_actor, 20, 100);
  clutter_text_set_editable (CLUTTER_TEXT (text_editable_actor), TRUE);
  clutter_text_set_selectable (CLUTTER_TEXT (text_editable_actor), TRUE);
  clutter_text_set_selection_color (CLUTTER_TEXT (text_editable_actor),
                                    &color_sel);
  clutter_text_set_line_wrap (CLUTTER_TEXT (text_editable_actor), TRUE);
  clutter_actor_grab_key_focus (text_editable_actor);
  clutter_actor_set_reactive (text_editable_actor, TRUE);
  dump_actor_default_atk_attributes (text_editable_actor);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), text_editable_actor);

  /* test button */
  button = clutter_group_new ();
  rectangle = clutter_rectangle_new_with_color (&color_rect);
  clutter_actor_set_size (rectangle, 75, 35);

  label = clutter_text_new_full ("Sans Bold 32px",
                                 "Test", &color_label);
  clutter_actor_set_position (button, 100, 200);
  clutter_container_add_actor (CLUTTER_CONTAINER (button), rectangle);
  clutter_container_add_actor (CLUTTER_CONTAINER (button), label);
  clutter_actor_set_reactive (button, TRUE);

  g_signal_connect_after (button, "button-press-event",
                          G_CALLBACK (button_press_cb), NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), button);
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;

  g_set_application_name ("AtkText");

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  cally_util_a11y_init (&argc, &argv);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cally - AtkText Test");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  make_ui (stage);

  clutter_actor_show_all (stage);

  test_atk_text (text_actor);
  test_atk_text (text_editable_actor);

  clutter_main ();

  return 0;
}
