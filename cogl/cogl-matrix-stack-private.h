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

#ifndef _COGL_MATRIX_STACK_PRIVATE_H_
#define _COGL_MATRIX_STACK_PRIVATE_H_

#include "cogl-object-private.h"
#include "cogl-matrix-stack.h"
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

struct _CoglMatrixEntry
{
  CoglMatrixEntry *parent;
  CoglMatrixOp op;
  unsigned int ref_count;

#ifdef COGL_DEBUG_ENABLED
  /* used for performance tracing */
  int composite_gets;
#endif
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

struct _CoglMatrixStack
{
  CoglObject _parent;

  CoglContext *context;

  CoglMatrixEntry *last_entry;
};

typedef struct _CoglMatrixEntryCache
{
  CoglMatrixEntry *entry;
  CoglBool flushed_identity;
  CoglBool flipped;
} CoglMatrixEntryCache;

void
_cogl_matrix_entry_identity_init (CoglMatrixEntry *entry);

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

#endif /* _COGL_MATRIX_STACK_PRIVATE_H_ */
