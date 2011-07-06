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

/* For an overview of the functionality implemented here, please see
 * cogl-buffer-array.h, which contains the gtk-doc section overview for the
 * Pixel Buffers API.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-object.h"
#include "cogl-pixel-buffer-private.h"
#include "cogl-pixel-buffer.h"

/*
 * GL/GLES compatibility defines for the buffer API:
 */

#if defined (HAVE_COGL_GL)

#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER GL_PIXEL_UNPACK_BUFFER_ARB
#endif

#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER GL_PIXEL_PACK_BUFFER_ARB
#endif

#endif

static void
_cogl_pixel_buffer_free (CoglPixelBuffer *buffer);

COGL_BUFFER_DEFINE (PixelBuffer, pixel_buffer)

static CoglPixelBuffer *
_cogl_pixel_buffer_new (unsigned int size)
{
  CoglPixelBuffer *pixel_buffer = g_slice_new0 (CoglPixelBuffer);
  CoglBuffer *buffer = COGL_BUFFER (pixel_buffer);
  gboolean use_malloc;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  if (!cogl_features_available (COGL_FEATURE_PBOS))
    use_malloc = TRUE;
  else
    use_malloc = FALSE;

  /* parent's constructor */
  _cogl_buffer_initialize (buffer,
                           size,
                           use_malloc,
                           COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK,
                           COGL_BUFFER_USAGE_HINT_TEXTURE,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

  /* return COGL_INVALID_HANDLE; */
  return _cogl_pixel_buffer_object_new (pixel_buffer);
}

CoglPixelBuffer *
cogl_pixel_buffer_new_with_size (unsigned int    width,
                                 unsigned int    height,
                                 CoglPixelFormat format,
                                 unsigned int   *rowstride)
{
  CoglPixelBuffer *buffer;
  CoglPixelBuffer *pixel_buffer;
  unsigned int stride;

  /* creating a buffer to store "any" format does not make sense */
  if (G_UNLIKELY (format == COGL_PIXEL_FORMAT_ANY))
    return COGL_INVALID_HANDLE;

  /* for now we fallback to cogl_pixel_buffer_new, later, we could ask
   * libdrm a tiled buffer for instance */
  stride = width * _cogl_get_format_bpp (format);
  if (rowstride)
    *rowstride = stride;

  buffer = _cogl_pixel_buffer_new (height * stride);
  if (G_UNLIKELY (buffer == COGL_INVALID_HANDLE))
    return COGL_INVALID_HANDLE;

  pixel_buffer = COGL_PIXEL_BUFFER (buffer);
  pixel_buffer->width = width;
  pixel_buffer->height = height;
  pixel_buffer->format = format;
  pixel_buffer->stride = stride;

  return buffer;
}

static void
_cogl_pixel_buffer_free (CoglPixelBuffer *buffer)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (buffer));

  g_slice_free (CoglPixelBuffer, buffer);
}

