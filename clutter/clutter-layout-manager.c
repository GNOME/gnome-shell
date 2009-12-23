/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-layout-manager
 * @short_description: Layout managers base class
 *
 * #ClutterLayoutManager is a base abstract class for layout managers. A
 * layout manager implements the layouting policy for a composite or a
 * container actor: it controls the preferred size of the actor to which
 * it has been paired, and it controls the allocation of its children.
 *
 * Any composite or container #ClutterActor subclass can delegate the
 * layouting of its children to a #ClutterLayoutManager. Clutter provides
 * a generic container using #ClutterLayoutManager called #ClutterBox.
 *
 * Clutter provides some simple #ClutterLayoutManager sub-classes, like
 * #ClutterFlowLayout and #ClutterBinLayout.
 *
 * <refsect2 id="ClutterLayoutManager-use-in-Actor">
 *   <title>Using a Layout Manager inside an Actor</title>
 *   <para>In order to use a #ClutterLayoutManager inside a #ClutterActor
 *   sub-class you should invoke clutter_layout_manager_get_preferred_width()
 *   inside the <structname>ClutterActor</structname>::get_preferred_width()
 *   virtual function and clutter_layout_manager_get_preferred_height()
 *   inside the <structname>ClutterActor</structname>::get_preferred_height()
 *   virtual function implementations. You should also call
 *   clutter_layout_manager_allocate() inside the implementation of the
 *   <structname>ClutterActor</structname>::allocate() virtual
 *   function.</para>
 *   <para>In order to receive notifications for changes in the layout
 *   manager policies you should also connect to the
 *   #ClutterLayoutManager::layout-changed signal and queue a relayout
 *   on your actor. The following code should be enough if the actor
 *   does not need to perform specific operations whenever a layout
 *   manager changes:</para>
 *   <informalexample><programlisting>
 *  g_signal_connect_swapped (layout_manager,
 *                            "layout-changed",
 *                            G_CALLBACK (clutter_actor_queue_relayout),
 *                            actor);
 *   </programlisting></informalexample>
 * </refsect2>
 *
 * <refsect2 id="ClutterLayoutManager-implementation">
 *   <title>Implementing a ClutterLayoutManager</title>
 *   <para>The implementation of a layout manager does not differ from
 *   the implementation of the size requisition and allocation bits of
 *   #ClutterActor, so you should read the relative documentation
 *   <link linkend="clutter-subclassing-ClutterActor">for subclassing
 *   ClutterActor</link>.</para>
 *   <para>The layout manager implementation can hold a back pointer
 *   to the #ClutterContainer by implementing the
 *   <function>set_container()</function> virtual function. The layout manager
 *   should not hold a real reference (i.e. call g_object_ref()) on the
 *   container actor, to avoid reference cycles.</para>
 *   <para>If the layout manager has properties affecting the layout
 *   policies then it should emit the #ClutterLayoutManager::layout-changed
 *   signal on itself by using the clutter_layout_manager_layout_changed()
 *   function whenever one of these properties changes.</para>
 *   <para>If the layout manager has layout properties, that is properties that
 *   should exist only as the result of the presence of a specific (layout
 *   manager, container actor, child actor) combination, and it wishes to store
 *   those properties inside a #ClutterLayoutMeta then it should override the
 *   <structname>ClutterLayoutManager</structname>::get_child_meta_type()
 *   virtual function to return the #GType of the #ClutterLayoutMeta sub-class
 *   used to store the layout properties; optionally, the #ClutterLayoutManager
 *   sub-class might also override the
 *   <structname>ClutterLayoutManager</structname>::create_child_meta() virtual
 *   function to control how the #ClutterLayoutMeta instance is created,
 *   otherwise the default implementation will be equivalent to:</para>
 *   <informalexample><programlisting>
 *  ClutterLayoutManagerClass *klass;
 *  GType meta_type;
 *
 *  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
 *  meta_type = klass->get_child_meta_type (manager);
 *
 *  return g_object_new (meta_type,
 *                       "manager", manager,
 *                       "container", container,
 *                       "actor", actor,
 *                       NULL);
 *   </programlisting></informalexample>
 *   <para>Where <varname>manager</varname> is the  #ClutterLayoutManager,
 *   <varname>container</varname> is the #ClutterContainer using the
 *   #ClutterLayoutManager and <varname>actor</varname> is the #ClutterActor
 *   child of the #ClutterContainer.</para>
 * </refsect2>
 *
 * <refsect2 id="ClutterLayoutManager-animation">
 *   <title>Animating a ClutterLayoutManager</title>
 *   <para>A layout manager is used to let a #ClutterContainer take complete
 *   ownership over the layout (that is: the position and sizing) of its
 *   children; this means that using the Clutter animation API, like
 *   clutter_actor_animate(), to animate the position and sizing of a child of
 *   a layout manager it is not going to work properly, as the animation will
 *   automatically override any setting done by the layout manager
 *   itself.</para>
 *   <para>It is possible for a #ClutterLayoutManager sub-class to animate its
 *   children layout by using the base class animation support. The
 *   #ClutterLayoutManager animation support consists of three virtual
 *   functions: <function>begin_animation()</function>,
 *   <function>get_animation_progress()</function> and
 *   <function>end_animation()</function>.</para>
 *   <variablelist>
 *     <varlistentry>
 *       <term><function>begin_animation (duration, easing)</function></term>
 *       <listitem><para>This virtual function is invoked when the layout
 *       manager should begin an animation. The implementation should set up
 *       the state for the animation and create the ancillary objects for
 *       animating the layout. The default implementation creates a
 *       #ClutterTimeline for the given duration and a #ClutterAlpha binding
 *       the timeline to the given easing mode. This function returns a
 *       #ClutterAlpha which should be used to control the animation from
 *       the caller perspective.</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term><function>get_animation_progress()</function></term>
 *       <listitem><para>This virtual function should be invoked when animating
 *       a layout manager. It returns the progress of the animation, using the
 *       same semantics as the #ClutterAlpha:alpha value.</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term><function>end_animation()</function></term>
 *       <listitem><para>This virtual function is invoked when the animation of
 *       a layout manager ends, and it is meant to be used for bookkeeping the
 *       objects created in the <function>begin_animation()</function>
 *       function. The default implementation will call it implicitly when the
 *       timeline is complete.</para></listitem>
 *     </varlistentry>
 *   </variablelist>
 *   <para>The simplest way to animate a layout is to create a #ClutterTimeline
 *   inside the <function>begin_animation()</function> virtual function, along
 *   with a #ClutterAlpha, and for each #ClutterTimeline::new-frame signal
 *   emission call clutter_layout_manager_layout_changed(), which will cause a
 *   relayout. The #ClutterTimeline::completed signal emission should cause
 *   clutter_layout_manager_end_animation() to be called. The default
 *   implementation provided internally by #ClutterLayoutManager does exactly
 *   this, so most sub-classes should either not override any animation-related
 *   virtual function or simply override <function>begin_animation()</function>
 *   and <function>end_animation()</function> to set up ad hoc state, and then
 *   chain up to the parent's implementation.</para>
 *   <example id="example-ClutterLayoutManager-animation">
 *     <title>Animation of a Layout Manager</title>
 *     <para>The code below shows how a #ClutterLayoutManager sub-class should
 *     provide animating the allocation of its children from within the
 *     <function>allocate()</function> virtual function implementation. The
 *     animation is computed between the last stable allocation performed
 *     before the animation started and the desired final allocation.</para>
 *     <para>The <varname>is_animating</varname> variable is stored inside the
 *     #ClutterLayoutManager sub-class and it is updated by overriding the
 *     <function>begin_animation()</function> and
 *     <function>end_animation()</function> virtual functions and chaining up
 *     to the base class implementation.</para>
 *     <para>The last stable allocation is stored within a #ClutterLayoutMeta
 *     sub-class used by the implementation.</para>
 *     <programlisting>
 * static void
 * my_layout_manager_allocate (ClutterLayoutManager   *manager,
 *                             ClutterContainer       *container,
 *                             const ClutterActorBox  *allocation,
 *                             ClutterAllocationFlags  flags)
 * {
 *   MyLayoutManager *self = MY_LAYOUT_MANAGER (manager);
 *   GList *children, *l;
 *
 *   children = clutter_container_get_children (container);
 *
 *   for (l = children; l != NULL; l = l-&gt;next)
 *     {
 *       ClutterActor *child = l->data;
 *       ClutterLayoutMeta *meta;
 *       MyLayoutMeta *my_meta;
 *
 *       /&ast; retrieve the layout meta-object &ast;/
 *       meta = clutter_layout_manager_get_child_meta (manager,
 *                                                     container,
 *                                                     child);
 *       my_meta = MY_LAYOUT_META (meta);
 *
 *       /&ast; compute the desired allocation for the child &ast;/
 *       compute_allocation (self, my_meta, child,
 *                           allocation, flags,
 *                           &amp;child_box);
 *
 *       /&ast; this is the additional code that deals with the animation
 *        &ast; of the layout manager
 *        &ast;/
 *       if (!self-&gt;is_animating)
 *         {
 *           /&ast; store the last stable allocation for later use &ast;/
 *           my_meta-&gt;last_alloc = clutter_actor_box_copy (&amp;child_box);
 *         }
 *       else
 *         {
 *           ClutterActorBox end = { 0, };
 *           gdouble p;
 *
 *           /&ast; get the progress of the animation &ast;/
 *           p = clutter_layout_manager_get_animation_progress (manager);
 *
 *           if (my_meta-&gt;last_alloc != NULL)
 *             {
 *               /&ast; copy the desired allocation as the final state &ast;/
 *               end = child_box;
 *
 *               /&ast; then interpolate the initial and final state
 *                &ast; depending on the progress of the animation,
 *                &ast; and put the result inside the box we will use
 *                &ast; to allocate the child
 *                &ast;/
 *               clutter_actor_box_interpolate (my_meta-&gt;last_alloc,
 *                                              &amp;end,
 *                                              p,
 *                                              &amp;child_box);
 *             }
 *           else
 *             {
 *               /&ast; if there is no stable allocation then the child was
 *                &ast; added while animating; one possible course of action
 *                &ast; is to just bail out and fall through to the allocation
 *                &ast; to position the child directly at its final state
 *                &ast;/
 *               my_meta-&gt;last_alloc =
 *                 clutter_actor_box_copy (&amp;child_box);
 *             }
 *         }
 *
 *       /&ast; allocate the child &ast;/
 *       clutter_actor_allocate (child, &child_box, flags);
 *     }
 *
 *   g_list_free (children);
 * }
 *     </programlisting>
 *   </example>
 *   <para>Sub-classes of #ClutterLayoutManager that support animations of the
 *   layout changes should call clutter_layout_manager_begin_animation()
 *   whenever a layout property changes value, e.g.:</para>
 *   <informalexample>
 *     <programlisting>
 * if (self->orientation != new_orientation)
 *   {
 *     ClutterLayoutManager *manager;
 *
 *     self->orientation = new_orientation;
 *
 *     manager = CLUTTER_LAYOUT_MANAGER (self);
 *     clutter_layout_manager_layout_changed (manager);
 *     clutter_layout_manager_begin_animation (manager, 500, CLUTTER_LINEAR);
 *
 *     g_object_notify (G_OBJECT (self), "orientation");
 *   }
 *     </programlisting>
 *   </informalexample>
 *   <para>The code above will animate a change in the
 *   <varname>orientation</varname> layout property of a layout manager.</para>
 * </refsect2>
 *
 * #ClutterLayoutManager is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-alpha.h"
