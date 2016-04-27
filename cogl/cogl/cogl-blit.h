/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 */

#ifndef __COGL_BLIT_H
#define __COGL_BLIT_H

#include <glib.h>
#include "cogl-object-private.h"
#include "cogl-texture.h"
#include "cogl-framebuffer.h"

/* This structures and functions are used when a series of blits needs
   to be performed between two textures. In this case there are
   multiple methods we can use, most of which involve transferring
   between an FBO bound to the texture. */

typedef struct _CoglBlitData CoglBlitData;

typedef CoglBool (* CoglBlitBeginFunc) (CoglBlitData *data);
typedef void (* CoglBlitEndFunc) (CoglBlitData *data);

typedef void (* CoglBlitFunc) (CoglBlitData *data,
                               int src_x,
                               int src_y,
                               int dst_x,
                               int dst_y,
                               int width,
                               int height);

typedef struct
{
  const char *name;
  CoglBlitBeginFunc begin_func;
  CoglBlitFunc blit_func;
  CoglBlitEndFunc end_func;
} CoglBlitMode;

struct _CoglBlitData
{
  CoglTexture *src_tex, *dst_tex;

  unsigned int src_width;
  unsigned int src_height;

  const CoglBlitMode *blit_mode;

  /* If we're not using an FBO then we g_malloc a buffer and copy the
     complete texture data in */
  unsigned char *image_data;
  CoglPixelFormat format;

  int bpp;

  CoglFramebuffer *src_fb;
  CoglFramebuffer *dest_fb;
  CoglPipeline *pipeline;
};

void
_cogl_blit_begin (CoglBlitData *data,
                  CoglTexture *dst_tex,
                  CoglTexture *src_tex);

void
_cogl_blit (CoglBlitData *data,
            int src_x,
            int src_y,
            int dst_x,
            int dst_y,
            int width,
            int height);

void
_cogl_blit_end (CoglBlitData *data);

#endif /* __COGL_BLIT_H */
