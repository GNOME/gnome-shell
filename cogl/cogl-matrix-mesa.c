/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 */
/*
 * Copyright (C) 1999-2005  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/*
 * file: cogl-matrix-mesa.c
 * Matrix operations.
 *
 * note
 * -# 4x4 transformation matrices are stored in memory in column major order.
 * -# Points/vertices are to be thought of as column vectors.
 * -# Transformation of a point p by a matrix M is: p' = M * p
 */

/*
 * Changes compared to the original code from Mesa:
 *
 * - instead of allocating matrix->m and matrix->inv using malloc, our
 *   public CoglMatrix typedef is large enough to directly contain the
 *   matrix, its inverse, a type and a set of flags.
 * - instead of having a _math_matrix_analyse which updates the type,
 *   flags and inverse, we have _math_matrix_update_inverse which
 *   essentially does the same thing (internally making use of
 *   _math_matrix_update_type_and_flags()) but with additional guards in
 *   place to bail out when the inverse matrix is still valid.
 * - when initializing a matrix with the identity matrix we don't
 *   immediately initialize the inverse matrix; rather we just set the
 *   dirty flag for the inverse (since it's likely the user won't request
 *   the inverse of the identity matrix)
 */

#include "cogl-matrix-mesa.h"

#include <string.h>
#include <math.h>


#define DEG2RAD (G_PI/180.0)

/* Dot product of two 2-element vectors */
#define DOT2(A,B)  ( (A)[0]*(B)[0] + (A)[1]*(B)[1] )

/* Dot product of two 3-element vectors */
#define DOT3(A,B)  ( (A)[0]*(B)[0] + (A)[1]*(B)[1] + (A)[2]*(B)[2] )

#define CROSS3(N, U, V) \
do { \
    (N)[0] = (U)[1]*(V)[2] - (U)[2]*(V)[1]; \
    (N)[1] = (U)[2]*(V)[0] - (U)[0]*(V)[2]; \
    (N)[2] = (U)[0]*(V)[1] - (U)[1]*(V)[0]; \
} while (0)

#define SUB_3V(DST, SRCA, SRCB) \
do { \
    (DST)[0] = (SRCA)[0] - (SRCB)[0]; \
    (DST)[1] = (SRCA)[1] - (SRCB)[1]; \
    (DST)[2] = (SRCA)[2] - (SRCB)[2]; \
} while (0)

#define LEN_SQUARED_3FV( V ) ((V)[0]*(V)[0]+(V)[1]*(V)[1]+(V)[2]*(V)[2])

/*
 * \defgroup MatFlags MAT_FLAG_XXX-flags
 *
 * Bitmasks to indicate different kinds of 4x4 matrices in CoglMatrix::flags
 */
/*@{*/
#define MAT_FLAG_IDENTITY       0     /*< is an identity matrix flag.
                                       *   (Not actually used - the identity
                                       *   matrix is identified by the absense
                                       *   of all other flags.)
                                       */
#define MAT_FLAG_GENERAL        0x1   /*< is a general matrix flag */
#define MAT_FLAG_ROTATION       0x2   /*< is a rotation matrix flag */
#define MAT_FLAG_TRANSLATION    0x4   /*< is a translation matrix flag */
#define MAT_FLAG_UNIFORM_SCALE  0x8   /*< is an uniform scaling matrix flag */
#define MAT_FLAG_GENERAL_SCALE  0x10  /*< is a general scaling matrix flag */
#define MAT_FLAG_GENERAL_3D     0x20  /*< general 3D matrix flag */
#define MAT_FLAG_PERSPECTIVE    0x40  /*< is a perspective proj matrix flag */
#define MAT_FLAG_SINGULAR       0x80  /*< is a singular matrix flag */
#define MAT_DIRTY_TYPE          0x100  /*< matrix type is dirty */
#define MAT_DIRTY_FLAGS         0x200  /*< matrix flags are dirty */
#define MAT_DIRTY_INVERSE       0x400  /*< matrix inverse is dirty */

/* angle preserving matrix flags mask */
#define MAT_FLAGS_ANGLE_PRESERVING (MAT_FLAG_ROTATION | \
				    MAT_FLAG_TRANSLATION | \
				    MAT_FLAG_UNIFORM_SCALE)

/* geometry related matrix flags mask */
#define MAT_FLAGS_GEOMETRY (MAT_FLAG_GENERAL | \
			    MAT_FLAG_ROTATION | \
			    MAT_FLAG_TRANSLATION | \
			    MAT_FLAG_UNIFORM_SCALE | \
			    MAT_FLAG_GENERAL_SCALE | \
			    MAT_FLAG_GENERAL_3D | \
			    MAT_FLAG_PERSPECTIVE | \
	                    MAT_FLAG_SINGULAR)

/* length preserving matrix flags mask */
#define MAT_FLAGS_LENGTH_PRESERVING (MAT_FLAG_ROTATION | \
				     MAT_FLAG_TRANSLATION)


/* 3D (non-perspective) matrix flags mask */
#define MAT_FLAGS_3D (MAT_FLAG_ROTATION | \
		      MAT_FLAG_TRANSLATION | \
		      MAT_FLAG_UNIFORM_SCALE | \
		      MAT_FLAG_GENERAL_SCALE | \
		      MAT_FLAG_GENERAL_3D)

/* dirty matrix flags mask */
#define MAT_DIRTY_ALL      (MAT_DIRTY_TYPE | \
			    MAT_DIRTY_FLAGS | \
			    MAT_DIRTY_INVERSE)

/*@}*/


/*
 * Test geometry related matrix flags.
 *
 * \param mat a pointer to a CoglMatrix structure.
 * \param a flags mask.
 *
 * \returns non-zero if all geometry related matrix flags are contained within
 * the mask, or zero otherwise.
 */
#define TEST_MAT_FLAGS(mat, a)  \
    ((MAT_FLAGS_GEOMETRY & (~(a)) & ((mat)->flags) ) == 0)



/*
 * Names of the corresponding CoglMatrixType values.
 */
static const char *types[] = {
   "COGL_MATRIX_TYPE_GENERAL",
   "COGL_MATRIX_TYPE_IDENTITY",
   "COGL_MATRIX_TYPE_3D_NO_ROT",
   "COGL_MATRIX_TYPE_PERSPECTIVE",
   "COGL_MATRIX_TYPE_2D",
   "COGL_MATRIX_TYPE_2D_NO_ROT",
   "COGL_MATRIX_TYPE_3D"
};


/*
 * Identity matrix.
 */
static float identity[16] = {
   1.0, 0.0, 0.0, 0.0,
   0.0, 1.0, 0.0, 0.0,
   0.0, 0.0, 1.0, 0.0,
   0.0, 0.0, 0.0, 1.0
};



/**********************************************************************/
/* \name Matrix multiplication */
/*@{*/

#define A(row,col)  a[(col<<2)+row]
#define B(row,col)  b[(col<<2)+row]
#define R(row,col)  result[(col<<2)+row]

/*
 * Perform a full 4x4 matrix multiplication.
 *
 * \param a matrix.
 * \param b matrix.
 * \param product will receive the product of \p a and \p b.
 *
 * \warning Is assumed that \p product != \p b. \p product == \p a is allowed.
 *
 * \note KW: 4*16 = 64 multiplications
 *
 * \author This \c matmul was contributed by Thomas Malik
 */