#include "clutter-debug.h"
#include "clutter-layout-manager.h"
#include "clutter-layout-meta.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-timeline.h"

#define LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED(m,method)   G_STMT_START {  \
        GObject *_obj = G_OBJECT (m);                                   \
        g_warning ("Layout managers of type %s do not implement "       \
                   "the ClutterLayoutManager::%s method",               \
                   G_OBJECT_TYPE_NAME (_obj),                           \
                   (method));                           } G_STMT_END

enum
{
  LAYOUT_CHANGED,

  LAST_SIGNAL
};

G_DEFINE_ABSTRACT_TYPE (ClutterLayoutManager,
                        clutter_layout_manager,
                        G_TYPE_INITIALLY_UNOWNED);

static GQuark quark_layout_meta  = 0;
static GQuark quark_layout_alpha = 0;

static guint manager_signals[LAST_SIGNAL] = { 0, };

static void
layout_manager_real_get_preferred_width (ClutterLayoutManager *manager,
                                         ClutterContainer     *container,
                                         gfloat                for_height,
                                         gfloat               *min_width_p,
                                         gfloat               *nat_width_p)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "get_preferred_width");

  if (min_width_p)
    *min_width_p = 0.0;

  if (nat_width_p)
    *nat_width_p = 0.0;
}

