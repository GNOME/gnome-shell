/* cogl-texture.h: Texture objects
 * This file is part of Clutter
 *
 * Copyright (C) 2008  Intel Corporation.
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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TEXTURE_H__
#define __COGL_TEXTURE_H__

G_BEGIN_DECLS

#include <cogl/cogl-types.h>

/**
 * SECTION:cogl-texture
 * @short_description: Fuctions for creating and manipulating textures
 *
 * COGL allows creating and manipulating GL textures using a uniform
 * API that tries to hide all the various complexities of creating,
 * loading and manipulating textures.
 */

/**
 * cogl_texture_new_with_size:
 * @width: width of texture in pixels.
 * @height: height of texture in pixels.
 * @max_waste: maximum extra horizontal and|or vertical margin pixels to make
 * texture fit GPU limitations.
 * @auto_mipmap: enable or disable automatic generation of mipmap pyramid
 * from the base level image whenever it is updated.
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 * texture.
 *
 * Create a new texture with specified dimensions and pixel format.
 *
 * Returns: a #CoglHandle to the newly created texture or COGL_INVALID_HANDLE
 * if texture creation failed.
 */
CoglHandle      cogl_texture_new_with_size    (guint           width,
                                               guint           height,
                                               gint            max_waste,
                                               gboolean        auto_mipmap,
                                               CoglPixelFormat internal_format);

/**
 * cogl_texture_new_from_file:
 * @filename: the file to load
 * @max_waste: maximum extra horizontal and|or vertical margin pixels to make
 * texture fit GPU limitations.
 * @auto_mipmap: enable or disable automatic generation of mipmap pyramid
 * from the base level image whenever it is updated.
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 * texture.
 * @error: a #GError or NULL.
 *
 * Load an image file from disk.
 *
 * Returns: a #CoglHandle to the newly created texture or COGL_INVALID_HANDLE
 * if creating the texture failed.
 */
CoglHandle      cogl_texture_new_from_file    (const gchar    *filename,
                                               gint            max_waste,
                                               gboolean        auto_mipmap,
                                               CoglPixelFormat internal_format,
                                               GError        **error);

/**
 * cogl_texture_new_from_data:
 * @width: width of texture in pixels.
 * @height: height of texture in pixels.
 * @max_waste: maximum extra horizontal and|or vertical margin pixels to make
 * @auto_mipmap: enable or disable automatic generation of mipmap pyramid
 * from the base level image whenever it is updated.
 * @format: the #CoglPixelFormat the buffer is stored in in RAM
 * @internal_format: the #CoglPixelFormat that will be used for storing the
 * buffer on the GPU.
 * @rowstride: the memory offset in bytes between the starts of scanlines in
 * @data.
 * @data: pointer the memory region where the source buffer resides.
 *
 * Create a new cogl texture based on data residing in memory.
 *
 * Returns: a #CoglHandle to the newly created texture or COGL_INVALID_HANDLE
 * if creating the texture failed.
 */
CoglHandle      cogl_texture_new_from_data    (guint            width,
                                               guint            height,
                                               gint             max_waste,
                                               gboolean         auto_mipmap,
                                               CoglPixelFormat  format,
                                               CoglPixelFormat  internal_format,
                                               guint            rowstride,
                                               const guchar    *data);

/**
 * cogl_texture_new_from_foreign:
 * @gl_handle: opengl target type of foreign texture
 * @gl_target: opengl handle of foreign texture.
 * @width: width of foreign texture
 * @height: height of foreign texture.
 * @x_pot_waste: maximum horizontal waste.
 * @y_pot_waste: maximum vertical waste.
 * @format: format of the foreign texture.
 *
 * Create a cogl texture based on an existing OpenGL texture, the width, height
 * and format are passed along since it is not possible to query this from a
 * handle with GLES 1.0.
 *
 * Returns: a #CoglHandle to the newly created texture or COGL_INVALID_HANDLE
 * if creating the texture failed.
 */
CoglHandle      cogl_texture_new_from_foreign (GLuint              gl_handle,
                                               GLenum              gl_target,
                                               GLuint              width,
                                               GLuint              height,
                                               GLuint              x_pot_waste,
                                               GLuint              y_pot_waste,
                                               CoglPixelFormat     format);

/**
 * cogl_texture_new_from_bitmap:
 * @handle: handle of the preloaded texture.
 * @max_waste: maximum extra horizontal and|or vertical margin pixels to make
 * texture fit GPU limitations.
 * @auto_mipmap: enable or disable automatic generation of mipmap pyramid
 * from the base level image whenever it is updated.
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 * texture.
 *
 * Create a cogl texture from a #CoglBitmap.
 *
 * Returns: a #CoglHandle to the newly created texture or COGL_INVALID_HANDLE
 * if creating the texture failed.
 */
CoglHandle      cogl_texture_new_from_bitmap (CoglBitmap     *bitmap,
                                              gint            max_waste,
                                              gboolean        auto_mipmap,
                                              CoglPixelFormat internal_format);

