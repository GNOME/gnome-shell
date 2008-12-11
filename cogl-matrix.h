#ifndef __COGL_MATRIX_H
#define __COGL_MATRIX_H

/* Note: This is ordered according to how OpenGL expects to get 4x4 matrices */
typedef struct {
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
} CoglMatrix;

/**
 * cogl_matrix_init_identity:
 * @matrix: A 4x4 transformation matrix
 *
 * Resets matrix to the identity matrix
 */
void cogl_matrix_init_identity (CoglMatrix *matrix);

/**
 * cogl_matrix_multiply:
 * @result: The address of a 4x4 matrix to store the result in
 * @a: A 4x4 transformation matrix
 * @b: A 4x4 transformation matrix
 *
 * This function multiples the two supplied matricies together and stores
 * the result in 'result'
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
 * a rotation of angle degrees around the specified 3D vector.
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

#endif /* __COGL_MATRIX_H */

