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

#ifndef __COGL_MATRIX_H
#define __COGL_MATRIX_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-matrix
 * @short_description: Fuctions for initializing and manipulating 4x4 matrices
 *
 * Matrices are used in Cogl to describe affine model-view transforms, texture
 * transforms, and projective transforms. This exposes a utility API that can
 * be used for direct manipulation of these matrices.
 */

typedef struct _CoglMatrix      CoglMatrix;

/**
 * CoglMatrix:
 *
 * A CoglMatrix holds a 4x4 transform matrix. This is a single precision,
 * column-major matrix which means it is compatible with what OpenGL expects.
 *
 * A CoglMatrix can represent transforms such as, rotations, scaling,
 * translation, sheering, and linear projections. You can combine these
 * transforms by multiplying multiple matrices in the order you want them
 * applied.
 *
 * The transformation of a vertex (x, y, z, w) by a CoglMatrix is given by:
 *
 * |[
 *   x_new = xx * x + xy * y + xz * z + xw * w
 *   y_new = yx * x + yy * y + yz * z + yw * w
 *   z_new = zx * x + zy * y + zz * z + zw * w
 *   w_new = wx * x + wy * y + wz * z + ww * w
 * ]|
 *
 * Where w is normally 1
 *
 * <note>You must consider the members of the CoglMatrix structure read only,
 * and all matrix modifications must be done via the cogl_matrix API. This
 * allows Cogl to annotate the matrices internally. Violation of this will give
 * undefined results. If you need to initialize a matrix with a constant other
 * than the identity matrix you can use cogl_matrix_init_from_array().</note>
 */
struct _CoglMatrix
{
    /*< private >*/

    /* column 0 */
    float xx;
    float yx;
    float zx;
    float wx;

    /* column 1 */
    float xy;
    float yy;
    float zy;
    float wy;

    /* column 2 */
    float xz;
    float yz;
    float zz;
    float wz;

    /* column 3 */
    float xw;
    float yw;
    float zw;
    float ww;

    /* Note: we may want to extend this later with private flags
     * and a cache of the inverse transform matrix. */
    float   inv[16];
    gulong  type;
    gulong  flags;
    gulong  _padding3;
};

/**
 * cogl_matrix_init_identity:
 * @matrix: A 4x4 transformation matrix
 *
 * Resets matrix to the identity matrix:
 *
 * |[
 *   .xx=1; .xy=0; .xz=0; .xw=0;
 *   .yx=0; .yy=1; .yz=0; .yw=0;
 *   .zx=0; .zy=0; .zz=1; .zw=0;
 *   .wx=0; .wy=0; .wz=0; .ww=1;
 * ]|
 */
void cogl_matrix_init_identity (CoglMatrix *matrix);

/**
 * cogl_matrix_multiply:
 * @result: The address of a 4x4 matrix to store the result in
 * @a: A 4x4 transformation matrix
 * @b: A 4x4 transformation matrix
 *
 * Multiplies the two supplied matrices together and stores
 * the resulting matrix inside @result
 */
void cogl_matrix_multiply (CoglMatrix *result,
			   const CoglMatrix *a,
			   const CoglMatrix *b);

/**
 * cogl_matrix_rotate:
 * @matrix: A 4x4 transformation matrix
 * @angle: The angle you want to rotate in degrees
 * @x: X component of your rotation vector
 * @y: Y component of your rotation vector
 * @z: Z component of your rotation vector
 *
 * Multiplies @matrix with a rotation matrix that applies a rotation
 * of @angle degrees around the specified 3D vector.
 */
void cogl_matrix_rotate (CoglMatrix *matrix,
			 float angle,
			 float x,
			 float y,
			 float z);

/* cogl_matrix_translate:
 * @matrix: A 4x4 transformation matrix
 * @x: The X translation you want to apply
 * @y: The Y translation you want to apply
 * @z: The Z translation you want to apply
 *
 * Multiplies @matrix with a transform matrix that translates along
 * the X, Y and Z axis.
 */
void cogl_matrix_translate (CoglMatrix *matrix,
			    float x,
			    float y,
			    float z);

/**
 * cogl_matrix_scale:
 * @matrix: A 4x4 transformation matrix
 * @sx: The X scale factor
 * @sy: The Y scale factor
 * @sz: The Z scale factor
 *
 * Multiplies @matrix with a transform matrix that scales along the X,
 * Y and Z axis.
 */
void cogl_matrix_scale (CoglMatrix *matrix,
			float sx,
			float sy,
			float sz);

