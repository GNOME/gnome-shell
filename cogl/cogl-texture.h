/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TEXTURE_H__
#define __COGL_TEXTURE_H__

/* We forward declare the CoglTexture type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglTexture CoglTexture;

#include <cogl/cogl-types.h>
#include <cogl/cogl-defines.h>
#if defined (COGL_ENABLE_EXPERIMENTAL_API)
#include <cogl/cogl-pixel-buffer.h>
#endif
#include <cogl/cogl-bitmap.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-texture
 * @short_description: Fuctions for creating and manipulating textures
 *
 * Cogl allows creating and manipulating textures using a uniform
 * API that tries to hide all the various complexities of creating,
 * loading and manipulating textures.
 */

#define COGL_TEXTURE(X) ((CoglTexture *)X)

#define COGL_TEXTURE_MAX_WASTE  127

/**
 * COGL_TEXTURE_ERROR:
 *
 * #GError domain for texture errors.
 *
 * Since: 2.0
 * Stability: Unstable
 */
#define COGL_TEXTURE_ERROR (cogl_texture_error_quark ())


/**
 * CoglTextureError:
 * @COGL_TEXTURE_ERROR_SIZE: Unsupported size
 *
 * Error codes that can be thrown when allocating textures.
 *
 * Since: 2.0
 * Stability: Unstable
 */
typedef enum {
  COGL_TEXTURE_ERROR_SIZE,
  COGL_TEXTURE_ERROR_FORMAT,
  COGL_TEXTURE_ERROR_BAD_PARAMETER,
  COGL_TEXTURE_ERROR_TYPE
} CoglTextureError;

/**
 * CoglTextureType:
 * @COGL_TEXTURE_TYPE_2D: A #CoglTexture2D
 * @COGL_TEXTURE_TYPE_3D: A #CoglTexture3D
 * @COGL_TEXTURE_TYPE_RECTANGLE: A #CoglTextureRectangle
 *
 * Constants representing the underlying hardware texture type of a
 * #CoglTexture.
 *
 * Stability: unstable
 * Since: 1.10
 */
typedef enum {
  COGL_TEXTURE_TYPE_2D,
  COGL_TEXTURE_TYPE_3D,
  COGL_TEXTURE_TYPE_RECTANGLE
} CoglTextureType;

GQuark cogl_texture_error_quark (void);

/**
 * cogl_texture_new_with_size:
 * @width: width of texture in pixels.
 * @height: height of texture in pixels.
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 *    texture.
 *
 * Creates a new #CoglTexture with the specified dimensions and pixel format.
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 *
 * Since: 0.8
 */
CoglTexture *
cogl_texture_new_with_size (unsigned int width,
                            unsigned int height,
                            CoglTextureFlags flags,
                            CoglPixelFormat internal_format);

/**
 * cogl_texture_new_from_file:
 * @filename: the file to load
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 *    texture. If %COGL_PIXEL_FORMAT_ANY is given then a premultiplied
 *    format similar to the format of the source data will be used. The
 *    default blending equations of Cogl expect premultiplied color data;
 *    the main use of passing a non-premultiplied format here is if you
 *    have non-premultiplied source data and are going to adjust the blend
 *    mode (see cogl_material_set_blend()) or use the data for something
 *    other than straight blending.
 * @error: return location for a #GError or %NULL
 *
 * Creates a #CoglTexture from an image file.
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 *
 * Since: 0.8
 */
CoglTexture *
cogl_texture_new_from_file (const char       *filename,
                            CoglTextureFlags   flags,
                            CoglPixelFormat    internal_format,
                            GError           **error);

/**
 * cogl_texture_new_from_data:
 * @width: width of texture in pixels
 * @height: height of texture in pixels
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
 *    scanlines in @data
 * @data: pointer the memory region where the source buffer resides
 *
 * Creates a new #CoglTexture based on data residing in memory.
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 *
 * Since: 0.8
 */
CoglTexture *
cogl_texture_new_from_data (unsigned int      width,
                            unsigned int      height,
                            CoglTextureFlags  flags,
                            CoglPixelFormat   format,
                            CoglPixelFormat   internal_format,
                            unsigned int      rowstride,
                            const uint8_t     *data);

/**
 * cogl_texture_new_from_foreign:
 * @gl_handle: opengl handle of foreign texture.
 * @gl_target: opengl target type of foreign texture
 * @width: width of foreign texture
 * @height: height of foreign texture.
 * @x_pot_waste: horizontal waste on the right hand edge of the texture.
 * @y_pot_waste: vertical waste on the bottom edge of the texture.
 * @format: format of the foreign texture.
 *
 * Creates a #CoglTexture based on an existing OpenGL texture; the
 * width, height and format are passed along since it is not always
 * possible to query these from OpenGL.
 *
 * The waste arguments allow you to create a Cogl texture that maps to
 * a region smaller than the real OpenGL texture. For instance if your
 * hardware only supports power-of-two textures you may load a
 * non-power-of-two image into a larger power-of-two texture and use
 * the waste arguments to tell Cogl which region should be mapped to
 * the texture coordinate range [0:1].
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 *
 * Since: 0.8
 */
