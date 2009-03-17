/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Chris Lord     <chris@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-shader-types.h"
#include "clutter-private.h"

static GTypeInfo shader_float_info = {
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
};

static GTypeFundamentalInfo shader_float_finfo = { 0, };

static GTypeInfo shader_int_info = {
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
};

static GTypeFundamentalInfo shader_int_finfo = { 0, };

static GTypeInfo shader_matrix_info = {
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
};

static GTypeFundamentalInfo shader_matrix_finfo = { 0, };

struct _ClutterShaderFloat
{
  gint    size;
  GLfloat value[4];
};

struct _ClutterShaderInt
{
  gint    size;
  COGLint value[4];
};

struct _ClutterShaderMatrix
{
  gint    size;
  GLfloat value[16];
};

static gpointer
clutter_value_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

/* Float */

static void
clutter_value_init_shader_float (GValue *value)
{
  value->data[0].v_pointer = g_slice_new0 (ClutterShaderFloat);
}

static void
clutter_value_free_shader_float (GValue *value)
{
  g_slice_free (ClutterShaderFloat, value->data[0].v_pointer);
}

static void
clutter_value_copy_shader_float (const GValue *src,
                                 GValue       *dest)
{
  dest->data[0].v_pointer =
    g_slice_dup (ClutterShaderFloat, src->data[0].v_pointer);
}

static gchar *
clutter_value_collect_shader_float (GValue      *value,
                                    guint        n_collect_values,
                                    GTypeCValue *collect_values,
                                    guint        collect_flags)
{
  gint float_count = collect_values[0].v_int;
  const float *floats = collect_values[1].v_pointer;

  if (!floats)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  clutter_value_init_shader_float (value);
  clutter_value_set_shader_float (value, float_count, floats);

  return NULL;
}

static gchar *
clutter_value_lcopy_shader_float (const GValue *value,
                                  guint         n_collect_values,
                                  GTypeCValue  *collect_values,
                                  guint         collect_flags)
{
  gint *float_count = collect_values[0].v_pointer;
  float **floats = collect_values[1].v_pointer;
  ClutterShaderFloat *shader_float = value->data[0].v_pointer;

  if (!float_count || !floats)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *float_count = shader_float->size;
  *floats = g_memdup (shader_float->value, shader_float->size * sizeof (float));

  return NULL;
}

static const GTypeValueTable _clutter_shader_float_value_table = {
  clutter_value_init_shader_float,
  clutter_value_free_shader_float,
  clutter_value_copy_shader_float,
  clutter_value_peek_pointer,
  "ip",
  clutter_value_collect_shader_float,
  "pp",
  clutter_value_lcopy_shader_float
};

GType
clutter_shader_float_get_type (void)
{
  static GType _clutter_shader_float_type = 0;

  if (G_UNLIKELY (_clutter_shader_float_type == 0))
    {
      shader_float_info.value_table = & _clutter_shader_float_value_table;
      _clutter_shader_float_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     I_("ClutterShaderFloat"),
                                     &shader_float_info,
                                     &shader_float_finfo, 0);
    }

  return _clutter_shader_float_type;
}


/* Integer */

static void
clutter_value_init_shader_int (GValue *value)
{
  value->data[0].v_pointer = g_slice_new0 (ClutterShaderInt);
}

static void
clutter_value_free_shader_int (GValue *value)
{
  g_slice_free (ClutterShaderInt, value->data[0].v_pointer);
}

static void
clutter_value_copy_shader_int (const GValue *src,
                               GValue       *dest)
{
  dest->data[0].v_pointer =
    g_slice_dup (ClutterShaderInt, src->data[0].v_pointer);
}

static gchar *
clutter_value_collect_shader_int (GValue      *value,
                                  guint        n_collect_values,
                                  GTypeCValue *collect_values,
                                  guint        collect_flags)
{
  gint int_count = collect_values[0].v_int;
  const COGLint *ints = collect_values[1].v_pointer;

  if (!ints)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  clutter_value_init_shader_int (value);
  clutter_value_set_shader_int (value, int_count, ints);

  return NULL;
}

