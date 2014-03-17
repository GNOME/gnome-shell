/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * ClutterContainer: Generic actor container interface.
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CONTAINER_H__
#define __CLUTTER_CONTAINER_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-child-meta.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CONTAINER                  (clutter_container_get_type ())
#define CLUTTER_CONTAINER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CONTAINER, ClutterContainer))
#define CLUTTER_IS_CONTAINER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CONTAINER))
#define CLUTTER_CONTAINER_GET_IFACE(obj)        (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_CONTAINER, ClutterContainerIface))

typedef struct _ClutterContainerIface   ClutterContainerIface;

/**
 * ClutterContainer:
 *
 * #ClutterContainer is an opaque structure whose members cannot be directly
 * accessed
 *
 * Since: 0.4
 */

/**
 * ClutterContainerIface:
 * @add: virtual function for adding an actor to the container. This virtual
 *   function is deprecated, and it should not be overridden.
 * @remove: virtual function for removing an actor from the container. This
 *   virtual function is deprecated, and it should not be overridden.
 * @foreach: virtual function for iterating over the container's children.
 *   This virtual function is deprecated, and it should not be overridden.
 * @foreach_with_internals: virtual functions for iterating over the
 *   container's children, both added using the #ClutterContainer API
 *   and internal children. The implementation of this virtual function
 *   is required only if the #ClutterContainer implementation has
 *   internal children. This virtual function is deprecated, and it should
 *   not be overridden.
 * @raise: virtual function for raising a child. This virtual function is
 *   deprecated and it should not be overridden.
 * @lower: virtual function for lowering a child. This virtual function is
 *   deprecated and it should not be overridden.
 * @sort_depth_order: virtual function for sorting the children of a
 *   container depending on their depth. This virtual function is deprecated
 *   and it should not be overridden.
 * @child_meta_type: The GType used for storing auxiliary information about
 *   each of the containers children.
 * @create_child_meta: virtual function that gets called for each added
 *   child, the function should instantiate an object of type
 *   #ClutterContainerIface::child_meta_type, set the container and actor
 *   fields in the instance and add the record to a data structure for
 *   subsequent access for #ClutterContainerIface::get_child_meta
 * @destroy_child_meta: virtual function that gets called when a child is
 *   removed; it shuld release all resources held by the record
 * @get_child_meta: return the record for a container child
 * @actor_added: class handler for #ClutterContainer::actor-added
 * @actor_removed: class handler for #ClutterContainer::actor-removed
 * @child_notify: class handler for #ClutterContainer::child-notify
 *
 * Base interface for container actors. The @add, @remove and @foreach
 * virtual functions must be provided by any implementation; the other
 * virtual functions are optional.
 *
 * Since: 0.4
 */
struct _ClutterContainerIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  void (* add)              (ClutterContainer *container,
                             ClutterActor     *actor);
  void (* remove)           (ClutterContainer *container,
                             ClutterActor     *actor);
  void (* foreach)          (ClutterContainer *container,
                             ClutterCallback   callback,
                             gpointer          user_data);

  void (* foreach_with_internals) (ClutterContainer *container,
                                   ClutterCallback   callback,
                                   gpointer          user_data);

  /* child stacking */
  void (* raise)            (ClutterContainer *container,
                             ClutterActor     *actor,
                             ClutterActor     *sibling);
  void (* lower)            (ClutterContainer *container,
                             ClutterActor     *actor,
                             ClutterActor     *sibling);
  void (* sort_depth_order) (ClutterContainer *container);

  /* ClutterChildMeta management */
  GType                child_meta_type;
  void              (* create_child_meta)  (ClutterContainer *container,
                                            ClutterActor     *actor);
  void              (* destroy_child_meta) (ClutterContainer *container,
                                            ClutterActor     *actor);
  ClutterChildMeta *(* get_child_meta)     (ClutterContainer *container,
                                            ClutterActor     *actor);

  /* signals */
  void (* actor_added)   (ClutterContainer *container,
                          ClutterActor     *actor);
  void (* actor_removed) (ClutterContainer *container,
                          ClutterActor     *actor);

  void (* child_notify)  (ClutterContainer *container,
                          ClutterActor     *child,
                          GParamSpec       *pspec);
};

CLUTTER_AVAILABLE_IN_ALL
GType clutter_container_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_ALL
ClutterActor *          clutter_container_find_child_by_name            (ClutterContainer *container,
                                                                         const gchar      *child_name);

CLUTTER_AVAILABLE_IN_ALL
GParamSpec *            clutter_container_class_find_child_property     (GObjectClass     *klass,
                                                                         const gchar      *property_name);
CLUTTER_AVAILABLE_IN_ALL
GParamSpec **           clutter_container_class_list_child_properties   (GObjectClass     *klass,
                                                                         guint            *n_properties);

CLUTTER_AVAILABLE_IN_ALL
void                    clutter_container_create_child_meta             (ClutterContainer *container,
                                                                         ClutterActor     *actor);
CLUTTER_AVAILABLE_IN_ALL
void                    clutter_container_destroy_child_meta            (ClutterContainer *container,
                                                                         ClutterActor     *actor);
CLUTTER_AVAILABLE_IN_ALL
ClutterChildMeta *      clutter_container_get_child_meta                (ClutterContainer *container,
                                                                         ClutterActor     *actor);

CLUTTER_AVAILABLE_IN_ALL
void                    clutter_container_child_set_property            (ClutterContainer *container,
                                                                         ClutterActor     *child,
                                                                         const gchar      * property,
                                                                         const GValue     *value);
CLUTTER_AVAILABLE_IN_ALL
void                    clutter_container_child_get_property            (ClutterContainer *container,
                                                                         ClutterActor     *child,
                                                                         const gchar      *property,
                                                                         GValue           *value);
CLUTTER_AVAILABLE_IN_ALL
void                    clutter_container_child_set                     (ClutterContainer *container,
                                                                         ClutterActor     *actor,
                                                                         const gchar      *first_prop,
                                                                         ...) G_GNUC_NULL_TERMINATED;
CLUTTER_AVAILABLE_IN_ALL
void                    clutter_container_child_get                     (ClutterContainer *container,
                                                                         ClutterActor     *actor,
                                                                         const gchar      *first_prop,
                                                                         ...) G_GNUC_NULL_TERMINATED;

CLUTTER_AVAILABLE_IN_ALL
void                    clutter_container_child_notify                  (ClutterContainer *container,
                                                                         ClutterActor     *child,
                                                                         GParamSpec       *pspec);

G_END_DECLS

#endif /* __CLUTTER_CONTAINER_H__ */
