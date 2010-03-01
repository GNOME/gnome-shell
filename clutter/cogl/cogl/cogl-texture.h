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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TEXTURE_H__
#define __COGL_TEXTURE_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-defines.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-texture
 * @short_description: Fuctions for creating and manipulating textures
 *
 * COGL allows creating and manipulating GL textures using a uniform
 * API that tries to hide all the various complexities of creating,
 * loading and manipulating textures.
 */

#define COGL_TEXTURE_MAX_WASTE  127

/**
 * cogl_texture_new_with_size:
 * @width: width of texture in pixels.
 * @height: height of texture in pixels.
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 *    texture.
 *
 * Creates a new COGL texture with the specified dimensions and pixel format.
 *
 * Return value: a #CoglHandle to the newly created texture or
 *   %COGL_INVALID_HANDLE on failure
 *
 * Since: 0.8
 */
CoglHandle
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
 * Creates a COGL texture from an image file.
 *
 * Return value: a #CoglHandle to the newly created texture or
 *    %COGL_INVALID_HANDLE on failure
 *
 * Since: 0.8
 */
CoglHandle
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
 * Creates a new COGL texture based on data residing in memory.
 *
 * Return value: a #CoglHandle to the newly created texture or
 *   %COGL_INVALID_HANDLE on failure
 *
 * Since: 0.8
 */
CoglHandle
cogl_texture_new_from_data (unsigned int      width,
                            unsigned int      height,
                            CoglTextureFlags  flags,
                            CoglPixelFormat   format,
                            CoglPixelFormat   internal_format,
                            unsigned int      rowstride,
                            const guint8     *data);

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
 * Creates a COGL texture based on an existing OpenGL texture; the
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
 * Return value: a #CoglHandle to the newly created texture or
 *   %COGL_INVALID_HANDLE on failure
 *
 * Since: 0.8
 */
CoglHandle
cogl_texture_new_from_foreign (GLuint          gl_handle,
                               GLenum          gl_target,
                               GLuint          width,
                               GLuint          height,
                               GLuint          x_pot_waste,
                               GLuint          y_pot_waste,
                               CoglPixelFormat format);

/**
 * cogl_texture_new_from_bitmap:
 * @bmp_handle: A CoglBitmap handle
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 * texture
 *
 * Creates a COGL texture from a CoglBitmap.
 *
 * Return value: a #CoglHandle to the newly created texture or
 *   %COGL_INVALID_HANDLE on failure
 *
 * Since: 1.0
 */
CoglHandle
cogl_texture_new_from_bitmap (CoglHandle       bmp_handle,
                              CoglTextureFlags flags,
                              CoglPixelFormat  internal_format);

/**
 * cogl_is_texture:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing texture object.
 *
 * Return value: %TRUE if the handle references a texture, and
 *   %FALSE otherwise
 */
gboolean
cogl_is_texture (CoglHandle handle);

/**
 * cogl_texture_get_width:
 * @handle: a #CoglHandle for a texture.
 *
 * Queries the width of a cogl texture.
 *
 * Return value: the width of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_width (CoglHandle handle);

/**
 * cogl_texture_get_height:
 * @handle: a #CoglHandle for a texture.
 *
 * Queries the height of a cogl texture.
 *
 * Return value: the height of the GPU side texture in pixels
 */
unsigned int
cogl_texture_get_height (CoglHandle handle);

/**
 * cogl_texture_get_format:
 * @handle: a #CoglHandle for a texture.
 *
 * Queries the #CoglPixelFormat of a cogl texture.
 *
 * Return value: the #CoglPixelFormat of the GPU side texture
 */
CoglPixelFormat
cogl_texture_get_format (CoglHandle handle);


/**
 * cogl_texture_get_rowstride:
 * @handle: a #CoglHandle for a texture.
 *
 * Queries the rowstride of a cogl texture.
 *
 * Return value: the offset in bytes between each consequetive row of pixels
 */
unsigned int
cogl_texture_get_rowstride (CoglHandle handle);

/**
 * cogl_texture_get_max_waste:
 * @handle: a #CoglHandle for a texture.
 *
 * Queries the maximum wasted (unused) pixels in one dimension of a GPU side
 * texture.
 *
 * Return value: the maximum waste
 */
int
cogl_texture_get_max_waste (CoglHandle handle);

/**
 * cogl_texture_is_sliced:
 * @handle: a #CoglHandle for a texture.
 *
 * Queries if a texture is sliced (stored as multiple GPU side tecture
 * objects).
 *
 * Return value: %TRUE if the texture is sliced, %FALSE if the texture
 *   is stored as a single GPU texture
 */
gboolean
cogl_texture_is_sliced (CoglHandle handle);

/**
 * cogl_texture_get_gl_texture:
 * @handle: a #CoglHandle for a texture.
 * @out_gl_handle: (out) (allow-none): pointer to return location for the
 *   textures GL handle, or %NULL.
 * @out_gl_target: (out) (allow-none): pointer to return location for the
 *   GL target type, or %NULL.
 *
 * Queries the GL handles for a GPU side texture through its #CoglHandle.
 *
 * If the texture is spliced the data for the first sub texture will be
 * queried.
 *
 * Return value: %TRUE if the handle was successfully retrieved, %FALSE
 *   if the handle was invalid
 */
gboolean
cogl_texture_get_gl_texture (CoglHandle   handle,
                             GLuint      *out_gl_handle,
                             GLenum      *out_gl_target);

