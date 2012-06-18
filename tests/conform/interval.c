#include <clutter/clutter.h>

#include "test-conform-common.h"

void
interval_initial_state (TestConformSimpleFixture *fixture G_GNUC_UNUSED,
                        gconstpointer dummy G_GNUC_UNUSED)
{
  ClutterInterval *interval;
  int initial, final;
  const GValue *value;

  interval = clutter_interval_new (G_TYPE_INT, 0, 100);
  g_assert (CLUTTER_IS_INTERVAL (interval));
  g_assert (clutter_interval_get_value_type (interval) == G_TYPE_INT);

  clutter_interval_get_interval (interval, &initial, &final);
  g_assert_cmpint (initial, ==, 0);
  g_assert_cmpint (final, ==, 100);

  value = clutter_interval_compute (interval, 0);
  g_assert (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 0);

  value = clutter_interval_compute (interval, 1);
  g_assert (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 100);

  value = clutter_interval_compute (interval, 0.5);
  g_assert (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 50);

  clutter_interval_set_final (interval, 200);
  value = clutter_interval_peek_final_value (interval);
  g_assert (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 200);

  g_object_unref (interval);
}
