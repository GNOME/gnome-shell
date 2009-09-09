#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

#define FONT "Mono Bold 14px"

static void
on_entry_paint (ClutterActor *actor,
                gpointer      data)
{
  ClutterActorBox allocation = { 0, };
  gfloat width, height;

  clutter_actor_get_allocation_box (actor, &allocation);
  width = allocation.x2 - allocation.x1;
  height = allocation.y2 - allocation.y1;

  cogl_set_source_color4ub (255, 255, 255, 255);
  cogl_path_round_rectangle (0, 0, width, height, 4.0, 1.0);
  cogl_path_stroke ();
}

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
  if (keyval == CLUTTER_u)
    {
      ClutterModifierType mods = clutter_event_get_state (event);

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

      if ((mods & CLUTTER_CONTROL_MASK) &&
          (mods & CLUTTER_SHIFT_MASK))
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

      g_debug ("added '%s' to '%s' (len:%d)", buf, str->str, str->len);

      attrs = pango_attr_list_new ();

      a = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      a->start_index = 0;
      a->end_index = str->len;
      pango_attr_list_insert (attrs, a);

      clutter_text_set_preedit_string (text, str->str, attrs, str->len);

      pango_attr_list_unref (attrs);

      return TRUE;
    }
  else if (is_unicode_mode && (keyval == CLUTTER_BackSpace))
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
           (keyval == CLUTTER_Return ||
            keyval == CLUTTER_KP_Enter ||
            keyval == CLUTTER_ISO_Enter ||
            keyval == CLUTTER_KP_Space))
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

  clutter_actor_set_width (retval, 200);

  clutter_text_set_font_name (CLUTTER_TEXT (retval), FONT);
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
              gunichar            password_char,
              gint                max_length)
{
  ClutterActor *retval = clutter_text_new_full (FONT, text, color);
  ClutterColor selection = { 0, };

  clutter_actor_set_width (retval, 200);
  clutter_actor_set_reactive (retval, TRUE);

  clutter_color_darken (color, &selection);

  clutter_text_set_editable (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_selectable (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_activatable (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_single_line_mode (CLUTTER_TEXT (retval), TRUE);
  clutter_text_set_password_char (CLUTTER_TEXT (retval), password_char);
  clutter_text_set_cursor_color (CLUTTER_TEXT (retval), &selection);
  clutter_text_set_max_length (CLUTTER_TEXT (retval), max_length);

  g_signal_connect (retval, "activate",
                    G_CALLBACK (on_entry_activate),
                    NULL);
  g_signal_connect (retval, "paint",
                    G_CALLBACK (on_entry_paint),
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
  ClutterActor *text;
  ClutterColor  entry_color       = {0x33, 0xff, 0x33, 0xff};
  ClutterColor  label_color       = {0xff, 0xff, 0xff, 0xff};
  ClutterColor  background_color  = {0x00, 0x00, 0x00, 0xff};
  ClutterUnits  h_padding, v_padding;
  gfloat        width, height;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &background_color);

  clutter_units_em_for_font (&h_padding, FONT, 2.0); /* 2em */
  clutter_units_em_for_font (&v_padding, FONT, 3.0); /* 3em */

  g_print ("padding: h:%.2f px, v:%.2f px\n",
           clutter_units_to_pixels (&h_padding),
           clutter_units_to_pixels (&v_padding));

  text = create_label (&label_color, "<b>Input field:</b>    ");
  clutter_actor_set_position (text, 10, 10);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), text);

  width = clutter_actor_get_width (text);
  height = clutter_actor_get_height (text);

  text = create_entry (&entry_color, "<i>some</i> text", 0, 0);
  clutter_actor_set_position (text,
                              width + 10 + clutter_units_to_pixels (&h_padding),
                              10);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), text);

  text = create_label (&label_color, "<i>A very long password field</i>: ");
  clutter_actor_set_position (text,
                              10,
                              height + 10 + clutter_units_to_pixels (&v_padding));
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), text);

  text = create_entry (&entry_color, "password", '*', 8);
  clutter_actor_set_position (text,
                              width + 10 + clutter_units_to_pixels (&h_padding),
                              height + 10 + clutter_units_to_pixels (&v_padding));
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), text);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
