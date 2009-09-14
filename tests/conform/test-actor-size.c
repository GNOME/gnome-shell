#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_fixed_size (TestConformSimpleFixture *fixture,
                 gconstpointer data)
{
  ClutterActor *rect;
  gboolean min_width_set, nat_width_set;
  gboolean min_height_set, nat_height_set;
  gfloat min_width, min_height;
  gfloat nat_width, nat_height;

  rect = clutter_rectangle_new ();

  if (g_test_verbose ())
    g_print ("Initial size is 0\n");

  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 0);
  g_assert_cmpfloat (clutter_actor_get_height (rect), ==, 0);

  clutter_actor_set_size (rect, 100, 100);

  if (g_test_verbose ())
    g_print ("Explicit size set\n");

  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 100);
  g_assert_cmpfloat (clutter_actor_get_height (rect), ==, 100);

  g_object_get (G_OBJECT (rect),
                "min-width-set", &min_width_set,
                "min-height-set", &min_height_set,
                "natural-width-set", &nat_width_set,
                "natural-height-set", &nat_height_set,
                NULL);

  if (g_test_verbose ())
    g_print ("Notification properties\n");

  g_assert (min_width_set && nat_width_set);
  g_assert (min_height_set && nat_height_set);

  clutter_actor_get_preferred_size (rect,
                                    &min_width, &min_height,
                                    &nat_width, &nat_height);

  if (g_test_verbose ())
    g_print ("Preferred size\n");

  g_assert_cmpfloat (min_width, ==, 100);
  g_assert_cmpfloat (min_height, ==, 100);
  g_assert_cmpfloat (min_width, ==, nat_width);
  g_assert_cmpfloat (min_height, ==, nat_height);

  clutter_actor_set_size (rect, -1, -1);

  if (g_test_verbose ())
    g_print ("Explicit size unset\n");

  g_object_get (G_OBJECT (rect),
                "min-width-set", &min_width_set,
                "min-height-set", &min_height_set,
                "natural-width-set", &nat_width_set,
                "natural-height-set", &nat_height_set,
                NULL);
  g_assert (!min_width_set && !nat_width_set);
  g_assert (!min_height_set && !nat_height_set);

  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 0);
  g_assert_cmpfloat (clutter_actor_get_height (rect), ==, 0);

  clutter_actor_destroy (rect);
}
