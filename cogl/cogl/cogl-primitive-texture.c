/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
