/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL2_PATH_FUNCTIONS_H__
#define __COGL2_PATH_FUNCTIONS_H__

#include <cogl/cogl-types.h>
#ifdef COGL_COMPILATION
#include "cogl-context.h"
#else
#include <cogl/cogl.h>
#endif
#ifdef COGL_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif

COGL_BEGIN_DECLS

#ifdef COGL_HAS_GTYPE_SUPPORT
/**
 * cogl_path_get_gtype:
 *
 * Returns: a #GType that can be used with the GLib type system.
 */
GType cogl_path_get_gtype (void);
#endif

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
 * Return value: (transfer full): a copy of the path in @path.
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

/**
 * cogl_framebuffer_fill_path:
 * @framebuffer: A #CoglFramebuffer
 * @pipeline: A #CoglPipeline to render with
 * @path: The #CoglPath to fill
 *
 * Fills the interior of the path using the fragment operations
 * defined by the pipeline.
 *
 * The interior of the shape is determined using the fill rule of the
 * path. See %CoglPathFillRule for details.
 *
 * <note>The result of referencing sliced textures in your current
 * pipeline when filling a path are undefined. You should pass
 * the %COGL_TEXTURE_NO_SLICING flag when loading any texture you will
 * use while filling a path.</note>
 *
 * Stability: unstable
 * Deprecated: 1.16: Use cogl_path_fill() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_path_fill)
void
cogl_framebuffer_fill_path (CoglFramebuffer *framebuffer,
                            CoglPipeline *pipeline,
                            CoglPath *path);

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

/**
 * cogl_framebuffer_stroke_path:
 * @framebuffer: A #CoglFramebuffer
 * @pipeline: A #CoglPipeline to render with
 * @path: The #CoglPath to stroke
 *
 * Strokes the edge of the path using the fragment operations defined
 * by the pipeline. The stroke line will have a width of 1 pixel
 * regardless of the current transformation matrix.
 *
 * Stability: unstable
 * Deprecated: 1.16: Use cogl_path_stroke() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_path_stroke)
void
cogl_framebuffer_stroke_path (CoglFramebuffer *framebuffer,
                              CoglPipeline *pipeline,
                              CoglPath *path);

/**
 * cogl_framebuffer_push_path_clip:
 * @framebuffer: A #CoglFramebuffer pointer
 * @path: The path to clip with.
 *
 * Sets a new clipping area using the silhouette of the specified,
 * filled @path.  The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * cogl_framebuffer_pop_clip().
 *
 * Since: 1.0
 * Stability: unstable
 */
void
cogl_framebuffer_push_path_clip (CoglFramebuffer *framebuffer,
                                 CoglPath *path);

#define cogl_clip_push_from_path cogl2_clip_push_from_path
/**
 * cogl_clip_push_from_path:
 * @path: The path to clip with.
 *
 * Sets a new clipping area using the silhouette of the specified,
 * filled @path.  The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * call cogl_clip_pop().
 *
 * Since: 1.8
 * Stability: Unstable
 * Deprecated: 1.16: Use cogl_framebuffer_push_path_clip() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_path_clip)
void
cogl_clip_push_from_path (CoglPath *path);

COGL_END_DECLS

#endif /* __COGL2_PATH_FUNCTIONS_H__ */

