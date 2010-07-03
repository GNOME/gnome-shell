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

#ifndef __COGL_PIXEL_ARRAY_PRIVATE_H__
#define __COGL_PIXEL_ARRAY_PRIVATE_H__

#include "cogl-handle.h"
#include "cogl-buffer-private.h"

#include <glib.h>

G_BEGIN_DECLS

#define COGL_PIXEL_ARRAY(array)     ((CoglPixelArray *)(array))

#define COGL_PIXEL_ARRAY_SET_FLAG(array, flag) \
  ((array)->flags |= (COGL_PIXEL_ARRAY_FLAG_ ## flag))

#define COGL_PIXEL_ARRAY_CLEAR_FLAG(array, flag) \
  ((array)->flags &= ~(COGL_PIXEL_ARRAY_FLAG_ ## flag))

#define COGL_PIXEL_ARRAY_FLAG_IS_SET(array, flag) \
  ((array)->flags & (COGL_PIXEL_ARRAY_FLAG_ ## flag))

typedef enum _CoglPixelArrayFlags
{
  COGL_PIXEL_ARRAY_FLAG_NONE = 0,
  COGL_PIXEL_ARRAY_FLAG_STORE_CREATED = 1 << 0,
} CoglPixelArrayFlags;

struct _CoglPixelArray
{
  CoglBuffer            _parent;

  CoglPixelArrayFlags  flags;

  GLenum                gl_target;
  CoglPixelFormat       format;
  unsigned int          width;
  unsigned int          height;
  unsigned int          stride;

};

GQuark
_cogl_handle_pixel_array_get_type (void);

G_END_DECLS

#endif /* __COGL_PIXEL_ARRAY_PRIVATE_H__ */