static void
layout_manager_real_get_preferred_height (ClutterLayoutManager *manager,
                                          ClutterContainer     *container,
                                          gfloat                for_width,
                                          gfloat               *min_height_p,
                                          gfloat               *nat_height_p)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "get_preferred_height");

  if (min_height_p)
    *min_height_p = 0.0;

  if (nat_height_p)
    *nat_height_p = 0.0;
}

static void
layout_manager_real_allocate (ClutterLayoutManager   *manager,
                              ClutterContainer       *container,
                              const ClutterActorBox  *allocation,
                              ClutterAllocationFlags  flags)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "allocate");
}

static ClutterLayoutMeta *
layout_manager_real_create_child_meta (ClutterLayoutManager *manager,
                                       ClutterContainer     *container,
                                       ClutterActor         *actor)
{
  ClutterLayoutManagerClass *klass;
  GType meta_type;

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  meta_type = klass->get_child_meta_type (manager);

  /* provide a default implementation to reduce common code */
  if (meta_type != G_TYPE_INVALID)
    {
      g_assert (g_type_is_a (meta_type, CLUTTER_TYPE_LAYOUT_META));

      return g_object_new (meta_type,
                           "manager", manager,
                           "container", container,
                           "actor", actor,
                           NULL);
    }

  return NULL;
}

