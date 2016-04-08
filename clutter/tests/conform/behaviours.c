#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include <clutter/clutter.h>

static void
behaviour_opacity (void)
{
  ClutterBehaviour *behaviour;
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;
  guint8 start, end;
  guint starti;

  timeline = clutter_timeline_new (500);
  alpha = clutter_alpha_new_full (timeline, CLUTTER_LINEAR);
  behaviour = clutter_behaviour_opacity_new (alpha, 0, 255);
  g_assert (CLUTTER_IS_BEHAVIOUR_OPACITY (behaviour));
  g_object_add_weak_pointer (G_OBJECT (behaviour), (gpointer *) &behaviour);
  g_object_add_weak_pointer (G_OBJECT (timeline), (gpointer *) &timeline);

  clutter_behaviour_opacity_get_bounds (CLUTTER_BEHAVIOUR_OPACITY (behaviour),
                                        &start,
                                        &end);

  if (g_test_verbose ())
    g_print ("BehaviourOpacity:bounds = %d, %d (expected: 0, 255)\n",
             start,
             end);

  g_assert_cmpint (start, ==, 0);
  g_assert_cmpint (end, ==, 255);

  clutter_behaviour_opacity_set_bounds (CLUTTER_BEHAVIOUR_OPACITY (behaviour),
                                        255,
                                        0);
  /* XXX: The gobject property is actually a unsigned int not unsigned char
   * property so we have to be careful not to corrupt the stack by passing
   * a guint8 pointer here... */
  starti = 0;
  g_object_get (G_OBJECT (behaviour), "opacity-start", &starti, NULL);

  if (g_test_verbose ())
    g_print ("BehaviourOpacity:start = %d (expected: 255)\n", start);

  g_assert_cmpint (starti, ==, 255);

  g_object_unref (behaviour);
  g_object_unref (timeline);

  g_assert_null (behaviour);
  g_assert_null (timeline);
}

static struct
{
  const gchar *path;
  GTestFunc func;
} behaviour_tests[] = {
  { "opacity", behaviour_opacity },
};

static const int n_behaviour_tests = G_N_ELEMENTS (behaviour_tests);

int
main (int argc, char *argv[])
{
  int i;

  clutter_test_init (&argc, &argv);

  for (i = 0; i < n_behaviour_tests; i++)
    {
      char *path = g_strconcat ("/behaviours/", behaviour_tests[i].path, NULL);

      clutter_test_add (path, behaviour_tests[i].func);

      g_free (path);
    }

  return clutter_test_run ();
}
