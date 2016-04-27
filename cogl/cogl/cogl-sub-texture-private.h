/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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

#ifndef __COGL_SUB_TEXTURE_PRIVATE_H
#define __COGL_SUB_TEXTURE_PRIVATE_H

#include "cogl-texture-private.h"

#include <glib.h>

struct _CoglSubTexture
{
  CoglTexture _parent;

  /* This is the texture that was passed in to
     _cogl_sub_texture_new. If this is also a sub texture then we will
     use the full texture from that to render instead of making a
     chain. However we want to preserve the next texture in case the
     user is expecting us to keep a reference and also so that we can
     later add a cogl_sub_texture_get_parent_texture() function. */
  CoglTexture *next_texture;
  /* This is the texture that will actually be used to draw. It will
     point to the end of the chain if a sub texture of a sub texture
     is created */
  CoglTexture *full_texture;

  /* The offset of the region represented by this sub-texture. This is
   * the offset in full_texture which won't necessarily be the same as
   * the offset passed to _cogl_sub_texture_new if next_texture is
   * actually already a sub texture */
  int sub_x;
  int sub_y;
};

#endif /* __COGL_SUB_TEXTURE_PRIVATE_H */