static void
matrix_multiply4x4 (float *result, const float *a, const float *b)
{
  int i;
  for (i = 0; i < 4; i++)
    {
      const float ai0 = A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
      R(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
      R(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
      R(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
      R(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
    }
}

/*
 * Multiply two matrices known to occupy only the top three rows, such
 * as typical model matrices, and orthogonal matrices.
 *
 * \param a matrix.
 * \param b matrix.
 * \param product will receive the product of \p a and \p b.
 */
static void
matrix_multiply3x4 (float *result, const float *a, const float *b)
{
  int i;
  for (i = 0; i < 3; i++)
    {
      const float ai0 = A(i,0), ai1 = A(i,1), ai2 = A(i,2), ai3 = A(i,3);
      R(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0);
      R(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1);
      R(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2);
      R(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3;
    }
  R(3,0) = 0;
  R(3,1) = 0;
  R(3,2) = 0;
  R(3,3) = 1;
}

#undef A
#undef B
#undef R

/*
 * Multiply a matrix by an array of floats with known properties.
 *
 * \param mat pointer to a CoglMatrix structure containing the left multiplication
 * matrix, and that will receive the product result.
 * \param m right multiplication matrix array.
 * \param flags flags of the matrix \p m.
 *
 * Joins both flags and marks the type and inverse as dirty.  Calls
 * matrix_multiply3x4() if both matrices are 3D, or matrix_multiply4x4()
 * otherwise.
 */
static void
matrix_multiply_array_with_flags (CoglMatrix *result,
                                  const float *array,
                                  unsigned int flags)
{
  result->flags |= (flags | MAT_DIRTY_TYPE | MAT_DIRTY_INVERSE);

  if (TEST_MAT_FLAGS (result, MAT_FLAGS_3D))
    matrix_multiply3x4 ((float *)result, (float *)result, array);
  else
    matrix_multiply4x4 ((float *)result, (float *)result, array);
}

/*
 * Matrix multiplication.
 *
 * \param dest destination matrix.
 * \param a left matrix.
 * \param b right matrix.
 *
 * Joins both flags and marks the type and inverse as dirty.  Calls
 * matrix_multiply3x4() if both matrices are 3D, or matrix_multiply4x4()
 * otherwise.
 */
void
_math_matrix_multiply (CoglMatrix *result,
                       const CoglMatrix *a,
                       const CoglMatrix *b)
{
  result->flags = (a->flags |
                   b->flags |
                   MAT_DIRTY_TYPE |
                   MAT_DIRTY_INVERSE);

  if (TEST_MAT_FLAGS(result, MAT_FLAGS_3D))
    matrix_multiply3x4 ((float *)result, (float *)a, (float *)b);
  else
    matrix_multiply4x4 ((float *)result, (float *)a, (float *)b);
}

/*
 * Matrix multiplication.
 *
 * \param dest left and destination matrix.
 * \param m right matrix array.
 *
 * Marks the matrix flags with general flag, and type and inverse dirty flags.
 * Calls matrix_multiply4x4() for the multiplication.
 */
void
_math_matrix_multiply_array (CoglMatrix *result, const float *array)
{
  result->flags |= (MAT_FLAG_GENERAL |
                  MAT_DIRTY_TYPE |
                  MAT_DIRTY_INVERSE |
                  MAT_DIRTY_FLAGS);

  matrix_multiply4x4 ((float *)result, (float *)result, (float *)array);
}

/*@}*/


/**********************************************************************/
/* \name Matrix output */
/*@{*/

/*
 * Print a matrix array.
 *
 * \param m matrix array.
 *
 * Called by _math_matrix_print() to print a matrix or its inverse.
 */
static void
print_matrix_floats (const float m[16])
{
  int i;
  for (i = 0;i < 4; i++)
    g_print ("\t%f %f %f %f\n", m[i], m[4+i], m[8+i], m[12+i] );
}

/*
 * Dumps the contents of a CoglMatrix structure.
 *
 * \param m pointer to the CoglMatrix structure.
 */
void
_math_matrix_print (const CoglMatrix *matrix)
{
  g_print ("Matrix type: %s, flags: %x\n",
           types[matrix->type], (int)matrix->flags);
  print_matrix_floats ((float *)matrix);
  g_print ("Inverse: \n");
  if (!(matrix->flags & MAT_DIRTY_INVERSE))
    {
      float prod[16];
      print_matrix_floats (matrix->inv);
      matrix_multiply4x4 (prod, (float *)matrix, matrix->inv);
      g_print ("Mat * Inverse:\n");
      print_matrix_floats (prod);
    }
  else
    g_print ("  - not available\n");
}

/*@}*/


/*
 * References an element of 4x4 matrix.
 *
 * \param m matrix array.
 * \param c column of the desired element.
 * \param r row of the desired element.
 *
 * \return value of the desired element.
 *
 * Calculate the linear storage index of the element and references it.
 */
#define MAT(m,r,c) (m)[(c)*4+(r)]


/**********************************************************************/
/* \name Matrix inversion */
/*@{*/

/*
 * Swaps the values of two floating pointer variables.
 *
 * Used by invert_matrix_general() to swap the row pointers.
 */
#define SWAP_ROWS(a, b) { float *_tmp = a; (a)=(b); (b)=_tmp; }

/*
 * Compute inverse of 4x4 transformation matrix.
 *
 * \param mat pointer to a CoglMatrix structure. The matrix inverse will be
 * stored in the CoglMatrix::inv attribute.
 *
 * \return TRUE for success, FALSE for failure (\p singular matrix).
 *
 * \author
 * Code contributed by Jacques Leroy jle@star.be
 *
 * Calculates the inverse matrix by performing the gaussian matrix reduction
 * with partial pivoting followed by back/substitution with the loops manually
 * unrolled.
 */
static gboolean
invert_matrix_general (CoglMatrix *matrix)
{
  const float *m = (float *)matrix;
  float *out = matrix->inv;
  float wtmp[4][8];
  float m0, m1, m2, m3, s;
  float *r0, *r1, *r2, *r3;

  r0 = wtmp[0], r1 = wtmp[1], r2 = wtmp[2], r3 = wtmp[3];

  r0[0] = MAT (m, 0, 0), r0[1] = MAT (m, 0, 1),
    r0[2] = MAT (m, 0, 2), r0[3] = MAT (m, 0, 3),
    r0[4] = 1.0, r0[5] = r0[6] = r0[7] = 0.0,

    r1[0] = MAT (m, 1, 0), r1[1] = MAT (m, 1, 1),
    r1[2] = MAT (m, 1, 2), r1[3] = MAT (m, 1, 3),
    r1[5] = 1.0, r1[4] = r1[6] = r1[7] = 0.0,

    r2[0] = MAT (m, 2, 0), r2[1] = MAT (m, 2, 1),
    r2[2] = MAT (m, 2, 2), r2[3] = MAT (m, 2, 3),
    r2[6] = 1.0, r2[4] = r2[5] = r2[7] = 0.0,

    r3[0] = MAT (m, 3, 0), r3[1] = MAT (m, 3, 1),
    r3[2] = MAT (m, 3, 2), r3[3] = MAT (m, 3, 3),
    r3[7] = 1.0, r3[4] = r3[5] = r3[6] = 0.0;

  /* choose pivot - or die */
  if (fabsf (r3[0]) > fabsf (r2[0]))
    SWAP_ROWS (r3, r2);
  if (fabsf (r2[0]) > fabsf (r1[0]))
    SWAP_ROWS (r2, r1);
  if (fabsf (r1[0]) > fabsf (r0[0]))
    SWAP_ROWS (r1, r0);
  if (0.0 == r0[0])
    return FALSE;

  /* eliminate first variable     */
  m1 = r1[0]/r0[0]; m2 = r2[0]/r0[0]; m3 = r3[0]/r0[0];
  s = r0[1]; r1[1] -= m1 * s; r2[1] -= m2 * s; r3[1] -= m3 * s;
  s = r0[2]; r1[2] -= m1 * s; r2[2] -= m2 * s; r3[2] -= m3 * s;
  s = r0[3]; r1[3] -= m1 * s; r2[3] -= m2 * s; r3[3] -= m3 * s;
  s = r0[4];
  if (s != 0.0) { r1[4] -= m1 * s; r2[4] -= m2 * s; r3[4] -= m3 * s; }
  s = r0[5];
  if (s != 0.0) { r1[5] -= m1 * s; r2[5] -= m2 * s; r3[5] -= m3 * s; }
  s = r0[6];
  if (s != 0.0) { r1[6] -= m1 * s; r2[6] -= m2 * s; r3[6] -= m3 * s; }
  s = r0[7];
  if (s != 0.0) { r1[7] -= m1 * s; r2[7] -= m2 * s; r3[7] -= m3 * s; }

  /* choose pivot - or die */
  if (fabsf (r3[1]) > fabsf (r2[1]))
    SWAP_ROWS (r3, r2);
  if (fabsf (r2[1]) > fabsf (r1[1]))
    SWAP_ROWS (r2, r1);
  if (0.0 == r1[1])
    return FALSE;

  /* eliminate second variable */
  m2 = r2[1] / r1[1]; m3 = r3[1] / r1[1];
  r2[2] -= m2 * r1[2]; r3[2] -= m3 * r1[2];
  r2[3] -= m2 * r1[3]; r3[3] -= m3 * r1[3];
  s = r1[4]; if (0.0 != s) { r2[4] -= m2 * s; r3[4] -= m3 * s; }
  s = r1[5]; if (0.0 != s) { r2[5] -= m2 * s; r3[5] -= m3 * s; }
  s = r1[6]; if (0.0 != s) { r2[6] -= m2 * s; r3[6] -= m3 * s; }
  s = r1[7]; if (0.0 != s) { r2[7] -= m2 * s; r3[7] -= m3 * s; }

  /* choose pivot - or die */
  if (fabsf (r3[2]) > fabsf (r2[2]))
    SWAP_ROWS (r3, r2);
  if (0.0 == r2[2])
    return FALSE;

  /* eliminate third variable */
  m3 = r3[2] / r2[2];
  r3[3] -= m3 * r2[3], r3[4] -= m3 * r2[4],
    r3[5] -= m3 * r2[5], r3[6] -= m3 * r2[6],
    r3[7] -= m3 * r2[7];

  /* last check */
  if (0.0 == r3[3])
    return FALSE;

  s = 1.0f / r3[3];             /* now back substitute row 3 */
  r3[4] *= s; r3[5] *= s; r3[6] *= s; r3[7] *= s;

  m2 = r2[3];                 /* now back substitute row 2 */
  s  = 1.0f / r2[2];
  r2[4] = s * (r2[4] - r3[4] * m2), r2[5] = s * (r2[5] - r3[5] * m2),
    r2[6] = s * (r2[6] - r3[6] * m2), r2[7] = s * (r2[7] - r3[7] * m2);
  m1 = r1[3];
  r1[4] -= r3[4] * m1, r1[5] -= r3[5] * m1,
    r1[6] -= r3[6] * m1, r1[7] -= r3[7] * m1;
  m0 = r0[3];
  r0[4] -= r3[4] * m0, r0[5] -= r3[5] * m0,
    r0[6] -= r3[6] * m0, r0[7] -= r3[7] * m0;

  m1 = r1[2];                 /* now back substitute row 1 */
  s  = 1.0f / r1[1];
  r1[4] = s * (r1[4] - r2[4] * m1), r1[5] = s * (r1[5] - r2[5] * m1),
    r1[6] = s * (r1[6] - r2[6] * m1), r1[7] = s * (r1[7] - r2[7] * m1);
  m0 = r0[2];
  r0[4] -= r2[4] * m0, r0[5] -= r2[5] * m0,
    r0[6] -= r2[6] * m0, r0[7] -= r2[7] * m0;

  m0 = r0[1];                 /* now back substitute row 0 */
  s  = 1.0f / r0[0];
  r0[4] = s * (r0[4] - r1[4] * m0), r0[5] = s * (r0[5] - r1[5] * m0),
    r0[6] = s * (r0[6] - r1[6] * m0), r0[7] = s * (r0[7] - r1[7] * m0);

  MAT (out, 0, 0) = r0[4]; MAT (out, 0, 1) = r0[5],
    MAT (out, 0, 2) = r0[6]; MAT (out, 0, 3) = r0[7],
    MAT (out, 1, 0) = r1[4]; MAT (out, 1, 1) = r1[5],
    MAT (out, 1, 2) = r1[6]; MAT (out, 1, 3) = r1[7],
    MAT (out, 2, 0) = r2[4]; MAT (out, 2, 1) = r2[5],
    MAT (out, 2, 2) = r2[6]; MAT (out, 2, 3) = r2[7],
    MAT (out, 3, 0) = r3[4]; MAT (out, 3, 1) = r3[5],
    MAT (out, 3, 2) = r3[6]; MAT (out, 3, 3) = r3[7];

  return TRUE;
}
#undef SWAP_ROWS

/*
 * Compute inverse of a general 3d transformation matrix.
 *
 * \param mat pointer to a CoglMatrix structure. The matrix inverse will be
 * stored in the CoglMatrix::inv attribute.
 *
 * \return TRUE for success, FALSE for failure (\p singular matrix).
 *
 * \author Adapted from graphics gems II.
 *
 * Calculates the inverse of the upper left by first calculating its
 * determinant and multiplying it to the symmetric adjust matrix of each
 * element. Finally deals with the translation part by transforming the
 * original translation vector using by the calculated submatrix inverse.
 */
static gboolean
invert_matrix_3d_general (CoglMatrix *matrix)
{
  const float *in = (float *)matrix;
  float *out = matrix->inv;
  float pos, neg, t;
  float det;

  /* Calculate the determinant of upper left 3x3 submatrix and
   * determine if the matrix is singular.
   */
  pos = neg = 0.0;
  t =  MAT (in,0,0) * MAT (in,1,1) * MAT (in,2,2);
  if (t >= 0.0) pos += t; else neg += t;

  t =  MAT (in,1,0) * MAT (in,2,1) * MAT (in,0,2);
  if (t >= 0.0) pos += t; else neg += t;

  t =  MAT (in,2,0) * MAT (in,0,1) * MAT (in,1,2);
  if (t >= 0.0) pos += t; else neg += t;

  t = -MAT (in,2,0) * MAT (in,1,1) * MAT (in,0,2);
  if (t >= 0.0) pos += t; else neg += t;

  t = -MAT (in,1,0) * MAT (in,0,1) * MAT (in,2,2);
  if (t >= 0.0) pos += t; else neg += t;

  t = -MAT (in,0,0) * MAT (in,2,1) * MAT (in,1,2);
  if (t >= 0.0) pos += t; else neg += t;

  det = pos + neg;

  if (det*det < 1e-25)
    return FALSE;

  det = 1.0f / det;
  MAT (out,0,0) =
    (  (MAT (in, 1, 1)*MAT (in, 2, 2) - MAT (in, 2, 1)*MAT (in, 1, 2) )*det);
  MAT (out,0,1) =
    (- (MAT (in, 0, 1)*MAT (in, 2, 2) - MAT (in, 2, 1)*MAT (in, 0, 2) )*det);
  MAT (out,0,2) =
    (  (MAT (in, 0, 1)*MAT (in, 1, 2) - MAT (in, 1, 1)*MAT (in, 0, 2) )*det);
  MAT (out,1,0) =
    (- (MAT (in,1,0)*MAT (in,2,2) - MAT (in,2,0)*MAT (in,1,2) )*det);
  MAT (out,1,1) =
    (  (MAT (in,0,0)*MAT (in,2,2) - MAT (in,2,0)*MAT (in,0,2) )*det);
  MAT (out,1,2) =
    (- (MAT (in,0,0)*MAT (in,1,2) - MAT (in,1,0)*MAT (in,0,2) )*det);
  MAT (out,2,0) =
    (  (MAT (in,1,0)*MAT (in,2,1) - MAT (in,2,0)*MAT (in,1,1) )*det);
  MAT (out,2,1) =
    (- (MAT (in,0,0)*MAT (in,2,1) - MAT (in,2,0)*MAT (in,0,1) )*det);
  MAT (out,2,2) =
    (  (MAT (in,0,0)*MAT (in,1,1) - MAT (in,1,0)*MAT (in,0,1) )*det);

  /* Do the translation part */
  MAT (out,0,3) = - (MAT (in, 0, 3) * MAT (out, 0, 0) +
                    MAT (in, 1, 3) * MAT (out, 0, 1) +
                    MAT (in, 2, 3) * MAT (out, 0, 2) );
  MAT (out,1,3) = - (MAT (in, 0, 3) * MAT (out, 1, 0) +
                    MAT (in, 1, 3) * MAT (out, 1, 1) +
                    MAT (in, 2, 3) * MAT (out, 1, 2) );
  MAT (out,2,3) = - (MAT (in, 0, 3) * MAT (out, 2 ,0) +
                    MAT (in, 1, 3) * MAT (out, 2, 1) +
                    MAT (in, 2, 3) * MAT (out, 2, 2) );

  return TRUE;
}

/*
 * Compute inverse of a 3d transformation matrix.
 *
 * \param mat pointer to a CoglMatrix structure. The matrix inverse will be
 * stored in the CoglMatrix::inv attribute.
 *
 * \return TRUE for success, FALSE for failure (\p singular matrix).
 *
 * If the matrix is not an angle preserving matrix then calls
 * invert_matrix_3d_general for the actual calculation. Otherwise calculates
 * the inverse matrix analyzing and inverting each of the scaling, rotation and
 * translation parts.
 */
static gboolean
invert_matrix_3d (CoglMatrix *matrix)
{
  const float *in = (float *)matrix;
  float *out = matrix->inv;

  if (!TEST_MAT_FLAGS(matrix, MAT_FLAGS_ANGLE_PRESERVING))
    return invert_matrix_3d_general (matrix);

  if (matrix->flags & MAT_FLAG_UNIFORM_SCALE)
    {
      float scale = (MAT (in, 0, 0) * MAT (in, 0, 0) +
                     MAT (in, 0, 1) * MAT (in, 0, 1) +
                     MAT (in, 0, 2) * MAT (in, 0, 2));

      if (scale == 0.0)
        return FALSE;

      scale = 1.0f / scale;

      /* Transpose and scale the 3 by 3 upper-left submatrix. */
      MAT (out, 0, 0) = scale * MAT (in, 0, 0);
      MAT (out, 1, 0) = scale * MAT (in, 0, 1);
      MAT (out, 2, 0) = scale * MAT (in, 0, 2);
      MAT (out, 0, 1) = scale * MAT (in, 1, 0);
      MAT (out, 1, 1) = scale * MAT (in, 1, 1);
      MAT (out, 2, 1) = scale * MAT (in, 1, 2);
      MAT (out, 0, 2) = scale * MAT (in, 2, 0);
      MAT (out, 1, 2) = scale * MAT (in, 2, 1);
      MAT (out, 2, 2) = scale * MAT (in, 2, 2);
    }
  else if (matrix->flags & MAT_FLAG_ROTATION)
    {
      /* Transpose the 3 by 3 upper-left submatrix. */
      MAT (out, 0, 0) = MAT (in, 0, 0);
      MAT (out, 1, 0) = MAT (in, 0, 1);
      MAT (out, 2, 0) = MAT (in, 0, 2);
      MAT (out, 0, 1) = MAT (in, 1, 0);
      MAT (out, 1, 1) = MAT (in, 1, 1);
      MAT (out, 2, 1) = MAT (in, 1, 2);
      MAT (out, 0, 2) = MAT (in, 2, 0);
      MAT (out, 1, 2) = MAT (in, 2, 1);
      MAT (out, 2, 2) = MAT (in, 2, 2);
    }
  else
    {
      /* pure translation */
      memcpy (out, identity, 16 * sizeof (float));
      MAT (out, 0, 3) = - MAT (in, 0, 3);
      MAT (out, 1, 3) = - MAT (in, 1, 3);
      MAT (out, 2, 3) = - MAT (in, 2, 3);
      return TRUE;
    }

  if (matrix->flags & MAT_FLAG_TRANSLATION)
    {
      /* Do the translation part */
      MAT (out,0,3) = - (MAT (in, 0, 3) * MAT (out, 0, 0) +
                        MAT (in, 1, 3) * MAT (out, 0, 1) +
                        MAT (in, 2, 3) * MAT (out, 0, 2) );
      MAT (out,1,3) = - (MAT (in, 0, 3) * MAT (out, 1, 0) +
                        MAT (in, 1, 3) * MAT (out, 1, 1) +
                        MAT (in, 2, 3) * MAT (out, 1, 2) );
      MAT (out,2,3) = - (MAT (in, 0, 3) * MAT (out, 2, 0) +
                        MAT (in, 1, 3) * MAT (out, 2, 1) +
                        MAT (in, 2, 3) * MAT (out, 2, 2) );
    }
  else
    MAT (out, 0, 3) = MAT (out, 1, 3) = MAT (out, 2, 3) = 0.0;

  return TRUE;
}

/*
 * Compute inverse of an identity transformation matrix.
 *
 * \param mat pointer to a CoglMatrix structure. The matrix inverse will be
 * stored in the CoglMatrix::inv attribute.
 *
 * \return always TRUE.
 *
 * Simply copies identity into CoglMatrix::inv.
 */
static gboolean
invert_matrix_identity (CoglMatrix *matrix)
{
  memcpy (matrix->inv, identity, 16 * sizeof (float));
  return TRUE;
}

/*
 * Compute inverse of a no-rotation 3d transformation matrix.
 *
 * \param mat pointer to a CoglMatrix structure. The matrix inverse will be
 * stored in the CoglMatrix::inv attribute.
 *
 * \return TRUE for success, FALSE for failure (\p singular matrix).
 *
 * Calculates the
 */
static gboolean
invert_matrix_3d_no_rotation (CoglMatrix *matrix)
{
  const float *in = (float *)matrix;
  float *out = matrix->inv;

  if (MAT (in,0,0) == 0 || MAT (in,1,1) == 0 || MAT (in,2,2) == 0)
    return FALSE;

  memcpy (out, identity, 16 * sizeof (float));
  MAT (out,0,0) = 1.0f / MAT (in,0,0);
  MAT (out,1,1) = 1.0f / MAT (in,1,1);
  MAT (out,2,2) = 1.0f / MAT (in,2,2);

  if (matrix->flags & MAT_FLAG_TRANSLATION)
    {
      MAT (out,0,3) = - (MAT (in,0,3) * MAT (out,0,0));
      MAT (out,1,3) = - (MAT (in,1,3) * MAT (out,1,1));
      MAT (out,2,3) = - (MAT (in,2,3) * MAT (out,2,2));
    }

  return TRUE;
}

/*
 * Compute inverse of a no-rotation 2d transformation matrix.
 *
 * \param mat pointer to a CoglMatrix structure. The matrix inverse will be
 * stored in the CoglMatrix::inv attribute.
 *
 * \return TRUE for success, FALSE for failure (\p singular matrix).
 *
 * Calculates the inverse matrix by applying the inverse scaling and
 * translation to the identity matrix.
 */
static gboolean
invert_matrix_2d_no_rotation (CoglMatrix *matrix)
{
  const float *in = (float *)matrix;
  float *out = matrix->inv;

  if (MAT (in, 0, 0) == 0 || MAT (in, 1, 1) == 0)
    return FALSE;

  memcpy (out, identity, 16 * sizeof (float));
  MAT (out, 0, 0) = 1.0f / MAT (in, 0, 0);
  MAT (out, 1, 1) = 1.0f / MAT (in, 1, 1);

  if (matrix->flags & MAT_FLAG_TRANSLATION)
    {
      MAT (out, 0, 3) = - (MAT (in, 0, 3) * MAT (out, 0, 0));
      MAT (out, 1, 3) = - (MAT (in, 1, 3) * MAT (out, 1, 1));
    }

  return TRUE;
}

#if 0
/* broken */
static gboolean
invert_matrix_perspective (CoglMatrix *matrix)
{
  const float *in = matrix;
  float *out = matrix->inv;

  if (MAT (in,2,3) == 0)
    return FALSE;

  memcpy( out, identity, 16 * sizeof(float) );

  MAT (out, 0, 0) = 1.0f / MAT (in, 0, 0);
  MAT (out, 1, 1) = 1.0f / MAT (in, 1, 1);

  MAT (out, 0, 3) = MAT (in, 0, 2);
  MAT (out, 1, 3) = MAT (in, 1, 2);

  MAT (out,2,2) = 0;
  MAT (out,2,3) = -1;

  MAT (out,3,2) = 1.0f / MAT (in,2,3);
  MAT (out,3,3) = MAT (in,2,2) * MAT (out,3,2);

  return TRUE;
}
#endif

/*
 * Matrix inversion function pointer type.
 */
typedef gboolean (*inv_mat_func)(CoglMatrix *matrix);

/*
 * Table of the matrix inversion functions according to the matrix type.
 */
static inv_mat_func inv_mat_tab[7] = {
    invert_matrix_general,
    invert_matrix_identity,
    invert_matrix_3d_no_rotation,
#if 0
    /* Don't use this function for now - it fails when the projection matrix
     * is premultiplied by a translation (ala Chromium's tilesort SPU).
     */
    invert_matrix_perspective,
#else
    invert_matrix_general,
#endif
    invert_matrix_3d,		/* lazy! */
    invert_matrix_2d_no_rotation,
    invert_matrix_3d
};

/*
 * Compute inverse of a transformation matrix.
 *
 * \param mat pointer to a CoglMatrix structure. The matrix inverse will be
 * stored in the CoglMatrix::inv attribute.
 *
 * \return TRUE for success, FALSE for failure (\p singular matrix).
 *
 * Calls the matrix inversion function in inv_mat_tab corresponding to the
 * given matrix type.  In case of failure, updates the MAT_FLAG_SINGULAR flag,
 * and copies the identity matrix into CoglMatrix::inv.
 */
gboolean
_math_matrix_update_inverse (CoglMatrix *matrix)
{
  if (matrix->flags & MAT_DIRTY_FLAGS ||
      matrix->flags & MAT_DIRTY_INVERSE)
    {
      _math_matrix_update_type_and_flags (matrix);

      if (inv_mat_tab[matrix->type](matrix))
        matrix->flags &= ~MAT_FLAG_SINGULAR;
      else
        {
          matrix->flags |= MAT_FLAG_SINGULAR;
          memcpy (matrix->inv, identity, 16 * sizeof (float));
        }

      matrix->flags &= ~MAT_DIRTY_INVERSE;
    }

  if (matrix->flags & MAT_FLAG_SINGULAR)
    return FALSE;
  else
    return TRUE;
}

/*@}*/


/**********************************************************************/
/* \name Matrix generation */
/*@{*/

/*
 * Generate a 4x4 transformation matrix from glRotate parameters, and
 * post-multiply the input matrix by it.
 *
 * \author
 * This function was contributed by Erich Boleyn (erich@uruk.org).
 * Optimizations contributed by Rudolf Opalla (rudi@khm.de).
 */
void
_math_matrix_rotate (CoglMatrix *matrix,
                     float angle,
                     float x,
                     float y,
                     float z)
{
  float xx, yy, zz, xy, yz, zx, xs, ys, zs, one_c, s, c;
  float m[16];
  gboolean optimized;

  s = sinf (angle * DEG2RAD);
  c = cosf (angle * DEG2RAD);

  memcpy (m, identity, 16 * sizeof (float));
  optimized = FALSE;

#define M(row,col)  m[col*4+row]

  if (x == 0.0f)
    {
      if (y == 0.0f)
        {
          if (z != 0.0f)
            {
              optimized = TRUE;
              /* rotate only around z-axis */
              M (0,0) = c;
              M (1,1) = c;
              if (z < 0.0f)
                {
                  M (0,1) = s;
                  M (1,0) = -s;
                }
              else
                {
                  M (0,1) = -s;
                  M (1,0) = s;
                }
            }
        }
      else if (z == 0.0f)
        {
          optimized = TRUE;
          /* rotate only around y-axis */
          M (0,0) = c;
          M (2,2) = c;
          if (y < 0.0f)
            {
              M (0,2) = -s;
              M (2,0) = s;
            }
          else
            {
              M (0,2) = s;
              M (2,0) = -s;
            }
        }
    }
  else if (y == 0.0f)
    {
      if (z == 0.0f)
        {
          optimized = TRUE;
          /* rotate only around x-axis */
          M (1,1) = c;
          M (2,2) = c;
          if (x < 0.0f)
            {
              M (1,2) = s;
              M (2,1) = -s;
            }
          else
            {
              M (1,2) = -s;
              M (2,1) = s;
            }
        }
    }

  if (!optimized)
    {
      const float mag = sqrtf (x * x + y * y + z * z);

      if (mag <= 1.0e-4)
        {
          /* no rotation, leave mat as-is */
          return;
        }

      x /= mag;
      y /= mag;
      z /= mag;


      /*
       *     Arbitrary axis rotation matrix.
       *
       *  This is composed of 5 matrices, Rz, Ry, T, Ry', Rz', multiplied
       *  like so:  Rz * Ry * T * Ry' * Rz'.  T is the final rotation
       *  (which is about the X-axis), and the two composite transforms
       *  Ry' * Rz' and Rz * Ry are (respectively) the rotations necessary
       *  from the arbitrary axis to the X-axis then back.  They are
       *  all elementary rotations.
       *
       *  Rz' is a rotation about the Z-axis, to bring the axis vector
       *  into the x-z plane.  Then Ry' is applied, rotating about the
       *  Y-axis to bring the axis vector parallel with the X-axis.  The
       *  rotation about the X-axis is then performed.  Ry and Rz are
       *  simply the respective inverse transforms to bring the arbitrary
       *  axis back to it's original orientation.  The first transforms
       *  Rz' and Ry' are considered inverses, since the data from the
       *  arbitrary axis gives you info on how to get to it, not how
       *  to get away from it, and an inverse must be applied.
       *
       *  The basic calculation used is to recognize that the arbitrary
       *  axis vector (x, y, z), since it is of unit length, actually
       *  represents the sines and cosines of the angles to rotate the
       *  X-axis to the same orientation, with theta being the angle about
       *  Z and phi the angle about Y (in the order described above)
       *  as follows:
       *
       *  cos ( theta ) = x / sqrt ( 1 - z^2 )
       *  sin ( theta ) = y / sqrt ( 1 - z^2 )
       *
       *  cos ( phi ) = sqrt ( 1 - z^2 )
       *  sin ( phi ) = z
       *
       *  Note that cos ( phi ) can further be inserted to the above
       *  formulas:
       *
       *  cos ( theta ) = x / cos ( phi )
       *  sin ( theta ) = y / sin ( phi )
       *
       *  ...etc.  Because of those relations and the standard trigonometric
       *  relations, it is pssible to reduce the transforms down to what
       *  is used below.  It may be that any primary axis chosen will give the
       *  same results (modulo a sign convention) using thie method.
       *
       *  Particularly nice is to notice that all divisions that might
       *  have caused trouble when parallel to certain planes or
       *  axis go away with care paid to reducing the expressions.
       *  After checking, it does perform correctly under all cases, since
       *  in all the cases of division where the denominator would have
       *  been zero, the numerator would have been zero as well, giving
       *  the expected result.
       */

      xx = x * x;
      yy = y * y;
      zz = z * z;
      xy = x * y;
      yz = y * z;
      zx = z * x;
      xs = x * s;
      ys = y * s;
      zs = z * s;
      one_c = 1.0f - c;

      /* We already hold the identity-matrix so we can skip some statements */
      M (0,0) = (one_c * xx) + c;
      M (0,1) = (one_c * xy) - zs;
      M (0,2) = (one_c * zx) + ys;
      /*    M (0,3) = 0.0f; */

      M (1,0) = (one_c * xy) + zs;
      M (1,1) = (one_c * yy) + c;
      M (1,2) = (one_c * yz) - xs;
      /*    M (1,3) = 0.0f; */

      M (2,0) = (one_c * zx) - ys;
      M (2,1) = (one_c * yz) + xs;
      M (2,2) = (one_c * zz) + c;
      /*    M (2,3) = 0.0f; */

      /*
         M (3,0) = 0.0f;
         M (3,1) = 0.0f;
         M (3,2) = 0.0f;
         M (3,3) = 1.0f;
         */
    }
#undef M

  matrix_multiply_array_with_flags (matrix, m, MAT_FLAG_ROTATION);
}

/*
 * Apply a perspective projection matrix.
 *
 * \param mat matrix to apply the projection.
 * \param left left clipping plane coordinate.
 * \param right right clipping plane coordinate.
 * \param bottom bottom clipping plane coordinate.
 * \param top top clipping plane coordinate.
 * \param nearval distance to the near clipping plane.
 * \param farval distance to the far clipping plane.
 *
 * Creates the projection matrix and multiplies it with \p mat, marking the
 * MAT_FLAG_PERSPECTIVE flag.
 */
void
_math_matrix_frustum (CoglMatrix *matrix,
                      float left,
                      float right,
                      float bottom,
                      float top,
                      float nearval,
                      float farval)
{
  float x, y, a, b, c, d;
  float m[16];

  x = (2.0f * nearval) / (right - left);
  y = (2.0f * nearval) / (top - bottom);
  a = (right + left) / (right - left);
  b = (top + bottom) / (top - bottom);
  c = -(farval + nearval) / ( farval - nearval);
  d = -(2.0f * farval * nearval) / (farval - nearval);  /* error? */

#define M(row,col)  m[col*4+row]
  M (0,0) = x;     M (0,1) = 0.0f;  M (0,2) = a;      M (0,3) = 0.0f;
  M (1,0) = 0.0f;  M (1,1) = y;     M (1,2) = b;      M (1,3) = 0.0f;
  M (2,0) = 0.0f;  M (2,1) = 0.0f;  M (2,2) = c;      M (2,3) = d;
  M (3,0) = 0.0f;  M (3,1) = 0.0f;  M (3,2) = -1.0f;  M (3,3) = 0.0f;
#undef M

  matrix_multiply_array_with_flags (matrix, m, MAT_FLAG_PERSPECTIVE);
}

/*
 * Apply an orthographic projection matrix.
 *
 * \param mat matrix to apply the projection.
 * \param left left clipping plane coordinate.
 * \param right right clipping plane coordinate.
 * \param bottom bottom clipping plane coordinate.
 * \param top top clipping plane coordinate.
 * \param nearval distance to the near clipping plane.
 * \param farval distance to the far clipping plane.
 *
 * Creates the projection matrix and multiplies it with \p mat, marking the
 * MAT_FLAG_GENERAL_SCALE and MAT_FLAG_TRANSLATION flags.
 */
void
_math_matrix_ortho (CoglMatrix *matrix,
                    float left,
                    float right,
                    float bottom,
                    float top,
                    float nearval,
                    float farval)
{
  float m[16];

#define M(row,col)  m[col*4+row]
  M (0,0) = 2.0f / (right-left);
  M (0,1) = 0.0f;
  M (0,2) = 0.0f;
  M (0,3) = -(right+left) / (right-left);

  M (1,0) = 0.0f;
  M (1,1) = 2.0f / (top-bottom);
  M (1,2) = 0.0f;
  M (1,3) = -(top+bottom) / (top-bottom);

  M (2,0) = 0.0f;
  M (2,1) = 0.0f;
  M (2,2) = -2.0f / (farval-nearval);
  M (2,3) = -(farval+nearval) / (farval-nearval);

  M (3,0) = 0.0f;
  M (3,1) = 0.0f;
  M (3,2) = 0.0f;
  M (3,3) = 1.0f;
#undef M

  matrix_multiply_array_with_flags (matrix, m,
                                    (MAT_FLAG_GENERAL_SCALE |
                                     MAT_FLAG_TRANSLATION));
}

/*
 * Multiply a matrix with a general scaling matrix.
 *
 * \param mat matrix.
 * \param x x axis scale factor.
 * \param y y axis scale factor.
 * \param z z axis scale factor.
 *
 * Multiplies in-place the elements of \p mat by the scale factors. Checks if
 * the scales factors are roughly the same, marking the MAT_FLAG_UNIFORM_SCALE
 * flag, or MAT_FLAG_GENERAL_SCALE. Marks the MAT_DIRTY_TYPE and
 * MAT_DIRTY_INVERSE dirty flags.
 */
void
_math_matrix_scale (CoglMatrix *matrix, float x, float y, float z)
{
  float *m = (float *)matrix;
  m[0] *= x;   m[4] *= y;   m[8]  *= z;
  m[1] *= x;   m[5] *= y;   m[9]  *= z;
  m[2] *= x;   m[6] *= y;   m[10] *= z;
  m[3] *= x;   m[7] *= y;   m[11] *= z;

  if (fabsf (x - y) < 1e-8 && fabsf (x - z) < 1e-8)
    matrix->flags |= MAT_FLAG_UNIFORM_SCALE;
  else
    matrix->flags |= MAT_FLAG_GENERAL_SCALE;

  matrix->flags |= (MAT_DIRTY_TYPE | MAT_DIRTY_INVERSE);
}

/*
 * Multiply a matrix with a translation matrix.
 *
 * \param mat matrix.
 * \param x translation vector x coordinate.
 * \param y translation vector y coordinate.
 * \param z translation vector z coordinate.
 *
 * Adds the translation coordinates to the elements of \p mat in-place.  Marks
 * the MAT_FLAG_TRANSLATION flag, and the MAT_DIRTY_TYPE and MAT_DIRTY_INVERSE
 * dirty flags.
 */
void
_math_matrix_translate (CoglMatrix *matrix, float x, float y, float z)
{
  float *m = (float *)matrix;
  m[12] = m[0] * x + m[4] * y + m[8]  * z + m[12];
  m[13] = m[1] * x + m[5] * y + m[9]  * z + m[13];
  m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
  m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];

  matrix->flags |= (MAT_FLAG_TRANSLATION |
                    MAT_DIRTY_TYPE |
                    MAT_DIRTY_INVERSE);
}


/*
 * Set matrix to do viewport and depthrange mapping.
 * Transforms Normalized Device Coords to window/Z values.
 */
void
_math_matrix_viewport (CoglMatrix *matrix, int x, int y, int width, int height,
                       float zNear, float zFar, float depthMax)
{
  float *m = (float *)matrix;
  m[MAT_SX] = (float)width / 2.0f;
  m[MAT_TX] = m[MAT_SX] + x;
  m[MAT_SY] = (float) height / 2.0f;
  m[MAT_TY] = m[MAT_SY] + y;
  m[MAT_SZ] = depthMax * ((zFar - zNear) / 2.0f);
  m[MAT_TZ] = depthMax * ((zFar - zNear) / 2.0f + zNear);
  matrix->flags = MAT_FLAG_GENERAL_SCALE | MAT_FLAG_TRANSLATION;
  matrix->type = COGL_MATRIX_TYPE_3D_NO_ROT;
}


/*
 * Set a matrix to the identity matrix.
 *
 * \param mat matrix.
 *
 * Copies ::identity into \p CoglMatrix::m, and into CoglMatrix::inv if
 * not NULL. Sets the matrix type to identity, resets the flags. It
 * doesn't initialize the inverse matrix, it just marks it dirty.
 */
void
_math_matrix_init_identity (CoglMatrix *matrix)
{
  memcpy (matrix, identity, 16 * sizeof (float));

  matrix->type = COGL_MATRIX_TYPE_IDENTITY;
  matrix->flags = MAT_DIRTY_INVERSE;
}

/*@}*/


/**********************************************************************/
/* \name Matrix analysis */
/*@{*/

#define ZERO(x) (1<<x)
#define ONE(x)  (1<<(x+16))

#define MASK_NO_TRX      (ZERO(12) | ZERO(13) | ZERO(14))
#define MASK_NO_2D_SCALE ( ONE(0)  | ONE(5))

#define MASK_IDENTITY    ( ONE(0)  | ZERO(4)  | ZERO(8)  | ZERO(12) |\
                          ZERO(1)  |  ONE(5)  | ZERO(9)  | ZERO(13) |\
                          ZERO(2)  | ZERO(6)  |  ONE(10) | ZERO(14) |\
                          ZERO(3)  | ZERO(7)  | ZERO(11) |  ONE(15) )

#define MASK_2D_NO_ROT   (           ZERO(4)  | ZERO(8)  |           \
                          ZERO(1)  |            ZERO(9)  |           \
                          ZERO(2)  | ZERO(6)  |  ONE(10) | ZERO(14) |\
                          ZERO(3)  | ZERO(7)  | ZERO(11) |  ONE(15) )

#define MASK_2D          (                      ZERO(8)  |           \
                          ZERO(9)  |           \
                          ZERO(2)  | ZERO(6)  |  ONE(10) | ZERO(14) |\
                          ZERO(3)  | ZERO(7)  | ZERO(11) |  ONE(15) )


#define MASK_3D_NO_ROT   (           ZERO(4)  | ZERO(8)  |           \
                          ZERO(1)  |            ZERO(9)  |           \
                          ZERO(2)  | ZERO(6)  |                      \
                          ZERO(3)  | ZERO(7)  | ZERO(11) |  ONE(15) )

