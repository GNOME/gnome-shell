#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include <clutter/clutter.h>

static void
rectangle_set_size (void)
{
  ClutterActor *rect = clutter_rectangle_new ();

  /* initial positioning */
  g_assert_cmpint (clutter_actor_get_x (rect), ==, 0);
  g_assert_cmpint (clutter_actor_get_y (rect), ==, 0);

  clutter_actor_set_size (rect, 100, 100);

  /* make sure that changing the size does not affect the
   * rest of the bounding box
   */
  g_assert_cmpint (clutter_actor_get_x (rect), ==, 0);
  g_assert_cmpint (clutter_actor_get_y (rect), ==, 0);

  g_assert_cmpint (clutter_actor_get_width (rect), ==, 100);
  g_assert_cmpint (clutter_actor_get_height (rect), ==, 100);

  clutter_actor_destroy (rect);
}

static void
rectangle_set_color (void)
{
  ClutterActor *rect = clutter_rectangle_new ();
  ClutterColor white = { 255, 255, 255, 255 };
  ClutterColor black = {   0,   0,   0, 255 };
  ClutterColor check = { 0, };

  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &black);
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &check);
  g_assert_cmpint (check.blue, ==, black.blue);

  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &white);
  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &check);
  g_assert_cmpint (check.green, ==, white.green);

  g_assert_cmpint (clutter_actor_get_opacity (rect), ==, white.alpha);

  clutter_actor_destroy (rect);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/rectangle/set-size", rectangle_set_size)
  CLUTTER_TEST_UNIT ("/rectangle/set-color", rectangle_set_color)
)
