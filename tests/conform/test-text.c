#include <glib.h>
#include <clutter/clutter.h>
#include <string.h>

#include "test-conform-common.h"

typedef struct {
  gunichar   unichar;
  const char bytes[6];
  gint       nbytes;
} TestData;

static const TestData
test_text_data[] = {
  { 0xe4,   "\xc3\xa4",     2 }, /* LATIN SMALL LETTER A WITH DIAERESIS */
  { 0x2665, "\xe2\x99\xa5", 3 }  /* BLACK HEART SUIT */
};

void
text_utf8_validation (void)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];
      gunichar unichar;
      char bytes[6];
      int nbytes;

      g_assert (g_unichar_validate (t->unichar));

      nbytes = g_unichar_to_utf8 (t->unichar, bytes);
      bytes[nbytes] = '\0';
      g_assert (nbytes == t->nbytes);
      g_assert (memcmp (t->bytes, bytes, nbytes) == 0);

      unichar = g_utf8_get_char_validated (bytes, nbytes);
      g_assert (unichar == t->unichar);
    }
}

static int
get_nbytes (ClutterText *text)
{
  const char *s = clutter_text_get_text (text);
  return strlen (s);
}

static int
get_nchars (ClutterText *text)
{
  const char *s = clutter_text_get_text (text);
  g_assert (g_utf8_validate (s, -1, NULL));
  return g_utf8_strlen (s, -1);
}

#define DONT_MOVE_CURSOR    (-2)

static void
insert_unichar (ClutterText *text, gunichar unichar, int position)
{
  if (position > DONT_MOVE_CURSOR)
    {
      clutter_text_set_cursor_position (text, position);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, position);
    }

  clutter_text_insert_unichar (text, unichar);
}

