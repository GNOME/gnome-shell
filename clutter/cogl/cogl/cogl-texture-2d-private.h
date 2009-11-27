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

#ifndef __COGL_TEXTURE_2D_H
#define __COGL_TEXTURE_2D_H

#include "cogl-handle.h"
#include "cogl-material-private.h"
#include "cogl-texture-private.h"

#define COGL_TEXTURE_2D(tex) ((CoglTexture2D *) tex)

typedef struct _CoglTexture2D CoglTexture2D;

struct _CoglTexture2D
{
  CoglTexture     _parent;

  /* The internal format of the GL texture represented as a
     CoglPixelFormat */
  CoglPixelFormat format;
  /* The internal format of the GL texture represented as a GL enum */
  GLenum          gl_format;
  /* The texture object number */
  GLuint          gl_texture;
  gint            width;
  gint            height;
  GLenum          min_filter;
  GLenum          mag_filter;
  GLint           wrap_mode;
  gboolean        auto_mipmap;
  gboolean        mipmaps_dirty;
};

GQuark
_cogl_handle_texture_2d_get_type (void);

CoglHandle
_cogl_texture_2d_new_with_size (unsigned int     width,
                                unsigned int     height,
                                CoglTextureFlags flags,
                                CoglPixelFormat  internal_format);

CoglHandle
_cogl_texture_2d_new_from_bitmap (CoglHandle       bmp_handle,
                                  CoglTextureFlags flags,
                                  CoglPixelFormat  internal_format);

#endif /* __COGL_TEXTURE_2D_H */