#define MASK_3D          (                                           \
                          \
                          \
                          ZERO(3)  | ZERO(7)  | ZERO(11) |  ONE(15) )


#define MASK_PERSPECTIVE (           ZERO(4)  |            ZERO(12) |\
                          ZERO(1)  |                       ZERO(13) |\
                          ZERO(2)  | ZERO(6)  |                      \
                          ZERO(3)  | ZERO(7)  |            ZERO(15) )

#define SQ(x) ((x)*(x))

/*
 * Determine type and flags from scratch.
 *
 * \param mat matrix.
 *
 * This is expensive enough to only want to do it once.
 */
static void
analyse_from_scratch (CoglMatrix *matrix)
{
  const float *m = (float *)matrix;
  unsigned int mask = 0;
  unsigned int i;

  for (i = 0 ; i < 16 ; i++)
    {
      if (m[i] == 0.0) mask |= (1<<i);
    }

  if (m[0] == 1.0f) mask |= (1<<16);
  if (m[5] == 1.0f) mask |= (1<<21);
  if (m[10] == 1.0f) mask |= (1<<26);
  if (m[15] == 1.0f) mask |= (1<<31);

  matrix->flags &= ~MAT_FLAGS_GEOMETRY;

  /* Check for translation - no-one really cares
  */
  if ((mask & MASK_NO_TRX) != MASK_NO_TRX)
    matrix->flags |= MAT_FLAG_TRANSLATION;

  /* Do the real work
  */
  if (mask == (unsigned int) MASK_IDENTITY)
    matrix->type = COGL_MATRIX_TYPE_IDENTITY;
  else if ((mask & MASK_2D_NO_ROT) == (unsigned int) MASK_2D_NO_ROT)
    {
      matrix->type = COGL_MATRIX_TYPE_2D_NO_ROT;

      if ((mask & MASK_NO_2D_SCALE) != MASK_NO_2D_SCALE)
        matrix->flags |= MAT_FLAG_GENERAL_SCALE;
    }
  else if ((mask & MASK_2D) == (unsigned int) MASK_2D)
    {
      float mm = DOT2 (m, m);
      float m4m4 = DOT2 (m+4,m+4);
      float mm4 = DOT2 (m,m+4);

      matrix->type = COGL_MATRIX_TYPE_2D;

      /* Check for scale */
      if (SQ (mm-1) > SQ (1e-6) ||
          SQ (m4m4-1) > SQ (1e-6))
        matrix->flags |= MAT_FLAG_GENERAL_SCALE;

      /* Check for rotation */
      if (SQ (mm4) > SQ (1e-6))
        matrix->flags |= MAT_FLAG_GENERAL_3D;
      else
        matrix->flags |= MAT_FLAG_ROTATION;

    }
  else if ((mask & MASK_3D_NO_ROT) == (unsigned int) MASK_3D_NO_ROT)
    {
      matrix->type = COGL_MATRIX_TYPE_3D_NO_ROT;

      /* Check for scale */
      if (SQ (m[0]-m[5]) < SQ (1e-6) &&
          SQ (m[0]-m[10]) < SQ (1e-6))
        {
          if (SQ (m[0]-1.0) > SQ (1e-6))
            matrix->flags |= MAT_FLAG_UNIFORM_SCALE;
        }
      else
        matrix->flags |= MAT_FLAG_GENERAL_SCALE;
    }
  else if ((mask & MASK_3D) == (unsigned int) MASK_3D)
    {
      float c1 = DOT3 (m,m);
      float c2 = DOT3 (m+4,m+4);
      float c3 = DOT3 (m+8,m+8);
      float d1 = DOT3 (m, m+4);
      float cp[3];

      matrix->type = COGL_MATRIX_TYPE_3D;

      /* Check for scale */
      if (SQ (c1-c2) < SQ (1e-6) && SQ (c1-c3) < SQ (1e-6))
        {
          if (SQ (c1-1.0) > SQ (1e-6))
            matrix->flags |= MAT_FLAG_UNIFORM_SCALE;
          /* else no scale at all */
        }
      else
        matrix->flags |= MAT_FLAG_GENERAL_SCALE;

      /* Check for rotation */
      if (SQ (d1) < SQ (1e-6))
        {
          CROSS3 ( cp, m, m+4);
          SUB_3V ( cp, cp, (m+8));
          if (LEN_SQUARED_3FV(cp) < SQ(1e-6))
            matrix->flags |= MAT_FLAG_ROTATION;
          else
            matrix->flags |= MAT_FLAG_GENERAL_3D;
        }
      else
        matrix->flags |= MAT_FLAG_GENERAL_3D; /* shear, etc */
    }
  else if ((mask & MASK_PERSPECTIVE) == MASK_PERSPECTIVE && m[11]==-1.0f)
    {
      matrix->type = COGL_MATRIX_TYPE_PERSPECTIVE;
      matrix->flags |= MAT_FLAG_GENERAL;
    }
  else
    {
      matrix->type = COGL_MATRIX_TYPE_GENERAL;
      matrix->flags |= MAT_FLAG_GENERAL;
    }
}

