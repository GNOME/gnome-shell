/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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

#ifndef __COGL_SUB_TEXTURE_H
#define __COGL_SUB_TEXTURE_H

#include "cogl-handle.h"
#include "cogl-texture-private.h"

#define COGL_SUB_TEXTURE(tex) ((CoglSubTexture *) tex)

typedef struct _CoglSubTexture CoglSubTexture;

struct _CoglSubTexture
{
  CoglTexture _parent;

  /* This is the texture that was passed in to
     _cogl_sub_texture_new. If this is also a sub texture then we will
     use the full texture from that to render instead of making a
     chain. However we want to preserve the next texture in case the
     user is expecting us to keep a reference and also so that we can
     later add a cogl_sub_texture_get_full_texture() function. */
  CoglHandle  next_texture;
  /* This is the texture that will actually be used to draw. It will
     point to the end of the chain if a sub texture of a sub texture
     is created */
  CoglHandle  full_texture;

  /* The region represented by this sub-texture. This is the region of
     full_texture which won't necessarily be the same as the region
     passed to _cogl_sub_texture_new if next_texture is actually
     already a sub texture */
  int         sub_x;
  int         sub_y;
  int         sub_width;
  int         sub_height;
};

GQuark
_cogl_handle_sub_texture_get_type (void);

CoglHandle
_cogl_sub_texture_new (CoglHandle next_texture,
                       int sub_x,
                       int sub_y,
                       int sub_width,
                       int sub_height);

#endif /* __COGL_SUB_TEXTURE_H */
