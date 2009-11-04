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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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


/**
 * \file math/m_matrix.h
 * Defines basic structures for matrix-handling.
 */

#ifndef _M_MATRIX_H
#define _M_MATRIX_H

#include <cogl-matrix.h>

#include <glib.h>

/**
 * \name Symbolic names to some of the entries in the matrix
 *
 * These are handy for the viewport mapping, which is expressed as a matrix.
 */
/*@{*/
#define MAT_SX 0
#define MAT_SY 5
#define MAT_SZ 10
#define MAT_TX 12
#define MAT_TY 13
#define MAT_TZ 14
/*@}*/


/**
 * Different kinds of 4x4 transformation matrices.
 * We use these to select specific optimized vertex transformation routines.
 */
enum CoglMatrixType {
   COGL_MATRIX_TYPE_GENERAL,	/**< general 4x4 matrix */
   COGL_MATRIX_TYPE_IDENTITY,	/**< identity matrix */
   COGL_MATRIX_TYPE_3D_NO_ROT,	/**< orthogonal projection and others... */
   COGL_MATRIX_TYPE_PERSPECTIVE,	/**< perspective projection matrix */
   COGL_MATRIX_TYPE_2D,		/**< 2-D transformation */
   COGL_MATRIX_TYPE_2D_NO_ROT,	/**< 2-D scale & translate only */
   COGL_MATRIX_TYPE_3D		/**< 3-D transformation */
} ;


#if 0
/**
 * Matrix type to represent 4x4 transformation matrices.
 */
typedef struct {
   float *m;		/**< 16 matrix elements (16-byte aligned) */
   float *inv;	/**< optional 16-element inverse (16-byte aligned) */
   unsigned int flags;        /**< possible values determined by (of \link
                         * MatFlags MAT_FLAG_* flags\endlink)
                         */
   enum CoglMatrixType type;
} CoglMatrix;
#endif


void
_math_matrix_multiply (CoglMatrix *result,
                       const CoglMatrix *a,
                       const CoglMatrix *b);

void
_math_matrix_multiply_array (CoglMatrix *result, const float *b);

void
_math_matrix_init_from_array (CoglMatrix *matrix, const float *array);

void
_math_matrix_translate (CoglMatrix *matrix, float x, float y, float z);

void
_math_matrix_rotate (CoglMatrix *matrix, float angle,
		     float x, float y, float z);

void
_math_matrix_scale (CoglMatrix *matrix, float x, float y, float z);

void
_math_matrix_ortho (CoglMatrix *matrix,
		    float left, float right,
		    float bottom, float top,
		    float nearval, float farval);

void
_math_matrix_frustum (CoglMatrix *matrix,
		      float left, float right,
		      float bottom, float top,
		      float nearval, float farval);

void
_math_matrix_viewport (CoglMatrix *matrix,
                       int x, int y, int width, int height,
                       float z_near, float z_far, float depth_max);

void
_math_matrix_init_identity (CoglMatrix *matrix);

gboolean
_math_matrix_update_inverse (CoglMatrix *matrix);

void
_math_matrix_update_type_and_flags (CoglMatrix *matrix);

void
_math_matrix_print (const CoglMatrix *matrix);

gboolean
_math_matrix_is_length_preserving (const CoglMatrix *matrix);

gboolean
_math_matrix_has_rotation (const CoglMatrix *matrix);

gboolean
_math_matrix_is_general_scale (const CoglMatrix *matrix);

gboolean
_math_matrix_is_dirty (const CoglMatrix *matrix);


/**
 * \name Related functions that don't actually operate on CoglMatrix structs
 */
/*@{*/

void
_math_transposef ( float to[16], const float from[16]);

void
_math_transposed (double to[16], const double from[16]);

void
_math_transposefd (float to[16], const double from[16]);


/*
 * Transform a point (column vector) by a matrix:   Q = M * P
 */
#define TRANSFORM_POINT( Q, M, P )					\
   Q[0] = M[0] * P[0] + M[4] * P[1] + M[8] *  P[2] + M[12] * P[3];	\
   Q[1] = M[1] * P[0] + M[5] * P[1] + M[9] *  P[2] + M[13] * P[3];	\
   Q[2] = M[2] * P[0] + M[6] * P[1] + M[10] * P[2] + M[14] * P[3];	\
   Q[3] = M[3] * P[0] + M[7] * P[1] + M[11] * P[2] + M[15] * P[3];


#define TRANSFORM_POINT3( Q, M, P )				\
   Q[0] = M[0] * P[0] + M[4] * P[1] + M[8] *  P[2] + M[12];	\
   Q[1] = M[1] * P[0] + M[5] * P[1] + M[9] *  P[2] + M[13];	\
   Q[2] = M[2] * P[0] + M[6] * P[1] + M[10] * P[2] + M[14];	\
   Q[3] = M[3] * P[0] + M[7] * P[1] + M[11] * P[2] + M[15];


/*
 * Transform a normal (row vector) by a matrix:  [NX NY NZ] = N * MAT
 */
#define TRANSFORM_NORMAL( TO, N, MAT )				\
do {								\
   TO[0] = N[0] * MAT[0] + N[1] * MAT[1] + N[2] * MAT[2];	\
   TO[1] = N[0] * MAT[4] + N[1] * MAT[5] + N[2] * MAT[6];	\
   TO[2] = N[0] * MAT[8] + N[1] * MAT[9] + N[2] * MAT[10];	\
} while (0)


/**
 * Transform a direction by a matrix.
 */
#define TRANSFORM_DIRECTION( TO, DIR, MAT )			\
do {								\
   TO[0] = DIR[0] * MAT[0] + DIR[1] * MAT[4] + DIR[2] * MAT[8];	\
   TO[1] = DIR[0] * MAT[1] + DIR[1] * MAT[5] + DIR[2] * MAT[9];	\
   TO[2] = DIR[0] * MAT[2] + DIR[1] * MAT[6] + DIR[2] * MAT[10];\
} while (0)


void
_mesa_transform_vector (float u[4], const float v[4], const float m[16]);


/*@}*/


#endif
