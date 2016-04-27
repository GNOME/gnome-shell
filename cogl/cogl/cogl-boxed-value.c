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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-boxed-value.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"

CoglBool
_cogl_boxed_value_equal (const CoglBoxedValue *bva,
                         const CoglBoxedValue *bvb)
{
  const void *pa, *pb;

  if (bva->type != bvb->type)
    return FALSE;

  switch (bva->type)
    {
    case COGL_BOXED_NONE:
      return TRUE;

    case COGL_BOXED_INT:
      if (bva->size != bvb->size || bva->count != bvb->count)
        return FALSE;

      if (bva->count == 1)
        {
          pa = bva->v.int_value;
          pb = bvb->v.int_value;
        }
      else
        {
          pa = bva->v.int_array;
          pb = bvb->v.int_array;
        }

      return !memcmp (pa, pb, sizeof (int) * bva->size * bva->count);

    case COGL_BOXED_FLOAT:
      if (bva->size != bvb->size || bva->count != bvb->count)
        return FALSE;

      if (bva->count == 1)
        {
          pa = bva->v.float_value;
          pb = bvb->v.float_value;
        }
      else
        {
          pa = bva->v.float_array;
          pb = bvb->v.float_array;
        }

      return !memcmp (pa, pb, sizeof (float) * bva->size * bva->count);

    case COGL_BOXED_MATRIX:
      if (bva->size != bvb->size ||
          bva->count != bvb->count)
        return FALSE;

      if (bva->count == 1)
        {
          pa = bva->v.matrix;
          pb = bvb->v.matrix;
        }
      else
        {
          pa = bva->v.array;
          pb = bvb->v.array;
        }

      return !memcmp (pa, pb,
                      sizeof (float) * bva->size * bva->size * bva->count);
    }

  g_warn_if_reached ();

  return FALSE;
}

static void
_cogl_boxed_value_tranpose (float *dst,
                            int size,
                            const float *src)
{
  int y, x;

  /* If the value is transposed we'll just transpose it now as it
   * is copied into the boxed value instead of passing TRUE to
   * glUniformMatrix because that is not supported on GLES and it
   * doesn't seem like the GL driver would be able to do anything
   * much smarter than this anyway */

  for (y = 0; y < size; y++)
    for (x = 0; x < size; x++)
      *(dst++) = src[y + x * size];
}

static void
_cogl_boxed_value_set_x (CoglBoxedValue *bv,
                         int size,
                         int count,
                         CoglBoxedType type,
                         size_t value_size,
                         const void *value,
                         CoglBool transpose)
{
  if (count == 1)
    {
      if (bv->count > 1)
        g_free (bv->v.array);

      if (transpose)
        _cogl_boxed_value_tranpose (bv->v.float_value,
                                    size,
                                    value);
      else
        memcpy (bv->v.float_value, value, value_size);
    }
  else
    {
      if (bv->count > 1)
        {
          if (bv->count != count ||
              bv->size != size ||
              bv->type != type)
            {
              g_free (bv->v.array);
              bv->v.array = g_malloc (count * value_size);
            }
        }
      else
        bv->v.array = g_malloc (count * value_size);

      if (transpose)
        {
          int value_num;

          for (value_num = 0; value_num < count; value_num++)
            _cogl_boxed_value_tranpose (bv->v.float_array +
                                        value_num * size * size,
                                        size,
                                        (const float *) value +
                                        value_num * size * size);
        }
      else
        memcpy (bv->v.array, value, count * value_size);
    }

  bv->type = type;
  bv->size = size;
  bv->count = count;
}

void
_cogl_boxed_value_set_1f (CoglBoxedValue *bv,
                          float value)
{
  _cogl_boxed_value_set_x (bv,
                           1, 1, COGL_BOXED_FLOAT,
                           sizeof (float), &value, FALSE);
}

void
_cogl_boxed_value_set_1i (CoglBoxedValue *bv,
                          int value)
{
  _cogl_boxed_value_set_x (bv,
                           1, 1, COGL_BOXED_INT,
                           sizeof (int), &value, FALSE);
}

