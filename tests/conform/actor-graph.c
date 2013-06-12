#include <clutter/clutter.h>
#include "test-conform-common.h"

void
actor_add_child (TestConformSimpleFixture *fixture,
                 gconstpointer dummy)
{
  ClutterActor *actor = clutter_actor_new ();
  ClutterActor *iter;

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo",
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "bar",
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "baz",
                                                NULL));

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 3);

  iter = clutter_actor_get_first_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");

  iter = clutter_actor_get_next_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");

  iter = clutter_actor_get_next_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "baz");
  g_assert (iter == clutter_actor_get_last_child (actor));
  g_assert (clutter_actor_get_next_sibling (iter) == NULL);

  iter = clutter_actor_get_last_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "baz");

  iter = clutter_actor_get_previous_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");

  iter = clutter_actor_get_previous_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");
  g_assert (iter == clutter_actor_get_first_child (actor));
  g_assert (clutter_actor_get_previous_sibling (iter) == NULL);

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

void
actor_insert_child (TestConformSimpleFixture *fixture,
                    gconstpointer data)
{
  ClutterActor *actor = clutter_actor_new ();
  ClutterActor *iter;

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_insert_child_at_index (actor,
                                       g_object_new (CLUTTER_TYPE_ACTOR,
                                                     "name", "foo",
                                                     NULL),
                                       0);

  iter = clutter_actor_get_first_child (actor);
  g_assert (iter != NULL);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");
  g_assert (iter == clutter_actor_get_child_at_index (actor, 0));

  clutter_actor_insert_child_below (actor,
                                    g_object_new (CLUTTER_TYPE_ACTOR,
                                                  "name", "bar",
                                                  NULL),
                                    iter);

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 2);

  iter = clutter_actor_get_first_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");
  iter = clutter_actor_get_next_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");
  g_assert (iter == clutter_actor_get_child_at_index (actor, 1));

  iter = clutter_actor_get_first_child (actor);
  clutter_actor_insert_child_above (actor,
                                    g_object_new (CLUTTER_TYPE_ACTOR,
                                                  "name", "baz",
                                                  NULL),
                                    iter);

  iter = clutter_actor_get_last_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");

  iter = clutter_actor_get_previous_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "baz");

  iter = clutter_actor_get_previous_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");

  clutter_actor_remove_all_children (actor);

  clutter_actor_insert_child_at_index (actor,
                                       g_object_new (CLUTTER_TYPE_ACTOR,
                                                     "name", "1",
                                                     NULL),
                                       0);
  iter = clutter_actor_get_child_at_index (actor, 0);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "1");
  g_assert (clutter_actor_get_first_child (actor) == iter);
  g_assert (clutter_actor_get_last_child (actor) == iter);

  clutter_actor_insert_child_at_index (actor,
                                       g_object_new (CLUTTER_TYPE_ACTOR,
                                                     "name", "2",
                                                     NULL),
                                       0);
  iter = clutter_actor_get_child_at_index (actor, 0);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "2");
  g_assert (clutter_actor_get_first_child (actor) == iter);
  iter = clutter_actor_get_child_at_index (actor, 1);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "1");
  g_assert (clutter_actor_get_last_child (actor) == iter);

  clutter_actor_insert_child_at_index (actor,
                                       g_object_new (CLUTTER_TYPE_ACTOR,
                                                     "name", "3",
                                                     NULL),
                                       -1);
  iter = clutter_actor_get_child_at_index (actor, 2);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "3");
  g_assert (clutter_actor_get_last_child (actor) == iter);

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

void
actor_remove_child (TestConformSimpleFixture *fixture,
                    gconstpointer data)
{
  ClutterActor *actor = clutter_actor_new ();
  ClutterActor *iter;

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo",
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "bar",
                                                NULL));

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 2);

  g_assert (clutter_actor_get_first_child (actor) != clutter_actor_get_last_child (actor));

  iter = clutter_actor_get_first_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");

  iter = clutter_actor_get_last_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");

  clutter_actor_remove_child (actor, clutter_actor_get_first_child (actor));

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 1);

  iter = clutter_actor_get_first_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");
  g_assert (clutter_actor_get_first_child (actor) == clutter_actor_get_last_child (actor));

  clutter_actor_remove_child (actor, clutter_actor_get_first_child (actor));

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 0);
  g_assert (clutter_actor_get_first_child (actor) == NULL);
  g_assert (clutter_actor_get_last_child (actor) == NULL);

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

void
actor_raise_child (TestConformSimpleFixture *fixture,
                   gconstpointer dummy)
{
  ClutterActor *actor = clutter_actor_new ();
  ClutterActor *iter;
  gboolean show_on_set_parent;

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo",
                                                "visible", FALSE,
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "bar",
                                                "visible", FALSE,
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "baz",
                                                "visible", FALSE,
                                                NULL));

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 3);

  iter = clutter_actor_get_child_at_index (actor, 1);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");

  clutter_actor_set_child_above_sibling (actor, iter,
                                         clutter_actor_get_child_at_index (actor, 2));

  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 0)),
                   ==,
                   "foo");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 1)),
                   ==,
                   "baz");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 2)),
                   ==,
                   "bar");
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (iter));
  g_object_get (iter, "show-on-set-parent", &show_on_set_parent, NULL);
  g_assert (!show_on_set_parent);

  iter = clutter_actor_get_child_at_index (actor, 0);
  clutter_actor_set_child_above_sibling (actor, iter, NULL);
  g_object_add_weak_pointer (G_OBJECT (iter), (gpointer *) &iter);

  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 0)),
                   ==,
                   "baz");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 1)),
                   ==,
                   "bar");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 2)),
                   ==,
                   "foo");
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (iter));
  g_object_get (iter, "show-on-set-parent", &show_on_set_parent, NULL);
  g_assert (!show_on_set_parent);

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
  g_assert (iter == NULL);
}