static gchar *
clutter_value_lcopy_shader_int (const GValue *value,
                                guint         n_collect_values,
                                GTypeCValue  *collect_values,
                                guint         collect_flags)
{
  gint *int_count = collect_values[0].v_pointer;
  COGLint **ints = collect_values[1].v_pointer;
  ClutterShaderInt *shader_int = value->data[0].v_pointer;

  if (!int_count || !ints)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *int_count = shader_int->size;
  *ints = g_memdup (shader_int->value, shader_int->size * sizeof (COGLint));

  return NULL;
}

static const GTypeValueTable _clutter_shader_int_value_table = {
  clutter_value_init_shader_int,
  clutter_value_free_shader_int,
  clutter_value_copy_shader_int,
  clutter_value_peek_pointer,
  "ip",
  clutter_value_collect_shader_int,
  "pp",
  clutter_value_lcopy_shader_int
};

GType
clutter_shader_int_get_type (void)
{
  static GType _clutter_shader_int_type = 0;

  if (G_UNLIKELY (_clutter_shader_int_type == 0))
    {
      shader_int_info.value_table = & _clutter_shader_int_value_table;
      _clutter_shader_int_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     I_("ClutterShaderInt"),
                                     &shader_int_info,
                                     &shader_int_finfo, 0);
    }

  return _clutter_shader_int_type;
}


/* Matrix */

static void
clutter_value_init_shader_matrix (GValue *value)
{
  value->data[0].v_pointer = g_slice_new0 (ClutterShaderMatrix);
}

static void
clutter_value_free_shader_matrix (GValue *value)
{
  g_slice_free (ClutterShaderMatrix, value->data[0].v_pointer);
}

static void
clutter_value_copy_shader_matrix (const GValue *src,
                                  GValue       *dest)
{
  dest->data[0].v_pointer =
    g_slice_dup (ClutterShaderMatrix, src->data[0].v_pointer);
}

static gchar *
clutter_value_collect_shader_matrix (GValue      *value,
                                     guint        n_collect_values,
                                     GTypeCValue *collect_values,
                                     guint        collect_flags)
{
  gint float_count = collect_values[0].v_int;
  const float *floats = collect_values[1].v_pointer;

  if (!floats)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  clutter_value_init_shader_matrix (value);
  clutter_value_set_shader_matrix (value, float_count, floats);

  return NULL;
}

static gchar *
clutter_value_lcopy_shader_matrix (const GValue *value,
                                   guint         n_collect_values,
                                   GTypeCValue  *collect_values,
                                   guint         collect_flags)
{
  gint *float_count = collect_values[0].v_pointer;
  float **floats = collect_values[1].v_pointer;
  ClutterShaderFloat *shader_float = value->data[0].v_pointer;

  if (!float_count || !floats)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *float_count = shader_float->size;
  *floats = g_memdup (shader_float->value,
                      shader_float->size * shader_float->size * sizeof (float));

  return NULL;
}

static const GTypeValueTable _clutter_shader_matrix_value_table = {
  clutter_value_init_shader_matrix,
  clutter_value_free_shader_matrix,
  clutter_value_copy_shader_matrix,
  clutter_value_peek_pointer,
  "ip",
  clutter_value_collect_shader_matrix,
  "pp",
  clutter_value_lcopy_shader_matrix
};

GType
clutter_shader_matrix_get_type (void)
{
  static GType _clutter_shader_matrix_type = 0;

  if (G_UNLIKELY (_clutter_shader_matrix_type == 0))
    {
      shader_matrix_info.value_table = & _clutter_shader_matrix_value_table;
      _clutter_shader_matrix_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     I_("ClutterShaderMatrix"),
                                     &shader_matrix_info,
                                     &shader_matrix_finfo, 0);
    }

  return _clutter_shader_matrix_type;
}


/* Utility functions */

/**
 * clutter_value_set_shader_float:
 * @value: a #GValue
 * @size: number of floating point values in @floats
 * @floats: an array of floating point values
 *
 * Sets @floats as the contents of @value. The passed #GValue
 * must have been initialized using %CLUTTER_TYPE_SHADER_FLOAT.
 *
 * Since: 0.8
 */
void
clutter_value_set_shader_float (GValue         *value,
                                gint            size,
                                const gfloat   *floats)
{
  ClutterShaderFloat *shader_float;
  gint i;

  g_return_if_fail (CLUTTER_VALUE_HOLDS_SHADER_FLOAT (value));

  shader_float = value->data[0].v_pointer;

  shader_float->size = size;

  for (i = 0; i < size; i++)
    shader_float->value[i] = floats[i];
}

