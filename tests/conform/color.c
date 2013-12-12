#include <clutter/clutter.h>

static void
color_hls_roundtrip (void)
{
  ClutterColor color;
  gfloat hue, luminance, saturation;

  /* test luminance only */
  clutter_color_from_string (&color, "#7f7f7f");
  g_assert_cmpuint (color.red, ==, 0x7f);
  g_assert_cmpuint (color.green, ==, 0x7f);
  g_assert_cmpuint (color.blue, ==, 0x7f);

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

  g_assert_cmpuint (color.red, ==, 0x7f);
  g_assert_cmpuint (color.green, ==, 0x7f);
  g_assert_cmpuint (color.blue, ==, 0x7f);

  /* full conversion */
  clutter_color_from_string (&color, "#7f8f7f");
  color.alpha = 255;

  g_assert_cmpuint (color.red, ==, 0x7f);
  g_assert_cmpuint (color.green, ==, 0x8f);
  g_assert_cmpuint (color.blue, ==, 0x7f);

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

  g_assert_cmpuint (color.red, ==, 0x7f);
  g_assert_cmpuint (color.green, ==, 0x8f);
  g_assert_cmpuint (color.blue, ==, 0x7f);

  /* the alpha channel should be untouched */
  g_assert_cmpuint (color.alpha, ==, 255);
}

static void
color_from_string_invalid (void)
{
  ClutterColor color;

  g_assert (!clutter_color_from_string (&color, "ff0000ff"));
  g_assert (!clutter_color_from_string (&color, "#decaffbad"));
  g_assert (!clutter_color_from_string (&color, "ponies"));
  g_assert (!clutter_color_from_string (&color, "rgb(255, 0, 0, 0)"));
  g_assert (!clutter_color_from_string (&color, "rgba(1.0, 0, 0)"));
  g_assert (!clutter_color_from_string (&color, "hsl(100, 0, 0)"));
  g_assert (!clutter_color_from_string (&color, "hsla(10%, 0%, 50%)"));
  g_assert (!clutter_color_from_string (&color, "hsla(100%, 0%, 50%, 20%)"));
}

