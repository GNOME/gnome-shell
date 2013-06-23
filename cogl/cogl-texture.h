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
#ifdef __COGL_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void CoglTexture;
#else
typedef struct _CoglTexture CoglTexture;
#define COGL_TEXTURE(X) ((CoglTexture *)X)
#endif

#include <cogl/cogl-types.h>
#include <cogl/cogl-macros.h>
#include <cogl/cogl-defines.h>
#if defined (COGL_ENABLE_EXPERIMENTAL_API)
#include <cogl/cogl-pixel-buffer.h>
#endif
#include <cogl/cogl-bitmap.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-texture
 * @short_description: Functions for creating and manipulating textures
 *
 * Cogl allows creating and manipulating textures using a uniform
 * API that tries to hide all the various complexities of creating,
 * loading and manipulating textures.
 */

#define COGL_TEXTURE_MAX_WASTE  127

/**
 * COGL_TEXTURE_ERROR:
 *
 * #CoglError domain for texture errors.
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

uint32_t cogl_texture_error_quark (void);

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
 * Return value: (transfer full): A newly created #CoglTexture or %NULL on failure
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
 * @error: return location for a #CoglError or %NULL
 *
 * Creates a #CoglTexture from an image file.
 *
 * Return value: (transfer full): A newly created #CoglTexture or
 *               %NULL on failure
 *
 * Since: 0.8
 */
CoglTexture *
cogl_texture_new_from_file (const char       *filename,
                            CoglTextureFlags   flags,
                            CoglPixelFormat    internal_format,
                            CoglError           **error);

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
 * Return value: (transfer full): A newly created #CoglTexture or
 *               %NULL on failure
 *
 * Since: 0.8
 */
CoglTexture *
cogl_texture_new_from_data (int width,
                            int height,
                            CoglTextureFlags flags,
                            CoglPixelFormat format,
                            CoglPixelFormat internal_format,
                            int rowstride,
                            const uint8_t *data);

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
 * Return value: (transfer full): A newly created #CoglTexture or
 *               %NULL on failure
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
 * Return value: (transfer full): A newly created #CoglTexture or
 *               %NULL on failure
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

typedef enum _CoglTextureComponents
{
  COGL_TEXTURE_COMPONENTS_A = 1,
  COGL_TEXTURE_COMPONENTS_RGB,
  COGL_TEXTURE_COMPONENTS_RGBA,
  COGL_TEXTURE_COMPONENTS_DEPTH
} CoglTextureComponents;

/**
 * cogl_texture_set_components:
 * @texture: a #CoglTexture pointer.
 *
 * Affects the internal storage format for this texture by
 * determinging what components will be required for sampling later.
 *
 * This api affects how data is uploaded to the GPU since unused
 * components can potentially be discarded from source data.
 *
 * By default the required components are automatically determined
 * using the format of the source data that is first uploaded to
 * the given @texture.
 */
void
cogl_texture_set_components (CoglTexture *texture,
                             CoglTextureComponents components);

/**
 * cogl_texture_get_components:
 * @texture: a #CoglTexture pointer.
 *
 * Queries what components the given @texture stores internally as set
 * via cogl_texture_set_components().
 *
 * By default the required components are automatically determined
 * using the format of the source data that is first uploaded to
 * the given @texture.
 */
CoglBool
cogl_texture_get_components (CoglTexture *texture);

/**
 * cogl_texture_set_premultiplied:
 * @texture: a #CoglTexture pointer.
 * @premultiplied: Whether any internally stored red, green or blue
 *                 components are pre-multiplied by an alpha
 *                 component.
 *
 * Affects the internal storage format for this texture by determining
 * whether red, green and blue color components should be stored as
 * pre-multiplied alpha values.
 *
 * This api affects how data is uploaded to the GPU since Cogl will
 * convert source data to have premultiplied or unpremultiplied
 * components according to this state.
 *
 * For example if you create a texture via
 * cogl_texture_2d_new_with_size() and then upload data via
 * cogl_texture_set_data() passing a source format of
 * %COGL_PIXEL_FORMAT_RGBA_8888 then Cogl will internally multiply the
 * red, green and blue components of the source data by the alpha
 * component, for each pixel so that the internally stored data has
 * pre-multiplied alpha components. If you instead upload data that
 * already has pre-multiplied components by passing
 * %COGL_PIXEL_FORMAT_RGBA_8888_PRE as the source format to
 * cogl_texture_set_data() then the data can be uploaded without being
 * converted.
 *
 * By default the @premultipled state is @TRUE.
 */
void
cogl_texture_set_premultiplied (CoglTexture *texture,
                                CoglBool premultiplied);