static GType
layout_manager_real_get_child_meta_type (ClutterLayoutManager *manager)
{
  return G_TYPE_INVALID;
}

static ClutterAlpha *
layout_manager_real_begin_animation (ClutterLayoutManager *manager,
                                     guint                 duration,
                                     gulong                mode)
{
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;

  alpha = g_object_get_qdata (G_OBJECT (manager), quark_layout_alpha);
  if (alpha != NULL)
    {
      clutter_alpha_set_mode (alpha, mode);

      timeline = clutter_alpha_get_timeline (alpha);
      clutter_timeline_set_duration (timeline, duration);
      clutter_timeline_rewind (timeline);

      return alpha;
    };

  timeline = clutter_timeline_new (duration);

  alpha = clutter_alpha_new_full (timeline, mode);

  /* let the alpha take ownership of the timeline */
  g_object_unref (timeline);

  g_signal_connect_swapped (timeline, "completed",
                            G_CALLBACK (clutter_layout_manager_end_animation),
                            manager);
  g_signal_connect_swapped (timeline, "new-frame",
                            G_CALLBACK (clutter_layout_manager_layout_changed),
                            manager);

  g_object_set_qdata_full (G_OBJECT (manager),
                           quark_layout_alpha, alpha,
                           (GDestroyNotify) g_object_unref);

  clutter_timeline_start (timeline);

  return alpha;
}

static gdouble
layout_manager_real_get_animation_progress (ClutterLayoutManager *manager)
{
  ClutterAlpha *alpha;

  alpha = g_object_get_qdata (G_OBJECT (manager), quark_layout_alpha);
  if (alpha == NULL)
    return 1.0;

  return clutter_alpha_get_alpha (alpha);
}

static void
layout_manager_real_end_animation (ClutterLayoutManager *manager)
{
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;

  alpha = g_object_get_qdata (G_OBJECT (manager), quark_layout_alpha);
  if (alpha == NULL)
    return;

  timeline = clutter_alpha_get_timeline (alpha);
  g_assert (timeline != NULL);

  if (clutter_timeline_is_playing (timeline))
    clutter_timeline_stop (timeline);

  g_signal_handlers_disconnect_by_func (timeline,
                                        G_CALLBACK (clutter_layout_manager_end_animation),
                                        manager);
  g_signal_handlers_disconnect_by_func (timeline,
                                        G_CALLBACK (clutter_layout_manager_layout_changed),
                                        manager);

  g_object_set_qdata (G_OBJECT (manager), quark_layout_alpha, NULL);

  clutter_layout_manager_layout_changed (manager);
}

