#include <clutter/clutter.h>

#include "test-conform-common.h"

void
state_base (TestConformSimpleFixture *fixture G_GNUC_UNUSED,
            gconstpointer dummy G_GNUC_UNUSED)
{
  ClutterScript *script = clutter_script_new ();
  GObject *state = NULL;
  GError *error = NULL;
  gchar *test_file;
  GList *states, *keys;
  ClutterStateKey *state_key;
  guint duration;

  test_file = clutter_test_get_data_file ("test-state-1.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s\n", error->message);

  g_free (test_file);

#if GLIB_CHECK_VERSION (2, 20, 0)
  g_assert_no_error (error);
#else
  g_assert (error == NULL);
#endif

  state = clutter_script_get_object (script, "state");
  g_assert (CLUTTER_IS_STATE (state));

  states = clutter_state_get_states (CLUTTER_STATE (state));
  g_assert (states != NULL);

  g_assert (g_list_find (states, g_intern_static_string ("clicked")));
  g_list_free (states);

  duration = clutter_state_get_duration (CLUTTER_STATE (state), "base", "clicked");
  g_assert_cmpint (duration, ==, 250);

  duration = clutter_state_get_duration (CLUTTER_STATE (state), "clicked", "base");
  g_assert_cmpint (duration, ==, 150);

  keys = clutter_state_get_keys (CLUTTER_STATE (state), "base", "clicked",
                                 clutter_script_get_object (script, "rect"),
                                 "opacity");
  
  g_assert (keys != NULL);
  g_assert_cmpint (g_list_length (keys), ==, 1);

  state_key = keys->data;
  g_assert (clutter_state_key_get_object (state_key) == clutter_script_get_object (script, "rect"));
  g_assert (clutter_state_key_get_mode (state_key) == CLUTTER_LINEAR);
  g_assert_cmpstr (clutter_state_key_get_property_name (state_key), ==, "opacity");

  g_list_free (keys);
  keys = clutter_state_get_keys (CLUTTER_STATE (state), NULL, NULL, NULL, NULL);
  g_assert_cmpint (g_list_length (keys), ==, 2);
  g_list_free (keys);



  clutter_state_set (CLUTTER_STATE (state), "base", "clicked", state, "state", CLUTTER_LINEAR, "foo", NULL);

  keys = clutter_state_get_keys (CLUTTER_STATE (state), "base", "clicked",
                                 NULL, NULL);
  
  g_assert (keys != NULL);
  g_assert_cmpint (g_list_length (keys), ==, 2);
  g_list_free (keys);

  states = clutter_state_get_states (CLUTTER_STATE (state));
  g_assert_cmpint (g_list_length (states), ==, 2);
  g_list_free (states);

  clutter_state_remove_key (CLUTTER_STATE (state), NULL, "clicked", NULL, NULL);
  states = clutter_state_get_states (CLUTTER_STATE (state));

  /* removing the "clicked" state, will also cause the "base" state to be removed
   * since in the .json there is no default source state
   */
  g_assert_cmpint (g_list_length (states), ==, 0);
  g_list_free (states);

  g_object_unref (script);
}
