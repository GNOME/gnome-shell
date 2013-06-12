#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "test-conform-common.h"

void
actor_initial_state (TestConformSimpleFixture *fixture,
                     gconstpointer             data)
{
  ClutterActor *actor;

  actor = clutter_actor_new ();
  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  if (g_test_verbose ())
    g_print ("initial state - visible: %s, realized: %s, mapped: %s\n",
             CLUTTER_ACTOR_IS_VISIBLE (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_REALIZED (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_MAPPED (actor) ? "yes" : "no");

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

void
actor_shown_not_parented (TestConformSimpleFixture *fixture,
                          gconstpointer             data)
{
  ClutterActor *actor;

  actor = clutter_rectangle_new ();
  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_show (actor);

  if (g_test_verbose ())
    g_print ("show without a parent - visible: %s, realized: %s, mapped: %s\n",
             CLUTTER_ACTOR_IS_VISIBLE (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_REALIZED (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_MAPPED (actor) ? "yes" : "no");

  g_assert (!CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);
}

void
actor_realized (TestConformSimpleFixture *fixture,
                gconstpointer             data)
{
  ClutterActor *actor;
  ClutterActor *stage;

  stage = clutter_stage_new ();

  actor = clutter_actor_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));

  clutter_actor_hide (actor); /* don't show, so won't map */
  clutter_actor_add_child (stage, actor);
  clutter_actor_realize (actor);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));

  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (stage);
}

void
actor_mapped (TestConformSimpleFixture *fixture,
              gconstpointer             data)
{
  ClutterActor *actor;
  ClutterActor *stage;

  stage = clutter_stage_new ();
  clutter_actor_show (stage);

  actor = clutter_actor_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clutter_actor_add_child (stage, actor);

  if (g_test_verbose ())
    g_print ("adding to a container should map - "
             "visible: %s, realized: %s, mapped: %s\n",
             CLUTTER_ACTOR_IS_VISIBLE (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_REALIZED (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_MAPPED (actor) ? "yes" : "no");

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_hide (actor);

  if (g_test_verbose ())
    g_print ("hiding should unmap - "
             "visible: %s, realized: %s, mapped: %s\n",
             CLUTTER_ACTOR_IS_VISIBLE (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_REALIZED (actor) ? "yes" : "no",
             CLUTTER_ACTOR_IS_MAPPED (actor) ? "yes" : "no");

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (stage);
}

void
actor_visibility_not_recursive (TestConformSimpleFixture *fixture,
                                gconstpointer             data)
{
  ClutterActor *actor, *group;
  ClutterActor *stage;

  stage = clutter_stage_new ();
  group = clutter_actor_new ();
  actor = clutter_actor_new ();

  clutter_actor_hide (group); /* don't show, so won't map */
  clutter_actor_hide (actor); /* don't show, so won't map */

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (stage)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_add_child (stage, group);
  clutter_actor_add_child (group, actor);

  clutter_actor_show (actor);
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (group));
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (stage));

  clutter_actor_show (stage);
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (group));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (stage));

  clutter_actor_hide (actor);
  clutter_actor_hide (group);
  clutter_actor_hide (stage);
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_show (stage);
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (stage);
}

void
actor_realize_not_recursive (TestConformSimpleFixture *fixture,
                             gconstpointer             data)
{
  ClutterActor *actor, *group;
  ClutterActor *stage;

  stage = clutter_stage_new ();
  clutter_actor_show (stage);

  group = clutter_actor_new ();

  actor = clutter_actor_new ();

  clutter_actor_hide (group); /* don't show, so won't map */
  clutter_actor_hide (actor); /* don't show, so won't map */

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));

  clutter_actor_add_child (stage, group);
  clutter_actor_add_child (group, actor);

  clutter_actor_realize (group);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (group));

  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));

  /* realizing group did not realize the child */
  g_assert (!CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (stage);
}

void
actor_map_recursive (TestConformSimpleFixture *fixture,
                     gconstpointer             data)
{
  ClutterActor *actor, *group;
  ClutterActor *stage;

  stage = clutter_stage_new ();
  clutter_actor_show (stage);

  group = clutter_actor_new ();

  actor = clutter_actor_new ();

  clutter_actor_hide (group); /* hide at first */
  clutter_actor_show (actor); /* show at first */

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));
  g_assert ((CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_add_child (stage, group);
  clutter_actor_add_child (group, actor);

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));
  g_assert ((CLUTTER_ACTOR_IS_VISIBLE (actor)));

  /* show group, which should map and realize both
   * group and child.
   */
  clutter_actor_show (group);
  g_assert (CLUTTER_ACTOR_IS_REALIZED (group));
  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (CLUTTER_ACTOR_IS_MAPPED (group));
  g_assert (CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (group));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (stage);
}