/**
 * cogl_texture_get_premultiplied:
 * @texture: a #CoglTexture pointer.
 *
 * Queries the pre-multiplied alpha status for internally stored red,
 * green and blue components for the given @texture as set by
 * cogl_texture_set_premultiplied().
 *
 * By default the pre-multipled state is @TRUE.
 *
 * Return value: %TRUE if red, green and blue components are
 *               internally stored pre-multiplied by the alpha
 *               value or %FALSE if not.
 */
CoglBool
cogl_texture_get_premultiplied (CoglTexture *texture);

/**
 * cogl_texture_get_width:
 * @texture: a #CoglTexture pointer.
 *
 * Queries the width of a cogl texture.
 *
 * Return value: the width of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_width (CoglTexture *texture);

/**
 * cogl_texture_get_height:
 * @texture: a #CoglTexture pointer.
 *
 * Queries the height of a cogl texture.
 *
 * Return value: the height of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_height (CoglTexture *texture);

/**
 * cogl_texture_get_max_waste:
 * @texture: a #CoglTexture pointer.
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
 * @texture: a #CoglTexture pointer.
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
 * @texture: a #CoglTexture pointer.
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
 * @texture: a #CoglTexture pointer.
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
 * @texture: a #CoglTexture.
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
 * cogl_texture_set_data:
 * @texture a #CoglTexture.
 * @format: the #CoglPixelFormat used in the source @data buffer.
 * @rowstride: rowstride of the source @data buffer (computed from
 *             the texture width and @format if it equals 0)
 * @data: the source data, pointing to the first top-left pixel to set
 * @level: The mipmap level to update (Normally 0 for the largest,
 *         base texture)
 * @error: A #CoglError to return exceptional errors
 *
 * Sets all the pixels for a given mipmap @level by copying the pixel
 * data pointed to by the @data argument into the given @texture.
 *
 * @data should point to the first pixel to copy corresponding
 * to the top left of the mipmap @level being set.
 *
 * If @rowstride equals 0 then it will be automatically calculated
 * from the width of the mipmap level and the bytes-per-pixel for the
 * given @format.
 *
 * A mipmap @level of 0 corresponds to the largest, base image of a
 * texture and @level 1 is half the width and height of level 0. If
 * dividing any dimension of the previous level by two results in a
 * fraction then round the number down (floor()), but clamp to 1
 * something like this:
 *
 * |[
 *  next_width = MAX (1, floor (prev_width));
 * ]|
 *
 * You can determine the number of mipmap levels for a given texture
 * like this:
 *
 * |[
 *  n_levels = 1 + floor (log2 (max_dimension));
 * ]|
 *
 * Where %max_dimension is the larger of cogl_texture_get_width() and
 * cogl_texture_get_height().
 *
 * It is an error to pass a @level number >= the number of levels that
 * @texture can have according to the above calculation.
 *
 * <note>Since the storage for a #CoglTexture is allocated lazily then
 * if the given @texture has not previously been allocated then this
 * api can return %FALSE and throw an exceptional @error if there is
 * not enough memory to allocate storage for @texture.</note>
 *
 * Return value: %TRUE if the data upload was successful, and
 *               %FALSE otherwise
 */
CoglBool
cogl_texture_set_data (CoglTexture *texture,
                       CoglPixelFormat format,
                       int rowstride,
                       const uint8_t *data,
                       int level,
                       CoglError **error);

/**
 * cogl_texture_set_region_from_bitmap:
 * @texture: a #CoglTexture pointer
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
 * Return value: (transfer full): A newly created #CoglTexture or
 *               %NULL on failure
 * Since: 1.2
 */
CoglTexture *
cogl_texture_new_from_sub_texture (CoglTexture *full_texture,
                                   int sub_x,
                                   int sub_y,
                                   int sub_width,
                                   int sub_height);

/**
 * cogl_texture_allocate:
 * @texture: A #CoglTexture
 * @error: A #CoglError to return exceptional errors or %NULL
 *
 * Explicitly allocates the storage for the given @texture which
 * allows you to be sure that there is enough memory for the
 * texture and if not then the error can be handled gracefully.
 *
 * <note>Normally applications don't need to use this api directly
 * since the texture will be implicitly allocated when data is set on
 * the texture, or if the texture is attached to a #CoglOffscreen
 * framebuffer and rendered too.</note>
 *
 * Return value: %TRUE if the texture was successfully allocated,
 *               otherwise %FALSE and @error will be updated if it
 *               wasn't %NULL.
 */
CoglBool
cogl_texture_allocate (CoglTexture *texture,
                       CoglError **error);

COGL_END_DECLS

#endif /* __COGL_TEXTURE_H__ */
