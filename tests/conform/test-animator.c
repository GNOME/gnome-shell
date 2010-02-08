#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_animator_properties (TestConformSimpleFixture *fixture,
                          gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  GObject *animator = NULL;
  GError *error = NULL;
  gchar *test_file;
  GList *keys;
  ClutterAnimatorKey *key;
  GValue value = { 0, };

  test_file = clutter_test_get_data_file ("test-animator-2.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);
  g_assert (error == NULL);

  animator = clutter_script_get_object (script, "animator");
  g_assert (CLUTTER_IS_ANIMATOR (animator));

  /* get all the keys */
  keys = clutter_animator_get_keys (CLUTTER_ANIMATOR (animator),
                                    NULL, NULL, -1.0);
  g_assert_cmpint (g_list_length (keys), ==, 3);

  key = g_list_nth_data (keys, 1);
  g_assert (key != NULL);

  if (g_test_verbose ())
    {
      g_print ("keys[1] = \n"
               ".object = %s\n"
               ".progress = %.2f\n"
               ".name = '%s'\n"
               ".type = '%s'\n",
               clutter_get_script_id (clutter_animator_key_get_object (key)),
               clutter_animator_key_get_progress (key),
               clutter_animator_key_get_property_name (key),
               g_type_name (clutter_animator_key_get_property_type (key)));
    }

  g_assert (clutter_animator_key_get_object (key) != NULL);
  g_assert_cmpfloat (clutter_animator_key_get_progress (key), ==, 0.2);
  g_assert_cmpstr (clutter_animator_key_get_property_name (key), ==, "x");

  g_assert (clutter_animator_key_get_property_type (key) == G_TYPE_FLOAT);

  g_value_init (&value, G_TYPE_FLOAT);
  g_assert (clutter_animator_key_get_value (key, &value));
  g_assert_cmpfloat (g_value_get_float (&value), ==, 150.0);
  g_value_unset (&value);

  g_list_free (keys);
  g_object_unref (script);
  g_free (test_file);
}

void
test_animator_base (TestConformSimpleFixture *fixture,
                    gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  GObject *animator = NULL;
  GError *error = NULL;
  guint duration = 0;
  gchar *test_file;

  test_file = clutter_test_get_data_file ("test-animator-1.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);
  g_assert (error == NULL);

  animator = clutter_script_get_object (script, "animator");
  g_assert (CLUTTER_IS_ANIMATOR (animator));

  duration = clutter_animator_get_duration (CLUTTER_ANIMATOR (animator));
  g_assert_cmpint (duration, ==, 1000);

  g_object_unref (script);
  g_free (test_file);
}
