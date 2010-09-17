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
 * <refsect2 id="ClutterConstraint-implementation">
 *   <title>Implemting a ClutterConstraint</title>
 *   <para>Creating a sub-class of #ClutterConstraint requires the
 *   implementation of the <function>update_allocation()</function>
 *   virtual function.</para>
 *   <para>The <function>update_allocation()</function> virtual function
 *   is called during the allocation sequence of a #ClutterActor, and
 *   allows any #ClutterConstraint attached to that actor to modify the
 *   allocation before it is passed to the <function>allocate()</function>
 *   implementation.</para>
 *   <para>The #ClutterActorBox passed to the
 *   <function>update_allocation()</function> implementation contains the
 *   original allocation of the #ClutterActor, plus the eventual modifications
 *   applied by the other #ClutterConstraint<!-- -->s.</para>
 *   <note><para>Constraints are queried in the same order as they were
 *   applied using clutter_actor_add_constraint() or
 *   clutter_actor_add_constraint_with_name().</para></note>
 *   <para>It is not necessary for a #ClutterConstraint sub-class to chain
 *   up to the parent's implementation.</para>
 *   <para>If a #ClutterConstraint is parametrized - i.e. if it contains
 *   properties that affect the way the constraint is implemented - it should
 *   call clutter_actor_queue_relayout() on the actor to which it is attached
 *   to whenever any parameter is changed. The actor to which it is attached
 *   can be recovered at any point using clutter_actor_meta_get_actor().</para>
 * </refsect2>
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
