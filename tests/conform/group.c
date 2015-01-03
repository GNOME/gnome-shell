#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include <clutter/clutter.h>

static void
group_depth_sorting (void)
{
  ClutterActor *group;
  ClutterActor *child, *test;
  ClutterGroup *g;
  GList *children;

  group = clutter_group_new ();
  g = CLUTTER_GROUP (group);

  child = clutter_rectangle_new ();
  clutter_actor_set_size (child, 20, 20);
  clutter_actor_set_depth (child, 0);
  clutter_actor_set_name (child, "zero");
  clutter_container_add_actor (CLUTTER_CONTAINER (group), child);

  children = clutter_container_get_children (CLUTTER_CONTAINER (group));
  g_assert (children->data == child);
  g_assert (children->next == NULL);
  g_list_free (children);

  child = clutter_rectangle_new ();
  clutter_actor_set_size (child, 20, 20);
  clutter_actor_set_depth (child, 10);
  clutter_actor_set_name (child, "plus-ten");
  clutter_container_add_actor (CLUTTER_CONTAINER (group), child);

  test = clutter_group_get_nth_child (g, 0);
  g_assert_cmpstr (clutter_actor_get_name (test), ==, "zero");

  test = clutter_group_get_nth_child (g, 1);
  g_assert_cmpstr (clutter_actor_get_name (test), ==, "plus-ten");

  child = clutter_rectangle_new ();
  clutter_actor_set_size (child, 20, 20);
  clutter_actor_set_depth (child, -10);
  clutter_actor_set_name (child, "minus-ten");
  clutter_container_add_actor (CLUTTER_CONTAINER (group), child);

  g_assert_cmpint (clutter_group_get_n_children (g), ==, 3);

  test = clutter_group_get_nth_child (g, 0);
  g_assert_cmpstr (clutter_actor_get_name (test), ==, "minus-ten");

  test = clutter_group_get_nth_child (g, 1);
  g_assert_cmpstr (clutter_actor_get_name (test), ==, "zero");

  test = clutter_group_get_nth_child (g, 2);
  g_assert_cmpstr (clutter_actor_get_name (test), ==, "plus-ten");

  clutter_actor_destroy (group);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/group/depth-sorting", group_depth_sorting)
)
