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
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 */

#ifndef __COGL_CURRENT_MATRIX_H
#define __COGL_CURRENT_MATRIX_H

#include <cogl/cogl-matrix.h>

/**
 * CoglMatrixMode:
 * @COGL_MATRIX_MODELVIEW: Select model-view matrix stack
 * @COGL_MATRIX_PROJECTION: Select projection matrix stack
 * @COGL_MATRIX_TEXTURE: Select texture matrix stack
 *
 * There are several matrix stacks affected by the COGL current matrix
 * operations (which are private). Code should always leave the
 * model-view matrix active, switching to the projection matrix stack
 * only temporarily in order to modify the projection matrix. Most
 * COGL and Clutter APIs (other than the current matrix operations)
 * will assume the model-view matrix is active when the API is
 * invoked.
 *
 * Since: 1.0
 */
typedef enum
{
  COGL_MATRIX_MODELVIEW   = 1,
  COGL_MATRIX_PROJECTION  = 2,
  COGL_MATRIX_TEXTURE  = 3
} CoglMatrixMode;

#define COGL_TYPE_MATRIX_MODE     (cogl_matrix_mode_get_type ())
GType cogl_matrix_mode_get_type (void) G_GNUC_CONST;

void _cogl_set_current_matrix            (CoglMatrixMode    mode);
void _cogl_current_matrix_push           (void);
void _cogl_current_matrix_pop            (void);
void _cogl_current_matrix_identity       (void);
void _cogl_current_matrix_load           (const CoglMatrix *matrix);
void _cogl_current_matrix_multiply       (const CoglMatrix *matrix);
void _cogl_current_matrix_rotate         (float             angle,
                                          float             x,
                                          float             y,
                                          float             z);
void _cogl_current_matrix_scale          (float             x,
                                          float             y,
                                          float             z);
void _cogl_current_matrix_translate      (float             x,
                                          float             y,
                                          float             z);
void _cogl_current_matrix_frustum        (float             left,
                                          float             right,
                                          float             bottom,
                                          float             top,
                                          float             near_val,
                                          float             far_val);
void _cogl_current_matrix_ortho          (float             left,
                                          float             right,
                                          float             bottom,
                                          float             top,
                                          float             near_val,
                                          float             far_val);
void _cogl_get_matrix                    (CoglMatrixMode    mode,
                                          CoglMatrix       *matrix);
void _cogl_current_matrix_state_init     (void);
void _cogl_current_matrix_state_destroy  (void);
void _cogl_current_matrix_state_flush    (void);

#endif /* __COGL_CURRENT_MATRIX_H */
