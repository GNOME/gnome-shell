/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
                               unsigned int src_x,
                               unsigned int src_y,
                               unsigned int dst_x,
                               unsigned int dst_y,
                               unsigned int width,
                               unsigned int height);

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

  CoglFramebuffer *fb;
  CoglPipeline *pipeline;
};

void
_cogl_blit_begin (CoglBlitData *data,
                  CoglTexture *dst_tex,
                  CoglTexture *src_tex);

void
_cogl_blit (CoglBlitData *data,
            unsigned int src_x,
            unsigned int src_y,
            unsigned int dst_x,
            unsigned int dst_y,
            unsigned int width,
            unsigned int height);

void
_cogl_blit_end (CoglBlitData *data);

#endif /* __COGL_BLIT_H */