void
_cogl_boxed_value_set_float (CoglBoxedValue *bv,
                             int n_components,
                             int count,
                             const float *value)
{
  _cogl_boxed_value_set_x (bv,
                           n_components, count,
                           COGL_BOXED_FLOAT,
                           sizeof (float) * n_components, value, FALSE);
}

void
_cogl_boxed_value_set_int (CoglBoxedValue *bv,
                           int n_components,
                           int count,
                           const int *value)
{
  _cogl_boxed_value_set_x (bv,
                           n_components, count,
                           COGL_BOXED_INT,
                           sizeof (int) * n_components, value, FALSE);
}

void
_cogl_boxed_value_set_matrix (CoglBoxedValue *bv,
                              int dimensions,
                              int count,
                              CoglBool transpose,
                              const float *value)
{
  _cogl_boxed_value_set_x (bv,
                           dimensions, count,
                           COGL_BOXED_MATRIX,
                           sizeof (float) * dimensions * dimensions,
                           value,
                           transpose);
}

void
_cogl_boxed_value_copy (CoglBoxedValue *dst,
                        const CoglBoxedValue *src)
{
  *dst = *src;

  if (src->count > 1)
    {
      switch (src->type)
        {
        case COGL_BOXED_NONE:
          break;

        case COGL_BOXED_INT:
          dst->v.int_array = g_memdup (src->v.int_array,
                                       src->size * src->count * sizeof (int));
          break;

        case COGL_BOXED_FLOAT:
          dst->v.float_array = g_memdup (src->v.float_array,
                                         src->size *
                                         src->count *
                                         sizeof (float));
          break;

        case COGL_BOXED_MATRIX:
          dst->v.float_array = g_memdup (src->v.float_array,
                                         src->size * src->size *
                                         src->count * sizeof (float));
          break;
        }
    }
}

void
_cogl_boxed_value_destroy (CoglBoxedValue *bv)
{
  if (bv->count > 1)
    g_free (bv->v.array);
}

void
_cogl_boxed_value_set_uniform (CoglContext *ctx,
                               GLint location,
                               const CoglBoxedValue *value)
{
  switch (value->type)
    {
    case COGL_BOXED_NONE:
      break;

    case COGL_BOXED_INT:
      {
        const int *ptr;

        if (value->count == 1)
          ptr = value->v.int_value;
        else
          ptr = value->v.int_array;

        switch (value->size)
          {
          case 1:
            GE( ctx, glUniform1iv (location, value->count, ptr) );
            break;
          case 2:
            GE( ctx, glUniform2iv (location, value->count, ptr) );
            break;
          case 3:
            GE( ctx, glUniform3iv (location, value->count, ptr) );
            break;
          case 4:
            GE( ctx, glUniform4iv (location, value->count, ptr) );
            break;
          }
      }
      break;

    case COGL_BOXED_FLOAT:
      {
        const float *ptr;

        if (value->count == 1)
          ptr = value->v.float_value;
        else
          ptr = value->v.float_array;

        switch (value->size)
          {
          case 1:
            GE( ctx, glUniform1fv (location, value->count, ptr) );
            break;
          case 2:
            GE( ctx, glUniform2fv (location, value->count, ptr) );
            break;
          case 3:
            GE( ctx, glUniform3fv (location, value->count, ptr) );
            break;
          case 4:
            GE( ctx, glUniform4fv (location, value->count, ptr) );
            break;
          }
      }
      break;

    case COGL_BOXED_MATRIX:
      {
        const float *ptr;

        if (value->count == 1)
          ptr = value->v.matrix;
        else
          ptr = value->v.float_array;

        switch (value->size)
          {
          case 2:
            GE( ctx, glUniformMatrix2fv (location, value->count,
                                         FALSE, ptr) );
            break;
          case 3:
            GE( ctx, glUniformMatrix3fv (location, value->count,
                                         FALSE, ptr) );
            break;
          case 4:
            GE( ctx, glUniformMatrix4fv (location, value->count,
                                         FALSE, ptr) );
            break;
          }
      }
      break;
    }
}
