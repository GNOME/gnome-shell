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
 */

#ifndef __COGL_BUFFER_PRIVATE_H__
#define __COGL_BUFFER_PRIVATE_H__

#include <glib.h>

#include "cogl-handle.h"
#include "cogl-buffer.h"

G_BEGIN_DECLS

#define COGL_BUFFER(buffer)     ((CoglBuffer *)(buffer))

#define COGL_BUFFER_SET_FLAG(buffer, flag) \
  ((buffer)->flags |= (COGL_BUFFER_FLAG_ ## flag))

#define COGL_BUFFER_CLEAR_FLAG(buffer, flag) \
  ((buffer)->flags &= ~(COGL_BUFFER_FLAG_ ## flag))

#define COGL_BUFFER_FLAG_IS_SET(buffer, flag) \
  ((buffer)->flags & (COGL_BUFFER_FLAG_ ## flag))

typedef struct _CoglBuffer       CoglBuffer;
typedef struct _CoglBufferVtable CoglBufferVtable;

struct _CoglBufferVtable
{
  guint8 * (* map)          (CoglBuffer       *buffer,
                             CoglBufferAccess  access);

  void     (* unmap)        (CoglBuffer *buffer);

  gboolean (* set_data)     (CoglBuffer   *buffer,
                             unsigned int  offset,
                             const guint8 *data,
                             unsigned int  size);
};

typedef enum _CoglBufferFlags
{
  COGL_BUFFER_FLAG_NONE          = 0,
  COGL_BUFFER_FLAG_BUFFER_OBJECT = 1UL << 0,  /* real openGL buffer object */
  COGL_BUFFER_FLAG_MAPPED        = 1UL << 1
} CoglBufferFlags;

struct _CoglBuffer
{
  CoglHandleObject        _parent;
  const CoglBufferVtable *vtable;

  CoglBufferFlags         flags;

  GLuint                  gl_handle;  /* OpenGL handle */
  unsigned int            size;       /* size of the buffer, in bytes */
  CoglBufferUsageHint     usage_hint;
  CoglBufferUpdateHint    update_hint;

  guint8                 *data;      /* points to the mapped memory when
                                      * the CoglBuffer is a VBO, PBO, ... or
                                      * points to allocated memory in the
                                      * fallback paths */
};

void    _cogl_buffer_initialize         (CoglBuffer          *buffer,
                                         unsigned int         size,
                                         CoglBufferUsageHint  usage_hint,
                                         CoglBufferUpdateHint update_hint);
void    _cogl_buffer_fini               (CoglBuffer *buffer);
void    _cogl_buffer_bind               (CoglBuffer *buffer,
                                         GLenum      target);
GLenum  _cogl_buffer_access_to_gl_enum  (CoglBufferAccess access);
GLenum  _cogl_buffer_hints_to_gl_enum   (CoglBufferUsageHint  usage_hint,
                                         CoglBufferUpdateHint update_hint);

G_END_DECLS

#endif /* __COGL_BUFFER_PRIVATE_H__ */