static void
clutter_layout_manager_class_init (ClutterLayoutManagerClass *klass)
{
  quark_layout_meta =
    g_quark_from_static_string ("clutter-layout-manager-child-meta");
  quark_layout_alpha =
    g_quark_from_static_string ("clutter-layout-manager-alpha");

  klass->get_preferred_width = layout_manager_real_get_preferred_width;
  klass->get_preferred_height = layout_manager_real_get_preferred_height;
  klass->allocate = layout_manager_real_allocate;
  klass->create_child_meta = layout_manager_real_create_child_meta;
  klass->get_child_meta_type = layout_manager_real_get_child_meta_type;
  klass->begin_animation = layout_manager_real_begin_animation;
  klass->get_animation_progress = layout_manager_real_get_animation_progress;
  klass->end_animation = layout_manager_real_end_animation;

  /**
   * ClutterLayoutManager::layout-changed:
   * @manager: the #ClutterLayoutManager that emitted the signal
   *
   * The ::layout-changed signal is emitted each time a layout manager
   * has been changed. Every #ClutterActor using the @manager instance
   * as a layout manager should connect a handler to the ::layout-changed
   * signal and queue a relayout on themselves:
   *
   * |[
   *   static void layout_changed (ClutterLayoutManager *manager,
   *                               ClutterActor         *self)
   *   {
   *     clutter_actor_queue_relayout (self);
   *   }
   *   ...
   *     self->manager = g_object_ref_sink (manager);
   *     g_signal_connect (self->manager, "layout-changed",
   *                       G_CALLBACK (layout_changed),
   *                       self);
   * ]|
   *
   * Sub-classes of #ClutterLayoutManager that implement a layout that
   * can be controlled or changed using parameters should emit the
   * ::layout-changed signal whenever one of the parameters changes,
   * by using clutter_layout_manager_layout_changed().
   *
   * Since: 1.2
   */
  manager_signals[LAYOUT_CHANGED] =
    g_signal_new (I_("layout-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterLayoutManagerClass,
                                   layout_changed),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
clutter_layout_manager_init (ClutterLayoutManager *manager)
{
}

/**
 * clutter_layout_manager_get_preferred_width:
 * @manager: a #ClutterLayoutManager
 * @container: the #ClutterContainer using @manager
 * @for_height: the height for which the width should be computed, or -1
 * @min_width_p: (out) (allow-none): return location for the minimum width
 *   of the layout, or %NULL
 * @nat_width_p: (out) (allow-none): return location for the natural width
 *   of the layout, or %NULL
 *
 * Computes the minimum and natural widths of the @container according
 * to @manager.
 *
 * See also clutter_actor_get_preferred_width()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_get_preferred_width (ClutterLayoutManager *manager,
                                            ClutterContainer     *container,
                                            gfloat                for_height,
                                            gfloat               *min_width_p,
                                            gfloat               *nat_width_p)
{
  ClutterLayoutManagerClass *klass;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->get_preferred_width (manager, container, for_height,
                              min_width_p,
                              nat_width_p);
}

/**
 * clutter_layout_manager_get_preferred_height:
 * @manager: a #ClutterLayoutManager
 * @container: the #ClutterContainer using @manager
 * @for_width: the width for which the height should be computed, or -1
 * @min_height_p: (out) (allow-none): return location for the minimum height
 *   of the layout, or %NULL
 * @nat_height_p: (out) (allow-none): return location for the natural height
 *   of the layout, or %NULL
 *
 * Computes the minimum and natural heights of the @container according
 * to @manager.
 *
 * See also clutter_actor_get_preferred_height()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_get_preferred_height (ClutterLayoutManager *manager,
                                             ClutterContainer     *container,
                                             gfloat                for_width,
                                             gfloat               *min_height_p,
                                             gfloat               *nat_height_p)
{
  ClutterLayoutManagerClass *klass;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->get_preferred_height (manager, container, for_width,
                               min_height_p,
                               nat_height_p);
}

/**
 * clutter_layout_manager_allocate:
 * @manager: a #ClutterLayoutManager
 * @container: the #ClutterContainer using @manager
 * @allocation: the #ClutterActorBox containing the allocated area
 *   of @container
 * @flags: the allocation flags
 *
 * Allocates the children of @container given an area
 *
 * See also clutter_actor_allocate()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_allocate (ClutterLayoutManager   *manager,
                                 ClutterContainer       *container,
                                 const ClutterActorBox  *allocation,
                                 ClutterAllocationFlags  flags)
{
  ClutterLayoutManagerClass *klass;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (allocation != NULL);

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->allocate (manager, container, allocation, flags);
}

/**
 * clutter_layout_manager_layout_changed:
 * @manager: a #ClutterLayoutManager
 *
 * Emits the #ClutterLayoutManager::layout-changed signal on @manager
 *
 * This function should only be called by implementations of the
 * #ClutterLayoutManager class
 *
 * Since: 1.2
 */
void
clutter_layout_manager_layout_changed (ClutterLayoutManager *manager)
{
  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));

  g_signal_emit (manager, manager_signals[LAYOUT_CHANGED], 0);
}

/**
 * clutter_layout_manager_set_container:
 * @manager: a #ClutterLayoutManager
 * @container: (allow-none): a #ClutterContainer using @manager
 *
 * If the #ClutterLayoutManager sub-class allows it, allow
 * adding a weak reference of the @container using @manager
 * from within the layout manager
 *
 * The layout manager should not increase the reference
 * count of the @container
 *
 * Since: 1.2
 */
