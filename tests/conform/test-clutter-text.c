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
test_text_utf8_validation (TestConformSimpleFixture *fixture,
			    gconstpointer data)
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
test_text_empty (TestConformSimpleFixture *fixture,
		  gconstpointer data)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());

  g_assert_cmpstr (clutter_text_get_text (text), ==, "");
  g_assert_cmpint (*clutter_text_get_text (text), ==, '\0');
  g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
test_text_set_empty (TestConformSimpleFixture *fixture,
		      gconstpointer data)
{
  ClutterText *text = CLUTTER_TEXT (clutter_text_new ());

  /* annoyingly slightly different from initially empty */
  clutter_text_set_text (text, "");
  g_assert_cmpint (get_nchars (text), ==, 0);
  g_assert_cmpint (get_nbytes (text), ==, 0);
  g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
test_text_set_text (TestConformSimpleFixture *fixture,
		     gconstpointer data)
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
test_text_append_some (TestConformSimpleFixture *fixture,
		        gconstpointer data)
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
test_text_prepend_some (TestConformSimpleFixture *fixture,
		         gconstpointer data)
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
test_text_insert (TestConformSimpleFixture *fixture,
		   gconstpointer data)
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
test_text_delete_chars (TestConformSimpleFixture *fixture,
			 gconstpointer data)
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
test_text_get_chars (TestConformSimpleFixture *fixture,
                     gconstpointer             data)
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
test_text_delete_text (TestConformSimpleFixture *fixture,
			gconstpointer data)
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
test_text_password_char (TestConformSimpleFixture *fixture,
                         gconstpointer data)
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

static void
init_event (ClutterKeyEvent *event)
{
  event->type = CLUTTER_KEY_PRESS;
  event->time = 0;      /* not needed */
  event->flags = CLUTTER_EVENT_FLAG_SYNTHETIC;
  event->stage = NULL;  /* not needed */
  event->source = NULL; /* not needed */
  event->modifier_state = 0;
  event->hardware_keycode = 0; /* not needed */
}

static void
send_keyval (ClutterText *text, int keyval)
{
  ClutterKeyEvent event;

  init_event (&event);
  event.keyval = keyval;
  event.unicode_value = 0; /* should be ignored for cursor keys etc. */

  clutter_actor_event (CLUTTER_ACTOR (text), (ClutterEvent *) &event, FALSE);
}

static void
send_unichar (ClutterText *text, gunichar unichar)
{
  ClutterKeyEvent event;

  init_event (&event);
  event.keyval = 0; /* should be ignored for printable characters */
  event.unicode_value = unichar;

  clutter_actor_event (CLUTTER_ACTOR (text), (ClutterEvent *) &event, FALSE);
}

void
test_text_cursor (TestConformSimpleFixture *fixture,
		   gconstpointer data)
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
      send_keyval (text, CLUTTER_Left);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 1);

      send_keyval (text, CLUTTER_Left);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 0);

      send_keyval (text, CLUTTER_Left);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 0);

      /* delete text containing the cursor */
      clutter_text_set_cursor_position (text, 3);
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, 3);

      clutter_text_delete_text (text, 2, 4);
      send_keyval (text, CLUTTER_Left);

      /* FIXME: cursor position should be -1?
      g_assert_cmpint (clutter_text_get_cursor_position (text), ==, -1);
      */

      clutter_text_set_text (text, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (text));
}

void
test_text_event (TestConformSimpleFixture *fixture,
		  gconstpointer data)
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

