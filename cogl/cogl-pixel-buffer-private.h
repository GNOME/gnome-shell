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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Damien Lespiau <damien.lespiau@intel.com>
 */

#ifndef __COGL_PIXEL_BUFFER_PRIVATE_H__
#define __COGL_PIXEL_BUFFER_PRIVATE_H__

#include "cogl-handle.h"
#include "cogl-buffer-private.h"

#include <glib.h>

G_BEGIN_DECLS

#define COGL_PIXEL_BUFFER(buffer)     ((CoglPixelBuffer *)(buffer))

#define COGL_PIXEL_BUFFER_SET_FLAG(buffer, flag) \
  ((buffer)->flags |= (COGL_PIXEL_BUFFER_FLAG_ ## flag))

#define COGL_PIXEL_BUFFER_CLEAR_FLAG(buffer, flag) \
  ((buffer)->flags &= ~(COGL_PIXEL_BUFFER_FLAG_ ## flag))

#define COGL_PIXEL_BUFFER_FLAG_IS_SET(buffer, flag) \
  ((buffer)->flags & (COGL_PIXEL_BUFFER_FLAG_ ## flag))

typedef enum _CoglPixelBufferFlags
{
  COGL_PIXEL_BUFFER_FLAG_NONE = 0,
  COGL_PIXEL_BUFFER_FLAG_STORE_CREATED = 1 << 0,
} CoglPixelBufferFlags;

typedef struct _CoglPixelBuffer
{
  CoglBuffer            _parent;

  CoglPixelBufferFlags  flags;

  GLenum                gl_target;
  CoglPixelFormat       format;
  unsigned int          width;
  unsigned int          height;
  unsigned int          stride;

} CoglPixelBuffer;

GQuark
_cogl_handle_pixel_buffer_get_type (void);

G_END_DECLS

#endif /* __COGL_PIXEL_BUFFER_PRIVATE_H__ */
