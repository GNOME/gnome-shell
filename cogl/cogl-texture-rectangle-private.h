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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_TEXTURE_RECTANGLE_H
#define __COGL_TEXTURE_RECTANGLE_H

#include "cogl-pipeline-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-rectangle.h"

struct _CoglTextureRectangle
{
  CoglTexture _parent;

  /* The internal format of the texture represented as a
     CoglPixelFormat */
  CoglPixelFormat format;

  /* TODO: factor out these OpenGL specific members into some form
   * of driver private state. */

  /* The internal format of the GL texture represented as a GL enum */
  GLenum gl_format;
  /* The texture object number */
  GLuint gl_texture;
  GLenum gl_legacy_texobj_min_filter;
  GLenum gl_legacy_texobj_mag_filter;
  GLint gl_legacy_texobj_wrap_mode_s;
  GLint gl_legacy_texobj_wrap_mode_t;
  CoglBool is_foreign;
};

CoglTextureRectangle *
_cogl_texture_rectangle_new_from_foreign (GLuint gl_handle,
                                          GLuint width,
                                          GLuint height,
                                          CoglPixelFormat format);

#endif /* __COGL_TEXTURE_RECTANGLE_H */
