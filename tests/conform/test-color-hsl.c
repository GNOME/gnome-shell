#include <math.h>
#include <string.h>

#include <cogl/cogl.h>

#include "test-utils.h"

#define cogl_assert_float(a, b)         \
  do {                                  \
    if (fabsf ((a) - (b)) >= 0.0001f)   \
      g_assert_cmpfloat ((a), ==, (b)); \
  } while (0)

void
test_color_hsl (void)
{
  CoglColor color;
  float hue, saturation, luminance;

  cogl_color_init_from_4ub(&color, 108, 198, 78, 255);
  cogl_color_to_hsl(&color, &hue, &saturation, &luminance);

  cogl_assert_float(hue, 105.f);
  cogl_assert_float(saturation, 0.512821);
  cogl_assert_float(luminance, 0.541176);

  memset(&color, 0, sizeof (CoglColor));
  cogl_color_init_from_hsl(&color, hue, saturation, luminance);

  g_assert_cmpint (color.red, ==, 108);
  g_assert_cmpint (color.green, ==, 198);
  g_assert_cmpint (color.blue, ==, 78);
  g_assert_cmpint (color.alpha, ==, 255);

  memset(&color, 0, sizeof (CoglColor));
  cogl_color_init_from_hsl(&color, hue, 0, luminance);

  cogl_assert_float(color.red / 255.0f, luminance);
  cogl_assert_float(color.green / 255.0f, luminance);
  cogl_assert_float(color.blue / 255.0f, luminance);
  cogl_assert_float(color.alpha / 255.0f, 1.0f);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
