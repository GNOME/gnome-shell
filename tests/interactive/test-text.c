/* Try this text editor, it has issues but does work,
 * try ctrl+A, ctrl+C, ctrl+V, ctrl+X as well as selecting text with
 * mouse and keyboard, /Øyvind K
 */

#include <gmodule.h>
#include <clutter/clutter.h>

#define FONT "Mono Bold 22px"

static gchar *clipboard = NULL;

static gchar *runes =
"ᚠᛇᚻ᛫ᛒᛦᚦ᛫ᚠᚱᚩᚠᚢᚱ᛫ᚠᛁᚱᚪ᛫ᚷᛖᚻᚹᛦᛚᚳᚢᛗ\n"
"ᛋᚳᛖᚪᛚ᛫ᚦᛖᚪᚻ᛫ᛗᚪᚾᚾᚪ᛫ᚷᛖᚻᚹᛦᛚᚳ᛫ᛗᛁᚳᛚᚢᚾ᛫ᚻᛦᛏ᛫ᛞᚫᛚᚪᚾ\n"
"ᚷᛁᚠ᛫ᚻᛖ᛫ᚹᛁᛚᛖ᛫ᚠᚩᚱ᛫ᛞᚱᛁᚻᛏᚾᛖ᛫ᛞᚩᛗᛖᛋ᛫ᚻᛚᛇᛏᚪᚾ᛬\n";


static gboolean
select_all (ClutterText     *ttext,
            const gchar  *commandline,
            ClutterEvent *event)
{
  gint len;
  len = g_utf8_strlen (clutter_text_get_text (ttext), -1);

  clutter_text_set_cursor_position (ttext, 0);
  clutter_text_set_selection_bound (ttext, len);

  return TRUE;
}

static gboolean
copy (ClutterText     *ttext,
      const gchar  *commandline,
      ClutterEvent *event)
{
  if (clipboard)
    g_free (clipboard);
  clipboard = clutter_text_get_selection (ttext);
  return TRUE;
}

static gboolean
paste (ClutterText     *ttext,
       const gchar  *commandline,
       ClutterEvent *event)
{
  const gchar *p;
  if (!clipboard)
    return TRUE;

  for (p=clipboard; *p!='\0'; p=g_utf8_next_char (p))
    {
      clutter_text_insert_unichar (ttext, g_utf8_get_char_validated (p, 3));
    }
  return TRUE;
}

static gboolean
cut (ClutterText     *ttext,
     const gchar  *commandline,
     ClutterEvent *event)
{
  clutter_text_action (ttext, "copy", NULL);
  clutter_text_action (ttext, "truncate-selection", NULL);
  return TRUE;
}

static gboolean
pageup (ClutterText     *ttext,
        const gchar  *commandline,
        ClutterEvent *event)
{
  gint i;
  for (i=0;i<10;i++)
    clutter_text_action (ttext, "move-up", event);
  return TRUE;
}

static gboolean
pagedown (ClutterText     *ttext,
          const gchar  *commandline,
          ClutterEvent *event)
{
  gint i;
  for (i=0;i<10;i++)
    clutter_text_action (ttext, "move-down", event);
  return TRUE;
}

static void
cursor_event (ClutterText        *text,
              ClutterGeometry *geometry)
{
  gint y;

  y = clutter_actor_get_y (CLUTTER_ACTOR (text));

  if (y + geometry->y < 50)
    {
      clutter_actor_set_y (CLUTTER_ACTOR (text), y + 100);
    }
  else if (y + geometry->y > 720)
    {
      clutter_actor_set_y (CLUTTER_ACTOR (text), y - 100);
    }

}

G_MODULE_EXPORT gint
test_text_main (gint    argc,
                gchar **argv)
{
  ClutterActor         *stage;
  ClutterActor         *text;
  ClutterColor          text_color       = {0x33, 0xff, 0x33, 0xff};
  ClutterColor          cursor_color     = {0xff, 0x33, 0x33, 0xff};
  ClutterColor          background_color = {0x00, 0x00, 0x00, 0xff};
  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &background_color);

  text = clutter_text_new_full (FONT, "·", &text_color);

  clutter_container_add (CLUTTER_CONTAINER (stage), text, NULL);
  clutter_actor_set_position (text, 40, 30);
  clutter_actor_set_width (text, 1024);
  clutter_text_set_line_wrap (CLUTTER_TEXT (text), TRUE);

  clutter_actor_set_reactive (text, TRUE);
  clutter_stage_set_key_focus (CLUTTER_STAGE (stage), text);

  clutter_text_set_editable (CLUTTER_TEXT (text), TRUE);
  clutter_text_set_selectable (CLUTTER_TEXT (text), TRUE);
  clutter_text_set_cursor_color (CLUTTER_TEXT (text), &cursor_color);

#if 0
  clutter_text_add_action (CLUTTER_TEXT (text), "select-all", select_all);
  clutter_text_add_action (CLUTTER_TEXT (text), "copy",       copy);
  clutter_text_add_action (CLUTTER_TEXT (text), "paste",      paste);
  clutter_text_add_action (CLUTTER_TEXT (text), "cut",        cut);
  clutter_text_add_action (CLUTTER_TEXT (text), "pageup",     pageup);
  clutter_text_add_action (CLUTTER_TEXT (text), "pagedown",   pagedown);

  clutter_text_add_mapping (CLUTTER_TEXT (text),
        CLUTTER_a, CLUTTER_CONTROL_MASK, "select-all");
  clutter_text_add_mapping (CLUTTER_TEXT (text),
        CLUTTER_c, CLUTTER_CONTROL_MASK, "copy");
  clutter_text_add_mapping (CLUTTER_TEXT (text),
        CLUTTER_v, CLUTTER_CONTROL_MASK, "paste");
  clutter_text_add_mapping (CLUTTER_TEXT (text),
        CLUTTER_x, CLUTTER_CONTROL_MASK, "cut");
  clutter_text_add_mapping (CLUTTER_TEXT (text),
        CLUTTER_Page_Up, 0, "pageup");
  clutter_text_add_mapping (CLUTTER_TEXT (text),
        CLUTTER_Page_Down, 0, "pagedown");
#endif

  if (argv[1])
    {
      gchar                *utf8;
      g_file_get_contents (argv[1], &utf8, NULL, NULL);
      clutter_text_set_text (CLUTTER_TEXT (text), utf8);
    }
  else
    {
      clutter_text_set_text (CLUTTER_TEXT (text), runes);
    }

  g_signal_connect (text, "cursor-event", G_CALLBACK (cursor_event), NULL);

  clutter_actor_set_size (stage, 1024, 768);
  clutter_actor_show (stage);

  clutter_main ();
  return 0;
}
