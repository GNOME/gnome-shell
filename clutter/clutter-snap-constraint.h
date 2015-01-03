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

#ifndef __CLUTTER_SNAP_CONSTRAINT_H__
#define __CLUTTER_SNAP_CONSTRAINT_H__

#include <clutter/clutter-constraint.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SNAP_CONSTRAINT    (clutter_snap_constraint_get_type ())
#define CLUTTER_SNAP_CONSTRAINT(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SNAP_CONSTRAINT, ClutterSnapConstraint))
#define CLUTTER_IS_SNAP_CONSTRAINT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SNAP_CONSTRAINT))

/**
 * ClutterSnapConstraint:
 *
 * #ClutterSnapConstraint is an opaque structure
 * whose members cannot be directly accesses
 *
 * Since: 1.6
 */
typedef struct _ClutterSnapConstraint           ClutterSnapConstraint;
typedef struct _ClutterSnapConstraintClass      ClutterSnapConstraintClass;

CLUTTER_AVAILABLE_IN_1_6
GType clutter_snap_constraint_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_6
ClutterConstraint *     clutter_snap_constraint_new             (ClutterActor          *source,
                                                                 ClutterSnapEdge        from_edge,
                                                                 ClutterSnapEdge        to_edge,
                                                                 gfloat                 offset);

CLUTTER_AVAILABLE_IN_1_6
void                    clutter_snap_constraint_set_source      (ClutterSnapConstraint *constraint,
                                                                 ClutterActor          *source);
CLUTTER_AVAILABLE_IN_1_6
ClutterActor *          clutter_snap_constraint_get_source      (ClutterSnapConstraint *constraint);
CLUTTER_AVAILABLE_IN_1_6
void                    clutter_snap_constraint_set_edges       (ClutterSnapConstraint *constraint,
                                                                 ClutterSnapEdge        from_edge,
                                                                 ClutterSnapEdge        to_edge);
CLUTTER_AVAILABLE_IN_1_6
void                    clutter_snap_constraint_get_edges       (ClutterSnapConstraint *constraint,
                                                                 ClutterSnapEdge       *from_edge,
                                                                 ClutterSnapEdge       *to_edge);
CLUTTER_AVAILABLE_IN_1_6
void                    clutter_snap_constraint_set_offset      (ClutterSnapConstraint *constraint,
                                                                 gfloat                 offset);
CLUTTER_AVAILABLE_IN_1_6
gfloat                  clutter_snap_constraint_get_offset      (ClutterSnapConstraint *constraint);

G_END_DECLS

#endif /* __CLUTTER_SNAP_CONSTRAINT_H__ */
