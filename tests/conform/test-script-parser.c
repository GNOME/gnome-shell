#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_script_single (TestConformSimpleFixture *fixture,
                    gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  GObject *actor = NULL;
  GError *error = NULL;
  ClutterActor *rect;
  gchar *test_file;

  test_file = clutter_test_get_data_file ("test-script-single.json");
  clutter_script_load_from_file (script, test_file, &error);
  g_assert (error == NULL);

  actor = clutter_script_get_object (script, "test");
  g_assert (CLUTTER_IS_RECTANGLE (actor));

  rect = CLUTTER_ACTOR (actor);
  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 50.0);
  g_assert_cmpfloat (clutter_actor_get_y (rect), ==, 100.0);

  g_object_unref (script);

  clutter_actor_destroy (rect);
  g_free (test_file);
}