void
actor_show_on_set_parent (TestConformSimpleFixture *fixture,
                          gconstpointer             data)
{
  ClutterActor *actor, *group;
  gboolean show_on_set_parent;
  ClutterActor *stage;

  stage = clutter_stage_new ();

  group = clutter_actor_new ();

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));

  clutter_actor_add_child (stage, group);

  actor = clutter_actor_new ();
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));
  g_assert (show_on_set_parent);

  clutter_actor_add_child (group, actor);
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (show_on_set_parent);

  g_object_ref (actor);
  clutter_actor_remove_child (group, actor);
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (!CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (show_on_set_parent);

  clutter_actor_destroy (actor);
  clutter_actor_destroy (group);

  actor = clutter_actor_new ();
  clutter_actor_add_child (stage, actor);
  clutter_actor_hide (actor);
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (show_on_set_parent);

  clutter_actor_destroy (actor);

  actor = clutter_actor_new ();
  clutter_actor_hide (actor);
  clutter_actor_add_child (stage, actor);
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);
  g_assert (!CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (!show_on_set_parent);

  clutter_actor_destroy (actor);

  clutter_actor_destroy (stage);
}

void
clone_no_map (TestConformSimpleFixture *fixture,
              gconstpointer             data)
{
  ClutterActor *stage;
  ClutterActor *group;
  ClutterActor *actor;
  ClutterActor *clone;

  stage = clutter_stage_new ();
  clutter_actor_show (stage);

  group = clutter_actor_new ();
  actor = clutter_actor_new ();

  clutter_actor_hide (group);

  clutter_actor_add_child (group, actor);
  clutter_actor_add_child (stage, group);

  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clone = clutter_clone_new (group);

  clutter_actor_add_child (stage, clone);

  g_assert (CLUTTER_ACTOR_IS_MAPPED (clone));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clutter_actor_destroy (CLUTTER_ACTOR (clone));
  clutter_actor_destroy (CLUTTER_ACTOR (group));
  clutter_actor_destroy (stage);
}

void
actor_contains (TestConformSimpleFixture *fixture,
                gconstpointer             data)
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

void
default_stage (TestConformSimpleFixture *fixture,
               gconstpointer             data)
{
  ClutterActor *stage, *def_stage;

  stage = clutter_stage_new ();
  def_stage = clutter_stage_get_default ();

  if (clutter_feature_available (CLUTTER_FEATURE_STAGE_MULTIPLE))
    g_assert (stage != def_stage);
  else
    g_assert (stage == def_stage);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (def_stage));
}

void
actor_pivot_transformation (TestConformSimpleFixture *fixture,
                            gconstpointer             data)
{
  ClutterActor *stage, *actor_implicit, *actor_explicit;
  ClutterMatrix transform, result_implicit, result_explicit;
  ClutterActorBox allocation = CLUTTER_ACTOR_BOX_INIT (0, 0, 90, 30);
  gfloat angle = 30;

  stage = clutter_stage_new ();

  actor_implicit = clutter_actor_new ();
  actor_explicit = clutter_actor_new ();

  clutter_actor_add_child (stage, actor_implicit);
  clutter_actor_add_child (stage, actor_explicit);

  /* Fake allocation or pivot-point will not have any effect */
  clutter_actor_allocate (actor_implicit, &allocation, CLUTTER_ALLOCATION_NONE);
  clutter_actor_allocate (actor_explicit, &allocation, CLUTTER_ALLOCATION_NONE);

  clutter_actor_set_pivot_point (actor_implicit, 0.5, 0.5);
  clutter_actor_set_pivot_point (actor_explicit, 0.5, 0.5);

  /* Implict transformation */
  clutter_actor_set_rotation_angle (actor_implicit, CLUTTER_Z_AXIS, angle);

  /* Explict transformation */
  clutter_matrix_init_identity(&transform);
  cogl_matrix_rotate (&transform, angle, 0, 0, 1.0);
  clutter_actor_set_transform (actor_explicit, &transform);

  clutter_actor_get_transform (actor_implicit, &result_implicit);
  clutter_actor_get_transform (actor_explicit, &result_explicit);

  clutter_actor_destroy (stage);

  g_assert (cogl_matrix_equal (&result_implicit, &result_explicit));
}
