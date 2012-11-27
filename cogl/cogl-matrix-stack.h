/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010,2012 Intel Corporation.
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

#include "cogl-object-private.h"
#include "cogl-matrix.h"
#include "cogl-context.h"
#include "cogl-framebuffer.h"

typedef enum _CoglMatrixOp
{
  COGL_MATRIX_OP_LOAD_IDENTITY,
  COGL_MATRIX_OP_TRANSLATE,
  COGL_MATRIX_OP_ROTATE,
  COGL_MATRIX_OP_ROTATE_QUATERNION,
  COGL_MATRIX_OP_ROTATE_EULER,
  COGL_MATRIX_OP_SCALE,
  COGL_MATRIX_OP_MULTIPLY,
  COGL_MATRIX_OP_LOAD,
  COGL_MATRIX_OP_SAVE,
} CoglMatrixOp;

typedef struct _CoglMatrixEntry CoglMatrixEntry;

struct _CoglMatrixEntry
{
  CoglMatrixEntry *parent;
  CoglMatrixOp op;
  unsigned int ref_count;

  /* used for performance tracing */
  int composite_gets;
};

typedef struct _CoglMatrixEntryTranslate
{
  CoglMatrixEntry _parent_data;

  float x;
  float y;
  float z;

} CoglMatrixEntryTranslate;

typedef struct _CoglMatrixEntryRotate
{
  CoglMatrixEntry _parent_data;

  float angle;
  float x;
  float y;
  float z;

} CoglMatrixEntryRotate;

typedef struct _CoglMatrixEntryRotateEuler
{
  CoglMatrixEntry _parent_data;

  /* This doesn't store an actual CoglEuler in order to avoid the
   * padding */
  float heading;
  float pitch;
  float roll;
} CoglMatrixEntryRotateEuler;

typedef struct _CoglMatrixEntryRotateQuaternion
{
  CoglMatrixEntry _parent_data;

  /* This doesn't store an actual CoglQuaternion in order to avoid the
   * padding */
  float values[4];
} CoglMatrixEntryRotateQuaternion;

typedef struct _CoglMatrixEntryScale
{
  CoglMatrixEntry _parent_data;

  float x;
  float y;
  float z;

} CoglMatrixEntryScale;

typedef struct _CoglMatrixEntryMultiply
{
  CoglMatrixEntry _parent_data;

  CoglMatrix *matrix;

} CoglMatrixEntryMultiply;

typedef struct _CoglMatrixEntryLoad
{
  CoglMatrixEntry _parent_data;

  CoglMatrix *matrix;

} CoglMatrixEntryLoad;

typedef struct _CoglMatrixEntrySave
{
  CoglMatrixEntry _parent_data;

  CoglBool cache_valid;
  CoglMatrix *cache;

} CoglMatrixEntrySave;

typedef union _CoglMatrixEntryFull
{
  CoglMatrixEntry any;
  CoglMatrixEntryTranslate translate;
  CoglMatrixEntryRotate rotate;
  CoglMatrixEntryRotateEuler rotate_euler;
  CoglMatrixEntryRotateQuaternion rotate_quaternion;
  CoglMatrixEntryScale scale;
  CoglMatrixEntryMultiply multiply;
  CoglMatrixEntryLoad load;
  CoglMatrixEntrySave save;
} CoglMatrixEntryFull;

typedef struct _CoglMatrixStack
{
  CoglObject _parent;

  CoglMatrixEntry *last_entry;
} CoglMatrixStack;

typedef struct _CoglMatrixEntryCache
{
  CoglMatrixEntry *entry;
  CoglBool flushed_identity;
  CoglBool flipped;
} CoglMatrixEntryCache;

CoglMatrixStack *
_cogl_matrix_stack_new (void);

void
_cogl_matrix_stack_push (CoglMatrixStack *stack);

void
_cogl_matrix_stack_pop (CoglMatrixStack *stack);

void
_cogl_matrix_entry_identity_init (CoglMatrixEntry *entry);

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
_cogl_matrix_stack_rotate_quaternion (CoglMatrixStack *stack,
                                      const CoglQuaternion *quaternion);
void
_cogl_matrix_stack_rotate_euler (CoglMatrixStack *stack,
                                 const CoglEuler *euler);
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
_cogl_matrix_stack_orthographic (CoglMatrixStack *stack,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2,
                                 float near,
                                 float far);

CoglBool
_cogl_matrix_stack_get_inverse (CoglMatrixStack *stack,
                                CoglMatrix *inverse);

/* NB: This function only *sometimes* returns a pointer to a matrix if
 * the matrix returned didn't need to be composed of multiple
 * operations */
CoglMatrix *
_cogl_matrix_stack_get (CoglMatrixStack *stack,
                        CoglMatrix *matrix);

/* NB: This function only *sometimes* returns a pointer to a matrix if
 * the matrix returned didn't need to be composed of multiple
 * operations */
CoglMatrix *
_cogl_matrix_entry_get (CoglMatrixEntry *entry,
                        CoglMatrix *matrix);

void
_cogl_matrix_stack_set (CoglMatrixStack *stack,
                        const CoglMatrix *matrix);

CoglBool
_cogl_matrix_entry_calculate_translation (CoglMatrixEntry *entry0,
                                          CoglMatrixEntry *entry1,
                                          float *x,
                                          float *y,
                                          float *z);

/* If this returns TRUE then the entry is definitely the identity
 * matrix. If it returns FALSE it may or may not be the identity
 * matrix but no expensive comparison is performed to verify it. */
CoglBool
_cogl_matrix_entry_has_identity_flag (CoglMatrixEntry *entry);

CoglBool
_cogl_matrix_entry_fast_equal (CoglMatrixEntry *entry0,
                               CoglMatrixEntry *entry1);

CoglBool
_cogl_matrix_entry_equal (CoglMatrixEntry *entry0,
                          CoglMatrixEntry *entry1);

void
_cogl_matrix_entry_print (CoglMatrixEntry *entry);

CoglMatrixEntry *
_cogl_matrix_entry_ref (CoglMatrixEntry *entry);

void
_cogl_matrix_entry_unref (CoglMatrixEntry *entry);

typedef enum {
  COGL_MATRIX_MODELVIEW,
  COGL_MATRIX_PROJECTION,
  COGL_MATRIX_TEXTURE
} CoglMatrixMode;

void
_cogl_matrix_entry_flush_to_gl_builtins (CoglContext *ctx,
                                         CoglMatrixEntry *entry,
                                         CoglMatrixMode mode,
                                         CoglFramebuffer *framebuffer,
                                         CoglBool disable_flip);

void
_cogl_matrix_entry_cache_init (CoglMatrixEntryCache *cache);

CoglBool
_cogl_matrix_entry_cache_maybe_update (CoglMatrixEntryCache *cache,
                                       CoglMatrixEntry *entry,
                                       CoglBool flip);

void
_cogl_matrix_entry_cache_destroy (CoglMatrixEntryCache *cache);

CoglBool
_cogl_is_matrix_stack (void *object);

#endif /* __COGL_MATRIX_STACK_H */
