/* cogl-path.h: Path primitives
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

#ifndef __COGL_PATH_H__
#define __COGL_PATH_H__

#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-primitives
 * @short_description: Functions that draw various primitive shapes and
 * allow for construction of more complex paths.
 *
 * There are three levels on which drawing with cogl can be used. The
 * highest level functions construct various simple primitive shapes
 * to be either filled or stroked. Using a lower-level set of functions
 * more complex and arbitrary paths can be constructed by concatenating
 * straight line, bezier curve and arc segments. Additionally there
 * are utility functions that draw the most common primitives - rectangles
 * and trapezoids - in a maximaly optimized fashion.
 *
 * When constructing arbitrary paths, the current pen location is
 * initialized using the move_to command. The subsequent path segments
 * implicitly use the last pen location as their first vertex and move
 * the pen location to the last vertex they produce at the end. Also
 * there are special versions of functions that allow specifying the
 * vertices of the path segments relative to the last pen location
 * rather then in the absolute coordinates.
 */

/**
 * cogl_rectangle:
 * @x: X coordinate of the top-left corner
 * @y: Y coordinate of the top-left corner
 * @width: Width of the rectangle
 * @height: Height of the rectangle
 *
 * Fills a rectangle at the given coordinates with the current
 * drawing color in a highly optimizied fashion.
 **/
void            cogl_rectangle                (float               x,
                                               float               y,
                                               float               width,
                                               float               height);

/**
 * cogl_path_fill:
 *
 * Fills the constructed shape using the current drawing color. The
 * current path is then cleared. To use the path again, call
 * cogl_path_fill_preserve() instead.
 **/
void            cogl_path_fill                (void);

/**
 * cogl_path_fill_preserve:
 *
 * Fills the constructed shape using the current drawing color and
 * preserves the path to be used again.
 *
 * Since: 1.0
 **/
void            cogl_path_fill_preserve       (void);

/**
 * cogl_path_stroke:
 *
 * Strokes the constructed shape using the current drawing color and a
 * width of 1 pixel (regardless of the current transformation
 * matrix). To current path is then cleared. To use the path again,
 * call cogl_path_stroke_preserve() instead.
 **/
void            cogl_path_stroke              (void);


/**
 * cogl_path_stroke_preserve:
 *
 * Strokes the constructed shape using the current drawing color and
 * preserves the path to be used again.
 *
 * Since: 1.0
 **/
void            cogl_path_stroke_preserve     (void);

/**
 * cogl_path_new:
 *
 * Clears the current path and starts a new one.
 *
 * Since: 1.0
 */
void            cogl_path_new                 (void);

/**
 * cogl_path_move_to:
 * @x: X coordinate of the pen location to move to.
 * @y: Y coordinate of the pen location to move to.
 *
 * Moves the pen to the given location. If there is an existing path
 * this will start a new disjoint subpath.
  **/
void            cogl_path_move_to        (float        x,
                                          float        y);


/**
 * cogl_path_rel_move_to:
 * @x: X offset from the current pen location to move the pen to.
 * @y: Y offset from the current pen location to move the pen to.
 *
 * Moves the pen to the given offset relative to the current pen
 * location. If there is an existing path this will start a new
 * disjoint subpath.
 **/
void            cogl_path_rel_move_to    (float        x,
                                          float        y);

/**
 * cogl_path_line_to:
 * @x: X coordinate of the end line vertex
 * @y: Y coordinate of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates.
 **/
void            cogl_path_line_to        (float        x,
                                          float        y);

/**
 * cogl_path_rel_line_to:
 * @x: X offset from the current pen location of the end line vertex
 * @y: Y offset from the current pen location of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates relative to the current pen location.
 **/
void            cogl_path_rel_line_to    (float        x,
                                          float        y);


/**
 * cogl_path_arc:
 * @center_x: X coordinate of the elliptical arc center
 * @center_y: Y coordinate of the elliptical arc center
 * @radius_x: X radius of the elliptical arc
 * @radius_y: Y radious of the elliptical arc
 * @angle_1: Angle in the unit-circle at which the arc begin
 * @angle_2: Angle in the unit-circle at which the arc ends
 *
 * Adds an elliptical arc segment to the current path. A straight line
 * segment will link the current pen location with the first vertex
 * of the arc. If you perform a move_to to the arcs start just before
 * drawing it you create a free standing arc.
 **/
void            cogl_path_arc                 (float        center_x,
                                               float        center_y,
                                               float        radius_x,
                                               float        radius_y,
                                               float        angle_1,
                                               float        angle_2);



/**
 * cogl_path_curve_to:
 * @x1: X coordinate of the second bezier control point
 * @y1: Y coordinate of the second bezier control point
 * @x2: X coordinate of the third bezier control point
 * @y2: Y coordinate of the third bezier control point
 * @x3: X coordinate of the fourth bezier control point
 * @y3: Y coordinate of the fourth bezier control point
 *
 * Adds a cubic bezier curve segment to the current path with the given
 * second, third and fourth control points and using current pen location
 * as the first control point.
 **/
