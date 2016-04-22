/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *
 * Various references relating to quaternions:
 *
 * http://www.cs.caltech.edu/courses/cs171/quatut.pdf
 * http://mathworld.wolfram.com/Quaternion.html
 * http://www.gamedev.net/reference/articles/article1095.asp
 * http://www.cprogramming.com/tutorial/3d/quaternions.html
 * http://www.isner.com/tutorials/quatSpells/quaternion_spells_12.htm
 * http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q56
 * 3D Maths Primer for Graphics and Game Development ISBN-10: 1556229119
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl-util.h>
#include <cogl-quaternion.h>
#include <cogl-quaternion-private.h>
#include <cogl-matrix.h>
#include <cogl-vector.h>
#include <cogl-euler.h>
#include "cogl-gtype-private.h"

#include <string.h>
#include <math.h>

#define FLOAT_EPSILON 1e-03

COGL_GTYPE_DEFINE_BOXED (Quaternion, quaternion,
                         cogl_quaternion_copy,
                         cogl_quaternion_free);

static CoglQuaternion zero_quaternion =
{
  0.0,  0.0, 0.0, 0.0,
};

static CoglQuaternion identity_quaternion =
{
  1.0,  0.0, 0.0, 0.0,
};

/* This function is just here to be called from GDB so we don't really
   want to put a declaration in a header and we just add it here to
   avoid a warning */
void
_cogl_quaternion_print (CoglQuaternion *quarternion);

void
_cogl_quaternion_print (CoglQuaternion *quaternion)
{
  g_print ("[ %6.4f (%6.4f, %6.4f, %6.4f)]\n",
           quaternion->w,
           quaternion->x,
           quaternion->y,
           quaternion->z);
}

void
cogl_quaternion_init (CoglQuaternion *quaternion,
                      float angle,
                      float x,
                      float y,
                      float z)
{
  float axis[3] = { x, y, z};
  cogl_quaternion_init_from_angle_vector (quaternion, angle, axis);
}

void
cogl_quaternion_init_from_angle_vector (CoglQuaternion *quaternion,
                                        float angle,
                                        const float *axis3f_in)
{
  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */
  float axis[3];
  float half_angle;
  float sin_half_angle;

  /* XXX: Should we make cogl_vector3_normalize have separate in and
   * out args? */
  axis[0] = axis3f_in[0];
  axis[1] = axis3f_in[1];
  axis[2] = axis3f_in[2];
  cogl_vector3_normalize (axis);

  half_angle = angle * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f;
  sin_half_angle = sinf (half_angle);

  quaternion->w = cosf (half_angle);

  quaternion->x = axis[0] * sin_half_angle;
  quaternion->y = axis[1] * sin_half_angle;
  quaternion->z = axis[2] * sin_half_angle;

  cogl_quaternion_normalize (quaternion);
}

void
cogl_quaternion_init_identity (CoglQuaternion *quaternion)
{
  quaternion->w = 1.0;

  quaternion->x = 0.0;
  quaternion->y = 0.0;
  quaternion->z = 0.0;
}

void
cogl_quaternion_init_from_array (CoglQuaternion *quaternion,
                                 const float *array)
{
  quaternion->w = array[0];
  quaternion->x = array[1];
  quaternion->y = array[2];
  quaternion->z = array[3];
}

void
cogl_quaternion_init_from_x_rotation (CoglQuaternion *quaternion,
                                      float angle)
{
  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */
  float half_angle = angle * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f;

  quaternion->w = cosf (half_angle);

  quaternion->x = sinf (half_angle);
  quaternion->y = 0.0f;
  quaternion->z = 0.0f;
}

void
cogl_quaternion_init_from_y_rotation (CoglQuaternion *quaternion,
                                      float angle)
{
  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */
  float half_angle = angle * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f;

  quaternion->w = cosf (half_angle);

  quaternion->x = 0.0f;
  quaternion->y = sinf (half_angle);
  quaternion->z = 0.0f;
}

void
cogl_quaternion_init_from_z_rotation (CoglQuaternion *quaternion,
                                      float angle)
{
  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */
  float half_angle = angle * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f;

  quaternion->w = cosf (half_angle);

  quaternion->x = 0.0f;
  quaternion->y = 0.0f;
  quaternion->z = sinf (half_angle);
}

