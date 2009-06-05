#include <stdio.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_color_from_string (TestConformSimpleFixture *fixture,
                        gconstpointer data)
{
  ClutterColor color;

  clutter_color_from_string (&color, "#ff0000ff");
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0xff, 0, 0, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert (color.red   == 0xff);
  g_assert (color.green == 0);
  g_assert (color.blue  == 0);
  g_assert (color.alpha == 0xff);

  clutter_color_from_string (&color, "#0f0f");
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0, 0xff, 0, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert (color.red   == 0);
  g_assert (color.green == 0xff);
  g_assert (color.blue  == 0);
  g_assert (color.alpha == 0xff);

  clutter_color_from_string (&color, "#0000ff");
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0, 0, 0xff, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert (color.red   == 0);
  g_assert (color.green == 0);
  g_assert (color.blue  == 0xff);
  g_assert (color.alpha == 0xff);
}

void
test_color_to_string (TestConformSimpleFixture *fixture,
                      gconstpointer data)
{
  ClutterColor color;
  gchar *str;

  color.red = 0xcc;
  color.green = 0xcc;
  color.blue = 0xcc;
  color.alpha = 0x22;

  str = clutter_color_to_string (&color);
  g_assert_cmpstr (str, ==, "#cccccc22");

  g_free (str);
}
