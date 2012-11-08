/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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
  CoglTexture       _parent;
  GArray           *slice_x_spans;
  GArray           *slice_y_spans;
  GArray           *slice_textures;
  int               max_waste;

  int               width;
  int               height;
};

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_foreign (GLuint           gl_handle,
                                          GLenum           gl_target,
                                          GLuint           width,
                                          GLuint           height,
                                          GLuint           x_pot_waste,
                                          GLuint           y_pot_waste,
                                          CoglPixelFormat  format);

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_bitmap (CoglBitmap *bmp,
                                         CoglTextureFlags flags,
                                         CoglPixelFormat internal_format,
                                         CoglError **error);

#endif /* __COGL_TEXTURE_2D_SLICED_PRIVATE_H */