void
clutter_layout_manager_set_container (ClutterLayoutManager *manager,
                                      ClutterContainer     *container)
{
  ClutterLayoutManagerClass *klass;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (container == NULL || CLUTTER_IS_CONTAINER (container));

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  if (klass->set_container)
    klass->set_container (manager, container);
}

static inline ClutterLayoutMeta *
create_child_meta (ClutterLayoutManager *manager,
                   ClutterContainer     *container,
                   ClutterActor         *actor)
{
  ClutterLayoutManagerClass *klass;

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  if (klass->get_child_meta_type (manager) != G_TYPE_INVALID)
    return klass->create_child_meta (manager, container, actor);

  return NULL;
}

static inline ClutterLayoutMeta *
get_child_meta (ClutterLayoutManager *manager,
                ClutterContainer     *container,
                ClutterActor         *actor)
{
  ClutterLayoutMeta *layout = NULL;

  layout = g_object_get_qdata (G_OBJECT (actor), quark_layout_meta);
  if (layout != NULL)
    {
      ClutterChildMeta *child = CLUTTER_CHILD_META (layout);

      if (layout->manager == manager &&
          child->container == container &&
          child->actor == actor)
        return layout;

      /* if the LayoutMeta referenced is not attached to the
       * layout manager then we simply ask the layout manager
       * to replace it with the right one
       */
    }

  layout = create_child_meta (manager, container, actor);
  if (layout != NULL)
    {
      g_assert (CLUTTER_IS_LAYOUT_META (layout));
      g_object_set_qdata_full (G_OBJECT (actor), quark_layout_meta,
                               layout,
                               (GDestroyNotify) g_object_unref);
      return layout;
    }

  return NULL;
}

/**
 * clutter_layout_manager_get_child_meta:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 *
 * Retrieves the #ClutterLayoutMeta that the layout @manager associated
 * to the @actor child of @container, eventually by creating one if the
 * #ClutterLayoutManager supports layout properties
 *
 * Return value: a #ClutterLayoutMeta, or %NULL if the #ClutterLayoutManager
 *   does not have layout properties
 *
 * Since: 1.0
 */
ClutterLayoutMeta *
clutter_layout_manager_get_child_meta (ClutterLayoutManager *manager,
                                       ClutterContainer     *container,
                                       ClutterActor         *actor)
{
  g_return_val_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager), NULL);
  g_return_val_if_fail (CLUTTER_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  return get_child_meta (manager, container, actor);
}

static inline gboolean
layout_set_property_internal (ClutterLayoutManager *manager,
                              GObject              *gobject,
                              GParamSpec           *pspec,
                              const GValue         *value)
{
  if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_warning ("%s: Child property '%s' of the layout manager of "
                 "type '%s' is constructor-only",
                 G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (manager));
      return FALSE;
    }

  if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("%s: Child property '%s' of the layout manager of "
                 "type '%s' is not writable",
                 G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (manager));
      return FALSE;
    }

  g_object_set_property (gobject, pspec->name, value);

  return TRUE;
}

static inline gboolean
layout_get_property_internal (ClutterLayoutManager *manager,
                              GObject              *gobject,
                              GParamSpec           *pspec,
                              GValue               *value)
{
  if (!(pspec->flags & G_PARAM_READABLE))
    {
      g_warning ("%s: Child property '%s' of the layout manager of "
                 "type '%s' is not readable",
                 G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (manager));
      return FALSE;
    }

  g_object_get_property (gobject, pspec->name, value);

  return TRUE;
}

/**
 * clutter_layout_manager_child_set:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 * @first_property: the first property name
 * @Varargs: a list of property name and value pairs
 *
 * Sets a list of properties and their values on the #ClutterLayoutMeta
 * associated by @manager to a child of @container
 *
 * Languages bindings should use clutter_layout_manager_child_set_property()
 * instead
 *
 * Since: 1.2
 */
