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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl-util.h>
#include <cogl-euler.h>
#include <cogl-matrix.h>
#include "cogl-gtype-private.h"

#include <math.h>
#include <string.h>

COGL_GTYPE_DEFINE_BOXED (Euler, euler,
                         cogl_euler_copy,
                         cogl_euler_free);

void
cogl_euler_init (CoglEuler *euler,
                 float heading,
                 float pitch,
                 float roll)
{
  euler->heading = heading;
  euler->pitch = pitch;
  euler->roll = roll;
}

void
cogl_euler_init_from_matrix (CoglEuler *euler,
                             const CoglMatrix *matrix)
{
  /*
   * Extracting a canonical Euler angle from a matrix:
   * (where it is assumed the matrix contains no scaling, mirroring or
   *  skewing)
   *
   * A Euler angle is a combination of three rotations around mutually
   * perpendicular axis. For this algorithm they are:
   *
   * Heading: A rotation about the Y axis by an angle H:
   * | cosH  0  sinH|
   * |    0  1     0|
   * |-sinH  0  cosH|
   *
   * Pitch: A rotation around the X axis by an angle P:
   * |1     0      0|
   * |0  cosP  -sinP|
   * |0  sinP   cosP|
   *
   * Roll: A rotation about the Z axis by an angle R:
   * |cosR -sinR  0|
   * |sinR  cosR  0|
   * |   0     0  1|
   *
   * When multiplied as matrices this gives:
   *     | cosHcosR+sinHsinPsinR   sinRcosP  -sinHcosR+cosHsinPsinR|
   * M = |-cosHsinR+sinHsinPcosR   cosRcosP   sinRsinH+cosHsinPcosB|
   *     | sinHcosP               -sinP       cosHcosP             |
   *
   * Given that there are an infinite number of ways to represent
   * a given orientation, the "canonical" Euler angle is any such that:
   *  -180 < H < 180,
   *  -180 < R < 180 and
   *   -90 < P < 90
   *
   * M[3][2] = -sinP lets us immediately solve for P = asin(-M[3][2])
   *   (Note: asin has a range of +-90)
   * This gives cosP
   * This means we can use M[3][1] to calculate sinH:
   *   sinH = M[3][1]/cosP
   * And use M[3][3] to calculate cosH:
   *   cosH = M[3][3]/cosP
   * This lets us calculate H = atan2(sinH,cosH), but we optimise this:
   *   1st note: atan2(x, y) does: atan(x/y) and uses the sign of x and y to
   *   determine the quadrant of the final angle.
   *   2nd note: we know cosP is > 0 (ignoring cosP == 0)
   *   Therefore H = atan2((M[3][1]/cosP) / (M[3][3]/cosP)) can be simplified
   *   by skipping the division by cosP since it won't change the x/y ratio
   *   nor will it change their sign. This gives:
   *     H = atan2(M[3][1], M[3][3])
   * R is computed in the same way as H from M[1][2] and M[2][2] so:
   *     R = atan2(M[1][2], M[2][2])
   * Note: If cosP were == 0 then H and R could not be calculated as above
   * because all the necessary matrix values would == 0. In other words we are
   * pitched vertically and so H and R would now effectively rotate around the
   * same axis - known as "Gimbal lock". In this situation we will set all the
   * rotation on H and set R = 0.
   *   So with P = R = 0 we have cosP = 0, sinR = 0 and cosR = 1
   *   We can substitute those into the above equation for M giving:
   *   |    cosH      0     -sinH|
   *   |sinHsinP      0  cosHsinP|
   *   |       0  -sinP         0|
   *   And calculate H as atan2 (-M[3][2], M[1][1])
   */

  float sinP;
  float H; /* heading */
  float P; /* pitch */
  float R; /* roll */

  /* NB: CoglMatrix provides struct members named according to the
   * [row][column] indexed. So matrix->zx is row 3 column 1. */
  sinP = -matrix->zy;

  /* Determine the Pitch, avoiding domain errors with asin () which
   * might occur due to previous imprecision in manipulating the
   * matrix. */
  if (sinP <= -1.0f)
    P = -G_PI_2;
  else if (sinP >= 1.0f)
    P = G_PI_2;
  else
    P = asinf (sinP);

  /* If P is too close to 0 then we have hit Gimbal lock */
  if (sinP > 0.999f)
    {
      H = atan2f (-matrix->zy, matrix->xx);
      R = 0;
    }
  else
    {
      H = atan2f (matrix->zx, matrix->zz);
      R = atan2f (matrix->xy, matrix->yy);
    }

  euler->heading = H;
  euler->pitch = P;
  euler->roll = R;
}

CoglBool
cogl_euler_equal (const void *v1, const void *v2)
{
  const CoglEuler *a = v1;
  const CoglEuler *b = v2;

  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  if (v1 == v2)
    return TRUE;

  return (a->heading == b->heading &&
          a->pitch == b->pitch &&
          a->roll == b->roll);
}

CoglEuler *
cogl_euler_copy (const CoglEuler *src)
{
  if (G_LIKELY (src))
    {
      CoglEuler *new = g_slice_new (CoglEuler);
      memcpy (new, src, sizeof (float) * 3);
      return new;
    }
  else
    return NULL;
}

void
cogl_euler_free (CoglEuler *euler)
{
  g_slice_free (CoglEuler, euler);
}

