/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#define USE_MESA_MATRIX_API

#include <cogl.h>
#include <cogl-matrix.h>
#ifdef USE_MESA_MATRIX_API
#include <cogl-matrix-mesa.h>
#endif

#include <glib.h>
#include <math.h>
#include <string.h>

void
cogl_matrix_init_identity (CoglMatrix *matrix)
{
#ifndef USE_MESA_MATRIX_API
  matrix->xx = 1; matrix->xy = 0; matrix->xz = 0; matrix->xw = 0;
  matrix->yx = 0; matrix->yy = 1; matrix->yz = 0; matrix->yw = 0;
  matrix->zx = 0; matrix->zy = 0; matrix->zz = 1; matrix->zw = 0;
  matrix->wx = 0; matrix->wy = 0; matrix->wz = 0; matrix->ww = 1;
#else
  _math_matrix_init_identity (matrix);
#endif
}

void
cogl_matrix_multiply (CoglMatrix *result,
		      const CoglMatrix *a,
		      const CoglMatrix *b)
{
#ifndef USE_MESA_MATRIX_API
  CoglMatrix r;

  /* row 0 */
  r.xx = a->xx * b->xx + a->xy * b->yx + a->xz * b->zx + a->xw * b->wx;
  r.xy = a->xx * b->xy + a->xy * b->yy + a->xz * b->zy + a->xw * b->wy;
  r.xz = a->xx * b->xz + a->xy * b->yz + a->xz * b->zz + a->xw * b->wz;
  r.xw = a->xx * b->xw + a->xy * b->yw + a->xz * b->zw + a->xw * b->ww;

  /* row 1 */
  r.yx = a->yx * b->xx + a->yy * b->yx + a->yz * b->zx + a->yw * b->wx;
  r.yy = a->yx * b->xy + a->yy * b->yy + a->yz * b->zy + a->yw * b->wy;
  r.yz = a->yx * b->xz + a->yy * b->yz + a->yz * b->zz + a->yw * b->wz;
  r.yw = a->yx * b->xw + a->yy * b->yw + a->yz * b->zw + a->yw * b->ww;

  /* row 2 */
  r.zx = a->zx * b->xx + a->zy * b->yx + a->zz * b->zx + a->zw * b->wx;
  r.zy = a->zx * b->xy + a->zy * b->yy + a->zz * b->zy + a->zw * b->wy;
  r.zz = a->zx * b->xz + a->zy * b->yz + a->zz * b->zz + a->zw * b->wz;
  r.zw = a->zx * b->xw + a->zy * b->yw + a->zz * b->zw + a->zw * b->ww;

  /* row 3 */
  r.wx = a->wx * b->xx + a->wy * b->yx + a->wz * b->zx + a->ww * b->wx;
  r.wy = a->wx * b->xy + a->wy * b->yy + a->wz * b->zy + a->ww * b->wy;
  r.wz = a->wx * b->xz + a->wy * b->yz + a->wz * b->zz + a->ww * b->wz;
  r.ww = a->wx * b->xw + a->wy * b->yw + a->wz * b->zw + a->ww * b->ww;

  /* The idea was that having this unrolled; it might be easier for the
   * compiler to vectorize, but that's probably not true. Mesa does it
   * using a single for (i=0; i<4; i++) approach, maybe that's better...
   */

  *result = r;
#else
  _math_matrix_multiply (result, a, b);
#endif
}

void
cogl_matrix_rotate (CoglMatrix *matrix,
		    float angle,
		    float x,
		    float y,
		    float z)
{
#ifndef USE_MESA_MATRIX_API
  CoglMatrix rotation;
  CoglMatrix result;
  float c, s;

  angle *= G_PI / 180.0f;
  c = cosf (angle);
  s = sinf (angle);

  rotation.xx = x * x * (1.0f - c) + c;
  rotation.yx = y * x * (1.0f - c) + z * s;
  rotation.zx = x * z * (1.0f - c) - y * s;
  rotation.wx = 0.0f;

  rotation.xy = x * y * (1.0f - c) - z * s;
  rotation.yy = y * y * (1.0f - c) + c;
  rotation.zy = y * z * (1.0f - c) + x * s;
  rotation.wy = 0.0f;

  rotation.xz = x * z * (1.0f - c) + y * s;
  rotation.yz = y * z * (1.0f - c) - x * s;
  rotation.zz = z * z * (1.0f - c) + c;
  rotation.wz = 0.0f;

  rotation.xw = 0.0f;
  rotation.yw = 0.0f;
  rotation.zw = 0.0f;
  rotation.ww = 1.0f;

  cogl_matrix_multiply (&result, matrix, &rotation);
  *matrix = result;
#else
  _math_matrix_rotate (matrix, angle, x, y, z);
#endif
}

void
cogl_matrix_translate (CoglMatrix *matrix,
		       float x,
		       float y,
		       float z)
{
#ifndef USE_MESA_MATRIX_API
  matrix->xw = matrix->xx * x + matrix->xy * y + matrix->xz * z + matrix->xw;
  matrix->yw = matrix->yx * x + matrix->yy * y + matrix->yz * z + matrix->yw;
  matrix->zw = matrix->zx * x + matrix->zy * y + matrix->zz * z + matrix->zw;
  matrix->ww = matrix->wx * x + matrix->wy * y + matrix->wz * z + matrix->ww;
#else
  _math_matrix_translate (matrix, x, y, z);
#endif
}

