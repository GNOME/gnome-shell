#include <glib.h>
#include <clutter/clutter.h>

#include "test-conform-common.h"

typedef struct _BehaviourFixture BehaviourFixture;

typedef void (* BehaviourTestFunc) (BehaviourFixture *fixture);

struct _BehaviourFixture
{
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;
  ClutterActor *rect;
};

static void
opacity_behaviour (BehaviourFixture *fixture)
{
  ClutterBehaviour *behaviour;
  guint8 start, end;
  guint starti;

  behaviour = clutter_behaviour_opacity_new (fixture->alpha, 0, 255);
  g_assert (CLUTTER_IS_BEHAVIOUR_OPACITY (behaviour));

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
}

static const struct
{
  const gchar *desc;
  BehaviourTestFunc func;
} behaviour_tests[] = {
  { "BehaviourOpacity", opacity_behaviour }
};

static const gint n_behaviour_tests = G_N_ELEMENTS (behaviour_tests);

void
test_behaviours (TestConformSimpleFixture *fixture,
                 gconstpointer dummy)
{
  BehaviourFixture b_fixture;
  gint i;

  b_fixture.timeline = clutter_timeline_new (1000);
  b_fixture.alpha = clutter_alpha_new_full (b_fixture.timeline, CLUTTER_LINEAR);
  b_fixture.rect = clutter_rectangle_new ();

  g_object_ref_sink (b_fixture.alpha);
  g_object_unref (b_fixture.timeline);

  for (i = 0; i < n_behaviour_tests; i++)
    {
      if (g_test_verbose ())
        g_print ("Testing: %s\n", behaviour_tests[i].desc);

      behaviour_tests[i].func (&b_fixture);
    }

  g_object_unref (b_fixture.alpha);
  clutter_actor_destroy (b_fixture.rect);
}
