/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Intel Corporation.
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
 */

#include <config.h>

#include "cogl-types.h"
#include "cogl-texture.h"
#include "cogl-texture-private.h"
#include "cogl-object-private.h"
#include "deprecated/cogl-texture-deprecated.h"

CoglPixelFormat
cogl_texture_get_format (CoglTexture *texture)
{
  return _cogl_texture_get_format (texture);
}

unsigned int
cogl_texture_get_rowstride (CoglTexture *texture)
{
  CoglPixelFormat format = cogl_texture_get_format (texture);
  /* FIXME: This function should go away. It previously just returned
     the rowstride that was used to upload the data as far as I can
     tell. This is not helpful */

  /* Just guess at a suitable rowstride */
  return (_cogl_pixel_format_get_bytes_per_pixel (format)
          * cogl_texture_get_width (texture));
}

void *
cogl_texture_ref (void *object)
{
  if (!cogl_is_texture (object))
    return NULL;

  _COGL_OBJECT_DEBUG_REF (CoglTexture, object);

  cogl_object_ref (object);

  return object;
}

void
cogl_texture_unref (void *object)
{
  if (!cogl_is_texture (object))
    {
      g_warning (G_STRINGIFY (cogl_texture_unref)
                 ": Ignoring unref of CoglObject "
                 "due to type mismatch");
      return;
    }

  _COGL_OBJECT_DEBUG_UNREF (CoglTexture, object);

  cogl_object_unref (object);
}