void
cogl_matrix_scale (CoglMatrix *matrix,
		   float sx,
		   float sy,
		   float sz)
{
#ifndef USE_MESA_MATRIX_API
  matrix->xx *= sx; matrix->xy *= sy; matrix->xz *= sz;
  matrix->yx *= sx; matrix->yy *= sy; matrix->yz *= sz;
  matrix->zx *= sx; matrix->zy *= sy; matrix->zz *= sz;
  matrix->wx *= sx; matrix->wy *= sy; matrix->wz *= sz;
#else
  _math_matrix_scale (matrix, sx, sy, sz);
#endif
}

#if 0
gboolean
cogl_matrix_invert (CoglMatrix *matrix)
{
  /* TODO */
  /* Note: It might be nice to also use the flag based tricks that mesa does
   * to alow it to track the type of transformations a matrix represents
   * so it can use various assumptions to optimise the inversion.
   */
}
#endif

void
cogl_matrix_frustum (CoglMatrix *matrix,
                     float       left,
                     float       right,
                     float       bottom,
                     float       top,
                     float       z_near,
                     float       z_far)
{
#ifndef USE_MESA_MATRIX_API
  float x, y, a, b, c, d;
  CoglMatrix frustum;

  x = (2.0f * z_near) / (right - left);
  y = (2.0f * z_near) / (top - bottom);
  a = (right + left) / (right - left);
  b = (top + bottom) / (top - bottom);
  c = -(z_far + z_near) / ( z_far - z_near);
  d = -(2.0f * z_far* z_near) / (z_far - z_near);

  frustum.xx = x;
  frustum.yx = 0.0f;
  frustum.zx = 0.0f;
  frustum.wx = 0.0f;

  frustum.xy = 0.0f;
  frustum.yy = y;
  frustum.zy = 0.0f;
  frustum.wy = 0.0f;

  frustum.xz = a;
  frustum.yz = b;
  frustum.zz = c;
  frustum.wz = -1.0f;

  frustum.xw = 0.0f;
  frustum.yw = 0.0f;
  frustum.zw = d;
  frustum.ww = 0.0f;

  cogl_matrix_multiply (matrix, matrix, &frustum);
#else
  _math_matrix_frustum (matrix, left, right, bottom, top, z_near, z_far);
#endif
}

void
cogl_matrix_perspective (CoglMatrix *matrix,
                         float       fov_y,
                         float       aspect,
                         float       z_near,
                         float       z_far)
{
  float ymax = z_near * tan (fov_y * G_PI / 360.0);

  cogl_matrix_frustum (matrix,
                       -ymax * aspect,  /* left */
                       ymax * aspect,   /* right */
                       -ymax,           /* bottom */
                       ymax,            /* top */
                       z_near,
                       z_far);
}

void
cogl_matrix_ortho (CoglMatrix *matrix,
                   float left,
                   float right,
                   float bottom,
                   float top,
                   float near_val,
                   float far_val)
{
#ifndef USE_MESA_MATRIX_API
  CoglMatrix ortho;

  /* column 0 */
  ortho.xx = 2.0 / (right - left);
  ortho.yx = 0.0;
  ortho.zx = 0.0;
  ortho.wx = 0.0;

  /* column 1 */
  ortho.xy = 0.0;
  ortho.yy = 2.0 / (top - bottom);
  ortho.zy = 0.0;
  ortho.wy = 0.0;

  /* column 2 */
  ortho.xz = 0.0;
  ortho.yz = 0.0;
  ortho.zz = -2.0 / (far_val - near_val);
  ortho.wz = 0.0;

  /* column 3 */
  ortho.xw = -(right + left) / (right - left);
  ortho.yw = -(top + bottom) / (top - bottom);
  ortho.zw = -(far_val + near_val) / (far_val - near_val);
  ortho.ww = 1.0;

  cogl_matrix_multiply (matrix, matrix, &ortho);
#else
  _math_matrix_ortho (matrix, left, right, bottom, top, near_val, far_val);
#endif
}

void
cogl_matrix_init_from_array (CoglMatrix *matrix, const float *array)
{
#ifndef USE_MESA_MATRIX_API
  memcpy (matrix, array, sizeof (float) * 16);
#else
  _math_matrix_init_from_array (matrix, array);
#endif
}

const float *
cogl_matrix_get_array (const CoglMatrix *matrix)
{
  return (float *)matrix;
}

void
cogl_matrix_transform_point (const CoglMatrix *matrix,
                             float *x,
                             float *y,
                             float *z,
                             float *w)
{
  float _x = *x, _y = *y, _z = *z, _w = *w;

  *x = matrix->xx * _x + matrix->xy * _y + matrix->xz * _z + matrix->xw * _w;
  *y = matrix->yx * _x + matrix->yy * _y + matrix->yz * _z + matrix->yw * _w;
  *z = matrix->zx * _x + matrix->zy * _y + matrix->zz * _z + matrix->zw * _w;
  *w = matrix->wx * _x + matrix->wy * _y + matrix->wz * _z + matrix->ww * _w;
}


