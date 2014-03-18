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

#ifndef __CLUTTER_PATH_CONSTRAINT_H__
#define __CLUTTER_PATH_CONSTRAINT_H__

#include <clutter/clutter-constraint.h>
#include <clutter/clutter-path.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PATH_CONSTRAINT    (clutter_path_constraint_get_type ())
#define CLUTTER_PATH_CONSTRAINT(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_PATH_CONSTRAINT, ClutterPathConstraint))
#define CLUTTER_IS_PATH_CONSTRAINT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_PATH_CONSTRAINT))

/**
 * ClutterPathConstraint:
 *
 * #ClutterPathConstraint is an opaque structure
 * whose members cannot be directly accessed
 *
 * Since: 1.6
 */
typedef struct _ClutterPathConstraint           ClutterPathConstraint;
typedef struct _ClutterPathConstraintClass      ClutterPathConstraintClass;

CLUTTER_AVAILABLE_IN_1_6
GType clutter_path_constraint_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_6
ClutterConstraint *clutter_path_constraint_new        (ClutterPath           *path,
                                                       gfloat                 offset);

CLUTTER_AVAILABLE_IN_1_6
void               clutter_path_constraint_set_path   (ClutterPathConstraint *constraint,
                                                       ClutterPath           *path);
CLUTTER_AVAILABLE_IN_1_6
ClutterPath *      clutter_path_constraint_get_path   (ClutterPathConstraint *constraint);
CLUTTER_AVAILABLE_IN_1_6
void               clutter_path_constraint_set_offset (ClutterPathConstraint *constraint,
                                                       gfloat                 offset);
CLUTTER_AVAILABLE_IN_1_6
gfloat             clutter_path_constraint_get_offset (ClutterPathConstraint *constraint);

G_END_DECLS

#endif /* __CLUTTER_PATH_CONSTRAINT_H__ */