static void
color_from_string_valid (void)
{
  ClutterColor color;

  g_assert (clutter_color_from_string (&color, "#ff0000ff"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0xff, 0, 0, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 0xff);
  g_assert_cmpuint (color.green, ==, 0);
  g_assert_cmpuint (color.blue, ==, 0);
  g_assert_cmpuint (color.alpha, ==, 0xff);

  g_assert (clutter_color_from_string (&color, "#0f0f"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0, 0xff, 0, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 0);
  g_assert_cmpuint (color.green, ==, 0xff);
  g_assert_cmpuint (color.blue, ==, 0);
  g_assert_cmpuint (color.alpha, ==, 0xff);

  g_assert (clutter_color_from_string (&color, "#0000ff"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0, 0, 0xff, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 0);
  g_assert_cmpuint (color.green, ==, 0);
  g_assert_cmpuint (color.blue, ==, 0xff);
  g_assert_cmpuint (color.alpha, ==, 0xff);

  g_assert (clutter_color_from_string (&color, "#abc"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0xaa, 0xbb, 0xcc, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 0xaa);
  g_assert_cmpuint (color.green, ==, 0xbb);
  g_assert_cmpuint (color.blue, ==, 0xcc);
  g_assert_cmpuint (color.alpha, ==, 0xff);

  g_assert (clutter_color_from_string (&color, "#123abc"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 0x12, 0x3a, 0xbc, 0xff }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert (color.red   == 0x12);
  g_assert (color.green == 0x3a);
  g_assert (color.blue  == 0xbc);
  g_assert (color.alpha == 0xff);

  g_assert (clutter_color_from_string (&color, "rgb(255, 128, 64)"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 255, 128, 64, 255 }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 255);
  g_assert_cmpuint (color.green, ==, 128);
  g_assert_cmpuint (color.blue, ==, 64);
  g_assert_cmpuint (color.alpha, ==, 255);

  g_assert (clutter_color_from_string (&color, "rgba ( 30%, 0,    25%,  0.5 )   "));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { %.1f, 0, %.1f, 128 }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha,
               CLAMP (255.0 / 100.0 * 30.0, 0, 255),
               CLAMP (255.0 / 100.0 * 25.0, 0, 255));
    }
  g_assert_cmpuint (color.red, ==, (255.0 / 100.0 * 30.0));
  g_assert_cmpuint (color.green, ==, 0);
  g_assert_cmpuint (color.blue, ==, (255.0 / 100.0 * 25.0));
  g_assert_cmpuint (color.alpha, ==, 127);

  g_assert (clutter_color_from_string (&color, "rgb( 50%, -50%, 150% )"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 127, 0, 255, 255 }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 127);
  g_assert_cmpuint (color.green, ==, 0);
  g_assert_cmpuint (color.blue, ==, 255);
  g_assert_cmpuint (color.alpha, ==, 255);

  g_assert (clutter_color_from_string (&color, "hsl( 0, 100%, 50% )"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 255, 0, 0, 255 }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 255);
  g_assert_cmpuint (color.green, ==, 0);
  g_assert_cmpuint (color.blue, ==, 0);
  g_assert_cmpuint (color.alpha, ==, 255);

  g_assert (clutter_color_from_string (&color, "hsla( 0, 100%, 50%, 0.5 )"));
  if (g_test_verbose ())
    {
      g_print ("color = { %x, %x, %x, %x }, expected = { 255, 0, 0, 127 }\n",
               color.red,
               color.green,
               color.blue,
               color.alpha);
    }
  g_assert_cmpuint (color.red, ==, 255);
  g_assert_cmpuint (color.green, ==, 0);
  g_assert_cmpuint (color.blue, ==, 0);
  g_assert_cmpuint (color.alpha, ==, 127);
}

static void
color_to_string (void)
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

static void
color_operators (void)
{
  ClutterColor op1, op2;
  ClutterColor res;

  clutter_color_from_pixel (&op1, 0xff0000ff);
  g_assert_cmpuint (op1.red, ==, 0xff);
  g_assert_cmpuint (op1.green, ==, 0);
  g_assert_cmpuint (op1.blue, ==, 0);
  g_assert_cmpuint (op1.alpha, ==, 0xff);

  clutter_color_from_pixel (&op2, 0x00ff00ff);
  g_assert_cmpuint (op2.red, ==, 0);
  g_assert_cmpuint (op2.green, ==, 0xff);
  g_assert_cmpuint (op2.blue, ==, 0);
  g_assert_cmpuint (op2.alpha, ==, 0xff);

  if (g_test_verbose ())
    g_print ("Adding %x, %x; expected result: %x\n",
             clutter_color_to_pixel (&op1),
             clutter_color_to_pixel (&op2),
             0xffff00ff);

  clutter_color_add (&op1, &op2, &res);
  g_assert_cmpuint (clutter_color_to_pixel (&res), ==, 0xffff00ff);

  if (g_test_verbose ())
    g_print ("Checking alpha channel on color add\n");

  op1.alpha = 0xdd;
  op2.alpha = 0xcc;
  clutter_color_add (&op1, &op2, &res);
  g_assert_cmpuint (clutter_color_to_pixel (&res), ==, 0xffff00dd);

  clutter_color_from_pixel (&op1, 0xffffffff);
  clutter_color_from_pixel (&op2, 0xff00ffff);

  if (g_test_verbose ())
    g_print ("Subtracting %x, %x; expected result: %x\n",
             clutter_color_to_pixel (&op1),
             clutter_color_to_pixel (&op2),
             0x00ff00ff);

  clutter_color_subtract (&op1, &op2, &res);
  g_assert_cmpuint (clutter_color_to_pixel (&res), ==, 0x00ff00ff);

  if (g_test_verbose ())
    g_print ("Checking alpha channel on color subtract\n");

  op1.alpha = 0xdd;
  op2.alpha = 0xcc;
  clutter_color_subtract (&op1, &op2, &res);
  g_assert_cmpuint (clutter_color_to_pixel (&res), ==, 0x00ff00cc);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/color/hls-roundtrip", color_hls_roundtrip)
  CLUTTER_TEST_UNIT ("/color/from-string/invalid", color_from_string_invalid)
  CLUTTER_TEST_UNIT ("/color/from-string/valid", color_from_string_valid)
  CLUTTER_TEST_UNIT ("/color/to-string", color_to_string)
  CLUTTER_TEST_UNIT ("/color/operators", color_operators)
)
