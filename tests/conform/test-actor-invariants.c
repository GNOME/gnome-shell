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
test_realized (TestConformSimpleFixture *fixture,
               gconstpointer             data)
{
  ClutterActor *actor;

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));

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

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clutter_actor_show (actor);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (CLUTTER_ACTOR_IS_MAPPED (actor));

  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (actor);
}

void
test_show_on_set_parent (TestConformSimpleFixture *fixture,
                         gconstpointer             data)
{
  ClutterActor *actor, *group;
  gboolean show_on_set_parent;

  group = clutter_group_new ();

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));

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

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));
  g_assert (show_on_set_parent == TRUE);

  clutter_actor_destroy (actor);
  clutter_actor_destroy (group);
}