void
clutter_layout_manager_child_set (ClutterLayoutManager *manager,
                                  ClutterContainer     *container,
                                  ClutterActor         *actor,
                                  const gchar          *first_property,
                                  ...)
{
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  const gchar *pname;
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (first_property != NULL);

  meta = get_child_meta (manager, container, actor);
  if (meta == NULL)
    {
      g_warning ("Layout managers of type '%s' do not support "
                 "layout metadata",
                 g_type_name (G_OBJECT_TYPE (manager)));
      return;
    }

  klass = G_OBJECT_GET_CLASS (meta);

  va_start (var_args, first_property);

  pname = first_property;
  while (pname)
    {
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error;
      gboolean res;

      pspec = g_object_class_find_property (klass, pname);
      if (pspec == NULL)
        {
          g_warning ("%s: Layout managers of type '%s' have no layout "
                     "property named '%s'",
                     G_STRLOC, G_OBJECT_TYPE_NAME (manager), pname);
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      res = layout_set_property_internal (manager, G_OBJECT (meta),
                                          pspec,
                                          &value);

      g_value_unset (&value);

      if (!res)
        break;

      pname = va_arg (var_args, gchar*);
    }

  va_end (var_args);
}

/**
 * clutter_layout_manager_child_set_property:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 * @property_name: the name of the property to set
 * @value: a #GValue with the value of the property to set
 *
 * Sets a property on the #ClutterLayoutMeta created by @manager and
 * attached to a child of @container
 *
 * Since: 1.2
 */
void
clutter_layout_manager_child_set_property (ClutterLayoutManager *manager,
                                           ClutterContainer     *container,
                                           ClutterActor         *actor,
                                           const gchar          *property_name,
                                           const GValue         *value)
{
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  GParamSpec *pspec;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  meta = get_child_meta (manager, container, actor);
  if (meta == NULL)
    {
      g_warning ("Layout managers of type '%s' do not support "
                 "layout metadata",
                 g_type_name (G_OBJECT_TYPE (manager)));
      return;
    }

  klass = G_OBJECT_GET_CLASS (meta);

  pspec = g_object_class_find_property (klass, property_name);
  if (pspec == NULL)
    {
      g_warning ("%s: Layout managers of type '%s' have no layout "
                 "property named '%s'",
                 G_STRLOC, G_OBJECT_TYPE_NAME (manager), property_name);
      return;
    }

  layout_set_property_internal (manager, G_OBJECT (meta), pspec, value);
}

/**
 * clutter_layout_manager_child_get:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 * @first_property: the name of the first property
 * @Varargs: a list of property name and return location for the value pairs
 *
 * Retrieves the values for a list of properties out of the
 * #ClutterLayoutMeta created by @manager and attached to the
 * child of a @container
 *
 * Since: 1.2
 */
void
clutter_layout_manager_child_get (ClutterLayoutManager *manager,
                                  ClutterContainer     *container,
                                  ClutterActor         *actor,
                                  const gchar          *first_property,
                                  ...)
{
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  const gchar *pname;
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (first_property != NULL);

  meta = get_child_meta (manager, container, actor);
  if (meta == NULL)
    {
      g_warning ("Layout managers of type '%s' do not support "
                 "layout metadata",
                 g_type_name (G_OBJECT_TYPE (manager)));
      return;
    }

  klass = G_OBJECT_GET_CLASS (meta);

  va_start (var_args, first_property);

  pname = first_property;
  while (pname)
    {
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error;
      gboolean res;

      pspec = g_object_class_find_property (klass, pname);
      if (pspec == NULL)
        {
          g_warning ("%s: Layout managers of type '%s' have no layout "
                     "property named '%s'",
                     G_STRLOC, G_OBJECT_TYPE_NAME (manager), pname);
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));

      res = layout_get_property_internal (manager, G_OBJECT (meta),
                                          pspec,
                                          &value);
      if (!res)
        {
          g_value_unset (&value);
          break;
        }

      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          g_value_unset (&value);
          break;
        }

      g_value_unset (&value);

      pname = va_arg (var_args, gchar*);
    }

  va_end (var_args);
}

/**
 * clutter_layout_manager_child_get_property:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 * @property_name: the name of the property to get
 * @value: a #GValue with the value of the property to get
 *
 * Gets a property on the #ClutterLayoutMeta created by @manager and
 * attached to a child of @container
 *
 * The #GValue must already be initialized to the type of the property
 * and has to be unset with g_value_unset() after extracting the real
 * value out of it
 *
 * Since: 1.2
 */
void
clutter_layout_manager_child_get_property (ClutterLayoutManager *manager,
                                           ClutterContainer     *container,
                                           ClutterActor         *actor,
                                           const gchar          *property_name,
                                           GValue               *value)
{
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  GParamSpec *pspec;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  meta = get_child_meta (manager, container, actor);
  if (meta == NULL)
    {
      g_warning ("Layout managers of type %s do not support "
                 "layout metadata",
                 g_type_name (G_OBJECT_TYPE (manager)));
      return;
    }

  klass = G_OBJECT_GET_CLASS (meta);

  pspec = g_object_class_find_property (klass, property_name);
  if (pspec == NULL)
    {
      g_warning ("%s: Layout managers of type '%s' have no layout "
                 "property named '%s'",
                 G_STRLOC, G_OBJECT_TYPE_NAME (manager), property_name);
      return;
    }

  layout_get_property_internal (manager, G_OBJECT (meta), pspec, value);
}

