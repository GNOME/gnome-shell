/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2014 Intel Corporation.
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
