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
 * #ClutterConstraintClass.update_allocation() virtual function.
 *
 * #ClutterConstraint is available since Clutter 1.4
 *
 * ## Using Constraints
 *
 * Constraints can be used with fixed layout managers, like
 * #ClutterFixedLayout, or with actors implicitly using a fixed layout
 * manager, like #ClutterGroup and #ClutterStage.
 *
 * Constraints provide a way to build user interfaces by using
 * relations between #ClutterActors, without explicit fixed
 * positioning and sizing, similarly to how fluid layout managers like
 * #ClutterBoxLayout and #ClutterTableLayout lay out their children.
 *
 * Constraints are attached to a #ClutterActor, and are available
 * for inspection using clutter_actor_get_constraints().
 *
 * Clutter provides different implementation of the #ClutterConstraint
 * abstract class, for instance:
 *
 *  - #ClutterAlignConstraint, a constraint that can be used to align
 *  an actor to another one on either the horizontal or the vertical
 *  axis, using a normalized value between 0 and 1.
 *  - #ClutterBindConstraint, a constraint binds the X, Y, width or height
 *  of an actor to the corresponding position or size of a source actor,
 *  with or without an offset.
 *  - #ClutterSnapConstraint, a constraint that "snaps" together the edges
 *  of two #ClutterActors; if an actor uses two constraints on both its
 *  horizontal or vertical edges then it can also expand to fit the empty
 *  space.
 *
 * The [constraints example](https://git.gnome.org/browse/clutter/tree/examples/constraints.c?h=clutter-1.18)
 * uses various types of #ClutterConstraints to lay out three actors on a
 * resizable stage. Only the central actor has an explicit size, and no
 * actor has an explicit position.
 *
 *  - The #ClutterActor with #ClutterActor:name `layerA` is explicitly
 *  sized to 100 pixels by 25 pixels, and it's added to the #ClutterStage
 *  - two #ClutterAlignConstraints are used to anchor `layerA` to the
 *  center of the stage, by using 0.5 as the alignment #ClutterAlignConstraint:factor on
 *  both the X and Y axis
 *  - the #ClutterActor with #ClutterActor:name `layerB` is added to the
 *  #ClutterStage with no explicit size
 *  - the #ClutterActor:x and #ClutterActor:width of `layerB` are bound
 *  to the same properties of `layerA` using two #ClutterBindConstraint
 *  objects, thus keeping `layerB` aligned to `layerA`
 *  - the top edge of `layerB` is snapped together with the bottom edge
 *  of `layerA`; the bottom edge of `layerB` is also snapped together with
 *  the bottom edge of the #ClutterStage; an offset is  given to the two
 *  #ClutterSnapConstraintss to allow for some padding; since `layerB` is
 *  snapped between two different #ClutterActors, its height is stretched
 *  to match the gap
 *  - the #ClutterActor with #ClutterActor:name `layerC` mirrors `layerB`,
 *  snapping the top edge of the #ClutterStage to the top edge of `layerC`
 *  and the top edge of `layerA` to the bottom edge of `layerC`
 *
 * You can try resizing interactively the #ClutterStage and verify
 * that the three #ClutterActors maintain the same position and
 * size relative to each other, and to the #ClutterStage.
 *
 * It is important to note that Clutter does not avoid loops or
 * competing constraints; if two or more #ClutterConstraints
 * are operating on the same positional or dimensional attributes of an
 * actor, or if the constraints on two different actors depend on each
 * other, then the behavior is undefined.
 *
 * ## Implementing a ClutterConstraint
 *
 * Creating a sub-class of #ClutterConstraint requires the
 * implementation of the #ClutterConstraintClass.update_allocation()
 * virtual function.
 *
 * The `update_allocation()` virtual function is called during the
 * allocation sequence of a #ClutterActor, and allows any #ClutterConstraint
 * attached to that actor to modify the allocation before it is passed to
 * the actor's #ClutterActorClass.allocate() implementation.
 *
 * The #ClutterActorBox passed to the `update_allocation()` implementation
 * contains the original allocation of the #ClutterActor, plus the eventual
 * modifications applied by the other #ClutterConstraints, in the same order
 * the constraints have been applied to the actor.
 *
 * It is not necessary for a #ClutterConstraint sub-class to chain
 * up to the parent's implementation.
 *
 * If a #ClutterConstraint is parametrized - i.e. if it contains
 * properties that affect the way the constraint is implemented - it should
 * call clutter_actor_queue_relayout() on the actor to which it is attached
 * to whenever any parameter is changed. The actor to which it is attached
 * can be recovered at any point using clutter_actor_meta_get_actor().
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "clutter-constraint.h"

#include "clutter-actor.h"
#include "clutter-actor-meta-private.h"
#include "clutter-private.h"

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
clutter_constraint_notify (GObject    *gobject,
                           GParamSpec *pspec)
{
  if (strcmp (pspec->name, "enabled") == 0)
    {
      ClutterActorMeta *meta = CLUTTER_ACTOR_META (gobject);
      ClutterActor *actor = clutter_actor_meta_get_actor (meta);

      if (actor != NULL)
        clutter_actor_queue_relayout (actor);
    }

  if (G_OBJECT_CLASS (clutter_constraint_parent_class)->notify != NULL)
    G_OBJECT_CLASS (clutter_constraint_parent_class)->notify (gobject, pspec);
}

static void
clutter_constraint_class_init (ClutterConstraintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->notify = clutter_constraint_notify;

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
