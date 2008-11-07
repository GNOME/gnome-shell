#include <stdio.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_fixed_constants (TestConformSimpleFixture *fixture,
		      gconstpointer data)
{
  g_assert_cmpint (CFX_ONE, ==, CLUTTER_FLOAT_TO_FIXED (1.0));
  g_assert_cmpint (CFX_ONE, ==, CLUTTER_INT_TO_FIXED (1));

  g_assert_cmpint (CFX_HALF, ==, CLUTTER_FLOAT_TO_FIXED (0.5));

  g_assert_cmpfloat (CLUTTER_FIXED_TO_FLOAT (CFX_ONE), ==, 1.0);
  g_assert_cmpfloat (CLUTTER_FIXED_TO_FLOAT (CFX_HALF), ==, 0.5);
}

