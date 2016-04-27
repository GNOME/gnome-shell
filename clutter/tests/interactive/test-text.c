#include <stdlib.h>

#include <gmodule.h>
#include <clutter/clutter.h>

#define FONT "Mono Bold 24px"

static const gchar *runes =
"ᚠᛇᚻ᛫ᛒᛦᚦ᛫ᚠᚱᚩᚠᚢᚱ᛫ᚠᛁᚱᚪ᛫ᚷᛖᚻᚹᛦᛚᚳᚢᛗ\n"
"ᛋᚳᛖᚪᛚ᛫ᚦᛖᚪᚻ᛫ᛗᚪᚾᚾᚪ᛫ᚷᛖᚻᚹᛦᛚᚳ᛫ᛗᛁᚳᛚᚢᚾ᛫ᚻᛦᛏ᛫ᛞᚫᛚᚪᚾ\n"
"ᚷᛁᚠ᛫ᚻᛖ᛫ᚹᛁᛚᛖ᛫ᚠᚩᚱ᛫ᛞᚱᛁᚻᛏᚾᛖ᛫ᛞᚩᛗᛖᛋ᛫ᚻᛚᛇᛏᚪᚾ᛬\n";

G_MODULE_EXPORT gint
test_text_main (gint    argc,
                gchar **argv)
{
  ClutterActor *stage;
  ClutterActor *text, *text2;
  ClutterColor  text_color = { 0x33, 0xff, 0x33, 0xff };
  ClutterColor  cursor_color = { 0xff, 0x33, 0x33, 0xff };
  ClutterTextBuffer *buffer;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Text Editing");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Black);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  buffer = clutter_text_buffer_new_with_text ("·", -1);

  text = clutter_text_new_with_buffer (buffer);
  clutter_text_set_font_name (CLUTTER_TEXT (text), FONT);
  clutter_text_set_color (CLUTTER_TEXT (text), &text_color);

  clutter_container_add (CLUTTER_CONTAINER (stage), text, NULL);
  clutter_actor_set_position (text, 40, 30);
  clutter_actor_set_width (text, 1024);
  clutter_text_set_line_wrap (CLUTTER_TEXT (text), TRUE);

  clutter_actor_set_reactive (text, TRUE);
  clutter_stage_set_key_focus (CLUTTER_STAGE (stage), text);

  clutter_text_set_editable (CLUTTER_TEXT (text), TRUE);
  clutter_text_set_selectable (CLUTTER_TEXT (text), TRUE);
  clutter_text_set_cursor_color (CLUTTER_TEXT (text), &cursor_color);
  clutter_text_set_selected_text_color (CLUTTER_TEXT (text), CLUTTER_COLOR_Blue);

  text2 = clutter_text_new_with_buffer (buffer);
  clutter_text_set_color (CLUTTER_TEXT (text2), &text_color);
  clutter_container_add (CLUTTER_CONTAINER (stage), text2, NULL);
  clutter_actor_set_position (text2, 40, 300);
  clutter_actor_set_width (text2, 1024);
  clutter_text_set_line_wrap (CLUTTER_TEXT (text2), TRUE);

  clutter_actor_set_reactive (text2, TRUE);
  clutter_text_set_editable (CLUTTER_TEXT (text2), TRUE);
  clutter_text_set_selectable (CLUTTER_TEXT (text2), TRUE);
  clutter_text_set_cursor_color (CLUTTER_TEXT (text2), &cursor_color);
  clutter_text_set_selected_text_color (CLUTTER_TEXT (text2), CLUTTER_COLOR_Green);

  if (argv[1])
    {
      GError *error = NULL;
      gchar *utf8;

      g_file_get_contents (argv[1], &utf8, NULL, &error);
      if (error)
        {
          utf8 = g_strconcat ("Unable to open '", argv[1], "':\n",
                              error->message,
                              NULL);
          g_error_free (error);
        }

      clutter_text_set_text (CLUTTER_TEXT (text), utf8);
    }
  else
    clutter_text_set_text (CLUTTER_TEXT (text), runes);

  clutter_actor_set_size (stage, 1024, 768);
  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_text_describe (void)
{
  return "Multi-line text editing.";
}
