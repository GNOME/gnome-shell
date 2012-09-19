/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_BUFFER_PRIVATE_H__
#define __COGL_BUFFER_PRIVATE_H__

#include <glib.h>

#include "cogl-object-private.h"
#include "cogl-buffer.h"
#include "cogl-context.h"
#include "cogl-gl-header.h"

G_BEGIN_DECLS

typedef struct _CoglBufferVtable CoglBufferVtable;

struct _CoglBufferVtable
{
  void * (* map) (CoglBuffer       *buffer,
                  CoglBufferAccess  access,
                  CoglBufferMapHint hints);

  void (* unmap) (CoglBuffer *buffer);

  CoglBool (* set_data) (CoglBuffer   *buffer,
                         unsigned int  offset,
                         const void   *data,
                         unsigned int  size);
};

typedef enum _CoglBufferFlags
{
  COGL_BUFFER_FLAG_NONE            = 0,
  COGL_BUFFER_FLAG_BUFFER_OBJECT   = 1UL << 0,  /* real openGL buffer object */
  COGL_BUFFER_FLAG_MAPPED          = 1UL << 1,
  COGL_BUFFER_FLAG_MAPPED_FALLBACK = 1UL << 2
} CoglBufferFlags;

typedef enum {
  COGL_BUFFER_USAGE_HINT_TEXTURE,
  COGL_BUFFER_USAGE_HINT_ATTRIBUTE_BUFFER,
  COGL_BUFFER_USAGE_HINT_INDEX_BUFFER
} CoglBufferUsageHint;

typedef enum {
  COGL_BUFFER_BIND_TARGET_PIXEL_PACK,
  COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK,
  COGL_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER,
  COGL_BUFFER_BIND_TARGET_INDEX_BUFFER,

  COGL_BUFFER_BIND_TARGET_COUNT
} CoglBufferBindTarget;

struct _CoglBuffer
{
  CoglObject _parent;

  CoglContext *context;

  CoglBufferVtable vtable;

  CoglBufferBindTarget last_target;

  CoglBufferFlags flags;

  GLuint gl_handle; /* OpenGL handle */
  unsigned int size; /* size of the buffer, in bytes */
  CoglBufferUsageHint usage_hint;
  CoglBufferUpdateHint update_hint;

  /* points to the mapped memory when the CoglBuffer is a VBO, PBO,
   * ... or points to allocated memory in the fallback paths */
  uint8_t *data;

  int immutable_ref;

  unsigned int store_created:1;
};

/* This is used to register a type to the list of handle types that
   will be considered a texture in cogl_is_texture() */
void
_cogl_buffer_register_buffer_type (const CoglObjectClass *klass);

#define COGL_BUFFER_DEFINE(TypeName, type_name)                         \
  COGL_OBJECT_DEFINE_WITH_CODE                                          \
  (TypeName, type_name,                                                 \
   _cogl_buffer_register_buffer_type (&_cogl_##type_name##_class))

void
_cogl_buffer_initialize (CoglBuffer *buffer,
                         CoglContext *context,
                         size_t size,
                         CoglBufferBindTarget default_target,
                         CoglBufferUsageHint usage_hint,
                         CoglBufferUpdateHint update_hint);

void
_cogl_buffer_fini (CoglBuffer *buffer);

CoglBufferUsageHint
_cogl_buffer_get_usage_hint (CoglBuffer *buffer);

GLenum
_cogl_buffer_access_to_gl_enum (CoglBufferAccess access);

CoglBuffer *
_cogl_buffer_immutable_ref (CoglBuffer *buffer);

void
_cogl_buffer_immutable_unref (CoglBuffer *buffer);

/* This is a wrapper around cogl_buffer_map for internal use when we
   want to map the buffer for write only to replace the entire
   contents. If the map fails then it will fallback to writing to a
   temporary buffer. When _cogl_buffer_unmap_for_fill_or_fallback is
   called the temporary buffer will be copied into the array. Note
   that these calls share a global array so they can not be nested. */
void *
_cogl_buffer_map_for_fill_or_fallback (CoglBuffer *buffer);

void
_cogl_buffer_unmap_for_fill_or_fallback (CoglBuffer *buffer);

G_END_DECLS

#endif /* __COGL_BUFFER_PRIVATE_H__ */
