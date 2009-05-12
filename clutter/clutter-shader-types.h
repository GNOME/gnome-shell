/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008  Intel Corporation.
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

#ifndef __CLUTTER_SHADER_TYPES_H__
#define __CLUTTER_SHADER_TYPES_H__

#include <glib-object.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SHADER_FLOAT   (clutter_shader_float_get_type ())
#define CLUTTER_TYPE_SHADER_INT     (clutter_shader_int_get_type ())
#define CLUTTER_TYPE_SHADER_MATRIX  (clutter_shader_matrix_get_type ())

typedef struct _ClutterShaderFloat    ClutterShaderFloat;
typedef struct _ClutterShaderInt      ClutterShaderInt;
typedef struct _ClutterShaderMatrix   ClutterShaderMatrix;

/**
 * CLUTTER_VALUE_HOLDS_SHADER_FLOAT:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds a #ClutterShaderFloat.
 *
 * Since: 1.0
 */
#define CLUTTER_VALUE_HOLDS_SHADER_FLOAT(x) (G_VALUE_HOLDS ((x), CLUTTER_TYPE_SHADER_FLOAT))

/**
 * CLUTTER_VALUE_HOLDS_SHADER_INT:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds a #ClutterShaderInt.
 *
 * Since: 1.0
 */
#define CLUTTER_VALUE_HOLDS_SHADER_INT(x) (G_VALUE_HOLDS ((x), CLUTTER_TYPE_SHADER_INT))

/**
 * CLUTTER_VALUE_HOLDS_SHADER_MATRIX:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds a #ClutterShaderMatrix.
 *
 * Since: 1.0
 */
#define CLUTTER_VALUE_HOLDS_SHADER_MATRIX(x) (G_VALUE_HOLDS ((x), CLUTTER_TYPE_SHADER_MATRIX))

GType clutter_shader_float_get_type  (void) G_GNUC_CONST;
GType clutter_shader_int_get_type    (void) G_GNUC_CONST;
GType clutter_shader_matrix_get_type (void) G_GNUC_CONST;

void                    clutter_value_set_shader_float  (GValue       *value,
                                                         gint          size,
                                                         const gfloat *floats);
void                    clutter_value_set_shader_int    (GValue       *value,
                                                         gint          size,
                                                         const gint   *ints);
void                    clutter_value_set_shader_matrix (GValue       *value,
                                                         gint          size,
                                                         const gfloat *matrix);
G_CONST_RETURN gfloat * clutter_value_get_shader_float  (const GValue *value,
                                                         gsize        *length);
G_CONST_RETURN gint   * clutter_value_get_shader_int    (const GValue *value,
                                                         gsize        *length);
G_CONST_RETURN gfloat * clutter_value_get_shader_matrix (const GValue *value,
                                                         gsize        *length);

G_END_DECLS

#endif /* __CLUTTER_SHADER_TYPES_H__ */
