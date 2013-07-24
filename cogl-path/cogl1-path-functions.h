/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2012 Intel Corporation.
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

#ifndef __COGL_PATH_FUNCTIONS_H__
#define __COGL_PATH_FUNCTIONS_H__

/* The functions are declared separately because cogl-path.c needs to
   get the function declarations from the old 1.0 API without
   colliding with the enum declarations from the 2.0 API */

#include <cogl/cogl-types.h>

COGL_BEGIN_DECLS

/**
 * cogl_is_path:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing path object.
 *
 * Return value: %TRUE if the handle references a #CoglPath,
 *   %FALSE otherwise
 */
CoglBool
cogl_is_path (CoglHandle handle);

/**
 * cogl_path_set_fill_rule:
 * @fill_rule: The new fill rule.
 *
 * Sets the fill rule of the current path to @fill_rule. This will
 * affect how the path is filled when cogl_path_fill() is later
 * called. Note that the fill rule state is attached to the path so
 * calling cogl_get_path() will preserve the fill rule and calling
 * cogl_path_new() will reset the fill rule back to the default.
 *
 * Since: 1.4
 */
void
cogl_path_set_fill_rule (CoglPathFillRule fill_rule);

/**
 * cogl_path_get_fill_rule:
 *
 * Retrieves the fill rule set using cogl_path_set_fill_rule().
 *
 * Return value: the fill rule that is used for the current path.
 *
 * Since: 1.4
 */
CoglPathFillRule
cogl_path_get_fill_rule (void);

/**
 * cogl_path_fill:
 *
 * Fills the interior of the constructed shape using the current
 * drawing color. The current path is then cleared. To use the path
 * again, call cogl_path_fill_preserve() instead.
 *
 * The interior of the shape is determined using the fill rule of the
 * path. See %CoglPathFillRule for details.
 **/
void
cogl_path_fill (void);

/**
 * cogl_path_fill_preserve:
 *
 * Fills the interior of the constructed shape using the current
 * drawing color and preserves the path to be used again. See
 * cogl_path_fill() for a description what is considered the interior
 * of the shape.
 *
 * Since: 1.0
 **/
void
cogl_path_fill_preserve (void);

/**
 * cogl_path_stroke:
 *
 * Strokes the constructed shape using the current drawing color and a
 * width of 1 pixel (regardless of the current transformation
 * matrix). To current path is then cleared. To use the path again,
 * call cogl_path_stroke_preserve() instead.
 **/
void
cogl_path_stroke (void);

/**
 * cogl_path_stroke_preserve:
 *
 * Strokes the constructed shape using the current drawing color and
 * preserves the path to be used again.
 *
 * Since: 1.0
 **/
void
cogl_path_stroke_preserve (void);

/**
 * cogl_path_new:
 *
 * Clears the current path and starts a new one. Creating a new path
 * also resets the fill rule to the default which is
 * %COGL_PATH_FILL_RULE_EVEN_ODD.
 *
 * Since: 1.0
 */
void
cogl_path_new (void);

/**
 * cogl_path_move_to:
 * @x: X coordinate of the pen location to move to.
 * @y: Y coordinate of the pen location to move to.
 *
 * Moves the pen to the given location. If there is an existing path
 * this will start a new disjoint subpath.
  **/
void
cogl_path_move_to (float x,
                   float y);


/**
 * cogl_path_rel_move_to:
 * @x: X offset from the current pen location to move the pen to.
 * @y: Y offset from the current pen location to move the pen to.
 *
 * Moves the pen to the given offset relative to the current pen
 * location. If there is an existing path this will start a new
 * disjoint subpath.
 **/
void
cogl_path_rel_move_to (float x,
                       float y);

/**
 * cogl_path_line_to:
 * @x: X coordinate of the end line vertex
 * @y: Y coordinate of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates.
 **/
void
cogl_path_line_to (float x,
                   float y);

/**
 * cogl_path_rel_line_to:
 * @x: X offset from the current pen location of the end line vertex
 * @y: Y offset from the current pen location of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates relative to the current pen location.
 **/
void
cogl_path_rel_line_to (float x,
                       float y);


/**
 * cogl_path_arc:
 * @center_x: X coordinate of the elliptical arc center
 * @center_y: Y coordinate of the elliptical arc center
 * @radius_x: X radius of the elliptical arc
 * @radius_y: Y radius of the elliptical arc
 * @angle_1: Angle in degrees at which the arc begin
 * @angle_2: Angle in degrees at which the arc ends
 *
 * Adds an elliptical arc segment to the current path. A straight line
 * segment will link the current pen location with the first vertex
 * of the arc. If you perform a move_to to the arcs start just before
 * drawing it you create a free standing arc.
 *
 * The angles are measured in degrees where 0° is in the direction of
 * the positive X axis and 90° is in the direction of the positive Y
 * axis. The angle of the arc begins at @angle_1 and heads towards
 * @angle_2 (so if @angle_2 is less than @angle_1 it will decrease,
 * otherwise it will increase).
 **/
void
cogl_path_arc (float center_x,
               float center_y,
               float radius_x,
               float radius_y,
               float angle_1,
               float angle_2);

/**
 * cogl_path_curve_to:
 * @x_1: X coordinate of the second bezier control point
 * @y_1: Y coordinate of the second bezier control point
 * @x_2: X coordinate of the third bezier control point
 * @y_2: Y coordinate of the third bezier control point
 * @x_3: X coordinate of the fourth bezier control point
 * @y_3: Y coordinate of the fourth bezier control point
 *
 * Adds a cubic bezier curve segment to the current path with the given
 * second, third and fourth control points and using current pen location
 * as the first control point.
 **/
