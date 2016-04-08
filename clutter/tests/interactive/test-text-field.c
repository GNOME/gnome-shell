#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static void
on_entry_activate (ClutterText *text,
                   gpointer     data)
{
  g_print ("Text activated: %s (cursor: %d, selection at: %d)\n",
           clutter_text_get_text (text),
           clutter_text_get_cursor_position (text),
           clutter_text_get_selection_bound (text));
}

#define is_hex_digit(c)         (((c) >= '0' && (c) <= '9') || \
                                 ((c) >= 'a' && (c) <= 'f') || \
                                 ((c) >= 'A' && (c) <= 'F'))
#define to_hex_digit(c)         (((c) <= '9') ? (c) - '0' : ((c) & 7) + 9)

static gboolean
on_captured_event (ClutterText *text,
                   ClutterEvent *event,
                   gpointer      dummy G_GNUC_UNUSED)
{
  gboolean is_unicode_mode = FALSE;
  gunichar c;
  guint keyval;

  if (event->type != CLUTTER_KEY_PRESS)
    return FALSE;

  is_unicode_mode = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (text),
                                                        "unicode-mode"));

  c = clutter_event_get_key_unicode (event);
  keyval = clutter_event_get_key_symbol (event);
  if (keyval == CLUTTER_KEY_U)
    {
      if (is_unicode_mode)
        {
          GString *str = g_object_get_data (G_OBJECT (text), "unicode-str");

          clutter_text_set_preedit_string (text, NULL, NULL, 0);

          g_object_set_data (G_OBJECT (text), "unicode-mode",
                             GINT_TO_POINTER (FALSE));
          g_object_set_data (G_OBJECT (text), "unicode-str",
                             NULL);

          g_string_free (str, TRUE);

          return FALSE;
        }

      if (clutter_event_has_control_modifier (event))
        {
          PangoAttrList *attrs;
          PangoAttribute *a;
          GString *str = g_string_sized_new (5);

          g_string_append (str, "u");

          g_object_set_data (G_OBJECT (text),
                             "unicode-mode",
                             GINT_TO_POINTER (TRUE));
          g_object_set_data (G_OBJECT (text),
                             "unicode-str",
                             str);

          attrs = pango_attr_list_new ();

          a = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          a->start_index = 0;
          a->end_index = str->len;
          pango_attr_list_insert (attrs, a);

          clutter_text_set_preedit_string (text, str->str, attrs, str->len);

          pango_attr_list_unref (attrs);

          return TRUE;
        }

      return FALSE;
    }
  else if (is_unicode_mode && is_hex_digit (c))
    {
      GString *str = g_object_get_data (G_OBJECT (text), "unicode-str");
      PangoAttrList *attrs;
      PangoAttribute *a;
      gchar buf[8];
      gsize len;

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      g_string_append (str, buf);

      g_print ("added '%s' to '%s' (len:%d)\n",
               buf,
               str->str,
               (int) str->len);

      attrs = pango_attr_list_new ();

      a = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      a->start_index = 0;
      a->end_index = str->len;
      pango_attr_list_insert (attrs, a);

      clutter_text_set_preedit_string (text, str->str, attrs, str->len);

      pango_attr_list_unref (attrs);

      return TRUE;
    }
  else if (is_unicode_mode && (keyval == CLUTTER_KEY_BackSpace))
    {
      GString *str = g_object_get_data (G_OBJECT (text), "unicode-str");
      PangoAttrList *attrs;
      PangoAttribute *a;

      g_string_truncate (str, str->len - 1);

      attrs = pango_attr_list_new ();

      a = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      a->start_index = 0;
      a->end_index = str->len;
      pango_attr_list_insert (attrs, a);

      clutter_text_set_preedit_string (text, str->str, attrs, str->len);

      pango_attr_list_unref (attrs);

      return TRUE;
    }
  else if (is_unicode_mode &&
           (keyval == CLUTTER_KEY_Return ||
            keyval == CLUTTER_KEY_KP_Enter ||
            keyval == CLUTTER_KEY_ISO_Enter ||
            keyval == CLUTTER_KEY_KP_Space))
    {
      GString *str = g_object_get_data (G_OBJECT (text), "unicode-str");
      const gchar *contents = clutter_text_get_text (text);
      gunichar uchar = 0;
      gchar ch;
      gint i;

      clutter_text_set_preedit_string (text, NULL, NULL, 0);

      g_object_set_data (G_OBJECT (text), "unicode-mode",
                         GINT_TO_POINTER (FALSE));
      g_object_set_data (G_OBJECT (text), "unicode-str",
                         NULL);

      for (i = 0; i < str->len; i++)
        {
          ch = str->str[i];

          if (is_hex_digit (ch))
            uchar += ((gunichar) to_hex_digit (ch) << ((4 - i) * 4));
        }

      g_assert (g_unichar_validate (uchar));

      g_string_overwrite (str, 0, contents);
      g_string_insert_unichar (str,
                               clutter_text_get_cursor_position (text),
                               uchar);

      i = clutter_text_get_cursor_position (text);
      clutter_text_set_text (text, str->str);

      if (i >= 0)
        i += 1;
      else
        i = -1;

      clutter_text_set_cursor_position (text, i);
      clutter_text_set_selection_bound (text, i);

      g_string_free (str, TRUE);

      return TRUE;
    }
  else
    return FALSE;
}

