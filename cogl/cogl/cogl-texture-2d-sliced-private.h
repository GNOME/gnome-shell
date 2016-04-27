/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_TEXTURE_2D_SLICED_PRIVATE_H
#define __COGL_TEXTURE_2D_SLICED_PRIVATE_H

#include "cogl-bitmap-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-sliced.h"

#include <glib.h>

struct _CoglTexture2DSliced
{
  CoglTexture _parent;

  GArray *slice_x_spans;
  GArray *slice_y_spans;
  GArray *slice_textures;
  int max_waste;
  CoglPixelFormat internal_format;
};

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_foreign (CoglContext *context,
                                          unsigned int gl_handle,
                                          unsigned int gl_target,
                                          int width,
                                          int height,
                                          int x_pot_waste,
                                          int y_pot_waste,
                                          CoglPixelFormat format);

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_bitmap (CoglBitmap *bmp,
                                         int max_waste,
                                         CoglBool can_convert_in_place);

#endif /* __COGL_TEXTURE_2D_SLICED_PRIVATE_H */