/**
 * clutter_value_set_shader_int:
 * @value: a #GValue
 * @size: number of integer values in @ints
 * @ints: an array of integer values
 *
 * Sets @ints as the contents of @value. The passed #GValue
 * must have been initialized using %CLUTTER_TYPE_SHADER_INT.
 *
 * Since: 0.8
 */
void
clutter_value_set_shader_int (GValue         *value,
                              gint            size,
                              const gint     *ints)
{
  ClutterShaderInt *shader_int;
  gint i;

  g_return_if_fail (CLUTTER_VALUE_HOLDS_SHADER_INT (value));

  shader_int = value->data[0].v_pointer;

  shader_int->size = size;

  for (i = 0; i < size; i++)
    shader_int->value[i] = ints[i];
}

/**
 * clutter_value_set_shader_matrix:
 * @value: a #GValue
 * @size: number of floating point values in @floats
 * @matrix: a matrix of floating point values
 *
 * Sets @matrix as the contents of @value. The passed #GValue
 * must have been initialized using %CLUTTER_TYPE_SHADER_MATRIX.
 *
 * Since: 0.8
 */
void
clutter_value_set_shader_matrix (GValue       *value,
                                 gint          size,
                                 const gfloat *matrix)
{
  ClutterShaderMatrix *shader_matrix;
  gint i;

  g_return_if_fail (CLUTTER_VALUE_HOLDS_SHADER_MATRIX (value));

  shader_matrix = value->data[0].v_pointer;

  shader_matrix->size = size;

  for (i = 0; i < size * size; i++)
    shader_matrix->value[i] = matrix[i];
}

/**
 * clutter_value_get_shader_float:
 * @value: a #GValue
 * @length: return location for the number of returned floating
 *   point values, or %NULL
 *
 * Retrieves the list of floating point values stored inside
 * the passed #GValue. @value must have been initialized with
 * %CLUTTER_TYPE_SHADER_FLOAT.
 *
 * Return value: the pointer to a list of floating point values.
 *   The returned value is owned by the #GValue and should never
 *   be modified or freed.
 *
 * Since: 0.8
 */
G_CONST_RETURN gfloat *
clutter_value_get_shader_float (const GValue *value,
                                gsize        *length)
{
  ClutterShaderFloat *shader_float;

  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_SHADER_FLOAT (value), 0);

  shader_float = value->data[0].v_pointer;

  if (length)
    *length = shader_float->size;

  return shader_float->value;
}

/**
 * clutter_value_get_shader_int:
 * @value: a #GValue
 * @length: return location for the number of returned integer
 *   values, or %NULL
 *
 * Retrieves the list of integer values stored inside the passed
 * #GValue. @value must have been initialized with
 * %CLUTTER_TYPE_SHADER_INT.
 *
 * Return value: the pointer to a list of integer values.
 *   The returned value is owned by the #GValue and should never
 *   be modified or freed.
 *
 * Since: 0.8
 */
G_CONST_RETURN COGLint *
clutter_value_get_shader_int (const GValue *value,
                              gsize        *length)
{
  ClutterShaderInt *shader_int;

  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_SHADER_INT (value), 0);

  shader_int = value->data[0].v_pointer;

  if (length)
    *length = shader_int->size;

  return shader_int->value;
}

/**
 * clutter_value_get_shader_matrix:
 * @value: a #GValue
 * @length: (out): return location for the number of returned floating
 *   point values, or %NULL
 *
 * Retrieves a matrix of floating point values stored inside
 * the passed #GValue. @value must have been initialized with
 * %CLUTTER_TYPE_SHADER_MATRIX.
 *
 * Return value: (array length=length) (transfer none): the pointer to a matrix
 *   of floating point values. The returned value is owned by the #GValue and
 *   should never be modified or freed.
 *
 * Since: 0.8
 */
G_CONST_RETURN gfloat *
clutter_value_get_shader_matrix (const GValue *value,
                                 gsize        *length)
{
  ClutterShaderMatrix *shader_matrix;

  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_SHADER_MATRIX (value), 0);

  shader_matrix = value->data[0].v_pointer;

  if (length)
    *length = shader_matrix->size;

  return shader_matrix->value;
}
