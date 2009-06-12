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

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               actor);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (CLUTTER_ACTOR_IS_MAPPED (actor));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (actor);
}

void
test_realize_not_recursive (TestConformSimpleFixture *fixture,
                            gconstpointer             data)
{
  ClutterActor *actor, *group;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();

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
}

void
test_map_recursive (TestConformSimpleFixture *fixture,
                    gconstpointer             data)
{
  ClutterActor *actor, *group;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();

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
  g_object_get (G_OBJECT (actor),
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));
  g_assert (show_on_set_parent == TRUE);

  clutter_group_add (group, actor);
  g_object_get (G_OBJECT (actor),
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (show_on_set_parent == TRUE);

  g_object_ref (actor);
  clutter_actor_unparent (actor);
  g_object_get (G_OBJECT (actor),
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
}