void
cogl_quaternion_init_from_euler (CoglQuaternion *quaternion,
                                 const CoglEuler *euler)
{
  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */
  float sin_heading =
    sinf (euler->heading * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f);
  float sin_pitch =
    sinf (euler->pitch * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f);
  float sin_roll =
    sinf (euler->roll * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f);
  float cos_heading =
    cosf (euler->heading * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f);
  float cos_pitch =
    cosf (euler->pitch * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f);
  float cos_roll =
    cosf (euler->roll * _COGL_QUATERNION_DEGREES_TO_RADIANS * 0.5f);

  quaternion->w =
    cos_heading * cos_pitch * cos_roll +
    sin_heading * sin_pitch * sin_roll;

  quaternion->x =
    cos_heading * sin_pitch * cos_roll +
    sin_heading * cos_pitch * sin_roll;
  quaternion->y =
    sin_heading * cos_pitch * cos_roll -
    cos_heading * sin_pitch * sin_roll;
  quaternion->z =
    cos_heading * cos_pitch * sin_roll -
    sin_heading * sin_pitch * cos_roll;
}

void
cogl_quaternion_init_from_quaternion (CoglQuaternion *quaternion,
                                      CoglQuaternion *src)
{
  memcpy (quaternion, src, sizeof (float) * 4);
}

/* XXX: it could be nice to make something like this public... */
/*
 * COGL_MATRIX_READ:
 * @MATRIX: A 4x4 transformation matrix
 * @ROW: The row of the value you want to read
 * @COLUMN: The column of the value you want to read
 *
 * Reads a value from the given matrix using integers to index
 * into the matrix.
 */
#define COGL_MATRIX_READ(MATRIX, ROW, COLUMN) \
  (((const float *)matrix)[COLUMN * 4 + ROW])

void
cogl_quaternion_init_from_matrix (CoglQuaternion *quaternion,
                                  const CoglMatrix *matrix)
{
  /* Algorithm devised by Ken Shoemake, Ref:
   * http://campar.in.tum.de/twiki/pub/Chair/DwarfTutorial/quatut.pdf
   */

  /* 3D maths literature refers to the diagonal of a matrix as the
   * "trace" of a matrix... */
  float trace = matrix->xx + matrix->yy + matrix->zz;
  float root;

  if (trace > 0.0f)
    {
      root = sqrtf (trace + 1);
      quaternion->w = root * 0.5f;
      root = 0.5f / root;
      quaternion->x = (matrix->zy - matrix->yz) * root;
      quaternion->y = (matrix->xz - matrix->zx) * root;
      quaternion->z = (matrix->yx - matrix->xy) * root;
    }
  else
    {
#define X 0
#define Y 1
#define Z 2
#define W 3
      int h = X;
      if (matrix->yy > matrix->xx)
        h = Y;
      if (matrix->zz > COGL_MATRIX_READ (matrix, h, h))
        h = Z;
      switch (h)
        {
#define CASE_MACRO(i, j, k, I, J, K) \
        case I: \
          root = sqrtf ((COGL_MATRIX_READ (matrix, I, I) - \
                         (COGL_MATRIX_READ (matrix, J, J) + \
                          COGL_MATRIX_READ (matrix, K, K))) + \
                        COGL_MATRIX_READ (matrix, W, W)); \
          quaternion->i = root * 0.5f;\
          root = 0.5f / root;\
          quaternion->j = (COGL_MATRIX_READ (matrix, I, J) + \
                           COGL_MATRIX_READ (matrix, J, I)) * root; \
          quaternion->k = (COGL_MATRIX_READ (matrix, K, I) + \
                           COGL_MATRIX_READ (matrix, I, K)) * root; \
          quaternion->w = (COGL_MATRIX_READ (matrix, K, J) - \
                           COGL_MATRIX_READ (matrix, J, K)) * root;\
          break
          CASE_MACRO (x, y, z, X, Y, Z);
          CASE_MACRO (y, z, x, Y, Z, X);
          CASE_MACRO (z, x, y, Z, X, Y);
#undef CASE_MACRO
#undef X
#undef Y
#undef Z
        }
    }

  if (matrix->ww != 1.0f)
    {
      float s = 1.0 / sqrtf (matrix->ww);
      quaternion->w *= s;
      quaternion->x *= s;
      quaternion->y *= s;
      quaternion->z *= s;
    }
}

CoglBool
cogl_quaternion_equal (const void *v1, const void *v2)
{
  const CoglQuaternion *a = v1;
  const CoglQuaternion *b = v2;

  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  if (v1 == v2)
    return TRUE;

  return (a->w == b->w &&
          a->x == b->x &&
          a->y == b->y &&
          a->z == b->z);
}

