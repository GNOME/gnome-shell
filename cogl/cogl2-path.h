/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#ifndef __COGL2_PATH_H__
#define __COGL2_PATH_H__

#include <cogl/cogl-types.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-paths
 * @short_description: Functions for constructing and drawing 2D paths.
 *
 * There are two levels on which drawing with cogl-paths can be used.
 * The highest level functions construct various simple primitive
 * shapes to be either filled or stroked. Using a lower-level set of
 * functions more complex and arbitrary paths can be constructed by
 * concatenating straight line, bezier curve and arc segments.
 *
 * When constructing arbitrary paths, the current pen location is
 * initialized using the move_to command. The subsequent path segments
 * implicitly use the last pen location as their first vertex and move
 * the pen location to the last vertex they produce at the end. Also
 * there are special versions of functions that allow specifying the
 * vertices of the path segments relative to the last pen location
 * rather then in the absolute coordinates.
 */

typedef struct _CoglPath CoglPath;

#define COGL_PATH(obj) ((CoglPath *)(obj))

#define cogl_path_new cogl2_path_new
/**
 * cogl_path_new:
 *
 * Creates a new, empty path object. The default fill rule is
 * %COGL_PATH_FILL_RULE_EVEN_ODD.
 *
 * Return value: A pointer to a newly allocated #CoglPath, which can
 * be freed using cogl_object_unref().
 *
 * Since: 2.0
 */
CoglPath *
cogl_path_new (void);

/**
 * cogl_path_copy:
 * @path: A #CoglPath object
 *
 * Returns a new copy of the path in @path. The new path has a
 * reference count of 1 so you should unref it with
 * cogl_object_unref() if you no longer need it.
 *
 * Internally the path will share the data until one of the paths is
 * modified so copying paths should be relatively cheap.
 *
 * Return value: a copy of the path in @path.
 *
 * Since: 2.0
 */
CoglPath *
cogl_path_copy (CoglPath *path);

/**
 * cogl_is_path:
 * @object: A #CoglObject
 *
 * Gets whether the given object references an existing path object.
 *
 * Return value: %TRUE if the object references a #CoglPath,
 *   %FALSE otherwise.
 *
 * Since: 2.0
 */
CoglBool
cogl_is_path (void *object);

#define cogl_path_move_to cogl2_path_move_to
/**
 * cogl_path_move_to:
 * @x: X coordinate of the pen location to move to.
 * @y: Y coordinate of the pen location to move to.
 *
 * Moves the pen to the given location. If there is an existing path
 * this will start a new disjoint subpath.
 *
 * Since: 2.0
 */
void
cogl_path_move_to (CoglPath *path,
                   float x,
                   float y);

#define cogl_path_rel_move_to cogl2_path_rel_move_to
/**
 * cogl_path_rel_move_to:
 * @x: X offset from the current pen location to move the pen to.
 * @y: Y offset from the current pen location to move the pen to.
 *
 * Moves the pen to the given offset relative to the current pen
 * location. If there is an existing path this will start a new
 * disjoint subpath.
 *
 * Since: 2.0
 */
void
cogl_path_rel_move_to (CoglPath *path,
                       float x,
                       float y);

#define cogl_path_line_to cogl2_path_line_to
/**
 * cogl_path_line_to:
 * @x: X coordinate of the end line vertex
 * @y: Y coordinate of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates.
 *
 * Since: 2.0
 */
void
cogl_path_line_to (CoglPath *path,
                   float x,
                   float y);

#define cogl_path_rel_line_to cogl2_path_rel_line_to
/**
 * cogl_path_rel_line_to:
 * @x: X offset from the current pen location of the end line vertex
 * @y: Y offset from the current pen location of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates relative to the current pen location.
 *
 * Since: 2.0
 */
void
cogl_path_rel_line_to (CoglPath *path,
                       float x,
                       float y);

#define cogl_path_arc cogl2_path_arc
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
 *
 * Since: 2.0
 */
void
cogl_path_arc (CoglPath *path,
               float center_x,
               float center_y,
               float radius_x,
               float radius_y,
               float angle_1,
               float angle_2);

#define cogl_path_curve_to cogl2_path_curve_to
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
 *
 * Since: 2.0
 */
void
cogl_path_curve_to (CoglPath *path,
                    float x_1,
                    float y_1,
                    float x_2,
                    float y_2,
                    float x_3,
                    float y_3);

#define cogl_path_rel_curve_to cogl2_path_rel_curve_to
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
 *
 * Since: 2.0
 */
void
cogl_path_rel_curve_to (CoglPath *path,
                        float x_1,
                        float y_1,
                        float x_2,
                        float y_2,
                        float x_3,
                        float y_3);

#define cogl_path_close cogl2_path_close
/**
 * cogl_path_close:
 *
 * Closes the path being constructed by adding a straight line segment
 * to it that ends at the first vertex of the path.
 *
 * Since: 2.0
 */
void
cogl_path_close (CoglPath *path);

#define cogl_path_line cogl2_path_line
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
 *
 * Since: 2.0
 */
void
cogl_path_line (CoglPath *path,
                float x_1,
                float y_1,
                float x_2,
                float y_2);

#define cogl_path_polyline cogl2_path_polyline
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
 *
 * Since: 2.0
 */
