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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * ClutterContainer: Generic actor container interface.
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 */

#ifndef __CLUTTER_CONTAINER_H__
#define __CLUTTER_CONTAINER_H__

#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CONTAINER                  (clutter_container_get_type ())
#define CLUTTER_CONTAINER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CONTAINER, ClutterContainer))
#define CLUTTER_IS_CONTAINER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CONTAINER))
#define CLUTTER_CONTAINER_GET_IFACE(obj)        (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_CONTAINER, ClutterContainerIface))

typedef struct _ClutterContainer        ClutterContainer; /* dummy */
typedef struct _ClutterContainerIface   ClutterContainerIface;

/**
 * ClutterContainerIface:
 * @add: virtual function for adding an actor to the container
 * @remove: virtual function for removing an actor from the container
 * @foreach: virtual function for iterating over the container's children
 * @find_child_by_id: virtual function for searching a container children
 *   using its unique id. Should recurse through its children. This function
 *   is used when "picking" actors (e.g. by clutter_stage_get_actor_at_pos())
 * @raise: virtual function for raising a child
 * @lower: virtual function for lowering a child
 * @sort_depth_order: virtual function for sorting the children of a
 *   container depending on their depth
 * @actor_added: signal class handler for ClutterContainer::actor_added
 * @actor_removed: signal class handler for ClutterContainer::actor_removed
 * 
 * Base interface for container actors.
 *
 * Since: 0.4
 */
struct _ClutterContainerIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  void          (* add)              (ClutterContainer *container,
                                      ClutterActor     *actor);
  void          (* remove)           (ClutterContainer *container,
                                      ClutterActor     *actor);
  void          (* foreach)          (ClutterContainer *container,
                                      ClutterCallback   callback,
                                      gpointer          user_data);
  ClutterActor *(* find_child_by_id) (ClutterContainer *container,
                                      guint             child_id);
  void          (* raise)            (ClutterContainer *container,
                                      ClutterActor     *actor,
                                      ClutterActor     *sibling);
  void          (* lower)            (ClutterContainer *container,
                                      ClutterActor     *actor,
                                      ClutterActor     *sibling);
  void          (* sort_depth_order) (ClutterContainer *container);

  /* signals */
  void (* actor_added)   (ClutterContainer *container,
                          ClutterActor     *actor);
  void (* actor_removed) (ClutterContainer *container,
                          ClutterActor     *actor);
};

GType         clutter_container_get_type         (void) G_GNUC_CONST;

void          clutter_container_add              (ClutterContainer *container,
                                                  ClutterActor     *first_actor,
                                                  ...) G_GNUC_NULL_TERMINATED;
void          clutter_container_add_actor        (ClutterContainer *container,
                                                  ClutterActor     *actor);
void          clutter_container_add_valist       (ClutterContainer *container,
                                                  ClutterActor     *first_actor,
                                                  va_list           var_args);
void          clutter_container_remove           (ClutterContainer *container,
                                                  ClutterActor     *first_actor,
                                                  ...) G_GNUC_NULL_TERMINATED;
void          clutter_container_remove_actor     (ClutterContainer *container,
                                                  ClutterActor     *actor);
void          clutter_container_remove_valist    (ClutterContainer *container,
                                                  ClutterActor     *first_actor,
                                                  va_list           var_args);
GList *       clutter_container_get_children     (ClutterContainer *container);
void          clutter_container_foreach          (ClutterContainer *container,
                                                  ClutterCallback   callback,
                                                  gpointer          user_data);
ClutterActor *clutter_container_find_child_by_id (ClutterContainer *container,
                                                  guint             child_id);
void          clutter_container_raise            (ClutterContainer *container,
                                                  ClutterActor     *actor,
                                                  ClutterActor     *sibling);
void          clutter_container_lower            (ClutterContainer *container,
                                                  ClutterActor     *actor,
                                                  ClutterActor     *sibling);
void          clutter_container_sort_depth_order (ClutterContainer *container);

G_END_DECLS

#endif /* __CLUTTER_CONTAINER_H__ */