/**
 * cogl_is_texture:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing texture object.
 *
 * Returns: %TRUE if the handle references a texture,
 *   %FALSE otherwise
 */
gboolean        cogl_is_texture               (CoglHandle          handle);

/**
 * cogl_texture_get_width:
 * @handle: a #CoglHandle for a texture.
 *
 * Query the width of a cogl texture.
 *
 * Returns: the width of the GPU side texture in pixels:
 */
guint           cogl_texture_get_width        (CoglHandle          handle);

/**
 * cogl_texture_get_height:
 * @handle: a #CoglHandle for a texture.
 *
 * Query the height of a cogl texture.
 *
 * Returns: the height of the GPU side texture in pixels:
 */
guint           cogl_texture_get_height       (CoglHandle          handle);

/**
 * cogl_texture_get_format:
 * @handle: a #CoglHandle for a texture.
 *
 * Query the #CoglPixelFormat of a cogl texture.
 *
 * Returns: the #CoglPixelFormat of the GPU side texture.
 */
CoglPixelFormat cogl_texture_get_format       (CoglHandle          handle);


/**
 * cogl_texture_get_rowstride:
 * @handle: a #CoglHandle for a texture.
 *
 * Query the rowstride of a cogl texture.
 *
 * Returns: the offset in bytes between each consequetive row of pixels.
 */
guint           cogl_texture_get_rowstride    (CoglHandle          handle);

/**
 * cogl_texture_get_max_waste:
 * @handle: a #CoglHandle for a texture.
 *
 * Query the maximum wasted (unused) pixels in one dimension of a GPU side
 * texture.
 *
 * Returns: the maximum waste.
 */
gint            cogl_texture_get_max_waste    (CoglHandle          handle);

/**
 * cogl_texture_get_min_filter:
 * @handle: a #CoglHandle for a texture.
 *
 * Query the currently set downscaling filter for a cogl texture.
 *
 * Returns: the current downscaling filter for a cogl texture.
 */
COGLenum        cogl_texture_get_min_filter   (CoglHandle          handle);

/**
 * cogl_texture_get_mag_filter:
 * @handle: a #CoglHandle for a texture.
 *
 * Query the currently set downscaling filter for a cogl texture.
 *
 * Returns: the current downscaling filter for a cogl texture.
 */
COGLenum        cogl_texture_get_mag_filter   (CoglHandle          handle);

/**
 * cogl_texture_is_sliced:
 * @handle: a #CoglHandle for a texture.
 *
 * Query if a texture is sliced (stored as multiple GPU side tecture
 * objects).
 *
 * Returns: %TRUE if the texture is sliced, %FALSE if the texture
 * is stored as a single GPU texture.
 */
gboolean        cogl_texture_is_sliced        (CoglHandle          handle);

/**
 * cogl_texture_get_gl_texture:
 * @handle: a #CoglHandle for a texture.
 * @out_gl_handle: pointer to return location for the textures GL handle, or
 * NULL.
 * @out_gl_target: pointer to return location for the GL target type, or NULL.
 *
 * Query the GL handles for a GPU side texture through it's #CoglHandle,
 * if the texture is spliced the data for the first sub texture will be
 * queried.
 *
 * Returns: %TRUE if the handle was successfully retrieved %FALSE
 * if the handle was invalid.
 */
gboolean        cogl_texture_get_gl_texture   (CoglHandle         handle,
                                               GLuint            *out_gl_handle,
                                               GLenum            *out_gl_target);

/**
 * cogl_texture_get_data:
 * @handle: a #CoglHandle for a texture.
 * @format: the #CoglPixelFormat to store the texture as.
 * @rowstride: the rowstride of @data or retrieved from texture if none is
 * specified.
 * @data: memory location to write contents of buffer, or %NULL if we're
 * only querying the data size through the return value.
 *
 * Copy the pixel data from a cogl texture to system memory.
 *
 * Returns: the size of the texture data in bytes (or 0 if the texture
 * is not valid.)
 */
gint            cogl_texture_get_data         (CoglHandle          handle,
                                               CoglPixelFormat     format,
                                               guint               rowstride,
                                               guchar             *data);

/**
 * cogl_texture_set_filters:
 * @handle: a #CoglHandle.
 * @min_filter: the filter used when scaling the texture down.
 * @mag_filter: the filter used when magnifying the texture.
 *
 * Changes the decimation and interpolation filters used when the texture is
 * drawn at other scales than 100%.
 */
void            cogl_texture_set_filters      (CoglHandle          handle,
                                               COGLenum            min_filter,
                                               COGLenum            mag_filter);


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
 * Returns: %TRUE if the subregion upload was successful, otherwise %FALSE.
 */
