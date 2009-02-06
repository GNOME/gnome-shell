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
 * the result in #result
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

G_END_DECLS

#endif /* __COGL_MATRIX_H */

