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
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 */

#ifndef __COGL_MATRIX_STACK_H
#define __COGL_MATRIX_STACK_H

#include "cogl-matrix.h"

typedef struct _CoglMatrixStack CoglMatrixStack;

typedef enum {
  COGL_MATRIX_MODELVIEW,
  COGL_MATRIX_PROJECTION,
  COGL_MATRIX_TEXTURE
} CoglMatrixMode;

CoglMatrixStack* _cogl_matrix_stack_new           (void);
void             _cogl_matrix_stack_destroy       (CoglMatrixStack  *stack);
void             _cogl_matrix_stack_push          (CoglMatrixStack  *stack);
void             _cogl_matrix_stack_pop           (CoglMatrixStack  *stack);
void             _cogl_matrix_stack_load_identity (CoglMatrixStack  *stack);
void             _cogl_matrix_stack_scale         (CoglMatrixStack  *stack,
                                                   float             x,
                                                   float             y,
                                                   float             z);
void             _cogl_matrix_stack_translate     (CoglMatrixStack  *stack,
                                                   float             x,
                                                   float             y,
                                                   float             z);
void             _cogl_matrix_stack_rotate        (CoglMatrixStack  *stack,
                                                   float             angle,
                                                   float             x,
                                                   float             y,
                                                   float             z);
void             _cogl_matrix_stack_multiply      (CoglMatrixStack  *stack,
                                                   const CoglMatrix *matrix);
void             _cogl_matrix_stack_frustum       (CoglMatrixStack  *stack,
                                                   float             left,
                                                   float             right,
                                                   float             bottom,
                                                   float             top,
                                                   float             z_near,
                                                   float             z_far);
void             _cogl_matrix_stack_perspective   (CoglMatrixStack  *stack,
                                                   float             fov_y,
                                                   float             aspect,
                                                   float             z_near,
                                                   float             z_far);
void             _cogl_matrix_stack_ortho         (CoglMatrixStack  *stack,
                                                   float             left,
                                                   float             right,
                                                   float             bottom,
                                                   float             top,
                                                   float             z_near,
                                                   float             z_far);

gboolean         _cogl_matrix_stack_get_inverse   (CoglMatrixStack  *stack,
                                                   CoglMatrix       *inverse);
void             _cogl_matrix_stack_get           (CoglMatrixStack  *stack,
                                                   CoglMatrix       *matrix);
void             _cogl_matrix_stack_set           (CoglMatrixStack  *stack,
                                                   const CoglMatrix *matrix);
void             _cogl_matrix_stack_flush_to_gl   (CoglMatrixStack  *stack,
                                                   CoglMatrixMode    mode);
void             _cogl_matrix_stack_dirty         (CoglMatrixStack  *stack);

#endif /* __COGL_MATRIX_STACK_H */
