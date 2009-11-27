#include <stdio.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_cogl_fixed (TestConformSimpleFixture *fixture,
		 gconstpointer data)
{
  g_assert_cmpint (COGL_FIXED_1, ==, COGL_FIXED_FROM_FLOAT (1.0));
  g_assert_cmpint (COGL_FIXED_1, ==, COGL_FIXED_FROM_INT (1));

  g_assert_cmpint (COGL_FIXED_0_5, ==, COGL_FIXED_FROM_FLOAT (0.5));

  g_assert_cmpfloat (COGL_FIXED_TO_FLOAT (COGL_FIXED_1), ==, 1.0);
  g_assert_cmpfloat (COGL_FIXED_TO_FLOAT (COGL_FIXED_0_5), ==, 0.5);
}

