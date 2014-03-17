/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BIND_CONSTRAINT_H__
#define __CLUTTER_BIND_CONSTRAINT_H__

#include <clutter/clutter-constraint.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BIND_CONSTRAINT    (clutter_bind_constraint_get_type ())
#define CLUTTER_BIND_CONSTRAINT(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraint))
#define CLUTTER_IS_BIND_CONSTRAINT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BIND_CONSTRAINT))

/**
 * ClutterBindConstraint:
 *
 * <structname>ClutterBindConstraint</structname> is an opaque structure
 * whose members cannot be directly accessed
 *
 * Since: 1.4
 */
typedef struct _ClutterBindConstraint           ClutterBindConstraint;
typedef struct _ClutterBindConstraintClass      ClutterBindConstraintClass;

CLUTTER_AVAILABLE_IN_1_4
GType clutter_bind_constraint_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_4
ClutterConstraint *   clutter_bind_constraint_new            (ClutterActor          *source,
                                                              ClutterBindCoordinate  coordinate,
                                                              gfloat                 offset);

CLUTTER_AVAILABLE_IN_1_4
void                  clutter_bind_constraint_set_source     (ClutterBindConstraint *constraint,
                                                              ClutterActor          *source);
CLUTTER_AVAILABLE_IN_1_4
ClutterActor *        clutter_bind_constraint_get_source     (ClutterBindConstraint *constraint);
CLUTTER_AVAILABLE_IN_1_4
void                  clutter_bind_constraint_set_coordinate (ClutterBindConstraint *constraint,
                                                              ClutterBindCoordinate  coordinate);
CLUTTER_AVAILABLE_IN_1_4
ClutterBindCoordinate clutter_bind_constraint_get_coordinate (ClutterBindConstraint *constraint);
CLUTTER_AVAILABLE_IN_1_4
void                  clutter_bind_constraint_set_offset     (ClutterBindConstraint *constraint,
                                                              gfloat                 offset);
CLUTTER_AVAILABLE_IN_1_4
gfloat                clutter_bind_constraint_get_offset     (ClutterBindConstraint *constraint);

G_END_DECLS

#endif /* __CLUTTER_BIND_CONSTRAINT_H__ */
