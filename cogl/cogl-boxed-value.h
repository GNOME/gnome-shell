/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 */

#ifndef __COGL_BOXED_VALUE_H
#define __COGL_BOXED_VALUE_H

#include <glib.h>

#include "cogl-context.h"

typedef enum {
  COGL_BOXED_NONE,
  COGL_BOXED_INT,
  COGL_BOXED_FLOAT,
  COGL_BOXED_MATRIX
} CoglBoxedType;

typedef struct _CoglBoxedValue
{
  CoglBoxedType type;
  int size, count;

  union {
    float float_value[4];
    int int_value[4];
    float matrix[16];
    float *float_array;
    int *int_array;
    void *array;
  } v;
} CoglBoxedValue;

#define _cogl_boxed_value_init(bv)              \
  G_STMT_START {                                \
    CoglBoxedValue *_bv = (bv);                 \
    _bv->type = COGL_BOXED_NONE;                \
    _bv->count = 1;                             \
  } G_STMT_END

CoglBool
_cogl_boxed_value_equal (const CoglBoxedValue *bva,
                         const CoglBoxedValue *bvb);

void
_cogl_boxed_value_set_1f (CoglBoxedValue *bv,
                          float value);

void
_cogl_boxed_value_set_1i (CoglBoxedValue *bv,
                          int value);

void
_cogl_boxed_value_set_float (CoglBoxedValue *bv,
                             int n_components,
                             int count,
                             const float *value);

void
_cogl_boxed_value_set_int (CoglBoxedValue *bv,
                           int n_components,
                           int count,
                           const int *value);

void
_cogl_boxed_value_set_matrix (CoglBoxedValue *bv,
                              int dimensions,
                              int count,
                              CoglBool transpose,
                              const float *value);

/*
 * _cogl_boxed_value_copy:
 * @dst: The destination boxed value
 * @src: The source boxed value
 *
 * This copies @src to @dst. It is assumed that @dst is initialised.
 */
void
_cogl_boxed_value_copy (CoglBoxedValue *dst,
                        const CoglBoxedValue *src);

void
_cogl_boxed_value_destroy (CoglBoxedValue *bv);

void
_cogl_boxed_value_set_uniform (CoglContext *ctx,
                               int location,
                               const CoglBoxedValue *value);

#endif /* __COGL_BOXED_VALUE_H */
