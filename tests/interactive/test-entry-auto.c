#include <gmodule.h>
#include <clutter/clutter.h>
#include <string.h>

#ifndef g_assert_cmpint
#  define g_assert_cmpint(x,y,z) g_assert((x) y (z))
#endif

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

static void
selfcheck (const TestData *t)
{
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

static void
test_empty (ClutterEntry *entry, const TestData *unused)
{
  g_assert (clutter_entry_get_text (entry) == NULL);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
}

static void
test_set_empty (ClutterEntry *entry, const TestData *unused)
{
  /* annoyingly slightly different from initially empty */
  clutter_entry_set_text (entry, "");
  g_assert_cmpint (get_nchars (entry), ==, 0);
  g_assert_cmpint (get_nbytes (entry), ==, 0);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
}

static void
test_set_text (ClutterEntry *entry, const TestData *unused)
{
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
}

static void
test_insert (ClutterEntry *entry, const TestData *t)
{
  clutter_entry_insert_unichar (entry, t->unichar);
  clutter_entry_insert_unichar (entry, t->unichar);

  insert_unichar (entry, t->unichar, 1);
  g_assert_cmpint (get_nchars (entry), ==, 3);
  g_assert_cmpint (get_nbytes (entry), ==, 3 * t->nbytes);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 2);
}

static void
test_append_some (ClutterEntry *entry, const TestData *t)
{
  int i;

  for (i = 1; i <= 4; i++)
    {
      insert_unichar (entry, t->unichar, DONT_MOVE_CURSOR);
      g_assert_cmpint (get_nchars (entry), ==, i);
      g_assert_cmpint (get_nbytes (entry), ==, i * t->nbytes);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
    }
}

static void
test_prepend_some (ClutterEntry *entry, const TestData *t)
{
  int i;

  clutter_entry_insert_unichar (entry, t->unichar);
  g_assert_cmpint (get_nchars (entry), ==, 1);
  g_assert_cmpint (get_nbytes (entry), ==, 1 * t->nbytes);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);

  for (i = 2; i <= 4; i++)
    {
      insert_unichar (entry, t->unichar, 0);
      g_assert_cmpint (get_nchars (entry), ==, i);
      g_assert_cmpint (get_nbytes (entry), ==, i * t->nbytes);
      g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, 1);
    }
}

static void
test_delete_chars (ClutterEntry *entry, const TestData *t)
{
  int i;

  for (i = 0; i < 4; i++)
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
}

static void
test_delete_text (ClutterEntry *entry, const TestData *t)
{
  int i;

  for (i = 0; i < 4; i++)
    clutter_entry_insert_unichar (entry, t->unichar);

  clutter_entry_set_cursor_position (entry, 3);
  clutter_entry_delete_text (entry, 2, 4);
  g_assert_cmpint (get_nchars (entry), ==, 2);
  g_assert_cmpint (get_nbytes (entry), ==, 2 * t->nbytes);
  /* FIXME: cursor position should be -1?
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
  */
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

static void
send_unichar (ClutterEntry *entry, gunichar unichar)
{
  ClutterKeyEvent event;

  init_event (&event);
  event.keyval = 0; /* should be ignored for printable characters */
  event.unicode_value = unichar;

  clutter_entry_handle_key_event (entry, &event);
}

static void
test_cursor (ClutterEntry *entry, const TestData *t)
{
  int i;

  for (i = 0; i < 4; ++i)
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
}

static void
test_event (ClutterEntry *entry, const TestData *t)
{
  send_unichar (entry, t->unichar);
  g_assert_cmpint (get_nchars (entry), ==, 1);
  g_assert_cmpint (get_nbytes (entry), ==, 1 * t->nbytes);
  g_assert_cmpint (clutter_entry_get_cursor_position (entry), ==, -1);
}

static void
run (void (*test_func)(ClutterEntry *, const TestData *), const TestData *t)
{
  ClutterActor *entry;

  entry = clutter_entry_new ();
  test_func (CLUTTER_ENTRY (entry), t);
  clutter_actor_destroy (entry);
}

G_MODULE_EXPORT int
test_entry_auto_main (int argc, char **argv)
{
  int i;

  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  for (i = 0; i < G_N_ELEMENTS (test_data); ++i)
    selfcheck (&test_data[i]);

  clutter_init (&argc, &argv);

  run (test_empty, NULL);
  run (test_set_empty, NULL);
  run (test_set_text, NULL);

  for (i = 0; i < G_N_ELEMENTS (test_data); ++i)
    {
      const TestData *t = &test_data[i];

      run (test_append_some, t);
      run (test_prepend_some, t);
      run (test_insert, t);
      run (test_delete_chars, t);
      run (test_delete_text, t);
      run (test_cursor, t);
      run (test_event, t);
    }

  return 0;
}

