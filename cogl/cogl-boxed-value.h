/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