/*
 * Analyze a matrix given that its flags are accurate.
 *
 * This is the more common operation, hopefully.
 */
static void
analyse_from_flags (CoglMatrix *matrix)
{
  const float *m = (float *)matrix;

  if (TEST_MAT_FLAGS(matrix, 0))
    matrix->type = COGL_MATRIX_TYPE_IDENTITY;
  else if (TEST_MAT_FLAGS(matrix, (MAT_FLAG_TRANSLATION |
                                   MAT_FLAG_UNIFORM_SCALE |
                                   MAT_FLAG_GENERAL_SCALE)))
    {
      if ( m[10] == 1.0f && m[14] == 0.0f )
        matrix->type = COGL_MATRIX_TYPE_2D_NO_ROT;
      else
        matrix->type = COGL_MATRIX_TYPE_3D_NO_ROT;
    }
  else if (TEST_MAT_FLAGS (matrix, MAT_FLAGS_3D))
    {
      if (                               m[ 8]==0.0f
          &&                             m[ 9]==0.0f
          && m[2]==0.0f && m[6]==0.0f && m[10]==1.0f && m[14]==0.0f)
        {
          matrix->type = COGL_MATRIX_TYPE_2D;
        }
      else
        matrix->type = COGL_MATRIX_TYPE_3D;
    }
  else if (                 m[4]==0.0f                 && m[12]==0.0f
           && m[1]==0.0f                               && m[13]==0.0f
           && m[2]==0.0f && m[6]==0.0f
           && m[3]==0.0f && m[7]==0.0f && m[11]==-1.0f && m[15]==0.0f)
    {
      matrix->type = COGL_MATRIX_TYPE_PERSPECTIVE;
    }
  else
    matrix->type = COGL_MATRIX_TYPE_GENERAL;
}

