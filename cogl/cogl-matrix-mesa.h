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
 * \file math/m_matrix.h
 * Defines basic structures for matrix-handling.
 */

#ifndef _M_MATRIX_H
#define _M_MATRIX_H

#include <cogl-matrix.h>
#include <cogl-quaternion.h>

#include <glib.h>

/*
 * Symbolic names to some of the entries in the matrix
 *
 * These are handy for the viewport mapping, which is expressed as a matrix.
 */
#define MAT_SX 0
#define MAT_SY 5
#define MAT_SZ 10
#define MAT_TX 12
#define MAT_TY 13
#define MAT_TZ 14

/*
 * These identify different kinds of 4x4 transformation matrices and we use
 * this information to find fast-paths when available.
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

void
_math_matrix_multiply (CoglMatrix *result,
                       const CoglMatrix *a,
                       const CoglMatrix *b);

void
_math_matrix_multiply_array (CoglMatrix *result, const float *b);

void
_math_matrix_init_from_array (CoglMatrix *matrix, const float *array);

void
_math_matrix_init_from_quaternion (CoglMatrix *matrix,
                                   CoglQuaternion *quaternion);

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
                       float x, float y, float width, float height,
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

void
_math_transposef ( float to[16], const float from[16]);

#endif