void
actor_lower_child (TestConformSimpleFixture *fixture,
                   gconstpointer dummy)
{
  ClutterActor *actor = clutter_actor_new ();
  ClutterActor *iter;
  gboolean show_on_set_parent;

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo",
                                                "visible", FALSE,
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "bar",
                                                "visible", FALSE,
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "baz",
                                                "visible", FALSE,
                                                NULL));

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 3);

  iter = clutter_actor_get_child_at_index (actor, 1);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");

  clutter_actor_set_child_below_sibling (actor, iter,
                                         clutter_actor_get_child_at_index (actor, 0));

  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 0)),
                   ==,
                   "bar");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 1)),
                   ==,
                   "foo");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 2)),
                   ==,
                   "baz");
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (iter));
  g_object_get (iter, "show-on-set-parent", &show_on_set_parent, NULL);
  g_assert (!show_on_set_parent);

  iter = clutter_actor_get_child_at_index (actor, 2);
  clutter_actor_set_child_below_sibling (actor, iter, NULL);

  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 0)),
                   ==,
                   "baz");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 1)),
                   ==,
                   "bar");
  g_assert_cmpstr (clutter_actor_get_name (clutter_actor_get_child_at_index (actor, 2)),
                   ==,
                   "foo");
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (iter));
  g_object_get (iter, "show-on-set-parent", &show_on_set_parent, NULL);
  g_assert (!show_on_set_parent);

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

void
actor_replace_child (TestConformSimpleFixture *fixture,
                     gconstpointer dummy)
{
  ClutterActor *actor = clutter_actor_new ();
  ClutterActor *iter;

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo",
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "bar",
                                                NULL));

  iter = clutter_actor_get_child_at_index (actor, 0);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");

  clutter_actor_replace_child (actor, iter,
                               g_object_new (CLUTTER_TYPE_ACTOR,
                                             "name", "baz",
                                             NULL));

  iter = clutter_actor_get_child_at_index (actor, 0);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "baz");

  iter = clutter_actor_get_child_at_index (actor, 1);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");

  clutter_actor_replace_child (actor, iter,
                               g_object_new (CLUTTER_TYPE_ACTOR,
                                             "name", "qux",
                                             NULL));

  iter = clutter_actor_get_child_at_index (actor, 0);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "baz");

  iter = clutter_actor_get_child_at_index (actor, 1);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "qux");

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo"));

  clutter_actor_replace_child (actor, iter,
                               g_object_new (CLUTTER_TYPE_ACTOR,
                                             "name", "bar",
                                             NULL));

  iter = clutter_actor_get_last_child (actor);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "foo");
  iter = clutter_actor_get_previous_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "bar");
  iter = clutter_actor_get_previous_sibling (iter);
  g_assert_cmpstr (clutter_actor_get_name (iter), ==, "baz");

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

void
actor_remove_all (TestConformSimpleFixture *fixture,
                  gconstpointer dummy)
{
  ClutterActor *actor = clutter_actor_new ();

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo",
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "bar",
                                                NULL));
  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "baz",
                                                NULL));

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 3);

  clutter_actor_remove_all_children (actor);

  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 0);

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

static void
actor_added (ClutterContainer *container,
             ClutterActor     *child,
             gpointer          data)
{
  ClutterActor *actor = CLUTTER_ACTOR (container);
  int *counter = data;
  ClutterActor *old_child;

  if (g_test_verbose ())
    g_print ("Adding actor '%s'\n", clutter_actor_get_name (child));

  old_child = clutter_actor_get_child_at_index (actor, 0);
  if (old_child != child)
    clutter_actor_remove_child (actor, old_child);

  *counter += 1;
}

static void
actor_removed (ClutterContainer *container,
               ClutterActor     *child,
               gpointer          data)
{
  int *counter = data;

  if (g_test_verbose ())
    g_print ("Removing actor '%s'\n", clutter_actor_get_name (child));

  *counter += 1;
}

void
actor_container_signals (TestConformSimpleFixture *fixture G_GNUC_UNUSED,
                         gconstpointer data G_GNUC_UNUSED)
{
  ClutterActor *actor = clutter_actor_new ();
  int add_count, remove_count;

  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  add_count = remove_count = 0;
  g_signal_connect (actor,
                    "actor-added", G_CALLBACK (actor_added),
                    &add_count);
  g_signal_connect (actor,
                    "actor-removed", G_CALLBACK (actor_removed),
                    &remove_count);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "foo",
                                                NULL));

  g_assert_cmpint (add_count, ==, 1);
  g_assert_cmpint (remove_count, ==, 0);
  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 1);

  clutter_actor_add_child (actor, g_object_new (CLUTTER_TYPE_ACTOR,
                                                "name", "bar",
                                                NULL));

  g_assert_cmpint (add_count, ==, 2);
  g_assert_cmpint (remove_count, ==, 1);
  g_assert_cmpint (clutter_actor_get_n_children (actor), ==, 1);

  g_signal_handlers_disconnect_by_func (actor, G_CALLBACK (actor_added),
                                        &add_count);
  g_signal_handlers_disconnect_by_func (actor, G_CALLBACK (actor_removed),
                                        &remove_count);

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}
