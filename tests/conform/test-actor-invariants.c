#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "test-conform-common.h"

void
test_initial_state (TestConformSimpleFixture *fixture,
                    gconstpointer             data)
{
  ClutterActor *actor;

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (actor);
}

void
test_shown_not_parented (TestConformSimpleFixture *fixture,
                         gconstpointer             data)
{
  ClutterActor *actor;

  actor = clutter_rectangle_new ();

  clutter_actor_show (actor);

  g_assert (!CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (actor);
}

void
test_realized (TestConformSimpleFixture *fixture,
               gconstpointer             data)
{
  ClutterActor *actor;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));

  clutter_actor_hide (actor); /* don't show, so won't map */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               actor);
  clutter_actor_realize (actor);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));

  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (actor);
}

void
test_mapped (TestConformSimpleFixture *fixture,
             gconstpointer             data)
{
  ClutterActor *actor;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();
  clutter_actor_show (stage);

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               actor);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (actor);
  clutter_actor_hide (stage);
}

void
test_realize_not_recursive (TestConformSimpleFixture *fixture,
                            gconstpointer             data)
{
  ClutterActor *actor, *group;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();
  clutter_actor_show (stage);

  group = clutter_group_new ();

  actor = clutter_rectangle_new ();

  clutter_actor_hide (group); /* don't show, so won't map */
  clutter_actor_hide (actor); /* don't show, so won't map */

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               group);
  clutter_container_add_actor (CLUTTER_CONTAINER (group),
                               actor);

  clutter_actor_realize (group);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (group));

  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));

  /* realizing group did not realize the child */
  g_assert (!CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (group);
  clutter_actor_hide (stage);
}

void
test_map_recursive (TestConformSimpleFixture *fixture,
                    gconstpointer             data)
{
  ClutterActor *actor, *group;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();
  clutter_actor_show (stage);

  group = clutter_group_new ();

  actor = clutter_rectangle_new ();

  clutter_actor_hide (group); /* hide at first */
  clutter_actor_show (actor); /* show at first */

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));
  g_assert ((CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               group);
  clutter_container_add_actor (CLUTTER_CONTAINER (group),
                               actor);

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

  clutter_actor_destroy (group);
  clutter_actor_hide (stage);
}

void
test_show_on_set_parent (TestConformSimpleFixture *fixture,
                         gconstpointer             data)
{
  ClutterActor *actor, *group;
  gboolean show_on_set_parent;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();

  group = clutter_group_new ();

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               group);

  actor = clutter_rectangle_new ();
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));
  g_assert (show_on_set_parent == TRUE);

  clutter_container_add_actor (CLUTTER_CONTAINER (group), actor);
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (show_on_set_parent == TRUE);

  g_object_ref (actor);
  clutter_actor_unparent (actor);
  g_object_get (actor,
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (!CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (show_on_set_parent == TRUE);

  clutter_actor_destroy (actor);
  clutter_actor_destroy (group);
}

void
test_clone_no_map (TestConformSimpleFixture *fixture,
                   gconstpointer             data)
{
  ClutterActor *stage;
  ClutterActor *group;
  ClutterActor *actor;
  ClutterActor *clone;

  stage = clutter_stage_get_default ();
  clutter_actor_show (stage);

  group = clutter_group_new ();
  actor = clutter_rectangle_new ();

  clutter_actor_hide (group);

  clutter_container_add_actor (CLUTTER_CONTAINER (group), actor);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clone = clutter_clone_new (group);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), clone);

  g_assert (CLUTTER_ACTOR_IS_MAPPED (clone));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (group)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clutter_actor_destroy (CLUTTER_ACTOR (clone));
  clutter_actor_destroy (CLUTTER_ACTOR (group));

  clutter_actor_hide (stage);
}

void
test_contains (TestConformSimpleFixture *fixture,
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

  d.actor_a = clutter_group_new ();
  d.actor_b = clutter_group_new ();
  d.actor_c = clutter_group_new ();
  d.actor_d = clutter_group_new ();
  d.actor_e = clutter_group_new ();
  d.actor_f = clutter_group_new ();
  d.actor_g = clutter_group_new ();
  d.actor_h = clutter_group_new ();
  d.actor_i = clutter_group_new ();
  d.actor_j = clutter_group_new ();

  clutter_container_add (CLUTTER_CONTAINER (d.actor_a),
                         d.actor_b, d.actor_c, d.actor_d, NULL);

  clutter_container_add (CLUTTER_CONTAINER (d.actor_b),
                         d.actor_e, d.actor_f, NULL);

  clutter_container_add (CLUTTER_CONTAINER (d.actor_c),
                         d.actor_g, d.actor_h, NULL);

  clutter_container_add (CLUTTER_CONTAINER (d.actor_d),
                         d.actor_i, d.actor_j, NULL);

  for (y = 0; y < 10; y++)
    for (x = 0; x < 10; x++)
      g_assert_cmpint (clutter_actor_contains (actor_array[x],
                                               actor_array[y]),
                       ==,
                       expected_results[x * 10 + y]);
}
