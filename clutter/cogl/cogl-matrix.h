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
 * @short_description: Fuctions for initializing and manipulating 4x4
 *                     matrices.
 *
 * Matrices are used in Cogl to describe affine model-view transforms, texture
 * transforms, and projective transforms. This exposes a utility API that can
 * be used for direct manipulation of these matrices.
 */


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
 * <programlisting>
 * x_new = xx * x + xy * y + xz * z + xw * w
 * y_new = yx * x + yy * y + yz * z + yw * w
 * z_new = zx * x + zy * y + zz * z + zw * w
 * w_new = wx * x + wy * y + wz * z + ww * w
 * </programlisting>
 * Where w is normally 1
 *
 * Note: You must consider the members of the CoglMatrix structure read only,
 * and all matrix modifications must be done via the cogl_matrix API. This
 * allows Cogl to annotate the matrices internally. Violation of this will give
 * undefined results. If you need to initialize a matrix with a constant other
 * than the identity matrix you can use cogl_matrix_init_from_array().
 */
typedef struct _CoglMatrix {

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

    /*< private >*/

    /* Note: we may want to extend this later with private flags
     * and a cache of the inverse transform matrix. */
    float   _padding0[16];
    gulong  _padding1;
    gulong  _padding2;
    gulong  _padding3;
} CoglMatrix;

/**
 * cogl_matrix_init_identity:
 * @matrix: A 4x4 transformation matrix
 *
 * Resets matrix to the identity matrix:
 * <programlisting>
 * .xx=1; .xy=0; .xz=0; .xw=0;
 * .yx=0; .yy=1; .yz=0; .yw=0;
 * .zx=0; .zy=0; .zz=1; .zw=0;
 * .wx=0; .wy=0; .wz=0; .ww=1;
 * </programlisting>
 */
void cogl_matrix_init_identity (CoglMatrix *matrix);

/**
 * cogl_matrix_multiply:
 * @result: The address of a 4x4 matrix to store the result in
 * @a: A 4x4 transformation matrix
 * @b: A 4x4 transformation matrix
 *
 * This function multiples the two supplied matricies together and stores
 * the result in @result
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
 * This function multiples your matrix with a rotation matrix that applies
 * a rotation of #angle degrees around the specified 3D vector.
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
 * This function multiples your matrix with a transform matrix that translates
 * along the X, Y and Z axis.
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
 * This function multiples your matrix with a transform matrix that scales
 * along the X, Y and Z axis.
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
 * @near: positive distance to near depth clipping plane
 * @far: positive distance to far depth clipping plane
 *
 * Multiplies the matrix by the given frustum perspective matrix.
 *
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
 *          for the x axis.
 * @z_near: The distance to the near clip plane.
 *          Never pass 0 and always pass a positive number.
 * @z_far: The distance to the far clip plane. (Should always be positive)
 *
 * Multiplies the matrix by the described perspective matrix
 *
 * Note: you should be careful not to have to great a z_far / z_near ratio
 * since that will reduce the effectiveness of depth testing since there wont
 * be enough precision to identify the depth of objects near to each other.
 */
void
cogl_matrix_perspective (CoglMatrix *matrix,
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
 * @near: The coordinate for the near clipping plane (may be negative if
 *        the plane is behind the viewer)
 * @far: The coordinate for the far clipping plane (may be negative if
 *       the plane is behind the viewer)
 *
 * Multiples the matrix by a parallel projection matrix.
 */
void
cogl_matrix_ortho (CoglMatrix *matrix,
                   float       left,
                   float       right,
                   float       bottom,
                   float       top,
                   float       near,
                   float       far);

/**
 * cogl_matrix_init_from_array:
 * @matrix: A 4x4 transformation matrix
 * @array: A linear array of 16 floats (column-major order)
 *
 * This initialises @matrix with the contents of @array
 */
void cogl_matrix_init_from_array (CoglMatrix *matrix, const float *array);

/**
 * cogl_matrix_get_array:
 * @matrix: A 4x4 transformation matrix
 *
 * This casts a CoglMatrix to a float array which can be directly passed to
 * OpenGL.
 */
const float *cogl_matrix_get_array (const CoglMatrix *matrix);

/**
 * cogl_matrix_transform_point:
 * @matrix: A 4x4 transformation matrix
 * @x: The X component of your points position [in:out]
 * @y: The Y component of your points position [in:out]
 * @z: The Z component of your points position [in:out]
 * @w: The W component of your points position [in:out]
 *
 * This transforms a point whos position is given and returned
 * as four float components.
 */
void
cogl_matrix_transform_point (const CoglMatrix *matrix,
                             float *x,
                             float *y,
                             float *z,
                             float *w);

G_END_DECLS

#endif /* __COGL_MATRIX_H */

