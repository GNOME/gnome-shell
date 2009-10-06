#include <stdio.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_units_constructors (TestConformSimpleFixture *fixture,
                         gconstpointer data)
{
  ClutterUnits units;

  clutter_units_from_pixels (&units, 100);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_PIXEL);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 100.0);
  g_assert_cmpfloat (clutter_units_to_pixels (&units), ==, 100.0);

  clutter_units_from_em (&units, 5.0);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_EM);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 5.0);
  g_assert_cmpfloat (clutter_units_to_pixels (&units), !=, 5.0);
}

void
test_units_string (TestConformSimpleFixture *fixture,
                   gconstpointer data)
{
  ClutterUnits units;
  gchar *string;

  g_assert (clutter_units_from_string (&units, "10") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_PIXEL);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 10);

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

  g_assert (clutter_units_from_string (&units, "5.1mm") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_MM);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 5.1f);

  g_assert (clutter_units_from_string (&units, "5,mm") == FALSE);

  g_assert (clutter_units_from_string (&units, ".5pt") == TRUE);
  g_assert (clutter_units_get_unit_type (&units) == CLUTTER_UNIT_POINT);
  g_assert_cmpfloat (clutter_units_get_unit_value (&units), ==, 0.5f);

  g_assert (clutter_units_from_string (&units, "1 pony") == FALSE);

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
