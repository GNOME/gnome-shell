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

#ifndef __CLUTTER_ALIGN_CONSTRAINT_H__
#define __CLUTTER_ALIGN_CONSTRAINT_H__

#include <clutter/clutter-constraint.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ALIGN_CONSTRAINT           (clutter_align_constraint_get_type ())
#define CLUTTER_ALIGN_CONSTRAINT(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ALIGN_CONSTRAINT, ClutterAlignConstraint))
#define CLUTTER_IS_ALIGN_CONSTRAINT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ALIGN_CONSTRAINT))

/**
 * ClutterAlignConstraint:
 *
 * <structname>ClutterAlignConstraint</structname> is an opaque structure
 * whose members cannot be directly accesses
 *
 * Since: 1.4
 */
typedef struct _ClutterAlignConstraint  ClutterAlignConstraint;

/**
 * ClutterAlignAxis:
 * @CLUTTER_ALIGN_X_AXIS: Maintain the alignment on the X axis
 * @CLUTTER_ALIGN_Y_AXIS: Maintain the alignment on the Y axis
 *
 * Specifies the axis on which #ClutterAlignConstraint should maintain
 * the alignment
 *
 * Since: 1.4
 */
typedef enum { /*< prefix=CLUTTER_ALIGN >*/
  CLUTTER_ALIGN_X_AXIS,
  CLUTTER_ALIGN_Y_AXIS
} ClutterAlignAxis;

GType clutter_align_constraint_get_type (void) G_GNUC_CONST;

ClutterConstraint *clutter_align_constraint_new            (ClutterActor           *source,
                                                            ClutterAlignAxis        axis,
                                                            gfloat                  factor);

void               clutter_align_constraint_set_source     (ClutterAlignConstraint *align,
                                                            ClutterActor           *source);
ClutterActor *     clutter_align_constraint_get_source     (ClutterAlignConstraint *align);
void               clutter_align_constraint_set_align_axis (ClutterAlignConstraint *align,
                                                            ClutterAlignAxis        axis);
ClutterAlignAxis   clutter_align_constraint_get_align_axis (ClutterAlignConstraint *align);
void               clutter_align_constraint_set_factor     (ClutterAlignConstraint *align,
                                                            gfloat                  factor);
gfloat             clutter_align_constraint_get_factor     (ClutterAlignConstraint *align);

G_END_DECLS

#endif /* __CLUTTER_ALIGN_CONSTRAINT_H__ */