/**
 * cogl_matrix_frustum:
 * @matrix: A 4x4 transformation matrix
 * @left: coord of left vertical clipping plane
 * @right: coord of right vertical clipping plane
 * @bottom: coord of bottom horizontal clipping plane
 * @top: coord of top horizontal clipping plane
 * @z_near: positive distance to near depth clipping plane
 * @z_far: positive distance to far depth clipping plane
 *
 * Multiplies @matrix by the given frustum perspective matrix.
 */
void cogl_matrix_frustum (CoglMatrix *matrix,
                          float       left,
                          float       right,
                          float       bottom,
                          float       top,
                          float       z_near,
                          float       z_far);

/**
 * cogl_matrix_perspective:
 * @matrix: A 4x4 transformation matrix
 * @fov_y: A field of view angle for the Y axis
 * @aspect: The ratio of width to height determining the field of view angle
 *   for the x axis.
 * @z_near: The distance to the near clip plane. Never pass 0 and always pass
 *   a positive number.
 * @z_far: The distance to the far clip plane. (Should always be positive)
 *
 * Multiplies @matrix by the described perspective matrix
 *
 * <note>You should be careful not to have to great a @z_far / @z_near ratio
 * since that will reduce the effectiveness of depth testing since there wont
 * be enough precision to identify the depth of objects near to each
 * other.</note>
 */
void cogl_matrix_perspective (CoglMatrix *matrix,
                              float       fov_y,
                              float       aspect,
                              float       z_near,
                              float       z_far);

/**
 * cogl_matrix_ortho:
 * @matrix: A 4x4 transformation matrix
 * @left: The coordinate for the left clipping plane
 * @right: The coordinate for the right clipping plane
 * @bottom: The coordinate for the bottom clipping plane
 * @top: The coordinate for the top clipping plane
 * @z_near: The coordinate for the near clipping plane (may be negative if
 *   the plane is behind the viewer)
 * @z_far: The coordinate for the far clipping plane (may be negative if
 *   the plane is behind the viewer)
 *
 * Multiplies @matrix by a parallel projection matrix.
 */
void cogl_matrix_ortho (CoglMatrix *matrix,
                        float       left,
                        float       right,
                        float       bottom,
                        float       top,
                        float       z_near,
                        float       z_far);

/**
 * cogl_matrix_init_from_array:
 * @matrix: A 4x4 transformation matrix
 * @array: A linear array of 16 floats (column-major order)
 *
 * Initializes @matrix with the contents of @array
 */
void cogl_matrix_init_from_array (CoglMatrix *matrix,
                                  const float *array);

/**
 * cogl_matrix_get_array:
 * @matrix: A 4x4 transformation matrix
 *
 * Casts @matrix to a float array which can be directly passed to OpenGL.
 *
 * Return value: a pointer to the float array
 */
G_CONST_RETURN float *cogl_matrix_get_array (const CoglMatrix *matrix);

/**
 * cogl_matrix_get_inverse:
 * @matrix: A 4x4 transformation matrix
 * @inverse: (out): The destination for a 4x4 inverse transformation matrix
 *
 * Gets the inverse transform of a given matrix and uses it to initialize
 * a new #CoglMatrix.
 *
 * <note>Although the first parameter is annotated as const to indicate
 * that the transform it represents isn't modified this function may
 * technically save a copy of the inverse transform within the given
 * #CoglMatrix so that subsequent requests for the inverse transform may
 * avoid costly inversion calculations.</note>
 *
 * Return value: %TRUE if the inverse was successfully calculated or %FALSE
 *   for degenerate transformations that can't be inverted (in this case the
 *   @inverse matrix will simply be initialized with the identity matrix)
 *
 * Since: 1.2
 */
gboolean cogl_matrix_get_inverse (const CoglMatrix *matrix,
                                  CoglMatrix *inverse);

/**
 * cogl_matrix_transform_point:
 * @matrix: A 4x4 transformation matrix
 * @x: (in-out): The X component of your points position
 * @y: (in-out): The Y component of your points position
 * @z: (in-out): The Z component of your points position
 * @w: (in-out): The W component of your points position
 *
 * Transforms a point whos position is given and returned as four float
 * components.
 */
void cogl_matrix_transform_point (const CoglMatrix *matrix,
                                  float *x,
                                  float *y,
                                  float *z,
                                  float *w);

G_END_DECLS

#endif /* __COGL_MATRIX_H */