void
cogl_path_curve_to (float x_1,
                    float y_1,
                    float x_2,
                    float y_2,
                    float x_3,
                    float y_3);

/**
 * cogl_path_rel_curve_to:
 * @x_1: X coordinate of the second bezier control point
 * @y_1: Y coordinate of the second bezier control point
 * @x_2: X coordinate of the third bezier control point
 * @y_2: Y coordinate of the third bezier control point
 * @x_3: X coordinate of the fourth bezier control point
 * @y_3: Y coordinate of the fourth bezier control point
 *
 * Adds a cubic bezier curve segment to the current path with the given
 * second, third and fourth control points and using current pen location
 * as the first control point. The given coordinates are relative to the
 * current pen location.
 */
void
cogl_path_rel_curve_to (float x_1,
                        float y_1,
                        float x_2,
                        float y_2,
                        float x_3,
                        float y_3);

/**
 * cogl_path_close:
 *
 * Closes the path being constructed by adding a straight line segment
 * to it that ends at the first vertex of the path.
 **/
void
cogl_path_close (void);

/**
 * cogl_path_line:
 * @x_1: X coordinate of the start line vertex
 * @y_1: Y coordinate of the start line vertex
 * @x_2: X coordinate of the end line vertex
 * @y_2: Y coordinate of the end line vertex
 *
 * Constructs a straight line shape starting and ending at the given
 * coordinates. If there is an existing path this will start a new
 * disjoint sub-path.
 **/
void
cogl_path_line (float x_1,
                float y_1,
                float x_2,
                float y_2);

/**
 * cogl_path_polyline:
 * @coords: (in) (array) (transfer none): A pointer to the first element of an
 * array of fixed-point values that specify the vertex coordinates.
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
void
cogl_path_polyline (const float *coords,
                    int          num_points);


/**
 * cogl_path_polygon:
 * @coords: (in) (array) (transfer none): A pointer to the first element of
 * an array of fixed-point values that specify the vertex coordinates.
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
void
cogl_path_polygon (const float *coords,
                   int          num_points);


/**
 * cogl_path_rectangle:
 * @x_1: X coordinate of the top-left corner.
 * @y_1: Y coordinate of the top-left corner.
 * @x_2: X coordinate of the bottom-right corner.
 * @y_2: Y coordinate of the bottom-right corner.
 *
 * Constructs a rectangular shape at the given coordinates. If there
 * is an existing path this will start a new disjoint sub-path.
 **/
void
cogl_path_rectangle (float x_1,
                     float y_1,
                     float x_2,
                     float y_2);

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
void
cogl_path_ellipse (float center_x,
                   float center_y,
                   float radius_x,
                   float radius_y);

/**
 * cogl_path_round_rectangle:
 * @x_1: X coordinate of the top-left corner.
 * @y_1: Y coordinate of the top-left corner.
 * @x_2: X coordinate of the bottom-right corner.
 * @y_2: Y coordinate of the bottom-right corner.
 * @radius: Radius of the corner arcs.
 * @arc_step: Angle increment resolution for subdivision of
 * the corner arcs.
 *
 * Constructs a rectangular shape with rounded corners. If there is an
 * existing path this will start a new disjoint sub-path.
 **/
void
cogl_path_round_rectangle (float x_1,
                           float y_1,
                           float x_2,
                           float y_2,
                           float radius,
                           float arc_step);

/**
 * cogl_get_path: (skip)
 *
 * Gets a pointer to the current path. The path can later be used
 * again by calling cogl_path_set(). Note that the path isn't copied
 * so if you later call any functions to add to the path it will
 * affect the returned object too. No reference is taken on the path
 * so if you want to retain it you should take your own reference with
 * cogl_object_ref().
 *
 * Return value: a pointer to the current path.
 *
 * Since: 1.4
 */
CoglPath *
cogl_get_path (void);

/**
 * cogl_set_path: (skip)
 * @path: A #CoglPath object
 *
 * Replaces the current path with @path. A reference is taken on the
 * object so if you no longer need the path you should unref with
 * cogl_object_unref().
 *
 * Since: 1.4
 */
void
cogl_set_path (CoglPath *path);

/**
 * cogl_path_copy: (skip)
 * @path: A #CoglPath object
 *
 * Returns a new copy of the path in @path. The new path has a
 * reference count of 1 so you should unref it with
 * cogl_object_unref() if you no longer need it.
 *
 * Internally the path will share the data until one of the paths is
 * modified so copying paths should be relatively cheap.
 *
 * Return value: (transfer full): a copy of the path in @path.
 */
CoglPath *
cogl_path_copy (CoglPath *path);

/**
 * cogl_clip_push_from_path_preserve:
 *
 * Sets a new clipping area using the current path. The current path
 * is then cleared. The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * cogl_clip_pop().
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_framebuffer_push_path_clip() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_path_clip)
void
cogl_clip_push_from_path_preserve (void);

/**
 * cogl_clip_push_from_path:
 *
 * Sets a new clipping area using the current path. The current path
 * is then cleared. The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * cogl_clip_pop().
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_framebuffer_push_path_clip() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_path_clip)
void
cogl_clip_push_from_path (void);

COGL_END_DECLS

#endif /* __COGL_PATH_FUNCTIONS_H__ */

