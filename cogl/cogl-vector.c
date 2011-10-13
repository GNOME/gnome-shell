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

#include <cogl.h>
#include <cogl-util.h>
#include <cogl-vector.h>

#include <glib.h>
#include <math.h>
#include <string.h>

void
cogl_vector3_init (CoglVector3 *vector, float x, float y, float z)
{
  vector->x = x;
  vector->y = y;
  vector->z = z;
}

void
cogl_vector3_init_zero (CoglVector3 *vector)
{
  memset (vector, 0, sizeof (CoglVector3));
}

gboolean
cogl_vector3_equal (gconstpointer v1, gconstpointer v2)
{
  CoglVector3 *vector0 = (CoglVector3 *)v1;
  CoglVector3 *vector1 = (CoglVector3 *)v2;

  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  /* There's no point picking an arbitrary epsilon that's appropriate
   * for comparing the components so we just use == that will at least
   * consider -0 and 0 to be equal. */
  return
    vector0->x == vector1->x &&
    vector0->y == vector1->y &&
    vector0->z == vector1->z;
}

gboolean
cogl_vector3_equal_with_epsilon (const CoglVector3 *vector0,
                                 const CoglVector3 *vector1,
                                 float epsilon)
{
  _COGL_RETURN_VAL_IF_FAIL (vector0 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (vector1 != NULL, FALSE);

  if (fabsf (vector0->x - vector1->x) < epsilon &&
      fabsf (vector0->y - vector1->y) < epsilon &&
      fabsf (vector0->z - vector1->z) < epsilon)
    return TRUE;
  else
    return FALSE;
}

CoglVector3 *
cogl_vector3_copy (const CoglVector3 *vector)
{
  if (vector)
    return g_slice_dup (CoglVector3, vector);
  return NULL;
}

void
cogl_vector3_free (CoglVector3 *vector)
{
  g_slice_free (CoglVector3, vector);
}

void
cogl_vector3_invert (CoglVector3 *vector)
{
  vector->x = -vector->x;
  vector->y = -vector->y;
  vector->z = -vector->z;
}

void
cogl_vector3_add (CoglVector3 *result,
                  const CoglVector3 *a,
                  const CoglVector3 *b)
{
  result->x = a->x + b->x;
  result->y = a->y + b->y;
  result->z = a->z + b->z;
}

void
cogl_vector3_subtract (CoglVector3 *result,
                       const CoglVector3 *a,
                       const CoglVector3 *b)
{
  result->x = a->x - b->x;
  result->y = a->y - b->y;
  result->z = a->z - b->z;
}

void
cogl_vector3_multiply_scalar (CoglVector3 *vector,
                              float scalar)
{
  vector->x *= scalar;
  vector->y *= scalar;
  vector->z *= scalar;
}

void
cogl_vector3_divide_scalar (CoglVector3 *vector,
                            float scalar)
{
  float one_over_scalar = 1.0f / scalar;
  vector->x *= one_over_scalar;
  vector->y *= one_over_scalar;
  vector->z *= one_over_scalar;
}

void
cogl_vector3_normalize (CoglVector3 *vector)
{
  float mag_squared =
    vector->x * vector->x +
    vector->y * vector->y +
    vector->z * vector->z;

  if (mag_squared > 0.0f)
    {
      float one_over_mag = 1.0f / sqrtf (mag_squared);
      vector->x *= one_over_mag;
      vector->y *= one_over_mag;
      vector->z *= one_over_mag;
    }
}

float
cogl_vector3_magnitude (const CoglVector3 *vector)
{
  return sqrtf (vector->x * vector->x +
                vector->y * vector->y +
                vector->z * vector->z);
}

void
cogl_vector3_cross_product (CoglVector3 *result,
                            const CoglVector3 *a,
                            const CoglVector3 *b)
{
  CoglVector3 tmp;

  tmp.x = a->y * b->z - a->z * b->y;
  tmp.y = a->z * b->x - a->x * b->z;
  tmp.z = a->x * b->y - a->y * b->x;
  *result = tmp;
}

float
cogl_vector3_dot_product (const CoglVector3 *a, const CoglVector3 *b)
{
  return a->x * b->x + a->y * b->y + a->z * b->z;
}

float
cogl_vector3_distance (const CoglVector3 *a, const CoglVector3 *b)
{
  float dx = b->x - a->x;
  float dy = b->y - a->y;
  float dz = b->z - a->z;

  return sqrtf (dx * dx + dy * dy + dz * dz);
}

#if 0
void
cogl_vector4_init (CoglVector4 *vector, float x, float y, float z)
{
  vector->x = x;
  vector->y = y;
  vector->z = z;
  vector->w = w;
}

void
cogl_vector4_init_zero (CoglVector4 *vector)
{
  memset (vector, 0, sizeof (CoglVector4));
}

void
cogl_vector4_init_from_vector4 (CoglVector4 *vector, CoglVector4 *src)
{
  *vector4 = *src;
}

gboolean
cogl_vector4_equal (gconstpointer *v0, gconstpointer *v1)
{
  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  return memcmp (v1, v2, sizeof (float) * 4) == 0 ? TRUE : FALSE;
}

CoglVector4 *
cogl_vector4_copy (CoglVector4 *vector)
{
  if (vector)
    return g_slice_dup (CoglVector4, vector);
  return NULL;
}

void
cogl_vector4_free (CoglVector4 *vector)
{
  g_slice_free (CoglVector4, vector);
}

void
cogl_vector4_invert (CoglVector4 *vector)
{
  vector.x = -vector.x;
  vector.y = -vector.y;
  vector.z = -vector.z;
  vector.w = -vector.w;
}

void
cogl_vector4_add (CoglVector4 *result,
                  CoglVector4 *a,
                  CoglVector4 *b)
{
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  result.z = a.z + b.z;
  result.w = a.w + b.w;
}

void
cogl_vector4_subtract (CoglVector4 *result,
                       CoglVector4 *a,
                       CoglVector4 *b)
{
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  result.z = a.z - b.z;
  result.w = a.w - b.w;
}

void
cogl_vector4_divide (CoglVector4 *vector,
                     float scalar)
{
  float one_over_scalar = 1.0f / scalar;
  result.x *= one_over_scalar;
  result.y *= one_over_scalar;
  result.z *= one_over_scalar;
  result.w *= one_over_scalar;
}

#endif
