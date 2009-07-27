#include <stdio.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_color_hls_roundtrip (TestConformSimpleFixture *fixture,
                          gconstpointer data)
{
  ClutterColor color;
  gfloat hue, luminance, saturation;

  /* test luminance only */
  clutter_color_from_string (&color, "#7f7f7f");
  g_assert_cmpint (color.red,   ==, 0x7f);
  g_assert_cmpint (color.green, ==, 0x7f);
  g_assert_cmpint (color.blue,  ==, 0x7f);

  clutter_color_to_hls (&color, &hue, &luminance, &saturation);
  g_assert_cmpfloat (hue, ==, 0.0);
  g_assert (luminance >= 0.0 && luminance <= 1.0);
  g_assert_cmpfloat (saturation, ==, 0.0);
  if (g_test_verbose ())
    {
      g_print ("RGB = { %x, %x, %x }, HLS = { %.2f, %.2f, %.2f }\n",
               color.red,
               color.green,
               color.blue,
               hue,
               luminance,
               saturation);
    }

  color.red = color.green = color.blue = 0;
  clutter_color_from_hls (&color, hue, luminance, saturation);

  g_assert_cmpint (color.red,   ==, 0x7f);
  g_assert_cmpint (color.green, ==, 0x7f);
  g_assert_cmpint (color.blue,  ==, 0x7f);

  /* full conversion */
  clutter_color_from_string (&color, "#7f8f7f");
  color.alpha = 255;

  g_assert_cmpint (color.red,   ==, 0x7f);
  g_assert_cmpint (color.green, ==, 0x8f);
  g_assert_cmpint (color.blue,  ==, 0x7f);

  clutter_color_to_hls (&color, &hue, &luminance, &saturation);
  g_assert (hue >= 0.0 && hue < 360.0);
  g_assert (luminance >= 0.0 && luminance <= 1.0);
  g_assert (saturation >= 0.0 && saturation <= 1.0);
  if (g_test_verbose ())
    {
      g_print ("RGB = { %x, %x, %x }, HLS = { %.2f, %.2f, %.2f }\n",
               color.red,
               color.green,
               color.blue,
               hue,
               luminance,
               saturation);
    }

  color.red = color.green = color.blue = 0;
  clutter_color_from_hls (&color, hue, luminance, saturation);

  g_assert_cmpint (color.red,   ==, 0x7f);
  g_assert_cmpint (color.green, ==, 0x8f);
  g_assert_cmpint (color.blue,  ==, 0x7f);

  /* the alpha channel should be untouched */
  g_assert_cmpint (color.alpha, ==, 255);
}

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
