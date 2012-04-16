/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl-util.h>
#include <cogl-vector.h>

#include <glib.h>
#include <math.h>
#include <string.h>

#define X 0
#define Y 1
#define Z 2
#define W 3

void
cogl_vector3_init (float *vector, float x, float y, float z)
{
  vector[X] = x;
  vector[Y] = y;
  vector[Z] = z;
}

void
cogl_vector3_init_zero (float *vector)
{
  memset (vector, 0, sizeof (float) * 3);
}

CoglBool
cogl_vector3_equal (const void *v1, const void *v2)
{
  float *vector0 = (float *)v1;
  float *vector1 = (float *)v2;

  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  /* There's no point picking an arbitrary epsilon that's appropriate
   * for comparing the components so we just use == that will at least
   * consider -0 and 0 to be equal. */
  return
    vector0[X] == vector1[X] &&
    vector0[Y] == vector1[Y] &&
    vector0[Z] == vector1[Z];
}

CoglBool
cogl_vector3_equal_with_epsilon (const float *vector0,
                                 const float *vector1,
                                 float epsilon)
{
  _COGL_RETURN_VAL_IF_FAIL (vector0 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (vector1 != NULL, FALSE);

  if (fabsf (vector0[X] - vector1[X]) < epsilon &&
      fabsf (vector0[Y] - vector1[Y]) < epsilon &&
      fabsf (vector0[Z] - vector1[Z]) < epsilon)
    return TRUE;
  else
    return FALSE;
}

float *
cogl_vector3_copy (const float *vector)
{
  if (vector)
    return g_slice_copy (sizeof (float) * 3, vector);
  return NULL;
}

void
cogl_vector3_free (float *vector)
{
  g_slice_free1 (sizeof (float) * 3, vector);
}

void
cogl_vector3_invert (float *vector)
{
  vector[X] = -vector[X];
  vector[Y] = -vector[Y];
  vector[Z] = -vector[Z];
}

void
cogl_vector3_add (float *result,
                  const float *a,
                  const float *b)
{
  result[X] = a[X] + b[X];
  result[Y] = a[Y] + b[Y];
  result[Z] = a[Z] + b[Z];
}

void
cogl_vector3_subtract (float *result,
                       const float *a,
                       const float *b)
{
  result[X] = a[X] - b[X];
  result[Y] = a[Y] - b[Y];
  result[Z] = a[Z] - b[Z];
}

void
cogl_vector3_multiply_scalar (float *vector,
                              float scalar)
{
  vector[X] *= scalar;
  vector[Y] *= scalar;
  vector[Z] *= scalar;
}

void
cogl_vector3_divide_scalar (float *vector,
                            float scalar)
{
  float one_over_scalar = 1.0f / scalar;
  vector[X] *= one_over_scalar;
  vector[Y] *= one_over_scalar;
  vector[Z] *= one_over_scalar;
}

void
cogl_vector3_normalize (float *vector)
{
  float mag_squared =
    vector[X] * vector[X] +
    vector[Y] * vector[Y] +
    vector[Z] * vector[Z];

  if (mag_squared > 0.0f)
    {
      float one_over_mag = 1.0f / sqrtf (mag_squared);
      vector[X] *= one_over_mag;
      vector[Y] *= one_over_mag;
      vector[Z] *= one_over_mag;
    }
}

float
cogl_vector3_magnitude (const float *vector)
{
  return sqrtf (vector[X] * vector[X] +
                vector[Y] * vector[Y] +
                vector[Z] * vector[Z]);
}

void
cogl_vector3_cross_product (float *result,
                            const float *a,
                            const float *b)
{
  float tmp[3];

  tmp[X] = a[Y] * b[Z] - a[Z] * b[Y];
  tmp[Y] = a[Z] * b[X] - a[X] * b[Z];
  tmp[Z] = a[X] * b[Y] - a[Y] * b[X];
  result[X] = tmp[X];
  result[Y] = tmp[Y];
  result[Z] = tmp[Z];
}

float
cogl_vector3_dot_product (const float *a, const float *b)
{
  return a[X] * b[X] + a[Y] * b[Y] + a[Z] * b[Z];
}

float
cogl_vector3_distance (const float *a, const float *b)
{
  float dx = b[X] - a[X];
  float dy = b[Y] - a[Y];
  float dz = b[Z] - a[Z];

  return sqrtf (dx * dx + dy * dy + dz * dz);
}

#if 0
void
cogl_vector4_init (float *vector, float x, float y, float z)
{
  vector[X] = x;
  vector[Y] = y;
  vector[Z] = z;
  vector[W] = w;
}

void
cogl_vector4_init_zero (float *vector)
{
  memset (vector, 0, sizeof (CoglVector4));
}

void
cogl_vector4_init_from_vector4 (float *vector, float *src)
{
  *vector4 = *src;
}

CoglBool
cogl_vector4_equal (const void *v0, const void *v1)
{
  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  return memcmp (v1, v2, sizeof (float) * 4) == 0 ? TRUE : FALSE;
}

float *
cogl_vector4_copy (float *vector)
{
  if (vector)
    return g_slice_dup (CoglVector4, vector);
  return NULL;
}

void
cogl_vector4_free (float *vector)
{
  g_slice_free (CoglVector4, vector);
}

void
cogl_vector4_invert (float *vector)
{
  vector.x = -vector.x;
  vector.y = -vector.y;
  vector.z = -vector.z;
  vector.w = -vector.w;
}

void
cogl_vector4_add (float *result,
                  float *a,
                  float *b)
{
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  result.z = a.z + b.z;
  result.w = a.w + b.w;
}

void
cogl_vector4_subtract (float *result,
                       float *a,
                       float *b)
{
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  result.z = a.z - b.z;
  result.w = a.w - b.w;
}

void
cogl_vector4_divide (float *vector,
                     float scalar)
{
  float one_over_scalar = 1.0f / scalar;
  result.x *= one_over_scalar;
  result.y *= one_over_scalar;
  result.z *= one_over_scalar;
  result.w *= one_over_scalar;
}

#endif