CoglTexture *
cogl_texture_new_from_foreign (unsigned int gl_handle,
                               unsigned int gl_target,
                               unsigned int width,
                               unsigned int height,
                               unsigned int x_pot_waste,
                               unsigned int y_pot_waste,
                               CoglPixelFormat format);

/**
 * cogl_texture_new_from_bitmap:
 * @bitmap: A #CoglBitmap pointer
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 * texture
 *
 * Creates a #CoglTexture from a #CoglBitmap.
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 *
 * Since: 1.0
 */
CoglTexture *
cogl_texture_new_from_bitmap (CoglBitmap *bitmap,
                              CoglTextureFlags flags,
                              CoglPixelFormat internal_format);

/**
 * cogl_is_texture:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a texture object.
 *
 * Return value: %TRUE if the @object references a texture, and
 *   %FALSE otherwise
 */
CoglBool
cogl_is_texture (void *object);

/**
 * cogl_texture_get_width:
 * @texture a #CoglTexture pointer.
 *
 * Queries the width of a cogl texture.
 *
 * Return value: the width of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_width (CoglTexture *texture);

/**
 * cogl_texture_get_height:
 * @texture a #CoglTexture pointer.
 *
 * Queries the height of a cogl texture.
 *
 * Return value: the height of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_height (CoglTexture *texture);

/**
 * cogl_texture_get_format:
 * @texture a #CoglTexture pointer.
 *
 * Queries the #CoglPixelFormat of a cogl texture.
 *
 * Return value: the #CoglPixelFormat of the GPU side texture
 */
CoglPixelFormat
cogl_texture_get_format (CoglTexture *texture);


/**
 * cogl_texture_get_rowstride:
 * @texture a #CoglTexture pointer.
 *
 * Determines the bytes-per-pixel for the #CoglPixelFormat retrieved
 * from cogl_texture_get_format() and multiplies that by the texture's
 * width.
 *
 * <note>It's very unlikely that anyone would need to use this API to
 * query the internal rowstride of a #CoglTexture which can just be
 * considered an implementation detail. Actually it's not even useful
 * internally since underlying drivers are free to use a different
 * format</note>
 *
 * <note>This API is only here for backwards compatibility and
 * shouldn't be used in new code. In particular please don't be
 * mislead to pass the returned value to cogl_texture_get_data() for
 * the rowstride, since you should be passing the rowstride you desire
 * for your destination buffer not the rowstride of the source
 * texture.</note>
 *
 * Return value: The bytes-per-pixel for the current format
 *               multiplied by the texture's width
 *
 * Deprecated: 1.10: There's no replacement for the API but there's
 *                   also no known need for API either. It was just
 *                   a mistake that it was ever published.
 */
unsigned int
cogl_texture_get_rowstride (CoglTexture *texture);

/**
 * cogl_texture_get_max_waste:
 * @texture a #CoglTexture pointer.
 *
 * Queries the maximum wasted (unused) pixels in one dimension of a GPU side
 * texture.
 *
 * Return value: the maximum waste
 */
int
cogl_texture_get_max_waste (CoglTexture *texture);

/**
 * cogl_texture_is_sliced:
 * @texture a #CoglTexture pointer.
 *
 * Queries if a texture is sliced (stored as multiple GPU side tecture
 * objects).
 *
 * Return value: %TRUE if the texture is sliced, %FALSE if the texture
 *   is stored as a single GPU texture
 */
CoglBool
cogl_texture_is_sliced (CoglTexture *texture);

/**
 * cogl_texture_get_gl_texture:
 * @texture a #CoglTexture pointer.
 * @out_gl_handle: (out) (allow-none): pointer to return location for the
 *   textures GL handle, or %NULL.
 * @out_gl_target: (out) (allow-none): pointer to return location for the
 *   GL target type, or %NULL.
 *
 * Queries the GL handles for a GPU side texture through its #CoglTexture.
 *
 * If the texture is spliced the data for the first sub texture will be
 * queried.
 *
 * Return value: %TRUE if the handle was successfully retrieved, %FALSE
 *   if the handle was invalid
 */
CoglBool
cogl_texture_get_gl_texture (CoglTexture *texture,
                             unsigned int *out_gl_handle,
                             unsigned int *out_gl_target);

/**
 * cogl_texture_get_data:
 * @texture a #CoglTexture pointer.
 * @format: the #CoglPixelFormat to store the texture as.
 * @rowstride: the rowstride of @data in bytes or pass 0 to calculate
 *             from the bytes-per-pixel of @format multiplied by the
 *             @texture width.
 * @data: memory location to write the @texture's contents, or %NULL
 * to only query the data size through the return value.
 *
 * Copies the pixel data from a cogl texture to system memory.
 *
 * <note>Don't pass the value of cogl_texture_get_rowstride() as the
 * @rowstride argument, the rowstride should be the rowstride you
 * want for the destination @data buffer not the rowstride of the
 * source texture</note>
 *
 * Return value: the size of the texture data in bytes
 */
