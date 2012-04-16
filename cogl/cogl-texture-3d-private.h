/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_TEXTURE_3D_PRIVATE_H
#define __COGL_TEXTURE_3D_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-3d.h"

struct _CoglTexture3D
{
  CoglTexture     _parent;

  /* The internal format of the GL texture represented as a
     CoglPixelFormat */
  CoglPixelFormat format;
  /* The internal format of the GL texture represented as a GL enum */
  GLenum          gl_format;
  /* The texture object number */
  GLuint          gl_texture;
  int             width;
  int             height;
  int             depth;
  GLenum          min_filter;
  GLenum          mag_filter;
  GLint           wrap_mode_s;
  GLint           wrap_mode_t;
  GLint           wrap_mode_p;
  CoglBool        auto_mipmap;
  CoglBool        mipmaps_dirty;

  CoglTexturePixel first_pixel;
};

#endif /* __COGL_TEXTURE_3D_PRIVATE_H */
