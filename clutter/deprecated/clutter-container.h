/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011 Intel Corporation
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
 *
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CONTAINER_DEPRECATED_H__
#define __CLUTTER_CONTAINER_DEPRECATED_H__

#include <clutter/clutter-container.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_FOR(clutter_actor_add_child)
void            clutter_container_add                           (ClutterContainer *container,
                                                                 ClutterActor     *first_actor,
                                                                 ...) G_GNUC_NULL_TERMINATED;

CLUTTER_DEPRECATED_FOR(clutter_actor_add_child)
void            clutter_container_add_actor                     (ClutterContainer *container,
                                                                 ClutterActor     *actor);

CLUTTER_DEPRECATED_FOR(clutter_actor_add_child)
void            clutter_container_add_valist                    (ClutterContainer *container,
                                                                 ClutterActor     *first_actor,
                                                                 va_list           var_args);

CLUTTER_DEPRECATED_FOR(clutter_actor_remove_child)
void            clutter_container_remove                        (ClutterContainer *container,
                                                                 ClutterActor     *first_actor,
                                                                 ...) G_GNUC_NULL_TERMINATED;

CLUTTER_DEPRECATED_FOR(clutter_actor_remove_child)
void            clutter_container_remove_actor                  (ClutterContainer *container,
                                                                 ClutterActor     *actor);

CLUTTER_DEPRECATED_FOR(clutter_actor_remove_child)
void            clutter_container_remove_valist                 (ClutterContainer *container,
                                                                 ClutterActor     *first_actor,
                                                                 va_list           var_args);

CLUTTER_DEPRECATED_FOR(clutter_actor_get_children)
GList *         clutter_container_get_children                  (ClutterContainer *container);

CLUTTER_DEPRECATED
void            clutter_container_foreach                       (ClutterContainer *container,
                                                                 ClutterCallback   callback,
                                                                 gpointer          user_data);

CLUTTER_DEPRECATED
void            clutter_container_foreach_with_internals        (ClutterContainer *container,
                                                                 ClutterCallback   callback,
                                                                 gpointer          user_data);

CLUTTER_DEPRECATED_FOR(clutter_actor_set_child_above_sibling)
void            clutter_container_raise_child                   (ClutterContainer *container,
                                                                 ClutterActor     *actor,
                                                                 ClutterActor     *sibling);

CLUTTER_DEPRECATED_FOR(clutter_actor_set_child_below_sibling)
void            clutter_container_lower_child                   (ClutterContainer *container,
                                                                 ClutterActor     *actor,
                                                                 ClutterActor     *sibling);

CLUTTER_DEPRECATED
void            clutter_container_sort_depth_order              (ClutterContainer *container);

G_END_DECLS

#endif /* __CLUTTER_CONTAINER_DEPRECATED_H__ */
