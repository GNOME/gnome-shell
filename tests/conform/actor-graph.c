#include <clutter/clutter.h>

static void
actor_add_child (void)
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

static void
actor_insert_child (void)
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

static void
actor_remove_child (void)
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

static void
actor_raise_child (void)
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

static void
actor_lower_child (void)
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

static void
actor_replace_child (void)
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

static void
actor_remove_all (void)
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

static void
actor_container_signals (void)
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

static void
actor_contains (void)
{
  /* This build up the following tree:
   *
   *              a
   *          ╱   │   ╲
   *         ╱    │    ╲
   *        b     c     d
   *       ╱ ╲   ╱ ╲   ╱ ╲
   *      e   f g   h i   j
   */
  struct {
    ClutterActor *actor_a, *actor_b, *actor_c, *actor_d, *actor_e;
    ClutterActor *actor_f, *actor_g, *actor_h, *actor_i, *actor_j;
  } d;
  int x, y;
  ClutterActor **actor_array = &d.actor_a;

  /* Matrix of expected results */
  static const gboolean expected_results[] =
    {         /* a, b, c, d, e, f, g, h, i, j */
      /* a */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      /* b */    0, 1, 0, 0, 1, 1, 0, 0, 0, 0,
      /* c */    0, 0, 1, 0, 0, 0, 1, 1, 0, 0,
      /* d */    0, 0, 0, 1, 0, 0, 0, 0, 1, 1,
      /* e */    0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
      /* f */    0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
      /* g */    0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
      /* h */    0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
      /* i */    0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
      /* j */    0, 0, 0, 0, 0, 0, 0, 0, 0, 1
    };

  d.actor_a = clutter_actor_new ();
  d.actor_b = clutter_actor_new ();
  d.actor_c = clutter_actor_new ();
  d.actor_d = clutter_actor_new ();
  d.actor_e = clutter_actor_new ();
  d.actor_f = clutter_actor_new ();
  d.actor_g = clutter_actor_new ();
  d.actor_h = clutter_actor_new ();
  d.actor_i = clutter_actor_new ();
  d.actor_j = clutter_actor_new ();

  clutter_actor_add_child (d.actor_a, d.actor_b);
  clutter_actor_add_child (d.actor_a, d.actor_c);
  clutter_actor_add_child (d.actor_a, d.actor_d);

  clutter_actor_add_child (d.actor_b, d.actor_e);
  clutter_actor_add_child (d.actor_b, d.actor_f);

  clutter_actor_add_child (d.actor_c, d.actor_g);
  clutter_actor_add_child (d.actor_c, d.actor_h);

  clutter_actor_add_child (d.actor_d, d.actor_i);
  clutter_actor_add_child (d.actor_d, d.actor_j);

  for (y = 0; y < 10; y++)
    for (x = 0; x < 10; x++)
      g_assert_cmpint (clutter_actor_contains (actor_array[x],
                                               actor_array[y]),
                       ==,
                       expected_results[x * 10 + y]);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/actor/graph/add-child", actor_add_child)
  CLUTTER_TEST_UNIT ("/actor/graph/insert-child", actor_insert_child)
  CLUTTER_TEST_UNIT ("/actor/graph/remove-child", actor_remove_child)
  CLUTTER_TEST_UNIT ("/actor/graph/raise-child", actor_raise_child)
  CLUTTER_TEST_UNIT ("/actor/graph/lower-child", actor_lower_child)
  CLUTTER_TEST_UNIT ("/actor/graph/replace-child", actor_replace_child)
  CLUTTER_TEST_UNIT ("/actor/graph/remove-all", actor_remove_all)
  CLUTTER_TEST_UNIT ("/actor/graph/container-signals", actor_container_signals)
  CLUTTER_TEST_UNIT ("/actor/graph/contains", actor_contains)
)
