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

#include "cogl-handle.h"
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
  gboolean        auto_mipmap;
  gboolean        mipmaps_dirty;

  CoglTexturePixel first_pixel;
};

/*
 * cogl_texture_3d_new_from_bitmap:
 * @bmp_handle: A #CoglBitmap object.
 * @height: height of the texture in pixels.
 * @depth: depth of the texture in pixels.
 * @internal_format: the #CoglPixelFormat that will be used for storing
 *    the buffer on the GPU. If COGL_PIXEL_FORMAT_ANY is given then a
 *    premultiplied format similar to the format of the source data will
 *    be used. The default blending equations of Cogl expect premultiplied
 *    color data; the main use of passing a non-premultiplied format here
 *    is if you have non-premultiplied source data and are going to adjust
 *    the blend mode (see cogl_pipeline_set_blend()) or use the data for
 *    something other than straight blending.
 * @error: A GError return location.
 *
 * Creates a new 3D texture and initializes it with the images in
 * @bmp_handle. The images are assumed to be packed together after one
 * another in the increasing y axis. The height of individual image is
 * given as @height and the number of images is given in @depth. The
 * actual height of the bitmap can be larger than @height × @depth. In
 * this case it assumes there is padding between the images.
 *
 * Return value: the newly created texture or %NULL if
 *   there was an error.
 */
CoglTexture3D *
_cogl_texture_3d_new_from_bitmap (CoglBitmap *bmp,
                                  unsigned int height,
                                  unsigned int depth,
                                  CoglPixelFormat internal_format,
                                  GError **error);

#endif /* __COGL_TEXTURE_3D_PRIVATE_H */
