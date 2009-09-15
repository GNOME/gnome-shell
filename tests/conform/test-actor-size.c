#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "test-conform-common.h"

#define TEST_TYPE_ACTOR         (test_actor_get_type ())

typedef struct _TestActor               TestActor;
typedef struct _ClutterActorClass       TestActorClass;

struct _TestActor
{
  ClutterActor parent_instance;

  guint preferred_width_called  : 1;
  guint preferred_height_called : 1;
};

G_DEFINE_TYPE (TestActor, test_actor, CLUTTER_TYPE_ACTOR);

static void
test_actor_get_preferred_width (ClutterActor *self,
                                gfloat        for_height,
                                gfloat       *min_width_p,
                                gfloat       *nat_width_p)
{
  TestActor *test = (TestActor *) self;

  test->preferred_width_called = TRUE;

  if (for_height == 10)
    {
      *min_width_p = 10;
      *nat_width_p = 100;
    }
  else
    {
      *min_width_p = 100;
      *nat_width_p = 100;
    }
}

static void
test_actor_get_preferred_height (ClutterActor *self,
                                 gfloat        for_width,
                                 gfloat       *min_height_p,
                                 gfloat       *nat_height_p)
{
  TestActor *test = (TestActor *) self;

  test->preferred_height_called = TRUE;

  if (for_width == 10)
    {
      *min_height_p = 50;
      *nat_height_p = 100;
    }
  else
    {
      *min_height_p = 100;
      *nat_height_p = 100;
    }
}

static void
test_actor_class_init (TestActorClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_preferred_width = test_actor_get_preferred_width;
  actor_class->get_preferred_height = test_actor_get_preferred_height;
}

static void
test_actor_init (TestActor *self)
{
}

void
test_preferred_size (TestConformSimpleFixture *fixture,
                     gconstpointer             data)
{
  ClutterActor *test;
  TestActor *self;
  gfloat min_width, min_height;
  gfloat nat_width, nat_height;

  test = g_object_new (TEST_TYPE_ACTOR, NULL);
  self = (TestActor *) test;

  if (g_test_verbose ())
    g_print ("Preferred size\n");

  clutter_actor_get_preferred_size (test,
                                    &min_width, &min_height,
                                    &nat_width, &nat_height);

  g_assert (self->preferred_width_called);
  g_assert (self->preferred_height_called);
  g_assert_cmpfloat (min_width, ==, 100);
  g_assert_cmpfloat (min_height, ==, 100);
  g_assert_cmpfloat (nat_width, ==, min_width);
  g_assert_cmpfloat (nat_height, ==, min_height);

  if (g_test_verbose ())
    g_print ("Preferred width\n");
  self->preferred_width_called = FALSE;
  clutter_actor_get_preferred_width (test, 10, &min_width, &nat_width);
  g_assert (self->preferred_width_called);
  g_assert_cmpfloat (min_width, ==, 10);
  g_assert_cmpfloat (nat_width, ==, 100);

  if (g_test_verbose ())
    g_print ("Preferred height\n");
  self->preferred_height_called = FALSE;
  clutter_actor_get_preferred_height (test, 200, &min_height, &nat_height);
  g_assert (self->preferred_height_called);
  g_assert_cmpfloat (min_height, !=, 10);
  g_assert_cmpfloat (nat_height, ==, 100);

  if (g_test_verbose ())
    g_print ("Preferred width (cached)\n");
  self->preferred_width_called = FALSE;
  clutter_actor_get_preferred_width (test, 10, &min_width, &nat_width);
  g_assert (!self->preferred_width_called);
  g_assert_cmpfloat (min_width, ==, 10);
  g_assert_cmpfloat (nat_width, ==, 100);

  if (g_test_verbose ())
    g_print ("Preferred height (cache eviction)\n");
  self->preferred_height_called = FALSE;
  clutter_actor_get_preferred_height (test, 10, &min_height, &nat_height);
  g_assert (self->preferred_height_called);
  g_assert_cmpfloat (min_height, ==, 50);
  g_assert_cmpfloat (nat_height, ==, 100);

  clutter_actor_destroy (test);
}

void
test_fixed_size (TestConformSimpleFixture *fixture,
                 gconstpointer             data)
{
  ClutterActor *rect;
  gboolean min_width_set, nat_width_set;
  gboolean min_height_set, nat_height_set;
  gfloat min_width, min_height;
  gfloat nat_width, nat_height;

  rect = clutter_rectangle_new ();

  if (g_test_verbose ())
    g_print ("Initial size is 0\n");

  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 0);
  g_assert_cmpfloat (clutter_actor_get_height (rect), ==, 0);

  clutter_actor_set_size (rect, 100, 100);

  if (g_test_verbose ())
    g_print ("Explicit size set\n");

  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 100);
  g_assert_cmpfloat (clutter_actor_get_height (rect), ==, 100);

  g_object_get (G_OBJECT (rect),
                "min-width-set", &min_width_set,
                "min-height-set", &min_height_set,
                "natural-width-set", &nat_width_set,
                "natural-height-set", &nat_height_set,
                NULL);

  if (g_test_verbose ())
    g_print ("Notification properties\n");

  g_assert (min_width_set && nat_width_set);
  g_assert (min_height_set && nat_height_set);

  clutter_actor_get_preferred_size (rect,
                                    &min_width, &min_height,
                                    &nat_width, &nat_height);

  if (g_test_verbose ())
    g_print ("Preferred size\n");

  g_assert_cmpfloat (min_width, ==, 100);
  g_assert_cmpfloat (min_height, ==, 100);
  g_assert_cmpfloat (min_width, ==, nat_width);
  g_assert_cmpfloat (min_height, ==, nat_height);

  clutter_actor_set_size (rect, -1, -1);

  if (g_test_verbose ())
    g_print ("Explicit size unset\n");

  g_object_get (G_OBJECT (rect),
                "min-width-set", &min_width_set,
                "min-height-set", &min_height_set,
                "natural-width-set", &nat_width_set,
                "natural-height-set", &nat_height_set,
                NULL);
  g_assert (!min_width_set && !nat_width_set);
  g_assert (!min_height_set && !nat_height_set);

  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 0);
  g_assert_cmpfloat (clutter_actor_get_height (rect), ==, 0);

  clutter_actor_destroy (rect);
}
