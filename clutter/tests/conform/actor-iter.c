#include <glib.h>
#include <clutter/clutter.h>

static void
actor_iter_traverse_children (void)
{
  ClutterActorIter iter;
  ClutterActor *actor;
  ClutterActor *child;
  int i, n_actors;

  actor = clutter_actor_new ();
  clutter_actor_set_name (actor, "root");
  g_object_ref_sink (actor);

  n_actors = g_random_int_range (10, 50);
  for (i = 0; i < n_actors; i++)
    {
      char *name;

      name = g_strdup_printf ("actor%d", i);
      child = clutter_actor_new ();
      clutter_actor_set_name (child, name);

      clutter_actor_add_child (actor, child);

      g_free (name);
    }

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, n_actors);

  i = 0;
  clutter_actor_iter_init (&iter, actor);
  g_assert (clutter_actor_iter_is_valid (&iter));

  while (clutter_actor_iter_next (&iter, &child))
    {
      g_assert (CLUTTER_IS_ACTOR (child));
      g_assert (clutter_actor_get_parent (child) == actor);

      if (g_test_verbose ())
        g_print ("actor %d = '%s'\n", i, clutter_actor_get_name (child));

      if (i == 0)
        g_assert (child == clutter_actor_get_first_child (actor));

      if (i == (n_actors - 1))
        g_assert (child == clutter_actor_get_last_child (actor));

      i += 1;
    }

  g_assert_cmpint (i, ==, n_actors);

  i = 0;
  clutter_actor_iter_init (&iter, actor);
  g_assert (clutter_actor_iter_is_valid (&iter));

  while (clutter_actor_iter_prev (&iter, &child))
    {
      g_assert (CLUTTER_IS_ACTOR (child));
      g_assert (clutter_actor_get_parent (child) == actor);

      if (g_test_verbose ())
        g_print ("actor %d = '%s'\n", i, clutter_actor_get_name (child));

      if (i == 0)
        g_assert (child == clutter_actor_get_last_child (actor));

      if (i == (n_actors - 1))
        g_assert (child == clutter_actor_get_first_child (actor));

      i += 1;
    }

  g_object_unref (actor);
}

static void
actor_iter_traverse_remove (void)
{
  ClutterActorIter iter;
  ClutterActor *actor;
  ClutterActor *child;
  int i, n_actors;

  actor = clutter_actor_new ();
  clutter_actor_set_name (actor, "root");
  g_object_ref_sink (actor);

  n_actors = g_random_int_range (10, 50);
  for (i = 0; i < n_actors; i++)
    {
      char *name;

      name = g_strdup_printf ("actor%d", i);
      child = clutter_actor_new ();
      clutter_actor_set_name (child, name);

      clutter_actor_add_child (actor, child);

      g_free (name);
    }

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, n_actors);

  i = 0;
  clutter_actor_iter_init (&iter, actor);
  g_assert (clutter_actor_iter_is_valid (&iter));

  while (clutter_actor_iter_next (&iter, &child))
    {
      g_assert (CLUTTER_IS_ACTOR (child));
      g_assert (clutter_actor_get_parent (child) == actor);

      if (g_test_verbose ())
        g_print ("actor %d = '%s'\n", i, clutter_actor_get_name (child));

      if (i == 0)
        g_assert (child == clutter_actor_get_first_child (actor));

      if (i == (n_actors - 1))
        g_assert (child == clutter_actor_get_last_child (actor));

      clutter_actor_iter_remove (&iter);
      g_assert (clutter_actor_iter_is_valid (&iter));

      i += 1;
    }

  g_assert_cmpint (i, ==, n_actors);
  g_assert_cmpint (0, ==, clutter_actor_get_n_children (actor));
}

static void
actor_iter_assignment (void)
{
  ClutterActorIter iter_a, iter_b;
  ClutterActor *actor;
  ClutterActor *child;
  int i, n_actors;

  actor = clutter_actor_new ();
  clutter_actor_set_name (actor, "root");
  g_object_ref_sink (actor);

  n_actors = g_random_int_range (10, 50);
  for (i = 0; i < n_actors; i++)
    {
      char *name;

      name = g_strdup_printf ("actor[%02d]", i);
      child = clutter_actor_new ();
      clutter_actor_set_name (child, name);

      clutter_actor_add_child (actor, child);

      g_free (name);
    }

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, n_actors);

  i = 0;

  clutter_actor_iter_init (&iter_a, actor);

  iter_b = iter_a;

  g_assert (clutter_actor_iter_is_valid (&iter_a));
  g_assert (clutter_actor_iter_is_valid (&iter_b));

  while (clutter_actor_iter_next (&iter_a, &child))
    {
      g_assert (CLUTTER_IS_ACTOR (child));
      g_assert (clutter_actor_get_parent (child) == actor);

      if (g_test_verbose ())
        g_print ("actor %2d = '%s'\n", i, clutter_actor_get_name (child));

      if (i == 0)
        g_assert (child == clutter_actor_get_first_child (actor));

      if (i == (n_actors - 1))
        g_assert (child == clutter_actor_get_last_child (actor));

      i += 1;
    }

  g_assert_cmpint (i, ==, n_actors);

  i = n_actors - 1;

  while (clutter_actor_iter_prev (&iter_b, &child))
    {
      g_assert (clutter_actor_get_parent (child) == actor);

      if (g_test_verbose ())
        g_print ("actor %2d = '%s'\n", i, clutter_actor_get_name (child));

      if (i == n_actors - 1)
        g_assert (child == clutter_actor_get_last_child (actor));

      if (i == 0)
        g_assert (child == clutter_actor_get_first_child (actor));

      i -= 1;
    }

  g_assert_cmpint (i, ==, -1);

  g_object_unref (actor);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/actor/iter/traverse-children", actor_iter_traverse_children)
  CLUTTER_TEST_UNIT ("/actor/iter/traverse-remove", actor_iter_traverse_remove)
  CLUTTER_TEST_UNIT ("/actor/iter/assignment", actor_iter_assignment)
)