CoglQuaternion *
cogl_quaternion_copy (const CoglQuaternion *src)
{
  if (G_LIKELY (src))
    {
      CoglQuaternion *new = g_slice_new (CoglQuaternion);
      memcpy (new, src, sizeof (float) * 4);
      return new;
    }
  else
    return NULL;
}

void
cogl_quaternion_free (CoglQuaternion *quaternion)
{
  g_slice_free (CoglQuaternion, quaternion);
}

float
cogl_quaternion_get_rotation_angle (const CoglQuaternion *quaternion)
{
  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */

  /* FIXME: clamp [-1, 1] */
  return 2.0f * acosf (quaternion->w) * _COGL_QUATERNION_RADIANS_TO_DEGREES;
}

void
cogl_quaternion_get_rotation_axis (const CoglQuaternion *quaternion,
                                   float *vector3)
{
  float sin_half_angle_sqr;
  float one_over_sin_angle_over_2;

  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */

  /* NB: sin²(𝜃) + cos²(𝜃) = 1 */

  sin_half_angle_sqr = 1.0f - quaternion->w * quaternion->w;

  if (sin_half_angle_sqr <= 0.0f)
    {
      /* Either an identity quaternion or numerical imprecision.
       * Either way we return an arbitrary vector. */
      vector3[0] = 1;
      vector3[1] = 0;
      vector3[2] = 0;
      return;
    }

  /* Calculate 1 / sin(𝜃/2) */
  one_over_sin_angle_over_2 = 1.0f / sqrtf (sin_half_angle_sqr);

  vector3[0] = quaternion->x * one_over_sin_angle_over_2;
  vector3[1] = quaternion->y * one_over_sin_angle_over_2;
  vector3[2] = quaternion->z * one_over_sin_angle_over_2;
}

void
cogl_quaternion_normalize (CoglQuaternion *quaternion)
{
  float slen = _COGL_QUATERNION_NORM (quaternion);
  float factor = 1.0f / sqrtf (slen);

  quaternion->x *= factor;
  quaternion->y *= factor;
  quaternion->z *= factor;

  quaternion->w *= factor;

  return;
}

float
cogl_quaternion_dot_product (const CoglQuaternion *a,
                             const CoglQuaternion *b)
{
  return a->w * b->w + a->x * b->x + a->y * b->y + a->z * b->z;
}

void
cogl_quaternion_invert (CoglQuaternion *quaternion)
{
  quaternion->x = -quaternion->x;
  quaternion->y = -quaternion->y;
  quaternion->z = -quaternion->z;
}

void
cogl_quaternion_multiply (CoglQuaternion *result,
                          const CoglQuaternion *a,
                          const CoglQuaternion *b)
{
  float w = a->w;
  float x = a->x;
  float y = a->y;
  float z = a->z;

  _COGL_RETURN_IF_FAIL (b != result);

  result->w = w * b->w - x * b->x - y * b->y - z * b->z;

  result->x = w * b->x + x * b->w + y * b->z - z * b->y;
  result->y = w * b->y + y * b->w + z * b->x - x * b->z;
  result->z = w * b->z + z * b->w + x * b->y - y * b->x;
}

void
cogl_quaternion_pow (CoglQuaternion *quaternion, float exponent)
{
  float half_angle;
  float new_half_angle;
  float factor;

  /* Try and identify and nop identity quaternions to avoid
   * dividing by zero */
  if (fabs (quaternion->w) > 0.9999f)
    return;

  /* NB: We are using quaternions to represent an axis (a), angle (𝜃) pair
   * in this form:
   * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
   */

  /* FIXME: clamp [-1, 1] */
  /* Extract 𝜃/2 from w */
  half_angle = acosf (quaternion->w);

  /* Compute the new 𝜃/2 */
  new_half_angle = half_angle * exponent;

  /* Compute the new w value */
  quaternion->w = cosf (new_half_angle);

  /* And new xyz values */
  factor = sinf (new_half_angle) / sinf (half_angle);
  quaternion->x *= factor;
  quaternion->y *= factor;
  quaternion->z *= factor;
}