void
cogl_path_polyline (CoglPath *path,
                    const float *coords,
                    int num_points);

#define cogl_path_polygon cogl2_path_polygon
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
 *
 * Since: 2.0
 */
void
cogl_path_polygon (CoglPath *path,
                   const float *coords,
                   int num_points);

#define cogl_path_rectangle cogl2_path_rectangle
/**
 * cogl_path_rectangle:
 * @x_1: X coordinate of the top-left corner.
 * @y_1: Y coordinate of the top-left corner.
 * @x_2: X coordinate of the bottom-right corner.
 * @y_2: Y coordinate of the bottom-right corner.
 *
 * Constructs a rectangular shape at the given coordinates. If there
 * is an existing path this will start a new disjoint sub-path.
 *
 * Since: 2.0
 */
void
cogl_path_rectangle (CoglPath *path,
                     float x_1,
                     float y_1,
                     float x_2,
                     float y_2);

#define cogl_path_ellipse cogl2_path_ellipse
/**
 * cogl_path_ellipse:
 * @center_x: X coordinate of the ellipse center
 * @center_y: Y coordinate of the ellipse center
 * @radius_x: X radius of the ellipse
 * @radius_y: Y radius of the ellipse
 *
 * Constructs an ellipse shape. If there is an existing path this will
 * start a new disjoint sub-path.
 *
 * Since: 2.0
 */
void
cogl_path_ellipse (CoglPath *path,
                   float center_x,
                   float center_y,
                   float radius_x,
                   float radius_y);

#define cogl_path_round_rectangle cogl2_path_round_rectangle
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
 *
 * Since: 2.0
 */
void
cogl_path_round_rectangle (CoglPath *path,
                           float x_1,
                           float y_1,
                           float x_2,
                           float y_2,
                           float radius,
                           float arc_step);

/**
 * CoglPathFillRule:
 * @COGL_PATH_FILL_RULE_NON_ZERO: Each time the line crosses an edge of
 * the path from left to right one is added to a counter and each time
 * it crosses from right to left the counter is decremented. If the
 * counter is non-zero then the point will be filled. See <xref
 * linkend="fill-rule-non-zero"/>.
 * @COGL_PATH_FILL_RULE_EVEN_ODD: If the line crosses an edge of the
 * path an odd number of times then the point will filled, otherwise
 * it won't. See <xref linkend="fill-rule-even-odd"/>.
 *
 * #CoglPathFillRule is used to determine how a path is filled. There
 * are two options - 'non-zero' and 'even-odd'. To work out whether any
 * point will be filled imagine drawing an infinetely long line in any
 * direction from that point. The number of times and the direction
 * that the edges of the path crosses this line determines whether the
 * line is filled as described below. Any open sub paths are treated
 * as if there was an extra line joining the first point and the last
 * point.
 *
 * The default fill rule is %COGL_PATH_FILL_RULE_EVEN_ODD. The fill
 * rule is attached to the current path so preserving a path with
 * cogl_get_path() also preserves the fill rule. Calling
 * cogl_path_new() resets the current fill rule to the default.
 *
 * <figure id="fill-rule-non-zero">
 *   <title>Example of filling various paths using the non-zero rule</title>
 *   <graphic fileref="fill-rule-non-zero.png" format="PNG"/>
 * </figure>
 *
 * <figure id="fill-rule-even-odd">
 *   <title>Example of filling various paths using the even-odd rule</title>
 *   <graphic fileref="fill-rule-even-odd.png" format="PNG"/>
 * </figure>
 *
 * Since: 1.4
 */
typedef enum {
  COGL_PATH_FILL_RULE_NON_ZERO,
  COGL_PATH_FILL_RULE_EVEN_ODD
} CoglPathFillRule;

#define cogl_path_set_fill_rule cogl2_path_set_fill_rule
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
 * Since: 2.0
 */
void
cogl_path_set_fill_rule (CoglPath *path, CoglPathFillRule fill_rule);

#define cogl_path_get_fill_rule cogl2_path_get_fill_rule
/**
 * cogl_path_get_fill_rule:
 *
 * Retrieves the fill rule set using cogl_path_set_fill_rule().
 *
 * Return value: the fill rule that is used for the current path.
 *
 * Since: 2.0
 */
CoglPathFillRule
cogl_path_get_fill_rule (CoglPath *path);

#define cogl_path_fill cogl2_path_fill
/**
 * cogl_path_fill:
 *
 * Fills the interior of the constructed shape using the current
 * drawing color.
 *
 * The interior of the shape is determined using the fill rule of the
 * path. See %CoglPathFillRule for details.
 *
 * <note>The result of referencing sliced textures in your current
 * pipeline when filling a path are undefined. You should pass
 * the %COGL_TEXTURE_NO_SLICING flag when loading any texture you will
 * use while filling a path.</note>
 *
 * Since: 2.0
 */
void
cogl_path_fill (CoglPath *path);

#define cogl_path_stroke cogl2_path_stroke
/**
 * cogl_path_stroke:
 *
 * Strokes the constructed shape using the current drawing color and a
 * width of 1 pixel (regardless of the current transformation
 * matrix).
 *
 * Since: 2.0
 */
void
cogl_path_stroke (CoglPath *path);

COGL_END_DECLS

#endif /* __COGL2_PATH_H__ */

