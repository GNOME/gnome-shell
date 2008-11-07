#include <glib.h>
#include <clutter/clutter.h>
#include <string.h>

#include "test-conform-common.h"

typedef struct {
  gunichar   unichar;
  const char bytes[6];
  gint       nbytes;
} TestData;

const TestData
test_data[] = {
  { 0xe4,   "\xc3\xa4",     2 }, /* LATIN SMALL LETTER A WITH DIAERESIS */
  { 0x2665, "\xe2\x99\xa5", 3 }  /* BLACK HEART SUIT */
};

void
test_entry_utf8_validation (TestConformSimpleFixture *fixture,
			    gconstpointer data)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];
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
get_nbytes (ClutterEntry *entry)
{
  const char *s = clutter_entry_get_text (entry);
  return strlen (s);
}

static int
get_nchars (ClutterEntry *entry)
{
  const char *s = clutter_entry_get_text (entry);
  g_assert (g_utf8_validate (s, -1, NULL));
  return g_utf8_strlen (s, -1);
}

#define DONT_MOVE_CURSOR    (-2)

static void
insert_unichar (ClutterEntry *entry, gunichar unichar, int position)
{
  if (position > DONT_MOVE_CURSOR)
    {
      clutter_entry_set_cursor_position (entry, position);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, position);
    }

  clutter_entry_insert_unichar (entry, unichar);
}

void
test_entry_empty (TestConformSimpleFixture *fixture,
		  gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());

  g_assert (clutter_entry_get_text (entry) == NULL);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_set_empty (TestConformSimpleFixture *fixture,
		      gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());

  /* annoyingly slightly different from initially empty */
  clutter_entry_set_text (entry, "");
  g_assert_cmpint (get_nchars (entry), ==, 0);
  g_assert_cmpint (get_nbytes (entry), ==, 0);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_set_text (TestConformSimpleFixture *fixture,
		     gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());

  clutter_entry_set_text (entry, "abcdef");
  g_assert_cmpint (get_nchars (entry), ==, 6);
  g_assert_cmpint (get_nbytes (entry), ==, 6);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);

  clutter_entry_set_cursor_position (entry, 5);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 5);

  clutter_entry_set_text (entry, "");
  /* FIXME: cursor position should be -1?
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
  */

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_append_some (TestConformSimpleFixture *fixture,
		        gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];
      int j;

      for (j = 1; j <= 4; j++)
        {
          insert_unichar (entry, t->unichar, DONT_MOVE_CURSOR);
          g_assert_cmpint (get_nchars (entry), ==, j);
          g_assert_cmpint (get_nbytes (entry), ==, j * t->nbytes);
          g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
        }

      clutter_entry_set_text (entry, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_prepend_some (TestConformSimpleFixture *fixture,
		         gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];
      int j;

      clutter_entry_insert_unichar (entry, t->unichar);
      g_assert_cmpint (get_nchars (entry), ==, 1);
      g_assert_cmpint (get_nbytes (entry), ==, 1 * t->nbytes);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);

      for (j = 2; j <= 4; j++)
        {
          insert_unichar (entry, t->unichar, 0);
          g_assert_cmpint (get_nchars (entry), ==, j);
          g_assert_cmpint (get_nbytes (entry), ==, j * t->nbytes);
          g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 1);
        }

      clutter_entry_set_text (entry, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_insert (TestConformSimpleFixture *fixture,
		   gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];

      clutter_entry_insert_unichar (entry, t->unichar);
      clutter_entry_insert_unichar (entry, t->unichar);

      insert_unichar (entry, t->unichar, 1);
      g_assert_cmpint (get_nchars (entry), ==, 3);
      g_assert_cmpint (get_nbytes (entry), ==, 3 * t->nbytes);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 2);

      clutter_entry_set_text (entry, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_delete_chars (TestConformSimpleFixture *fixture,
			 gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];
      int j;

      for (j = 0; j < 4; j++)
        clutter_entry_insert_unichar (entry, t->unichar);

      clutter_entry_set_cursor_position (entry, 2);
      clutter_entry_delete_chars (entry, 1);
      g_assert_cmpint (get_nchars (entry), ==, 3);
      g_assert_cmpint (get_nbytes (entry), ==, 3 * t->nbytes);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 1);

      clutter_entry_set_cursor_position (entry, 2);
      clutter_entry_delete_chars (entry, 1);
      g_assert_cmpint (get_nchars (entry), ==, 2);
      g_assert_cmpint (get_nbytes (entry), ==, 2 * t->nbytes);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 1);

      clutter_entry_set_text (entry, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_delete_text (TestConformSimpleFixture *fixture,
			gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];
      int j;

      for (j = 0; j < 4; j++)
        clutter_entry_insert_unichar (entry, t->unichar);

      clutter_entry_set_cursor_position (entry, 3);
      clutter_entry_delete_text (entry, 2, 4);

      g_assert_cmpint (get_nchars (entry), ==, 2);
      g_assert_cmpint (get_nbytes (entry), ==, 2 * t->nbytes);

      /* FIXME: cursor position should be -1?
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
      */

      clutter_entry_set_text (entry, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
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
send_keyval (ClutterEntry *entry, int keyval)
{
  ClutterKeyEvent event;

  init_event (&event);
  event.keyval = keyval;
  event.unicode_value = 0; /* should be ignored for cursor keys etc. */

  clutter_entry_handle_key_event (entry, &event);
}

static inline void
send_unichar (ClutterEntry *entry, gunichar unichar)
{
  ClutterKeyEvent event;

  init_event (&event);
  event.keyval = 0; /* should be ignored for printable characters */
  event.unicode_value = unichar;

  clutter_entry_handle_key_event (entry, &event);
}

void
test_entry_cursor (TestConformSimpleFixture *fixture,
		   gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];
      int j;

      for (j = 0; j < 4; ++j)
        clutter_entry_insert_unichar (entry, t->unichar);

      clutter_entry_set_cursor_position (entry, 2);

      /* test cursor moves and is clamped */
      send_keyval (entry, CLUTTER_Left);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 1);

      send_keyval (entry, CLUTTER_Left);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 0);

      send_keyval (entry, CLUTTER_Left);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 0);

      /* delete text containing the cursor */
      clutter_entry_set_cursor_position (entry, 3);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 3);

      clutter_entry_delete_text (entry, 2, 4);
      send_keyval (entry, CLUTTER_Left);

      /* FIXME: cursor position should be -1?
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
      */

      clutter_entry_set_text (entry, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

void
test_entry_event (TestConformSimpleFixture *fixture,
		  gconstpointer data)
{
  ClutterEntry *entry = CLUTTER_ENTRY (clutter_entry_new ());
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      const TestData *t = &test_data[i];

      send_unichar (entry, t->unichar);

      g_assert_cmpint (get_nchars (entry), ==, 1);
      g_assert_cmpint (get_nbytes (entry), ==, 1 * t->nbytes);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);

      clutter_entry_set_text (entry, "");
    }

  clutter_actor_destroy (CLUTTER_ACTOR (entry));
}