/*
 * Analyze and update the type and flags of a matrix.
 *
 * \param mat matrix.
 *
 * If the matrix type is dirty then calls either analyse_from_scratch() or
 * analyse_from_flags() to determine its type, according to whether the flags
 * are dirty or not, respectively. If the matrix has an inverse and it's dirty
 * then calls matrix_invert(). Finally clears the dirty flags.
 */
void
_math_matrix_update_type_and_flags (CoglMatrix *matrix)
{
  if (matrix->flags & MAT_DIRTY_TYPE)
    {
      if (matrix->flags & MAT_DIRTY_FLAGS)
        analyse_from_scratch (matrix);
      else
        analyse_from_flags (matrix);
    }

  matrix->flags &= ~(MAT_DIRTY_FLAGS | MAT_DIRTY_TYPE);
}

/*@}*/


/*
 * Test if the given matrix preserves vector lengths.
 */
gboolean
_math_matrix_is_length_preserving (const CoglMatrix *m)
{
  return TEST_MAT_FLAGS (m, MAT_FLAGS_LENGTH_PRESERVING);
}


/*
 * Test if the given matrix does any rotation.
 * (or perhaps if the upper-left 3x3 is non-identity)
 */
gboolean
_math_matrix_has_rotation (const CoglMatrix *matrix)
{
  if (matrix->flags & (MAT_FLAG_GENERAL |
                       MAT_FLAG_ROTATION |
                       MAT_FLAG_GENERAL_3D |
                       MAT_FLAG_PERSPECTIVE))
    return TRUE;
  else
    return FALSE;
}


