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
 * <refsect2 id="ClutterConstraint-usage">
 *   <title>Using Constraints</title>
 *   <para>Constraints can be used with fixed layout managers, like
 *   #ClutterFixedLayout, or with actors implicitly using a fixed layout
 *   manager, like #ClutterGroup and #ClutterStage.</para>
 *   <para>Constraints provide a way to build user interfaces by using
 *   relations between #ClutterActor<!-- -->s, without explicit fixed
 *   positioning and sizing, similarly to how fluid layout managers like
 *   #ClutterBoxLayout and #ClutterTableLayout lay out their children.</para>
 *   <para>Constraints are attached to a #ClutterActor, and are available
 *   for inspection using clutter_actor_get_constraints().</para>
 *   <para>Clutter provides different implementation of the #ClutterConstraint
 *   abstract class, for instance:</para>
 *   <variablelist>
 *     <varlistentry>
 *       <term>#ClutterAlignConstraint</term>
 *       <listitem><simpara>this constraint can be used to align an actor
 *       to another one, on either the horizontal or the vertical axis; the
 *       #ClutterAlignConstraint uses a normalized offset between 0.0 (the
 *       top or the left of the source actor, depending on the axis) and
 *       1.0 (the bottom or the right of the source actor, depending on the
 *       axis).</simpara></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>#ClutterBindConstraint</term>
 *       <listitem><simpara>this constraint binds the X, Y, width or height
 *       of an actor to the corresponding position or size of a source
 *       actor; it can also apply an offset.</simpara></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>#ClutterSnapConstraint</term>
 *       <listitem><simpara>this constraint "snaps" together the edges of
 *       two #ClutterActor<!-- -->s; if an actor uses two constraints on
 *       both its horizontal or vertical edges then it can also expand to
 *       fit the empty space.</simpara></listitem>
 *     </varlistentry>
 *   </variablelist>
 *   <example id="ClutterConstraint-usage-example">
 *     <title>Usage of constraints</title>
 *     <para>The example below uses various #ClutterConstraint<!-- -->s to
 *     lay out three actors on a resizable stage. Only the central actor has
 *     an explicit size, and no actor has an explicit position.</para>
 *     <orderedlist>
 *       <listitem><simpara>The #ClutterRectangle with #ClutterActor:name
 *       <emphasis>layerA</emphasis> is explicitly sized to 100 pixels by 25
 *       pixels, and it's added to the #ClutterStage;</simpara></listitem>
 *       <listitem><simpara>two #ClutterAlignConstraint<!-- -->s are used
 *       to anchor <emphasis>layerA</emphasis> to the center of the stage,
 *       by using 0.5 as the alignment #ClutterAlignConstraint:factor on
 *       both the X and Y axis.</simpara></listitem>
 *       <listitem><simpara>the #ClutterRectangle with #ClutterActor:name
 *       <emphasis>layerB</emphasis> is added to the #ClutterStage with
 *       no explicit size;</simpara></listitem>
 *       <listitem><simpara>the #ClutterActor:x and #ClutterActor:width
 *       of <emphasis>layerB</emphasis> are bound to the same properties
 *       of <emphasis>layerA</emphasis> using two #ClutterBindConstraint
 *       objects, thus keeping <emphasis>layerB</emphasis> aligned to
 *       <emphasis>layerA</emphasis>;</simpara></listitem>
 *       <listitem><simpara>the top edge of <emphasis>layerB</emphasis> is
 *       snapped together with the bottom edge of <emphasis>layerA</emphasis>;
 *       the bottom edge of <emphasis>layerB</emphasis> is also snapped
 *       together with the bottom edge of the #ClutterStage; an offset is
 *       given to the two #ClutterSnapConstraint<!-- -->s to allow for some
 *       padding; since <emphasis>layerB</emphasis> is snapped between two
 *       different #ClutterActor<!-- -->s, its height is stretched to match
 *       the gap;</simpara></listitem>
 *       <listitem><simpara>the #ClutterRectangle with #ClutterActor:name
 *       <emphasis>layerC</emphasis> mirrors <emphasis>layerB</emphasis>,
 *       snapping the top edge of the #ClutterStage to the top edge of
 *       <emphasis>layerC</emphasis> and the top edge of
 *       <emphasis>layerA</emphasis> to the bottom edge of
 *       <emphasis>layerC</emphasis>;</simpara></listitem>
 *     </orderedlist>
 *     <figure id="constraints-example">
 *       <title>Constraints</title>
 *       <graphic fileref="constraints-example.png" format="PNG"/>
 *     </figure>
 *     <programlisting>
 *<xi:include xmlns:xi="http://www.w3.org/2001/XInclude" href="../../../../examples/constraints.c" parse="text">
 *  <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 *</xi:include>
 *     </programlisting>
 *     <para>You can try resizing interactively the #ClutterStage and verify
 *     that the three #ClutterActor<!-- -->s maintain the same position and
 *     size relative to each other, and to the #ClutterStage.</para>
 *   </example>
 *   <warning><para>It's important to note that Clutter does not avoid loops
 *   or competing constraints; if two or more #ClutterConstraint<!-- -->s
 *   are operating on the same positional or dimensional attributes of an
 *   actor, or if the constraints on two different actors depend on each
 *   other, then the behavior is undefined.</para></warning>
 * </refsect2>
 *
 * <refsect2 id="ClutterConstraint-implementation">
 *   <title>Implementing a ClutterConstraint</title>
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