void            cogl_path_curve_to            (float        x1,
                                               float        y1,
                                               float        x2,
                                               float        y2,
                                               float        x3,
                                               float        y3);

/**
 * cogl_path_rel_curve_to:
 * @x1: X coordinate of the second bezier control point
 * @y1: Y coordinate of the second bezier control point
 * @x2: X coordinate of the third bezier control point
 * @y2: Y coordinate of the third bezier control point
 * @x3: X coordinate of the fourth bezier control point
 * @y3: Y coordinate of the fourth bezier control point
 *
 * Adds a cubic bezier curve segment to the current path with the given
 * second, third and fourth control points and using current pen location
 * as the first control point. The given coordinates are relative to the
 * current pen location.
 */
void            cogl_path_rel_curve_to        (float        x1,
                                               float        y1,
                                               float        x2,
                                               float        y2,
                                               float        x3,
                                               float        y3);

/**
 * cogl_path_close:
 *
 * Closes the path being constructed by adding a straight line segment
 * to it that ends at the first vertex of the path.
 **/
void            cogl_path_close               (void);

/**
 * cogl_path_line:
 * @x1: X coordinate of the start line vertex
 * @y1: Y coordinate of the start line vertex
 * @x2: X coordinate of the end line vertex
 * @y2: Y coordinate of the end line vertex
 *
 * Constructs a straight line shape starting and ending at the given
 * coordinates. If there is an existing path this will start a new
 * disjoint sub-path.
 **/
void            cogl_path_line                (float        x1,
                                               float        y1,
                                               float        x2,
                                               float        y2);

/**
 * cogl_path_polyline:
 * @coords: A pointer to the first element of an array of fixed-point
 * values that specify the vertex coordinates.
 * @num_points: The total number of vertices.
 *
 * Constructs a series of straight line segments, starting from the
 * first given vertex coordinate. If there is an existing path this
 * will start a new disjoint sub-path. Each subsequent segment starts
 * where the previous one ended and ends at the next given vertex
 * coordinate.
 *
 * The coords array must contain 2 * num_points values. The first value
 * represents the X coordinate of the first vertex, the second value
 * represents the Y coordinate of the first vertex, continuing in the same
 * fashion for the rest of the vertices. (num_points - 1) segments will
 * be constructed.
 **/
void            cogl_path_polyline            (float       *coords,
                                               gint             num_points);


/**
 * cogl_path_polygon:
 * @coords: A pointer to the first element of an array of fixed-point
 * values that specify the vertex coordinates.
 * @num_points: The total number of vertices.
 *
 * Constructs a polygonal shape of the given number of vertices. If
 * there is an existing path this will start a new disjoint sub-path.
 *
 * The coords array must contain 2 * num_points values. The first value
 * represents the X coordinate of the first vertex, the second value
 * represents the Y coordinate of the first vertex, continuing in the same
 * fashion for the rest of the vertices.
 **/
void            cogl_path_polygon             (float       *coords,
                                               gint             num_points);


/**
 * cogl_path_rectangle:
 * @x: X coordinate of the top-left corner.
 * @y: Y coordinate of the top-left corner.
 * @width: Rectangle width.
 * @height: Rectangle height.
 *
 * Constructs a rectangular shape at the given coordinates. If there
 * is an existing path this will start a new disjoint sub-path.
 **/
void            cogl_path_rectangle           (float        x,
                                               float        y,
                                               float        width,
                                               float        height);

/**
 * cogl_path_ellipse:
 * @center_x: X coordinate of the ellipse center
 * @center_y: Y coordinate of the ellipse center
 * @radius_x: X radius of the ellipse
 * @radius_y: Y radius of the ellipse
 *
 * Constructs an ellipse shape. If there is an existing path this will
 * start a new disjoint sub-path.
 **/
void            cogl_path_ellipse             (float        center_x,
                                               float        center_y,
                                               float        radius_x,
                                               float        radius_y);

/**
 * cogl_path_round_rectangle:
 * @x: X coordinate of the top-left corner
 * @y: Y coordinate of the top-left corner
 * @width: Width of the rectangle
 * @height: Height of the rectangle
 * @radius: Radius of the corner arcs.
 * @arc_step: Angle increment resolution for subdivision of
 * the corner arcs.
 *
 * Constructs a rectangular shape with rounded corners. If there is an
 * existing path this will start a new disjoint sub-path.
  **/
void            cogl_path_round_rectangle     (float        x,
                                               float        y,
                                               float        width,
                                               float        height,
                                               float        radius,
                                               float        arc_step);

G_END_DECLS

#endif /* __COGL_PATH_H__ */