gboolean
_math_matrix_is_general_scale (const CoglMatrix *matrix)
{
  return (matrix->flags & MAT_FLAG_GENERAL_SCALE) ? TRUE : FALSE;
}


gboolean
_math_matrix_is_dirty (const CoglMatrix *matrix)
{
  return (matrix->flags & MAT_DIRTY_ALL) ? TRUE : FALSE;
}


/**********************************************************************/
/* \name Matrix setup */
/*@{*/

/*
 * Loads a matrix array into CoglMatrix.
 *
 * \param m matrix array.
 * \param mat matrix.
 *
 * Copies \p m into CoglMatrix::m and marks the MAT_FLAG_GENERAL and
 * MAT_DIRTY_ALL
 * flags.
 */
void
_math_matrix_init_from_array (CoglMatrix *matrix, const float *array)
{
  memcpy (matrix, array, 16 * sizeof (float));
  matrix->flags = (MAT_FLAG_GENERAL | MAT_DIRTY_ALL);
}

/*@}*/


/**********************************************************************/
/* \name Matrix transpose */
/*@{*/

/*
 * Transpose a float matrix.
 *
 * \param to destination array.
 * \param from source array.
 */
void
_math_transposef (float to[16], const float from[16])
{
  to[0] = from[0];
  to[1] = from[4];
  to[2] = from[8];
  to[3] = from[12];
  to[4] = from[1];
  to[5] = from[5];
  to[6] = from[9];
  to[7] = from[13];
  to[8] = from[2];
  to[9] = from[6];
  to[10] = from[10];
  to[11] = from[14];
  to[12] = from[3];
  to[13] = from[7];
  to[14] = from[11];
  to[15] = from[15];
}

