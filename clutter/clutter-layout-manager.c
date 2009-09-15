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
 * #ClutterFixedLayout and #ClutterBinLayout.
 *
 * #ClutterLayoutManager is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-debug.h"
#include "clutter-layout-manager.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

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

static GQuark quark_layout_meta = 0;
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

static ClutterChildMeta *
layout_manager_real_create_child_meta (ClutterLayoutManager *manager,
                                       ClutterContainer     *container,
                                       ClutterActor         *actor)
{
  return NULL;
}

static void
clutter_layout_manager_class_init (ClutterLayoutManagerClass *klass)
{
  quark_layout_meta =
    g_quark_from_static_string ("clutter-layout-manager-child-meta");

  klass->get_preferred_width = layout_manager_real_get_preferred_width;
  klass->get_preferred_height = layout_manager_real_get_preferred_height;
  klass->allocate = layout_manager_real_allocate;
  klass->create_child_meta = layout_manager_real_create_child_meta;

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

static inline ClutterChildMeta *
create_child_meta (ClutterLayoutManager *manager,
                   ClutterContainer     *container,
                   ClutterActor         *actor)
{
  ClutterLayoutManagerClass *klass;

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);

  return klass->create_child_meta (manager, container, actor);
}

static inline ClutterChildMeta *
get_child_meta (ClutterLayoutManager *manager,
                ClutterContainer     *container,
                ClutterActor         *actor)
{
  ClutterChildMeta *meta;

  meta = g_object_get_qdata (G_OBJECT (actor), quark_layout_meta);
  if (meta != NULL &&
      meta->container == container &&
      meta->actor == actor)
    return meta;

  return NULL;
}

/**
 * clutter_layout_manager_get_child_meta:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 *
 * Retrieves the #ClutterChildMeta that the layout @manager associated
 * to the @actor child of @container
 *
 * Return value: a #ClutterChildMeta or %NULL
 *
 * Since: 1.0
 */
ClutterChildMeta *
clutter_layout_manager_get_child_meta (ClutterLayoutManager *manager,
                                       ClutterContainer     *container,
                                       ClutterActor         *actor)
{
  g_return_val_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager), NULL);
  g_return_val_if_fail (CLUTTER_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  return get_child_meta (manager, container, actor);
}

/**
 * clutter_layout_manager_add_child_meta:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 *
 * Creates and binds a #ClutterChildMeta for @manager to
 * a child of @container
 *
 * This function should only be used when implementing containers
 * using #ClutterLayoutManager and not by application code
 *
 * Typically, containers should bind a #ClutterChildMeta created
 * by a #ClutterLayoutManager when adding a new child, e.g.:
 *
 * |[
 *   static void
 *   my_container_add (ClutterContainer *container,
 *                     ClutterActor     *actor)
 *   {
 *     MyContainer *self = MY_CONTAINER (container);
 *
 *     self->children = g_slist_append (self->children, actor);
 *     clutter_actor_set_parent (actor, CLUTTER_ACTOR (self));
 *
 *     clutter_layout_manager_add_child_meta (self->layout,
 *                                            container,
 *                                            actor);
 *
 *     clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
 *
 *     g_signal_emit_by_name (container, "actor-added");
 *   }
 * ]|
 *
 * The #ClutterChildMeta should be removed when removing an
 * actor; see clutter_layout_manager_remove_child_meta()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_add_child_meta (ClutterLayoutManager *manager,
                                       ClutterContainer     *container,
                                       ClutterActor         *actor)
{
  ClutterChildMeta *meta;

  meta = create_child_meta (manager, container, actor);
  if (meta == NULL)
    return;

  g_object_set_qdata_full (G_OBJECT (actor), quark_layout_meta,
                           meta,
                           (GDestroyNotify) g_object_unref);
}

/**
 * clutter_layout_manager_remove_child_meta:
 * @manager: a #ClutterLayoutManager
 * @container: a #ClutterContainer using @manager
 * @actor: a #ClutterActor child of @container
 *
 * Unbinds and unrefs a #ClutterChildMeta for @manager from
 * a child of @container
 *
 * This function should only be used when implementing containers
 * using #ClutterLayoutManager and not by application code
 *
 * Typically, containers should remove a #ClutterChildMeta created
 * by a #ClutterLayoutManager when removing a child, e.g.:
 *
 * |[
 *   static void
 *   my_container_remove (ClutterContainer *container,
 *                        ClutterActor     *actor)
 *   {
 *     MyContainer *self = MY_CONTAINER (container);
 *
 *     g_object_ref (actor);
 *
 *     self->children = g_slist_remove (self->children, actor);
 *     clutter_actor_unparent (actor);
 *
 *     clutter_layout_manager_remove_child_meta (self->layout,
 *                                               container,
 *                                               actor);
 *
 *     clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
 *
 *     g_signal_emit_by_name (container, "actor-removed");
 *
 *     g_object_unref (actor);
 *   }
 * ]|
 *
 * See also clutter_layout_manager_add_child_meta()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_remove_child_meta (ClutterLayoutManager *manager,
                                          ClutterContainer     *container,
                                          ClutterActor         *actor)
{
  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  if (get_child_meta (manager, container, actor))
    g_object_set_qdata (G_OBJECT (actor), quark_layout_meta, NULL);
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
 * Sets a list of properties and their values on the #ClutterChildMeta
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
  ClutterChildMeta *meta;
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
                 "child metadata",
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
          g_warning ("%s: Layout managers of type '%s' have no child "
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
 * Sets a property on the #ClutterChildMeta created by @manager and
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
  ClutterChildMeta *meta;
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
                 "child metadata",
                 g_type_name (G_OBJECT_TYPE (manager)));
      return;
    }

  klass = G_OBJECT_GET_CLASS (meta);

  pspec = g_object_class_find_property (klass, property_name);
  if (pspec == NULL)
    {
      g_warning ("%s: Layout managers of type '%s' have no child "
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
 * #ClutterChildMeta created by @manager and attached to the
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
  ClutterChildMeta *meta;
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
                 "child metadata",
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
          g_warning ("%s: Layout managers of type '%s' have no child "
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
 * Gets a property on the #ClutterChildMeta created by @manager and
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
  ClutterChildMeta *meta;
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
                 "child metadata",
                 g_type_name (G_OBJECT_TYPE (manager)));
      return;
    }

  klass = G_OBJECT_GET_CLASS (meta);

  pspec = g_object_class_find_property (klass, property_name);
  if (pspec == NULL)
    {
      g_warning ("%s: Layout managers of type '%s' have no child "
                 "property named '%s'",
                 G_STRLOC, G_OBJECT_TYPE_NAME (manager), property_name);
      return;
    }

  layout_get_property_internal (manager, G_OBJECT (meta), pspec, value);
}
