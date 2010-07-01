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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TEXTURE_3D_H
#define __COGL_TEXTURE_3D_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-texture-3d
 * @short_description: Fuctions for creating and manipulating 3D textures
 *
 * These functions allow 3D textures to be used. 3D textures can be
 * thought of as layers of 2D images arranged into a cuboid
 * shape. When choosing a texel from the texture, Cogl will take into
 * account the 'r' texture coordinate to select one of the images.
 */

/* All of the cogl-texture-3d API is currently experimental so we
 * suffix the actual symbols with _EXP so if somone is monitoring for
 * ABI changes it will hopefully be clearer to them what's going on if
 * any of the symbols dissapear at a later date.
 */
#define cogl_texture_3d_new_with_size cogl_texture_3d_new_with_size_EXP
#define cogl_texture_3d_new_from_data cogl_texture_3d_new_from_data_EXP
#define cogl_is_texture_3d cogl_is_texture_3d_EXP

/**
 * cogl_texture_3d_new_with_size:
 * @width: width of the texture in pixels.
 * @height: height of the texture in pixels.
 * @depth: depth of the texture in pixels.
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU
 *    storage of the texture.
 * @error: A GError return location.
 *
 * Creates a new Cogl 3D texture with the specified dimensions and
 * pixel format.
 *
 * Note that this function will throw a #GError if
 * %COGL_FEATURE_TEXTURE_3D is not advertised. It can also fail if the
 * requested dimensions are not supported by the GPU.
 *
 * Return value: a new handle to a CoglTexture3D object or
 *   %COGL_INVALID_HANDLE on failure.
 * Since: 1.4
 * Stability: Unstable
 */
CoglHandle
cogl_texture_3d_new_with_size (unsigned int     width,
                               unsigned int     height,
                               unsigned int     depth,
                               CoglTextureFlags flags,
                               CoglPixelFormat  internal_format,
                               GError         **error);

/**
 * cogl_texture_3d_new_from_data:
 * @width: width of the texture in pixels.
 * @height: height of the texture in pixels.
 * @depth: depth of the texture in pixels.
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE
 * @format: the #CoglPixelFormat the buffer is stored in in RAM
 * @internal_format: the #CoglPixelFormat that will be used for storing
 *    the buffer on the GPU. If COGL_PIXEL_FORMAT_ANY is given then a
 *    premultiplied format similar to the format of the source data will
 *    be used. The default blending equations of Cogl expect premultiplied
 *    color data; the main use of passing a non-premultiplied format here
 *    is if you have non-premultiplied source data and are going to adjust
 *    the blend mode (see cogl_material_set_blend()) or use the data for
 *    something other than straight blending.
 * @rowstride: the memory offset in bytes between the starts of
 *    scanlines in @data or 0 to infer it from the width and format
 * @image_stride: the number of bytes from one image to the next. This
 *    can be used to add padding between the images in a similar way
 *    that the rowstride can be used to add padding between
 *    rows. Alternatively 0 can be passed to infer the @image_stride
 *    from the @height.
 * @data: pointer the memory region where the source buffer resides
 * @error: A GError return location.
 *
 * Creates a new 3D texture and initializes it with @data. The data is
 * assumed to be packed array of @depth images. There can be padding
 * between the images using @image_stride.
 *
 * Note that this function will throw a #GError if
 * %COGL_FEATURE_TEXTURE_3D is not advertised. It can also fail if the
 * requested dimensions are not supported by the GPU.
 *
 * Return value: the newly created texture or %COGL_INVALID_HANDLE if
 *   there was an error.
 * Since: 1.4
 * Stability: Unstable
 */
CoglHandle
cogl_texture_3d_new_from_data (unsigned int      width,
                               unsigned int      height,
                               unsigned int      depth,
                               CoglTextureFlags  flags,
                               CoglPixelFormat   format,
                               CoglPixelFormat   internal_format,
                               unsigned int      rowstride,
                               unsigned int      image_stride,
                               const guint8     *data,
                               GError          **error);

/**
 * cogl_is_texture_3d:
 * @handle: a #CoglHandle
 *
 * Checks whether @handle is a #CoglHandle for a 3D texture.
 *
 * Return value: %TRUE if the passed handle represents a 3D texture
 *   and %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
gboolean
cogl_is_texture_3d (CoglHandle handle);

G_END_DECLS

#endif /* __COGL_TEXTURE_3D_H */
