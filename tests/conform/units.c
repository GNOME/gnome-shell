#include <clutter/clutter.h>

static void
units_cache (void)
{
  ClutterUnits units;
  ClutterSettings *settings;
  gfloat pixels;
  gint old_dpi;

  settings = clutter_settings_get_default ();
  g_object_get (settings, "font-dpi", &old_dpi, NULL);

  g_object_set (settings, "font-dpi", 96 * 1024, NULL);
  clutter_units_from_em (&units, 1.0);
  pixels = clutter_units_to_pixels (&units);

  g_object_set (settings, "font-dpi", ((96 * 2) * 1024), NULL);
  g_assert_cmpfloat (clutter_units_to_pixels (&units), !=, pixels);

  g_object_set (settings, "font-dpi", (96 * 1024), NULL);
  g_assert_cmpfloat (clutter_units_to_pixels (&units), ==, pixels);

  g_object_set (settings, "font-dpi", old_dpi, NULL);
}

static void
units_constructors (void)
{
  ClutterUnits units, units_cm;

  clutter_units_from_pixels (&units, 100);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_PIXEL);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 100.0);
  g_assert_cmpfloat (clutter_units_to_pixels (&units), ==, 100.0);

  clutter_units_from_em (&units, 5.0);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_EM);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 5.0);
  g_assert_cmpfloat (clutter_units_to_pixels (&units), !=, 5.0);

  clutter_units_from_cm (&units_cm, 5.0);
  g_assert (clutter_units_get_unit_type (&units_cm) == CLUTTER_UNIT_CM);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units_cm), ==, 5.0);
  g_assert_cmpfloat (clutter_units_to_pixels (&units_cm), !=, 5.0);

  clutter_units_from_mm (&units, 50.0);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_MM);
  g_assert_cmpfloat (clutter_units_to_pixels (&units),
                     ==,
                     clutter_units_to_pixels (&units_cm));
}

static void
units_string (void)
{
  ClutterUnits units;
  gchar *string;

  g_assert (clutter_units_from_string (&units, "") == FALSE);

  g_assert (clutter_units_from_string (&units, "10") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_PIXEL);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 10);

  g_assert (clutter_units_from_string (&units, "10 px") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_PIXEL);

  g_assert (clutter_units_from_string (&units, "10 mm") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_MM);

  g_assert (clutter_units_from_string (&units, "10 cm") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_CM);

  g_assert (clutter_units_from_string (&units, "10  ") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_PIXEL);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 10);

  g_assert (clutter_units_from_string (&units, "5 em") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_EM);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 5);

  g_assert (clutter_units_from_string (&units, "5 emeralds") == FALSE);

  g_assert (clutter_units_from_string (&units, "  16   mm") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_MM);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 16);

  g_assert (clutter_units_from_string (&units, "  24   pt   ") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_POINT);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 24);

  g_assert (clutter_units_from_string (&units, "  32   em   garbage") == FALSE);

  g_assert (clutter_units_from_string (&units, "5.1cm") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_CM);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 5.1f);

  g_assert (clutter_units_from_string (&units, "5,mm") == FALSE);

  g_assert (clutter_units_from_string (&units, ".5pt") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_POINT);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 0.5f);

  g_assert (clutter_units_from_string (&units, "1 omg!!pony") == FALSE);

  clutter_units_from_pt (&units, 24.0);
  string = clutter_units_to_string (&units);
  g_assert_cmpstr (string, ==, "24.0 pt");
  g_free (string);

  clutter_units_from_em (&units, 3.0);
  string = clutter_units_to_string (&units);
  g_assert_cmpstr (string, ==, "3.00 em");

  units.unit_type = CLUTTER_UNIT_PIXEL;
  units.value = 0;

  g_assert (clutter_units_from_string (&units, string) == TRUE);
  g_assert (clutter_units_get_unit_type (&units) != CLUTTER_UNIT_PIXEL);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_EM);
  g_assert_cmpint ((int) clutter_units_get_unit_value (&units), ==, 3);

  g_free (string);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/units/string", units_string)
  CLUTTER_TEST_UNIT ("/units/cache", units_cache)
  CLUTTER_TEST_UNIT ("/units/constructors", units_constructors)
)
