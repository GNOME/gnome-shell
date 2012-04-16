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

#include "cogl-private.h"
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

CoglPixelBuffer *
cogl_pixel_buffer_new (CoglContext *context,
                       size_t size,
                       const void *data)
{
  CoglPixelBuffer *pixel_buffer = g_slice_new0 (CoglPixelBuffer);
  CoglBuffer *buffer = COGL_BUFFER (pixel_buffer);
  CoglBool use_malloc;

  if (!(context->private_feature_flags & COGL_PRIVATE_FEATURE_PBOS))
    use_malloc = TRUE;
  else
    use_malloc = FALSE;

  /* parent's constructor */
  _cogl_buffer_initialize (buffer,
                           context,
                           size,
                           use_malloc,
                           COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK,
                           COGL_BUFFER_USAGE_HINT_TEXTURE,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

  _cogl_pixel_buffer_object_new (pixel_buffer);

  if (data)
    cogl_buffer_set_data (COGL_BUFFER (pixel_buffer),
                          0,
                          data,
                          size);

  return pixel_buffer;
}

static void
_cogl_pixel_buffer_free (CoglPixelBuffer *buffer)
{
  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (buffer));

  g_slice_free (CoglPixelBuffer, buffer);
}

