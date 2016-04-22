/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
#include "cogl-gtype-private.h"

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
COGL_GTYPE_DEFINE_CLASS (PixelBuffer, pixel_buffer)

static CoglPixelBuffer *
_cogl_pixel_buffer_new (CoglContext *context,
                        size_t size,
                        const void *data,
                        CoglError **error)
{
  CoglPixelBuffer *pixel_buffer = g_slice_new0 (CoglPixelBuffer);
  CoglBuffer *buffer = COGL_BUFFER (pixel_buffer);

  /* parent's constructor */
  _cogl_buffer_initialize (buffer,
                           context,
                           size,
                           COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK,
                           COGL_BUFFER_USAGE_HINT_TEXTURE,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

  _cogl_pixel_buffer_object_new (pixel_buffer);

  if (data)
    {
      if (!_cogl_buffer_set_data (COGL_BUFFER (pixel_buffer),
                                  0,
                                  data,
                                  size,
                                  error))
        {
          cogl_object_unref (pixel_buffer);
          return NULL;
        }
    }

  return pixel_buffer;
}

CoglPixelBuffer *
cogl_pixel_buffer_new (CoglContext *context,
                       size_t size,
                       const void *data)
{
  CoglError *ignore_error = NULL;
  CoglPixelBuffer *buffer =
    _cogl_pixel_buffer_new (context, size, data, &ignore_error);
  if (!buffer)
    cogl_error_free (ignore_error);
  return buffer;
}

static void
_cogl_pixel_buffer_free (CoglPixelBuffer *buffer)
{
  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (buffer));

  g_slice_free (CoglPixelBuffer, buffer);
}