/**
 * cogl_texture_get_data:
 * @handle: a #CoglHandle for a texture.
 * @format: the #CoglPixelFormat to store the texture as.
 * @rowstride: the rowstride of @data or retrieved from texture if none is
 * specified.
 * @data: memory location to write contents of buffer, or %NULL if we're
 * only querying the data size through the return value.
 *
 * Copies the pixel data from a cogl texture to system memory.
 *
 * Return value: the size of the texture data in bytes, or 0 if the texture
 *   is not valid
 */
int
cogl_texture_get_data (CoglHandle       handle,
                       CoglPixelFormat  format,
                       unsigned int     rowstride,
                       guint8          *data);

/**
 * cogl_texture_set_region:
 * @handle: a #CoglHandle.
 * @src_x: upper left coordinate to use from source data.
 * @src_y: upper left coordinate to use from source data.
 * @dst_x: upper left destination horizontal coordinate.
 * @dst_y: upper left destination vertical coordinate.
 * @dst_width: width of destination region to write.
 * @dst_height: height of destination region to write.
 * @width: width of source data buffer.
 * @height: height of source data buffer.
 * @format: the #CoglPixelFormat used in the source buffer.
 * @rowstride: rowstride of source buffer (computed from width if none
 * specified)
 * @data: the actual pixel data.
 *
 * Sets the pixels in a rectangular subregion of @handle from an in-memory
 * buffer containing pixel data.
 *
 * Return value: %TRUE if the subregion upload was successful, and
 *   %FALSE otherwise
 */
gboolean
cogl_texture_set_region (CoglHandle       handle,
                         int              src_x,
                         int              src_y,
                         int              dst_x,
                         int              dst_y,
                         unsigned int     dst_width,
                         unsigned int     dst_height,
                         int              width,
                         int              height,
                         CoglPixelFormat  format,
                         unsigned int     rowstride,
                         const guint8    *data);

/**
 * cogl_texture_new_from_sub_texture:
 * @full_texture: a #CoglHandle to an existing texture
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
 * Return value: a #CoglHandle to the new texture.
 *
 * Since: 1.2
 */
CoglHandle
cogl_texture_new_from_sub_texture (CoglHandle full_texture,
                                   int        sub_x,
                                   int        sub_y,
                                   int        sub_width,
                                   int        sub_height);

#if defined (COGL_ENABLE_EXPERIMENTAL_API)

/**
 * cogl_texture_new_from_buffer:
 * @buffer: the #CoglHandle of a pixel buffer
 * @width: width of texture in pixels or 0
 * @height: height of texture in pixels or 0
 * @flags: optional flags for the texture, or %COGL_TEXTURE_NONE
 * @format: the #CoglPixelFormat the buffer is stored in in RAM
 * @internal_format: the #CoglPixelFormat that will be used for storing
 *    the buffer on the GPU. If %COGL_PIXEL_FORMAT_ANY is given then a
 *    premultiplied format similar to the format of the source data will
 *    be used. The default blending equations of Cogl expect premultiplied
 *    color data; the main use of passing a non-premultiplied format here
 *    is if you have non-premultiplied source data and are going to adjust
 *    the blend mode (see cogl_material_set_blend()) or use the data for
 *    something other than straight blending
 * @rowstride: the memory offset in bytes between the starts of
 *    scanlines in @data. If 0 is given the row stride will be deduced from
 *    @width and @format or the stride given by cogl_pixel_buffer_new_for_size()
 * @offset: offset in bytes in @buffer from where the texture data starts
 *
 * Creates a new texture using the buffer specified by @handle. If the buffer
 * has been created using cogl_pixel_buffer_new_for_size() it's possible to omit
 * the height and width values already specified at creation time.
 *
 * Return value: a #CoglHandle to the new texture or %COGL_INVALID_HANDLE on
 *    failure
 *
 * Since: 1.2
 * Stability: Unstable
 */
CoglHandle
cogl_texture_new_from_buffer  (CoglHandle       buffer,
                               unsigned int     width,
                               unsigned int     height,
                               CoglTextureFlags flags,
                               CoglPixelFormat  format,
                               CoglPixelFormat  internal_format,
                               unsigned int     rowstride,
                               unsigned int     offset);

/* the function above is experimental, the actual symbol is suffixed by _EXP so
 * we can ensure ABI compatibility and leave the cogl_buffer namespace free for
 * future use. A bunch of defines translates the symbols documented above into
 * the real symbols */

CoglHandle
cogl_texture_new_from_buffer_EXP (CoglHandle       buffer,
                                  unsigned int     width,
                                  unsigned int     height,
                                  CoglTextureFlags flags,
                                  CoglPixelFormat  format,
                                  CoglPixelFormat  internal_format,
                                  unsigned int     rowstride,
                                  unsigned int     offset);

#define cogl_texture_new_from_buffer cogl_texture_new_from_buffer_EXP

#endif

#ifndef COGL_DISABLE_DEPRECATED

/**
 * cogl_texture_ref:
 * @handle: a @CoglHandle.
 *
 * Increment the reference count for a cogl texture.
 *
 * Deprecated: 1.2: Use cogl_handle_ref() instead
 *
 * Return value: the @handle.
 */
CoglHandle
cogl_texture_ref (CoglHandle handle) G_GNUC_DEPRECATED;

/**
 * cogl_texture_unref:
 * @handle: a @CoglHandle.
 *
 * Decrement the reference count for a cogl texture.
 *
 * Deprecated: 1.2: Use cogl_handle_unref() instead
 */
void
cogl_texture_unref (CoglHandle handle) G_GNUC_DEPRECATED;

#endif /* COGL_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __COGL_TEXTURE_H__ */
