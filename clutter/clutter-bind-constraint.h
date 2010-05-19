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

typedef struct _ClutterBindConstraint   ClutterBindConstraint;

typedef enum { /*< prefix=CLUTTER_BIND >*/
  CLUTTER_BIND_X,
  CLUTTER_BIND_Y,
  CLUTTER_BIND_Z
} ClutterBindCoordinate;

GType clutter_bind_constraint_get_type (void) G_GNUC_CONST;

ClutterConstraint *clutter_bind_constraint_new (ClutterActor          *source,
                                                ClutterBindCoordinate  coordinate,
                                                gfloat                 offset);

G_END_DECLS

#endif /* __CLUTTER_BIND_CONSTRAINT_H__ */
