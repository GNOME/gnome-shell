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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __COGL_PRIMITIVES_H
#define __COGL_PRIMITIVES_H

/**
 * SECTION:cogl-primitives
 * @short_description: Functions that draw various primitive 3D shapes
 *
 * The primitives API provides utilities for drawing some
 * common 3D shapes in a more convenient way than the CoglVertexBuffer
 * API provides.
 */

/**
 * cogl_rectangle:
 * @x_1: X coordinate of the top-left corner
 * @y_1: Y coordinate of the top-left corner
 * @x_2: X coordinate of the bottom-right corner
 * @y_2: Y coordinate of the bottom-right corner
 *
 * Fills a rectangle at the given coordinates with the current source material
 **/
void
cogl_rectangle (float x_1,
                float y_1,
                float x_2,
                float y_2);

/**
 * cogl_rectangle_with_texture_coords:
 * @x1: x coordinate upper left on screen.
 * @y1: y coordinate upper left on screen.
 * @x2: x coordinate lower right on screen.
 * @y2: y coordinate lower right on screen.
 * @tx1: x part of texture coordinate to use for upper left pixel
 * @ty1: y part of texture coordinate to use for upper left pixel
 * @tx2: x part of texture coordinate to use for lower right pixel
 * @ty2: y part of texture coordinate to use for left pixel
 *
 * Draw a rectangle using the current material and supply texture coordinates
 * to be used for the first texture layer of the material. To draw the entire
 * texture pass in @tx1=0.0 @ty1=0.0 @tx2=1.0 @ty2=1.0.
 *
 * Since: 1.0
 */
void
cogl_rectangle_with_texture_coords (float  x1,
                                    float  y1,
                                    float  x2,
                                    float  y2,
                                    float  tx1,
                                    float  ty1,
                                    float  tx2,
                                    float  ty2);

/**
 * cogl_rectangle_with_multitexture_coords:
 * @x1: x coordinate upper left on screen.
 * @y1: y coordinate upper left on screen.
 * @x2: x coordinate lower right on screen.
 * @y2: y coordinate lower right on screen.
 * @tex_coords: (in) (array) (transfer none): An array containing groups of
 *   4 float values: [tx1, ty1, tx2, ty2] that are interpreted as two texture
 *   coordinates; one for the upper left texel, and one for the lower right
 *   texel. Each value should be between 0.0 and 1.0, where the coordinate
 *   (0.0, 0.0) represents the top left of the texture, and (1.0, 1.0) the
 *   bottom right.
 * @tex_coords_len: The length of the tex_coords array. (e.g. for one layer
 *   and one group of texture coordinates, this would be 4)
 *
 * This function draws a rectangle using the current source material to
 * texture or fill with. As a material may contain multiple texture layers
 * this interface lets you supply texture coordinates for each layer of the
 * material.
 *
 * The first pair of coordinates are for the first layer (with the smallest
 * layer index) and if you supply less texture coordinates than there are
 * layers in the current source material then default texture coordinates
 * (0.0, 0.0, 1.0, 1.0) are generated.
 *
 * Since: 1.0
 */
void
cogl_rectangle_with_multitexture_coords (float        x1,
                                         float        y1,
                                         float        x2,
                                         float        y2,
                                         const float *tex_coords,
                                         int         tex_coords_len);

/**
 * cogl_rectangles_with_texture_coords:
 * @verts: (in) (array) (transfer none): an array of vertices
 * @n_rects: number of rectangles to draw
 *
 * Draws a series of rectangles in the same way that
 * cogl_rectangle_with_texture_coords() does. In some situations it can give a
 * significant performance boost to use this function rather than
 * calling cogl_rectangle_with_texture_coords() separately for each rectangle.
 *
 * @verts should point to an array of #float<!-- -->s with
 * @n_rects * 8 elements. Each group of 8 values corresponds to the
 * parameters x1, y1, x2, y2, tx1, ty1, tx2 and ty2 and have the same
 * meaning as in cogl_rectangle_with_texture_coords().
 *
 * Since: 0.8.6
 */
void
cogl_rectangles_with_texture_coords (const float *verts,
                                     unsigned int n_rects);

/**
 * cogl_rectangles:
 * @verts: (in) (array) (transfer none): an array of vertices
 * @n_rects: number of rectangles to draw
 *
 * Draws a series of rectangles in the same way that
 * cogl_rectangle() does. In some situations it can give a
 * significant performance boost to use this function rather than
 * calling cogl_rectangle() separately for each rectangle.
 *
 * @verts should point to an array of #float<!-- -->s with
 * @n_rects * 4 elements. Each group of 4 values corresponds to the
 * parameters x1, y1, x2, and y2, and have the same
 * meaning as in cogl_rectangle().
 *
 * Since: 1.0
 */
void
cogl_rectangles (const float *verts,
                 unsigned int n_rects);

/**
 * cogl_polygon:
 * @vertices: An array of #CoglTextureVertex structs
 * @n_vertices: The length of the vertices array
 * @use_color: %TRUE if the color member of #CoglTextureVertex should be used
 *
 * Draws a convex polygon using the current source material to fill / texture
 * with according to the texture coordinates passed.
 *
 * If @use_color is %TRUE then the color will be changed for each vertex using
 * the value specified in the color member of #CoglTextureVertex. This can be
 * used for example to make the texture fade out by setting the alpha value of
 * the color.
 *
 * All of the texture coordinates must be in the range [0,1] and repeating the
 * texture is not supported.
 *
 * Because of the way this function is implemented it will currently
 * only work if either the texture is not sliced or the backend is not
 * OpenGL ES and the minifying and magnifying functions are both set
 * to COGL_MATERIAL_FILTER_NEAREST.
 *
 * Since: 1.0
 */
void
cogl_polygon (const CoglTextureVertex  *vertices,
              unsigned int              n_vertices,
              gboolean                  use_color);

#endif /* __COGL_PRIMITIVES_H */