void
text_set_empty (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());

  g_assert_cmpstr (clutter_text_get_text (text), ==, "");
  g_assert_cmpint (*clutter_text_get_text (text), ==, '\0');
  g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);

  clutter_text_set_text (text, "");
  g_assert_cmpint (get_nchars (text), ==, 0);
  g_assert_cmpint (get_nbytes (text), ==, 0);
  g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_set_text (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());

  clutter_text_set_text (text, "abcdef");
  g_assert_cmpint (get_nchars (text), ==, 6);
  g_assert_cmpint (get_nbytes (text), ==, 6);
  g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);

  clutter_text_set_cursor_position (text, 5);
  g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 5);

  /* FIXME: cursor position should be -1?
  clutter_text_set_text (text, "");
  g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);
  */

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_append_some (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];
      int j;

      for (j = 1; j <= 4; j++)
        {
          insert_unichar (text, t->unichar, DONT_MOVE_CURSOR);

          g_assert_cmpint (get_nchars (text), ==, j);
          g_assert_cmpint (get_nbytes (text), ==, j * t->nbytes);
          g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);
        }

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_prepend_some (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];
      int j;

      clutter_text_insert_unichar (text, t->unichar);

      g_assert_cmpint (get_nchars (text), ==, 1);
      g_assert_cmpint (get_nbytes (text), ==, 1 * t->nbytes);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);

      for (j = 2; j <= 4; j++)
        {
          insert_unichar (text, t->unichar, 0);

          g_assert_cmpint (get_nchars (text), ==, j);
          g_assert_cmpint (get_nbytes (text), ==, j * t->nbytes);
          g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 1);
        }

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_insert (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];

      clutter_text_insert_unichar (text, t->unichar);
      clutter_text_insert_unichar (text, t->unichar);

      insert_unichar (text, t->unichar, 1);

      g_assert_cmpint (get_nchars (text), ==, 3);
      g_assert_cmpint (get_nbytes (text), ==, 3 * t->nbytes);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 2);

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_delete_chars (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];
      int j;

      for (j = 0; j < 4; j++)
        clutter_text_insert_unichar (text, t->unichar);

      clutter_text_set_cursor_position (text, 2);
      clutter_text_delete_chars (text, 1);
      g_assert_cmpint (get_nchars (text), ==, 3);
      g_assert_cmpint (get_nbytes (text), ==, 3 * t->nbytes);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 1);

      clutter_text_set_cursor_position (text, 2);
      clutter_text_delete_chars (text, 1);
      g_assert_cmpint (get_nchars (text), ==, 2);
      g_assert_cmpint (get_nbytes (text), ==, 2 * t->nbytes);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 1);

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_get_chars (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  gchar *chars;

  clutter_text_set_text (text, "00abcdef11");
  g_assert_cmpint (get_nchars (text), ==, 10);
  g_assert_cmpint (get_nbytes (text), ==, 10);
  g_assert_cmpstr (clutter_text_get_text (text), ==, "00abcdef11");

  chars = clutter_text_get_chars (text, 2, -1);
  g_assert_cmpstr (chars, ==, "abcdef11");
  g_free (chars);

  chars = clutter_text_get_chars (text, 0, 8);
  g_assert_cmpstr (chars, ==, "00abcdef");
  g_free (chars);

  chars = clutter_text_get_chars (text, 2, 8);
  g_assert_cmpstr (chars, ==, "abcdef");
  g_free (chars);

  chars = clutter_text_get_chars (text, 8, 12);
  g_assert_cmpstr (chars, ==, "11");
  g_free (chars);

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_delete_text (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];
      int j;

      for (j = 0; j < 4; j++)
        clutter_text_insert_unichar (text, t->unichar);

      clutter_text_set_cursor_position (text, 3);
      clutter_text_delete_text (text, 2, 4);

      g_assert_cmpint (get_nchars (text), ==, 2);
      g_assert_cmpint (get_nbytes (text), ==, 2 * t->nbytes);

      /* FIXME: cursor position should be -1?
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);
      */

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_password_char (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());

  g_assert_cmpint (clutter_text_get_password_char (text), ==, 0);

  clutter_text_set_text (text, "hello");
  g_assert_cmpstr (clutter_text_get_text (text), ==, "hello");

  clutter_text_set_password_char (text, '*');
  g_assert_cmpint (clutter_text_get_password_char (text), ==, '*');

  g_assert_cmpstr (clutter_text_get_text (text), ==, "hello");

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

static ClutterEvent *
init_event (void)
{
  ClutterEvent *retval = clutter_event_new (CLUTTER_KEY_PRESS);

  clutter_event_set_time (retval, CLUTTER_CURRENT_TIME);
  clutter_event_set_flags (retval, CLUTTER_EVENT_FLAG_SYNTHETIC);

  return retval;
}

static void
send_keyval (ClutterText *text, int keyval)
{
  ClutterEvent *event = init_event ();

  /* Unicode should be ignored for cursor keys etc. */
  clutter_event_set_key_unicode (event, 0);
  clutter_event_set_key_symbol (event, keyval);

  clutter_actor_event (CLUTTER_ACTOR (text), event, FALSE);

  clutter_event_free (event);
}

static void
send_unichar (ClutterText *text, gunichar unichar)
{
  ClutterEvent *event = init_event ();

  /* Key symbol should be ignored for printable characters */
  clutter_event_set_key_symbol (event, 0);
  clutter_event_set_key_unicode (event, unichar);

  clutter_actor_event (CLUTTER_ACTOR (text), event, FALSE);

  clutter_event_free (event);
}

void
text_cursor (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  int i;

  /* only editable entries listen to events */
  clutter_text_set_editable (text, TRUE);

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];
      int j;

      for (j = 0; j < 4; ++j)
        clutter_text_insert_unichar (text, t->unichar);

      clutter_text_set_cursor_position (text, 2);

      /* test cursor moves and is clamped */
      send_keyval (text, CLUTTER_KEY_Left);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 1);

      send_keyval (text, CLUTTER_KEY_Left);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 0);

      send_keyval (text, CLUTTER_KEY_Left);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 0);

      /* delete text containing the cursor */
      clutter_text_set_cursor_position (text, 3);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 3);

      clutter_text_delete_text (text, 2, 4);
      send_keyval (text, CLUTTER_KEY_Left);

      /* FIXME: cursor position should be -1?
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);
      */

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
text_event (void)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());
  int i;

  /* only editable entries listen to events */
  clutter_text_set_editable (text, TRUE);

  for (i = 0; i < G_N_ELEMENTS (test_text_data); i++)
    {
      const TestData *t = &test_text_data[i];

      send_unichar (text, t->unichar);

      g_assert_cmpint (get_nchars (text), ==, 1);
      g_assert_cmpint (get_nbytes (text), ==, 1 * t->nbytes);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

static inline void
validate_markup_attributes (ClutterText   *text,
                            PangoAttrType  attr_type,
                            int            start_index,
                            int            end_index)
{
  PangoLayout *layout;
  PangoAttrList *attrs;
  PangoAttrIterator *iter;

  layout = clutter_text_get_layout (text);
  g_assert (layout != NULL);

  attrs = pango_layout_get_attributes (layout);
  g_assert (attrs != NULL);

  iter = pango_attr_list_get_iterator (attrs);
  while (pango_attr_iterator_next (iter))
    {
      GSList *attributes = pango_attr_iterator_get_attrs (iter);
      PangoAttribute *a;

      if (attributes == NULL)
        break;

      g_assert (attributes->data != NULL);

      a = attributes->data;

      g_assert (a->klass->type == attr_type);
      g_assert_cmpint (a->start_index, ==, start_index);
      g_assert_cmpint (a->end_index, ==, end_index);

      g_slist_free_full (attributes, (GDestroyNotify) pango_attribute_destroy);
    }

  pango_attr_iterator_destroy (iter);
}

void
text_idempotent_use_markup (void)
{
  ClutterText *text;
  const char *contents = "foo <b>bar</b>";
  const char *display = "foo bar";
  int bar_start_index = strstr (display, "bar") - display;
  int bar_end_index = bar_start_index + strlen ("bar");

  /* case 1: text -> use_markup */
  if (g_test_verbose ())
    g_print ("text: '%s' -> use-markup: TRUE\n", contents);

  text = g_object_new (CLUTTER_TYPE_TEXT,
                       "text", contents, "use-markup", TRUE,
                       NULL);

  if (g_test_verbose ())
    g_print ("Contents: '%s' (expected: '%s')\n",
             clutter_text_get_text (text),
             display);

  g_assert_cmpstr (clutter_text_get_text (text), ==, display);

  validate_markup_attributes (text,
                              PANGO_ATTR_WEIGHT,
                              bar_start_index,
                              bar_end_index);

  clutter_actor_destroy (CLUTTER_ACTOR (text));

  /* case 2: use_markup -> text */
  if (g_test_verbose ())
    g_print ("use-markup: TRUE -> text: '%s'\n", contents);

  text = g_object_new (CLUTTER_TYPE_TEXT,
                       "use-markup", TRUE, "text", contents,
                       NULL);

  if (g_test_verbose ())
    g_print ("Contents: '%s' (expected: '%s')\n",
             clutter_text_get_text (text),
             display);

  g_assert_cmpstr (clutter_text_get_text (text), ==, display);

  validate_markup_attributes (text,
                              PANGO_ATTR_WEIGHT,
                              bar_start_index,
                              bar_end_index);

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}