/*
 * Transpose a double matrix.
 *
 * \param to destination array.
 * \param from source array.
 */
void
_math_transposed (double to[16], const double from[16])
{
  to[0] = from[0];
  to[1] = from[4];
  to[2] = from[8];
  to[3] = from[12];
  to[4] = from[1];
  to[5] = from[5];
  to[6] = from[9];
  to[7] = from[13];
  to[8] = from[2];
  to[9] = from[6];
  to[10] = from[10];
  to[11] = from[14];
  to[12] = from[3];
  to[13] = from[7];
  to[14] = from[11];
  to[15] = from[15];
}

/*
 * Transpose a double matrix and convert to float.
 *
 * \param to destination array.
 * \param from source array.
 */
void
_math_transposefd (float to[16], const double from[16])
{
  to[0] = (float)from[0];
  to[1] = (float)from[4];
  to[2] = (float)from[8];
  to[3] = (float)from[12];
  to[4] = (float)from[1];
  to[5] = (float)from[5];
  to[6] = (float)from[9];
  to[7] = (float)from[13];
  to[8] = (float)from[2];
  to[9] = (float)from[6];
  to[10] = (float)from[10];
  to[11] = (float)from[14];
  to[12] = (float)from[3];
  to[13] = (float)from[7];
  to[14] = (float)from[11];
  to[15] = (float)from[15];
}

/*@}*/


/*
 * Transform a 4-element row vector (1x4 matrix) by a 4x4 matrix.  This
 * function is used for transforming clipping plane equations and spotlight
 * directions.
 * Mathematically,  u = v * m.
 * Input:  v - input vector
 *         m - transformation matrix
 * Output:  u - transformed vector
 */
void
_mesa_transform_vector (float u[4], const float v[4], const float m[16])
{
  const float v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];
#define M(row,col)  m[row + col*4]
  u[0] = v0 * M (0,0) + v1 * M (1,0) + v2 * M (2,0) + v3 * M (3,0);
  u[1] = v0 * M (0,1) + v1 * M (1,1) + v2 * M (2,1) + v3 * M (3,1);
  u[2] = v0 * M (0,2) + v1 * M (1,2) + v2 * M (2,2) + v3 * M (3,2);
  u[3] = v0 * M (0,3) + v1 * M (1,3) + v2 * M (2,3) + v3 * M (3,3);
#undef M
}

