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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define USE_MESA_MATRIX_API

#include <cogl.h>
#include "cogl-debug.h"
#include <cogl-quaternion.h>
#include <cogl-quaternion-private.h>
#include <cogl-matrix.h>
#include <cogl-matrix-private.h>
#ifdef USE_MESA_MATRIX_API
#include <cogl-matrix-mesa.h>
#endif

#include <glib.h>
#include <math.h>
#include <string.h>

#ifdef _COGL_SUPPORTS_GTYPE_INTEGRATION
#include <cogl-gtype-private.h>
COGL_GTYPE_DEFINE_BOXED ("Matrix", matrix,
                         cogl_matrix_copy,
                         cogl_matrix_free);
#endif

void
cogl_matrix_init_identity (CoglMatrix *matrix)
{
#ifndef USE_MESA_MATRIX_API
  matrix->xx = 1; matrix->xy = 0; matrix->xz = 0; matrix->xw = 0;
  matrix->yx = 0; matrix->yy = 1; matrix->yz = 0; matrix->yw = 0;
  matrix->zx = 0; matrix->zy = 0; matrix->zz = 1; matrix->zw = 0;
  matrix->wx = 0; matrix->wy = 0; matrix->wz = 0; matrix->ww = 1;
#else
  _cogl_matrix_init_identity (matrix);
#endif
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_matrix_init_from_quaternion (CoglMatrix *matrix,
                                  CoglQuaternion *quaternion)
{
  _cogl_matrix_init_from_quaternion (matrix, quaternion);
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
  _cogl_matrix_multiply (result, a, b);
#endif
  _COGL_MATRIX_DEBUG_PRINT (result);
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
  _cogl_matrix_rotate (matrix, angle, x, y, z);
#endif
  _COGL_MATRIX_DEBUG_PRINT (matrix);
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
  _cogl_matrix_translate (matrix, x, y, z);
#endif
  _COGL_MATRIX_DEBUG_PRINT (matrix);
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
  _cogl_matrix_scale (matrix, sx, sy, sz);
#endif
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

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
  _cogl_matrix_frustum (matrix, left, right, bottom, top, z_near, z_far);
#endif
  _COGL_MATRIX_DEBUG_PRINT (matrix);
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
  _COGL_MATRIX_DEBUG_PRINT (matrix);
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
  _cogl_matrix_ortho (matrix, left, right, bottom, top, near_val, far_val);
#endif
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_matrix_view_2d_in_frustum (CoglMatrix *matrix,
                                float left,
                                float right,
                                float bottom,
                                float top,
                                float z_near,
                                float z_2d,
                                float width_2d,
                                float height_2d)
{
  float left_2d_plane = left / z_near * z_2d;
  float right_2d_plane = right / z_near * z_2d;
  float bottom_2d_plane = bottom / z_near * z_2d;
  float top_2d_plane = top / z_near * z_2d;

  float width_2d_start = right_2d_plane - left_2d_plane;
  float height_2d_start = top_2d_plane - bottom_2d_plane;

  /* Factors to scale from framebuffer geometry to frustum
   * cross-section geometry. */
  float width_scale = width_2d_start / width_2d;
  float height_scale = height_2d_start / height_2d;

  cogl_matrix_translate (matrix,
                         left_2d_plane, top_2d_plane, -z_2d);

  cogl_matrix_scale (matrix, width_scale, -height_scale, width_scale);
}

/* Assuming a symmetric perspective matrix is being used for your
 * projective transform this convenience function lets you compose a
 * view transform such that geometry on the z=0 plane will map to
 * screen coordinates with a top left origin of (0,0) and with the
 * given width and height.
 */
void
cogl_matrix_view_2d_in_perspective (CoglMatrix *matrix,
                                    float fov_y,
                                    float aspect,
                                    float z_near,
                                    float z_2d,
                                    float width_2d,
                                    float height_2d)
{
  float top = z_near * tan (fov_y * G_PI / 360.0);
  cogl_matrix_view_2d_in_frustum (matrix,
                                  -top * aspect,
                                  top * aspect,
                                  -top,
                                  top,
                                  z_near,
                                  z_2d,
                                  width_2d,
                                  height_2d);
}

void
cogl_matrix_init_from_array (CoglMatrix *matrix, const float *array)
{
#ifndef USE_MESA_MATRIX_API
  memcpy (matrix, array, sizeof (float) * 16);
#else
  _cogl_matrix_init_from_array (matrix, array);
#endif
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

gboolean
cogl_matrix_equal (gconstpointer v1, gconstpointer v2)
{
  const CoglMatrix *a = v1;
  const CoglMatrix *b = v2;

  g_return_val_if_fail (v1 != NULL, FALSE);
  g_return_val_if_fail (v2 != NULL, FALSE);

  /* We want to avoid having a fuzzy _equal() function (e.g. that uses
   * an arbitrary epsilon value) since this function noteably conforms
   * to the prototype suitable for use with g_hash_table_new() and a
   * fuzzy hash function isn't really appropriate for comparing hash
   * table keys since it's possible that you could end up fetching
   * different values if you end up with multiple similar keys in use
   * at the same time. If you consider that fuzzyness allows cases
   * such as A == B == C but A != C then you could also end up loosing
   * values in a hash table.
   *
   * We do at least use the == operator to compare values though so
   * that -0 is considered equal to 0.
   */

  /* XXX: We don't compare the flags, inverse matrix or padding */
  if (a->xx == b->xx &&
      a->xy == b->xy &&
      a->xz == b->xz &&
      a->xw == b->xw &&
      a->yx == b->yx &&
      a->yy == b->yy &&
      a->yz == b->yz &&
      a->yw == b->yw &&
      a->zx == b->zx &&
      a->zy == b->zy &&
      a->zz == b->zz &&
      a->zw == b->zw &&
      a->wx == b->wx &&
      a->wy == b->wy &&
      a->wz == b->wz &&
      a->ww == b->ww)
    return TRUE;
  else
    return FALSE;
}

CoglMatrix *
cogl_matrix_copy (const CoglMatrix *matrix)
{
  if (G_LIKELY (matrix))
    return g_slice_dup (CoglMatrix, matrix);

  return NULL;
}

void
cogl_matrix_free (CoglMatrix *matrix)
{
  g_slice_free (CoglMatrix, matrix);
}

const float *
cogl_matrix_get_array (const CoglMatrix *matrix)
{
  return (float *)matrix;
}

gboolean
cogl_matrix_get_inverse (const CoglMatrix *matrix, CoglMatrix *inverse)
{
#ifndef USE_MESA_MATRIX_API
#warning "cogl_matrix_get_inverse not supported without Mesa matrix API"
  cogl_matrix_init_identity (inverse);
  return FALSE;
#else
  if (_cogl_matrix_update_inverse ((CoglMatrix *)matrix))
    {
      cogl_matrix_init_from_array (inverse, matrix->inv);
      return TRUE;
    }
  else
    {
      cogl_matrix_init_identity (inverse);
      return FALSE;
    }
#endif
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

typedef struct _Point2f
{
  float x;
  float y;
} Point2f;

typedef struct _Point3f
{
  float x;
  float y;
  float z;
} Point3f;

typedef struct _Point4f
{
  float x;
  float y;
  float z;
  float w;
} Point4f;

static void
_cogl_matrix_transform_points_f2 (const CoglMatrix *matrix,
                                  size_t stride_in,
                                  const void *points_in,
                                  size_t stride_out,
                                  void *points_out,
                                  int n_points)
{
  int i;

  for (i = 0; i < n_points; i++)
    {
      Point2f p = *(Point2f *)((guint8 *)points_in + i * stride_in);
      Point3f *o = (Point3f *)((guint8 *)points_out + i * stride_out);

      o->x = matrix->xx * p.x + matrix->xy * p.y + matrix->xw;
      o->y = matrix->yx * p.x + matrix->yy * p.y + matrix->yw;
      o->z = matrix->zx * p.x + matrix->zy * p.y + matrix->zw;
    }
}

static void
_cogl_matrix_project_points_f2 (const CoglMatrix *matrix,
                                size_t stride_in,
                                const void *points_in,
                                size_t stride_out,
                                void *points_out,
                                int n_points)
{
  int i;

  for (i = 0; i < n_points; i++)
    {
      Point2f p = *(Point2f *)((guint8 *)points_in + i * stride_in);
      Point4f *o = (Point4f *)((guint8 *)points_out + i * stride_out);

      o->x = matrix->xx * p.x + matrix->xy * p.y + matrix->xw;
      o->y = matrix->yx * p.x + matrix->yy * p.y + matrix->yw;
      o->z = matrix->zx * p.x + matrix->zy * p.y + matrix->zw;
      o->w = matrix->wx * p.x + matrix->wy * p.y + matrix->ww;
    }
}

static void
_cogl_matrix_transform_points_f3 (const CoglMatrix *matrix,
                                  size_t stride_in,
                                  const void *points_in,
                                  size_t stride_out,
                                  void *points_out,
                                  int n_points)
{
  int i;

  for (i = 0; i < n_points; i++)
    {
      Point3f p = *(Point3f *)((guint8 *)points_in + i * stride_in);
      Point3f *o = (Point3f *)((guint8 *)points_out + i * stride_out);

      o->x = matrix->xx * p.x + matrix->xy * p.y +
             matrix->xz * p.z + matrix->xw;
      o->y = matrix->yx * p.x + matrix->yy * p.y +
             matrix->yz * p.z + matrix->yw;
      o->z = matrix->zx * p.x + matrix->zy * p.y +
             matrix->zz * p.z + matrix->zw;
    }
}

static void
_cogl_matrix_project_points_f3 (const CoglMatrix *matrix,
                                size_t stride_in,
                                const void *points_in,
                                size_t stride_out,
                                void *points_out,
                                int n_points)
{
  int i;

  for (i = 0; i < n_points; i++)
    {
      Point3f p = *(Point3f *)((guint8 *)points_in + i * stride_in);
      Point4f *o = (Point4f *)((guint8 *)points_out + i * stride_out);

      o->x = matrix->xx * p.x + matrix->xy * p.y +
             matrix->xz * p.z + matrix->xw;
      o->y = matrix->yx * p.x + matrix->yy * p.y +
             matrix->yz * p.z + matrix->yw;
      o->z = matrix->zx * p.x + matrix->zy * p.y +
             matrix->zz * p.z + matrix->zw;
      o->w = matrix->wx * p.x + matrix->wy * p.y +
             matrix->wz * p.z + matrix->ww;
    }
}

static void
_cogl_matrix_project_points_f4 (const CoglMatrix *matrix,
                                size_t stride_in,
                                const void *points_in,
                                size_t stride_out,
                                void *points_out,
                                int n_points)
{
  int i;

  for (i = 0; i < n_points; i++)
    {
      Point4f p = *(Point4f *)((guint8 *)points_in + i * stride_in);
      Point4f *o = (Point4f *)((guint8 *)points_out + i * stride_out);

      o->x = matrix->xx * p.x + matrix->xy * p.y +
             matrix->xz * p.z + matrix->xw * p.w;
      o->y = matrix->yx * p.x + matrix->yy * p.y +
             matrix->yz * p.z + matrix->yw * p.w;
      o->z = matrix->zx * p.x + matrix->zy * p.y +
             matrix->zz * p.z + matrix->zw * p.w;
      o->w = matrix->wx * p.x + matrix->wy * p.y +
             matrix->wz * p.z + matrix->ww * p.w;
    }
}

void
cogl_matrix_transform_points (const CoglMatrix *matrix,
                              int n_components,
                              size_t stride_in,
                              const void *points_in,
                              size_t stride_out,
                              void *points_out,
                              int n_points)
{
  /* The results of transforming always have three components... */
  g_return_if_fail (stride_out >= sizeof (Point3f));

  if (n_components == 2)
    _cogl_matrix_transform_points_f2 (matrix,
                                      stride_in, points_in,
                                      stride_out, points_out,
                                      n_points);
  else
    {
      g_return_if_fail (n_components == 3);

      _cogl_matrix_transform_points_f3 (matrix,
                                        stride_in, points_in,
                                        stride_out, points_out,
                                        n_points);
    }
}

void
cogl_matrix_project_points (const CoglMatrix *matrix,
                            int n_components,
                            size_t stride_in,
                            const void *points_in,
                            size_t stride_out,
                            void *points_out,
                            int n_points)
{
  if (n_components == 2)
    _cogl_matrix_project_points_f2 (matrix,
                                    stride_in, points_in,
                                    stride_out, points_out,
                                    n_points);
  else if (n_components == 3)
    _cogl_matrix_project_points_f3 (matrix,
                                    stride_in, points_in,
                                    stride_out, points_out,
                                    n_points);
  else
    {
      g_return_if_fail (n_components == 4);

      _cogl_matrix_project_points_f4 (matrix,
                                      stride_in, points_in,
                                      stride_out, points_out,
                                      n_points);
    }
}
