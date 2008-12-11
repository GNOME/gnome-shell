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

void cogl_matrix_init_identity (CoglMatrix *matrix);

void cogl_matrix_multiply (CoglMatrix *result,
			   const CoglMatrix *a,
			   const CoglMatrix *b);

void cogl_matrix_rotate (CoglMatrix *matrix,
			 float angle,
			 float x,
			 float y,
			 float z);

void cogl_matrix_translate (CoglMatrix *matrix,
			    float x,
			    float y,
			    float z);

void cogl_matrix_scale (CoglMatrix *matrix,
			float sx,
			float sy,
			float sz);

#endif /* __COGL_MATRIX_H */

