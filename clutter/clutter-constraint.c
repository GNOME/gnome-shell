/**
 * SECTION:clutter-constraint
 * @Title: ClutterConstraint
 * @Short_Description: Abstract class for constraints on position or size
 * @See_Also: #ClutterAction
 *
 * #ClutterConstraint is a base abstract class for modifiers of a #ClutterActor
 * position or size.
 *
 * A #ClutterConstraint sub-class should contain the logic for modifying
 * the position or size of the #ClutterActor to which it is applied, by
 * updating the actor's allocation. Each #ClutterConstraint can change the
 * allocation of the actor to which they are applied by overriding the
 * <function>update_allocation()</function> virtual function.
 *
 * #ClutterConstraint is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-constraint.h"

#include "clutter-actor.h"
#include "clutter-actor-meta-private.h"

G_DEFINE_ABSTRACT_TYPE (ClutterConstraint,
                        clutter_constraint,
                        CLUTTER_TYPE_ACTOR_META);

static void
constraint_update_allocation (ClutterConstraint *constraint,
                              ClutterActor      *actor,
                              ClutterActorBox   *allocation)
{
}

static void
clutter_constraint_class_init (ClutterConstraintClass *klass)
{
  klass->update_allocation = constraint_update_allocation;
}

static void
clutter_constraint_init (ClutterConstraint *self)
{
}

void
_clutter_constraint_update_allocation (ClutterConstraint *constraint,
                                       ClutterActor      *actor,
                                       ClutterActorBox   *allocation)
{
  g_return_if_fail (CLUTTER_IS_CONSTRAINT (constraint));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (allocation != NULL);

  CLUTTER_CONSTRAINT_GET_CLASS (constraint)->update_allocation (constraint,
                                                                actor,
                                                                allocation);
}