/**
 * clutter_layout_manager_find_child_property:
 * @manager: a #ClutterLayoutManager
 * @name: the name of the property
 *
 * Retrieves the #GParamSpec for the layout property @name inside
 * the #ClutterLayoutMeta sub-class used by @manager
 *
 * Return value: (transfer none): a #GParamSpec describing the property,
 *   or %NULL if no property with that name exists. The returned
 *   #GParamSpec is owned by the layout manager and should not be
 *   modified or freed
 *
 * Since: 1.2
 */
GParamSpec *
clutter_layout_manager_find_child_property (ClutterLayoutManager *manager,
                                            const gchar          *name)
{
  ClutterLayoutManagerClass *klass;
  GObjectClass *meta_klass;
  GParamSpec *pspec;
  GType meta_type;

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  meta_type = klass->get_child_meta_type (manager);
  if (meta_type == G_TYPE_INVALID)
    return NULL;

  meta_klass = g_type_class_ref (meta_type);

  pspec = g_object_class_find_property (meta_klass, name);

  g_type_class_unref (meta_klass);

  return pspec;
}

/**
 * clutter_layout_manager_list_child_properties:
 * @manager: a #ClutterLayoutManager
 * @n_pspecs: (out): return location for the number of returned
 *   #GParamSpec<!-- -->s
 *
 * Retrieves all the #GParamSpec<!-- -->s for the layout properties
 * stored inside the #ClutterLayoutMeta sub-class used by @manager
 *
 * Return value: (transfer full): the newly-allocated, %NULL-terminated
 *   array of #GParamSpec<!-- -->s. Use g_free() to free the resources
 *   allocated for the array
 *
 * Since: 1.2
 */
GParamSpec **
clutter_layout_manager_list_child_properties (ClutterLayoutManager *manager,
                                              guint                *n_pspecs)
{
  ClutterLayoutManagerClass *klass;
  GObjectClass *meta_klass;
  GParamSpec **pspecs;
  GType meta_type;

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  meta_type = klass->get_child_meta_type (manager);
  if (meta_type == G_TYPE_INVALID)
    return NULL;

  meta_klass = g_type_class_ref (meta_type);

  pspecs = g_object_class_list_properties (meta_klass, n_pspecs);

  g_type_class_unref (meta_klass);

  return pspecs;
}

/**
 * clutter_layout_manager_begin_animation:
 * @manager: a #ClutterLayoutManager
 * @duration: the duration of the animation, in milliseconds
 * @mode: the easing mode of the animation
 *
 * Begins an animation of @duration milliseconds, using the provided
 * easing @mode
 *
 * The easing mode can be specified either as a #ClutterAnimationMode
 * or as a logical id returned by clutter_alpha_register_func()
 *
 * The result of this function depends on the @manager implementation
 *
 * Return value: (transfer none): The #ClutterAlpha created by the
 *   layout manager; the returned instance is owned by the layout
 *   manager and should not be unreferenced
 *
 * Since: 1.2
 */
ClutterAlpha *
clutter_layout_manager_begin_animation (ClutterLayoutManager *manager,
                                        guint                 duration,
                                        gulong                mode)
{
  ClutterLayoutManagerClass *klass;

  g_return_val_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager), NULL);

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);

  return klass->begin_animation (manager, duration, mode);
}

/**
 * clutter_layout_manager_end_animation:
 * @manager: a #ClutterLayoutManager
 *
 * Ends an animation started by clutter_layout_manager_begin_animation()
 *
 * The result of this call depends on the @manager implementation
 *
 * Since: 1.2
 */
void
clutter_layout_manager_end_animation (ClutterLayoutManager *manager)
{
  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));

  CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager)->end_animation (manager);
}

/**
 * clutter_layout_manager_get_animation_progress:
 * @manager: a #ClutterLayoutManager
 *
 * Retrieves the progress of the animation, if one has been started by
 * clutter_layout_manager_begin_animation()
 *
 * The returned value has the same semantics of the #ClutterAlpha:alpha
 * value
 *
 * Return value: the progress of the animation
 *
 * Since: 1.2
 */
gdouble
clutter_layout_manager_get_animation_progress (ClutterLayoutManager *manager)
{
  ClutterLayoutManagerClass *klass;

  g_return_val_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager), 1.0);

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);

  return klass->get_animation_progress (manager);
}
