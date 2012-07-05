/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd
 * Copyright (C) 2009, 2010 Intel Corp
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
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_ACTOR_DEPRECATED_H__
#define __CLUTTER_ACTOR_DEPRECATED_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_IN_1_10
void            clutter_actor_set_geometry      (ClutterActor          *self,
                                                 const ClutterGeometry *geometry);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_get_allocation_geometry)
void            clutter_actor_get_geometry      (ClutterActor          *self,
                                                 ClutterGeometry       *geometry);
CLUTTER_DEPRECATED_IN_1_8
guint32         clutter_actor_get_gid           (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_8
ClutterActor *  clutter_get_actor_by_gid        (guint32                id_);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_remove_child() and clutter_actor_add_child())
void            clutter_actor_reparent          (ClutterActor          *self,
                                                 ClutterActor          *new_parent);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_add_child)
void            clutter_actor_set_parent        (ClutterActor          *self,
                                                 ClutterActor          *parent);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_remove_child)
void            clutter_actor_unparent          (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_set_child_above_sibling)
void            clutter_actor_raise             (ClutterActor          *self,
                                                 ClutterActor          *below);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_set_child_below_sibling)
void            clutter_actor_lower             (ClutterActor          *self,
                                                 ClutterActor          *above);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_set_child_above_sibling() with NULL sibling)
void            clutter_actor_raise_top         (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_set_child_below_sibling() with NULL sibling)
void            clutter_actor_lower_bottom      (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_10
void            clutter_actor_push_internal     (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_10
void            clutter_actor_pop_internal      (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_10
void            clutter_actor_show_all          (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_10
void            clutter_actor_hide_all          (ClutterActor          *self);

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_actor_set_z_position)
void            clutter_actor_set_depth         (ClutterActor          *self,
                                                 gfloat                 depth);

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_actor_get_z_position)
gfloat          clutter_actor_get_depth         (ClutterActor          *self);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_DEPRECATED_H__ */
