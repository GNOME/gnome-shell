/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_MATRIX_STACK_H
#define __COGL_MATRIX_STACK_H

#include "cogl-matrix.h"
#include "cogl-context.h"

typedef struct _CoglMatrixStack CoglMatrixStack;

typedef enum {
  COGL_MATRIX_MODELVIEW,
  COGL_MATRIX_PROJECTION,
  COGL_MATRIX_TEXTURE
} CoglMatrixMode;

typedef void (* CoglMatrixStackFlushFunc) (CoglContext *context,
                                           gboolean is_identity,
                                           const CoglMatrix *matrix,
                                           void *user_data);

CoglMatrixStack *
_cogl_matrix_stack_new (void);

void
_cogl_matrix_stack_push (CoglMatrixStack *stack);

void
_cogl_matrix_stack_pop (CoglMatrixStack *stack);

void
_cogl_matrix_stack_load_identity (CoglMatrixStack *stack);

void
_cogl_matrix_stack_scale (CoglMatrixStack *stack,
                          float x,
                          float y,
                          float z);
void
_cogl_matrix_stack_translate (CoglMatrixStack *stack,
                              float x,
                              float y,
                              float z);
void
_cogl_matrix_stack_rotate (CoglMatrixStack *stack,
                           float angle,
                           float x,
                           float y,
                           float z);
void
_cogl_matrix_stack_multiply (CoglMatrixStack *stack,
                             const CoglMatrix *matrix);
void
_cogl_matrix_stack_frustum (CoglMatrixStack *stack,
                            float left,
                            float right,
                            float bottom,
                            float top,
                            float z_near,
                            float z_far);
void
_cogl_matrix_stack_perspective (CoglMatrixStack *stack,
                                float fov_y,
                                float aspect,
                                float z_near,
                                float z_far);
void
_cogl_matrix_stack_ortho (CoglMatrixStack *stack,
                          float left,
                          float right,
                          float bottom,
                          float top,
                          float z_near,
                          float z_far);
gboolean
_cogl_matrix_stack_get_inverse (CoglMatrixStack *stack,
                                CoglMatrix *inverse);
void
_cogl_matrix_stack_get (CoglMatrixStack *stack,
                        CoglMatrix *matrix);
void
_cogl_matrix_stack_set (CoglMatrixStack *stack,
                        const CoglMatrix *matrix);
void
_cogl_matrix_stack_flush_to_gl (CoglContext *ctx,
                                CoglMatrixStack *stack,
                                CoglMatrixMode mode);
void
_cogl_matrix_stack_dirty (CoglMatrixStack  *stack);

unsigned int
_cogl_matrix_stack_get_age (CoglMatrixStack *stack);

/* If this returns TRUE then the top of the matrix is definitely the
   identity matrix. If it returns FALSE it may or may not be the
   identity matrix but no expensive comparison is performed to verify
   it. */
gboolean
_cogl_matrix_stack_has_identity_flag (CoglMatrixStack *stack);

void
_cogl_prepare_matrix_stack_for_flush (CoglContext *context,
                                      CoglMatrixStack *stack,
                                      CoglMatrixMode mode,
                                      CoglMatrixStackFlushFunc callback,
                                      void *user_data);

gboolean
_cogl_matrix_stack_equal (CoglMatrixStack *stack0,
                          CoglMatrixStack *stack1);

#endif /* __COGL_MATRIX_STACK_H */