void
cogl_quaternion_slerp (CoglQuaternion *result,
                       const CoglQuaternion *a,
                       const CoglQuaternion *b,
                       float t)
{
  float cos_difference;
  float qb_w;
  float qb_x;
  float qb_y;
  float qb_z;
  float fa;
  float fb;

  _COGL_RETURN_IF_FAIL (t >=0 && t <= 1.0f);

  if (t == 0)
    {
      *result = *a;
      return;
    }
  else if (t == 1)
    {
      *result = *b;
      return;
    }

  /* compute the cosine of the angle between the two given quaternions */
  cos_difference = cogl_quaternion_dot_product (a, b);

  /* If negative, use -b. Two quaternions q and -q represent the same angle but
   * may produce a different slerp. We choose b or -b to rotate using the acute
   * angle.
   */
  if (cos_difference < 0.0f)
    {
      qb_w = -b->w;
      qb_x = -b->x;
      qb_y = -b->y;
      qb_z = -b->z;
      cos_difference = -cos_difference;
    }
  else
    {
      qb_w = b->w;
      qb_x = b->x;
      qb_y = b->y;
      qb_z = b->z;
    }

  /* If we have two unit quaternions the dot should be <= 1.0 */
  g_assert (cos_difference < 1.1f);


  /* Determine the interpolation factors for each quaternion, simply using
   * linear interpolation for quaternions that are nearly exactly the same.
   * (this will avoid divisions by zero)
   */

  if (cos_difference > 0.9999f)
    {
      fa = 1.0f - t;
      fb = t;

      /* XXX: should we also normalize() at the end in this case? */
    }
  else
    {
      /* Calculate the sin of the angle between the two quaternions using the
       * trig identity: sin²(𝜃) + cos²(𝜃) = 1
       */
      float sin_difference =  sqrtf (1.0f - cos_difference * cos_difference);

      float difference = atan2f (sin_difference, cos_difference);
      float one_over_sin_difference = 1.0f / sin_difference;
      fa = sinf ((1.0f - t) * difference) * one_over_sin_difference;
      fb = sinf (t * difference) * one_over_sin_difference;
    }

  /* Finally interpolate the two quaternions */

  result->x = fa * a->x + fb * qb_x;
  result->y = fa * a->y + fb * qb_y;
  result->z = fa * a->z + fb * qb_z;
  result->w = fa * a->w + fb * qb_w;
}

void
cogl_quaternion_nlerp (CoglQuaternion *result,
                       const CoglQuaternion *a,
                       const CoglQuaternion *b,
                       float t)
{
  float cos_difference;
  float qb_w;
  float qb_x;
  float qb_y;
  float qb_z;
  float fa;
  float fb;

  _COGL_RETURN_IF_FAIL (t >=0 && t <= 1.0f);

  if (t == 0)
    {
      *result = *a;
      return;
    }
  else if (t == 1)
    {
      *result = *b;
      return;
    }

  /* compute the cosine of the angle between the two given quaternions */
  cos_difference = cogl_quaternion_dot_product (a, b);

  /* If negative, use -b. Two quaternions q and -q represent the same angle but
   * may produce a different slerp. We choose b or -b to rotate using the acute
   * angle.
   */
  if (cos_difference < 0.0f)
    {
      qb_w = -b->w;
      qb_x = -b->x;
      qb_y = -b->y;
      qb_z = -b->z;
      cos_difference = -cos_difference;
    }
  else
    {
      qb_w = b->w;
      qb_x = b->x;
      qb_y = b->y;
      qb_z = b->z;
    }

  /* If we have two unit quaternions the dot should be <= 1.0 */
  g_assert (cos_difference < 1.1f);

  fa = 1.0f - t;
  fb = t;

  result->x = fa * a->x + fb * qb_x;
  result->y = fa * a->y + fb * qb_y;
  result->z = fa * a->z + fb * qb_z;
  result->w = fa * a->w + fb * qb_w;

  cogl_quaternion_normalize (result);
}

void
cogl_quaternion_squad (CoglQuaternion *result,
                       const CoglQuaternion *prev,
                       const CoglQuaternion *a,
                       const CoglQuaternion *b,
                       const CoglQuaternion *next,
                       float t)
{
  CoglQuaternion slerp0;
  CoglQuaternion slerp1;

  cogl_quaternion_slerp (&slerp0, a, b, t);
  cogl_quaternion_slerp (&slerp1, prev, next, t);
  cogl_quaternion_slerp (result, &slerp0, &slerp1, 2.0f * t * (1.0f - t));
}

const CoglQuaternion *
cogl_get_static_identity_quaternion (void)
{
  return &identity_quaternion;
}

const CoglQuaternion *
cogl_get_static_zero_quaternion (void)
{
  return &zero_quaternion;
}

