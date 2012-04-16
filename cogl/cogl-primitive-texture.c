/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 * Authors:
 *  Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-primitive-texture.h"
#include "cogl-texture-private.h"

CoglBool
cogl_is_primitive_texture (void *object)
{
  return (cogl_is_texture (object) &&
          COGL_TEXTURE (object)->vtable->is_primitive);
}

void
cogl_primitive_texture_set_auto_mipmap (CoglPrimitiveTexture *primitive_texture,
                                        CoglBool value)
{
  CoglTexture *texture;

  _COGL_RETURN_IF_FAIL (cogl_is_primitive_texture (primitive_texture));

  texture = COGL_TEXTURE (primitive_texture);

  g_assert (texture->vtable->set_auto_mipmap != NULL);

  texture->vtable->set_auto_mipmap (texture, value);
}