static ClutterActor *
create_label (const ClutterColor *color,
              const gchar        *text)
{
  ClutterActor *retval = clutter_text_new ();

  clutter_text_set_color (CLUTTER_TEXT (retval), color);
  clutter_text_set_markup (CLUTTER_TEXT (retval), text);
  clutter_text_set_editable (CLUTTER_TEXT (retval), FALSE);
  clutter_text_set_selectable (CLUTTER_TEXT (retval), FALSE);
  clutter_text_set_single_line_mode (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_ellipsize (CLUTTER_TEXT (retval), PANGO_ELLIPSIZE_END);

  return retval;
}

static ClutterActor *
create_entry (const ClutterColor *color,
              const gchar        *text,
              PangoAttrList      *attrs,
              gunichar            password_char,
              gint                max_length)
{
  ClutterActor *retval = clutter_text_new_full (NULL, text, color);
  ClutterColor selection = { 0, };
  ClutterColor selected_text = { 0x00, 0x00, 0xff, 0xff };

  clutter_actor_set_reactive (retval, TRUE);

  clutter_color_darken (color, &selection);

  clutter_text_set_editable (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_selectable (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_activatable (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_single_line_mode (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_password_char (CLUTTER_TEXT (retval), password_char);
  clutter_text_set_cursor_color (CLUTTER_TEXT (retval), &selection);
  clutter_text_set_max_length (CLUTTER_TEXT (retval), max_length);
  clutter_text_set_selected_text_color (CLUTTER_TEXT (retval), &selected_text);
  clutter_actor_set_background_color (retval, CLUTTER_COLOR_LightGray);
  if (attrs)
    clutter_text_set_attributes (CLUTTER_TEXT (retval), attrs);

  g_signal_connect (retval, "activate",
                    G_CALLBACK (on_entry_activate),
                    NULL);
  g_signal_connect (retval, "captured-event",
                    G_CALLBACK (on_captured_event),
                    NULL);

  return retval;
}

G_MODULE_EXPORT gint
test_text_field_main (gint    argc,
                      gchar **argv)
{
  ClutterActor *stage;
  ClutterActor *box, *label, *entry;
  ClutterLayoutManager *table;
  PangoAttrList *entry_attrs;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Text Fields");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Black);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  table = clutter_table_layout_new ();
  clutter_table_layout_set_column_spacing (CLUTTER_TABLE_LAYOUT (table), 6);
  clutter_table_layout_set_row_spacing (CLUTTER_TABLE_LAYOUT (table), 6);

  box = clutter_actor_new ();
  clutter_actor_set_layout_manager (box, table);
  clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage, CLUTTER_BIND_WIDTH, -24.0));
  clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage, CLUTTER_BIND_HEIGHT, -24.0));
  clutter_actor_set_position (box, 12, 12);
  clutter_actor_add_child (stage, box);

  label = create_label (CLUTTER_COLOR_White, "<b>Input field:</b>");
  g_object_set (label, "min-width", 150.0, NULL);
  clutter_actor_add_child (box, label);
  clutter_layout_manager_child_set (table, CLUTTER_CONTAINER (box), label,
                                    "row", 0,
                                    "column", 0,
                                    "x-expand", FALSE,
                                    "y-expand", FALSE,
                                    NULL);

  entry_attrs = pango_attr_list_new ();
  pango_attr_list_insert (entry_attrs, pango_attr_underline_new (PANGO_UNDERLINE_ERROR));
  pango_attr_list_insert (entry_attrs, pango_attr_underline_color_new (65535, 0, 0));
  entry = create_entry (CLUTTER_COLOR_Black, "somme misspeeled textt", entry_attrs, 0, 0);
  clutter_actor_add_child (box, entry);
  clutter_layout_manager_child_set (table, CLUTTER_CONTAINER (box), entry,
                                    "row", 0,
                                    "column", 1,
                                    "x-expand", TRUE,
                                    "x-fill", TRUE,
                                    "y-expand", FALSE,
                                    NULL);
  clutter_actor_grab_key_focus (entry);

  label = create_label (CLUTTER_COLOR_White, "<b>A very long password field:</b>");
  clutter_actor_add_child (box, label);
  clutter_layout_manager_child_set (table, CLUTTER_CONTAINER (box), label,
                                    "row", 1,
                                    "column", 0,
                                    "x-expand", FALSE,
                                    "y-expand", FALSE,
                                    NULL);

  entry = create_entry (CLUTTER_COLOR_Black, "password", NULL, '*', 8);
  clutter_actor_add_child (box, entry);
  clutter_layout_manager_child_set (table, CLUTTER_CONTAINER (box), entry,
                                    "row", 1,
                                    "column", 1,
                                    "x-expand", TRUE,
                                    "x-fill", TRUE,
                                    "y-expand", FALSE,
                                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_text_field_describe (void)
{
  return
"Text actor single-line and password mode support\n"
"\n"
"This test checks the :single-line-mode and :password-char properties of\n"
"the ClutterText actor, plus the password hint feature and the :max-length\n"
"property.";
}
