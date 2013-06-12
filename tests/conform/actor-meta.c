#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "test-conform-common.h"

void
actor_meta_clear (TestConformSimpleFixture *fixture G_GNUC_UNUSED,
                  gconstpointer             data G_GNUC_UNUSED)
{
  ClutterActor *actor, *stage;

  stage = clutter_stage_new ();

  actor = clutter_actor_new ();
  g_object_ref_sink (actor);
  g_object_add_weak_pointer (G_OBJECT (actor), (gpointer *) &actor);

  clutter_actor_add_action (actor, clutter_click_action_new ());
  clutter_actor_add_constraint (actor, clutter_bind_constraint_new (stage, CLUTTER_BIND_ALL, 0));
  clutter_actor_add_effect (actor, clutter_blur_effect_new ());

  g_assert (clutter_actor_has_actions (actor));
  g_assert (clutter_actor_has_constraints (actor));
  g_assert (clutter_actor_has_effects (actor));

  clutter_actor_clear_actions (actor);
  g_assert (!clutter_actor_has_actions (actor));

  clutter_actor_clear_constraints (actor);
  g_assert (!clutter_actor_has_constraints (actor));

  clutter_actor_clear_effects (actor);
  g_assert (!clutter_actor_has_effects (actor));

  clutter_actor_destroy (actor);
  g_assert (actor == NULL);

  clutter_actor_destroy (stage);
}