int
cogl_texture_get_data (CoglTexture *texture,
                       CoglPixelFormat format,
                       unsigned int rowstride,
                       uint8_t *data);

/**
 * cogl_texture_set_region:
 * @texture a #CoglTexture.
 * @src_x: upper left coordinate to use from source data.
 * @src_y: upper left coordinate to use from source data.
 * @dst_x: upper left destination horizontal coordinate.
 * @dst_y: upper left destination vertical coordinate.
 * @dst_width: width of destination region to write. (Must be less
 *   than or equal to @width)
 * @dst_height: height of destination region to write. (Must be less
 *   than or equal to @height)
 * @width: width of source data buffer.
 * @height: height of source data buffer.
 * @format: the #CoglPixelFormat used in the source buffer.
 * @rowstride: rowstride of source buffer (computed from width if none
 * specified)
 * @data: the actual pixel data.
 *
 * Sets the pixels in a rectangular subregion of @texture from an in-memory
 * buffer containing pixel data.
 *
 * <note>The region set can't be larger than the source @data</note>
 *
 * Return value: %TRUE if the subregion upload was successful, and
 *   %FALSE otherwise
 */
CoglBool
cogl_texture_set_region (CoglTexture *texture,
                         int src_x,
                         int src_y,
                         int dst_x,
                         int dst_y,
                         unsigned int dst_width,
                         unsigned int dst_height,
                         int width,
                         int height,
                         CoglPixelFormat format,
                         unsigned int rowstride,
                         const uint8_t *data);

#if defined (COGL_ENABLE_EXPERIMENTAL_API)

/**
 * cogl_texture_set_region_from_bitmap:
 * @texture a #CoglTexture pointer
 * @src_x: upper left coordinate to use from the source bitmap.
 * @src_y: upper left coordinate to use from the source bitmap
 * @dst_x: upper left destination horizontal coordinate.
 * @dst_y: upper left destination vertical coordinate.
 * @dst_width: width of destination region to write. (Must be less
 *   than or equal to the bitmap width)
 * @dst_height: height of destination region to write. (Must be less
 *   than or equal to the bitmap height)
 * @bitmap: The source bitmap to read from
 *
 * Copies a specified source region from @bitmap to the position
 * (@src_x, @src_y) of the given destination texture @handle.
 *
 * <note>The region updated can't be larger than the source
 * bitmap</note>
 *
 * Return value: %TRUE if the subregion upload was successful, and
 *   %FALSE otherwise
 *
 * Since: 1.8
 * Stability: unstable
 */
CoglBool
cogl_texture_set_region_from_bitmap (CoglTexture *texture,
                                     int src_x,
                                     int src_y,
                                     int dst_x,
                                     int dst_y,
                                     unsigned int dst_width,
                                     unsigned int dst_height,
                                     CoglBitmap *bitmap);
#endif

/**
 * cogl_texture_new_from_sub_texture:
 * @full_texture: a #CoglTexture pointer
 * @sub_x: X coordinate of the top-left of the subregion
 * @sub_y: Y coordinate of the top-left of the subregion
 * @sub_width: Width in pixels of the subregion
 * @sub_height: Height in pixels of the subregion
 *
 * Creates a new texture which represents a subregion of another
 * texture. The GL resources will be shared so that no new texture
 * data is actually allocated.
 *
 * Sub textures have undefined behaviour texture coordinates outside
 * of the range [0,1] are used. They also do not work with
 * CoglVertexBuffers.
 *
 * The sub texture will keep a reference to the full texture so you do
 * not need to keep one separately if you only want to use the sub
 * texture.
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 * Since: 1.2
 */
CoglTexture *
cogl_texture_new_from_sub_texture (CoglTexture *full_texture,
                                   int sub_x,
                                   int sub_y,
                                   int sub_width,
                                   int sub_height);

#ifndef COGL_DISABLE_DEPRECATED

/**
 * cogl_texture_ref:
 * @texture: a #CoglTexture.
 *
 * Increment the reference count for a cogl texture.
 *
 * Deprecated: 1.2: Use cogl_object_ref() instead
 *
 * Return value: the @texture pointer.
 */
void *
cogl_texture_ref (void *texture) G_GNUC_DEPRECATED;

/**
 * cogl_texture_unref:
 * @texture: a #CoglTexture.
 *
 * Decrement the reference count for a cogl texture.
 *
 * Deprecated: 1.2: Use cogl_object_unref() instead
 */
void
cogl_texture_unref (void *texture) G_GNUC_DEPRECATED;

#endif /* COGL_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __COGL_TEXTURE_H__ */