gboolean        cogl_texture_set_region       (CoglHandle          handle,
                                               gint                src_x,
                                               gint                src_y,
                                               gint                dst_x,
                                               gint                dst_y,
                                               guint               dst_width,
                                               guint               dst_height,
                                               gint                width,
                                               gint                height,
                                               CoglPixelFormat     format,
                                               guint               rowstride,
                                               const guchar       *data);

/**
 * cogl_texture_ref:
 * @handle: a @CoglHandle.
 *
 * Increment the reference count for a cogl texture.
 *
 * Returns: the @handle.
 */
CoglHandle      cogl_texture_ref              (CoglHandle          handle);

/**
 * cogl_texture_unref:
 * @handle: a @CoglHandle.
 *
 * Deccrement the reference count for a cogl texture.
 */
void            cogl_texture_unref            (CoglHandle          handle);

/**
 * cogl_texture_rectangle:
 * @handle: a @CoglHandle.
 * @x1: x coordinate upper left on screen.
 * @y1: y coordinate upper left on screen.
 * @x2: x coordinate lower right on screen.
 * @y2: y coordinate lower right on screen.
 * @tx1: x part of texture coordinate to use for upper left pixel
 * @ty1: y part of texture coordinate to use for upper left pixel
 * @tx2: x part of texture coordinate to use for lower right pixel
 * @ty2: y part of texture coordinate to use for left pixel
 *
 * Draw a rectangle from a texture to the display, to draw the entire
 * texture pass in @tx1=0.0 @ty1=0.0 @tx2=1.0 @ty2=1.0.
 */
void            cogl_texture_rectangle        (CoglHandle          handle,
                                               CoglFixed        x1,
                                               CoglFixed        y1,
                                               CoglFixed        x2,
                                               CoglFixed        y2,
                                               CoglFixed        tx1,
                                               CoglFixed        ty1,
                                               CoglFixed        tx2,
                                               CoglFixed        ty2);

/**
 * cogl_texture_polygon:
 * @handle: A CoglHandle for a texture
 * @n_vertices: The length of the vertices array
 * @vertices: An array of #CoglTextureVertex structs
 * @use_color: %TRUE if the color member of #CoglTextureVertex should be used
 *
 * Draws a polygon from a texture with the given model and texture
 * coordinates. This can be used to draw arbitrary shapes textured
 * with a COGL texture. If @use_color is %TRUE then the current COGL
 * color will be changed for each vertex using the value specified in
 * the color member of #CoglTextureVertex. This can be used for
 * example to make the texture fade out by setting the alpha value of
 * the color.
 *
 * All of the texture coordinates must be in the range [0,1] and
 * repeating the texture is not supported.
 *
 * Because of the way this function is implemented it will currently
 * only work if either the texture is not sliced or the backend is not
 * OpenGL ES and the minifying and magnifying functions are both set
 * to CGL_NEAREST.
 */
void            cogl_texture_polygon          (CoglHandle          handle,
                                               guint               n_vertices,
                                               CoglTextureVertex  *vertices,
                                               gboolean            use_color);

/**
 * cogl_bitmap_new_from_file:
 * @filename: the file to load.
 * @error: a #GError or %NULL.
 *
 * Load an image file from disk. This function can be safely called from 
 * within a thread.
 *
 * Returns: A #CoglBitmap to the new loaded image data, or %NULL if loading 
 * the image failed.
 *
 * Since: 1.0
 */
CoglBitmap *    cogl_bitmap_new_from_file     (const gchar    *filename,
                                               GError        **error);

/**
 * cogl_bitmap_get_size_from_file:
 * @filename: the file to check
 * @width: return location for the bitmap width
 * @height: return location for the bitmap height
 *
 * Parses an image file enough to extract the width and height
 * of the bitmap.
 *
 * Since: 1.0
 */
gboolean        cogl_bitmap_get_size_from_file (const gchar   *filename,
                                                gint          *width,
                                                gint          *height);

/**
 * cogl_bitmap_free:
 * @bmp: a #CoglBitmap.
 *
 * Frees a #CoglBitmap.
 */
void            cogl_bitmap_free              (CoglBitmap     *bmp);

/**
 * cogl_texture_multiple_rectangles:
 * @handle: a @CoglHandle.
 * @verts: an array of vertices
 * @n_rects: number of rectangles to draw
 *
 * Draws a series of rectangles in the same way that
 * cogl_texture_rectangle() does. In some situations it can give a
 * significant performance boost to use this function rather than
 * calling cogl_texture_rectangle() separately for each rectangle.
 *
 * @verts should point to an array of #CoglFixed<!-- -->s with
 * @n_rects * 8 elements. Each group of 8 values corresponds to the
 * parameters x1, y1, x2, y2, tx1, ty1, tx2 and ty2 and have the same
 * meaning as in cogl_texture_rectangle().
 *
 * Since: 0.8.6
 */
void            cogl_texture_multiple_rectangles
                                              (CoglHandle          handle,
                                               const CoglFixed    *verts,
                                               guint               n_rects);

G_END_DECLS

#endif /* __COGL_TEXTURE_H__ */
