/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"

#include <string.h>
#include <gmodule.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

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
 * cogl_fast_fill_rectangle:
 * @x: X coordinate of the top-left corner
 * @y: Y coordinate of the top-left corner
 * @width: Width of the rectangle
 * @height: Height of the rectangle
 *
 * Fills a rectangle at the given coordinates with the current
 * drawing color in a highly optimizied fashion.
 **/
void
cogl_fast_fill_rectangle (gint x,
			  gint y,
			  guint width,
			  guint height)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (ctx->color_alpha < 255
	       ? COGL_ENABLE_BLEND : 0);
  
  GE( glRecti (x, y, x + width, y + height) );
}

/**
 * cogl_fast_fill_rectanglex:
 * @x: X coordinate of the top-left corner
 * @y: Y coordinate of the top-left corner
 * @width: Width of the rectangle
 * @height: Height of the rectangle
 *
 * A fixed-point version of cogl_fast_fill_rectangle.
 **/
void
cogl_fast_fill_rectanglex (ClutterFixed x,
			   ClutterFixed y,
			   ClutterFixed width,
			   ClutterFixed height)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (ctx->color_alpha < 255
	       ? COGL_ENABLE_BLEND : 0);
  
  GE( glRectf (CLUTTER_FIXED_TO_FLOAT (x),
	       CLUTTER_FIXED_TO_FLOAT (y),
	       CLUTTER_FIXED_TO_FLOAT (x + width),
	       CLUTTER_FIXED_TO_FLOAT (y + height)) );
}

/**
 * cogl_fast_fill_trapezoid:
 * @y1: Y coordinate of the top two vertices.
 * @x11: X coordinate of the top-left vertex.
 * @x21: X coordinate of the top-right vertex.
 * @y2: Y coordinate of the bottom two vertices.
 * @x12: X coordinate of the bottom-left vertex.
 * @x22: X coordinate of the bottom-right vertex.
 *
 * Fills a trapezoid at the given coordinates with the current
 * drawing color in a highly optimized fashion.
 **/
void
cogl_fast_fill_trapezoid (gint y1,
			  gint x11,
			  gint x21,
			  gint y2,
			  gint x12,
			  gint x22)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (ctx->color_alpha < 255
	       ? COGL_ENABLE_BLEND : 0);
  
  GE( glBegin (GL_QUADS) );
  GE( glVertex2i (x11, y1) );
  GE( glVertex2i (x21, y1) );
  GE( glVertex2i (x22, y2) );
  GE( glVertex2i (x12, y2) );
  GE( glEnd () );
}

/**
 * cogl_fast_fill_trapezoidx:
 * @y1: Y coordinate of the top two vertices.
 * @x11: X coordinate of the top-left vertex.
 * @x21: X coordinate of the top-right vertex.
 * @y2: Y coordinate of the bottom two vertices.
 * @x12: X coordinate of the bottom-left vertex.
 * @x22: X coordinate of the bottom-right vertex.
 *
 * A fixed-point version of cogl_fast_fill_trapezoid.
 **/
void
cogl_fast_fill_trapezoidx (ClutterFixed y1,
			   ClutterFixed x11,
			   ClutterFixed x21,
			   ClutterFixed y2,
			   ClutterFixed x12,
			   ClutterFixed x22)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (ctx->color_alpha < 255
	       ? COGL_ENABLE_BLEND : 0);
  
  GE( glBegin (GL_QUADS) );
  
  GE( glVertex2f (CLUTTER_FIXED_TO_FLOAT (x11),
		  CLUTTER_FIXED_TO_FLOAT (y1))  );
  GE( glVertex2f (CLUTTER_FIXED_TO_FLOAT (x21),
		  CLUTTER_FIXED_TO_FLOAT (y1))  );
  GE( glVertex2f (CLUTTER_FIXED_TO_FLOAT (x22),
		  CLUTTER_FIXED_TO_FLOAT (y2))  );
  GE( glVertex2f (CLUTTER_FIXED_TO_FLOAT (x12),
		  CLUTTER_FIXED_TO_FLOAT (y2))  );
  GE( glEnd () );
}

static void
_cogl_path_clear_nodes ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes)
    g_free(ctx->path_nodes);
  
  ctx->path_nodes = (CoglFloatVec2*) g_malloc (2 * sizeof(CoglFloatVec2));
  ctx->path_nodes_size = 0;
  ctx->path_nodes_cap = 2;
}

static void
_cogl_path_add_node (ClutterFixed x,
		     ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  CoglFloatVec2   *new_nodes = NULL;
  
  if (ctx->path_nodes_size == ctx->path_nodes_cap)
    {
      new_nodes = g_realloc (ctx->path_nodes,
			     2 * ctx->path_nodes_cap
			     * sizeof (CoglFloatVec2));
      
      if (new_nodes == NULL) return;

      ctx->path_nodes = new_nodes;
      ctx->path_nodes_cap *= 2;
    }
  
  ctx->path_nodes [ctx->path_nodes_size] .x = CLUTTER_FIXED_TO_FLOAT (x);
  ctx->path_nodes [ctx->path_nodes_size] .y = CLUTTER_FIXED_TO_FLOAT (y);
  ctx->path_nodes_size++;
    
  if (ctx->path_nodes_size == 1)
    {
      ctx->path_nodes_min.x = ctx->path_nodes_max.x = x;
      ctx->path_nodes_min.y = ctx->path_nodes_max.y = y;
    }
  else
    {
      if (x < ctx->path_nodes_min.x) ctx->path_nodes_min.x = x;
      if (x > ctx->path_nodes_max.x) ctx->path_nodes_max.x = x;
      if (y < ctx->path_nodes_min.y) ctx->path_nodes_min.y = y;
      if (y > ctx->path_nodes_max.y) ctx->path_nodes_max.y = y;
    }
}

static void
_cogl_path_stroke_nodes ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
	       | (ctx->color_alpha < 255
		  ? COGL_ENABLE_BLEND : 0));
  
  GE( glVertexPointer (2, GL_FLOAT, 0, ctx->path_nodes) );
  GE( glDrawArrays (GL_LINE_STRIP, 0, ctx->path_nodes_size) );
}

static void
_cogl_path_fill_nodes ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  guint bounds_x;
  guint bounds_y;
  guint bounds_w;
  guint bounds_h;
  
  GE( glClear (GL_STENCIL_BUFFER_BIT) );

  GE( glEnable (GL_STENCIL_TEST) );
  GE( glStencilFunc (GL_ALWAYS, 0x0, 0x0) );
  GE( glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT) );
  GE( glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE) );
  
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
	       | (ctx->color_alpha < 255
		  ? COGL_ENABLE_BLEND : 0));
  
  GE( glVertexPointer (2, GL_FLOAT, 0, ctx->path_nodes) );
  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, ctx->path_nodes_size) );
  
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
  GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );
  
  bounds_x = CLUTTER_FIXED_FLOOR (ctx->path_nodes_min.x);
  bounds_y = CLUTTER_FIXED_FLOOR (ctx->path_nodes_min.y);
  bounds_w = CLUTTER_FIXED_CEIL (ctx->path_nodes_max.x - ctx->path_nodes_min.x);
  bounds_h = CLUTTER_FIXED_CEIL (ctx->path_nodes_max.y - ctx->path_nodes_min.y);
  
  cogl_fast_fill_rectangle (bounds_x, bounds_y, bounds_w, bounds_h);
  
  GE( glDisable (GL_STENCIL_TEST) );
}

/**
 * cogl_fill:
 *
 * Fills the constructed shape using the current drawing color.
 **/
void
cogl_fill ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes_size == 0)
    return;
  
  _cogl_path_fill_nodes();
  
}

/**
 * cogl_stroke:
 *
 * Strokes the constructed shape using the current drawing color
 * and a width of 1 pixel (regardless of the current transformation
 * matrix).
 **/
void
cogl_stroke ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes_size == 0)
    return;
  
  _cogl_path_stroke_nodes();
}

/**
 * cogl_path_move_to:
 * @x: X coordinate of the pen location to move to.
 * @y: Y coordinate of the pen location to move to.
 *
 * Clears the previously constructed shape and begins a new path
 * contour by moving the pen to the given coordinates.
 **/
void
cogl_path_move_to (ClutterFixed x,
		   ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  /* FIXME: handle multiple contours maybe? */
  
  _cogl_path_clear_nodes ();
  _cogl_path_add_node (x, y);
  
  ctx->path_start.x = x;
  ctx->path_start.y = y;
  
  ctx->path_pen = ctx->path_start;
}

/**
 * cogl_path_move_to_rel:
 * @x: X offset from the current pen location to move the pen to.
 * @y: Y offset from the current pen location to move the pen to.
 *
 * Clears the previously constructed shape and begins a new path
 * contour by moving the pen to the given coordinates relative
 * to the current pen location.
 **/
void
cogl_path_move_to_rel (ClutterFixed x,
		       ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_move_to (ctx->path_pen.x + x,
		     ctx->path_pen.y + y);
}

/**
 * cogl_path_line_to:
 * @x: X coordinate of the end line vertex
 * @y: Y coordinate of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates.
 **/
void
cogl_path_line_to (ClutterFixed x,
		   ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  _cogl_path_add_node (x, y);
  
  ctx->path_pen.x = x;
  ctx->path_pen.y = y;
}

/**
 * cogl_path_line_to:
 * @x: X offset from the current pen location of the end line vertex
 * @y: Y offset from the current pen location of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates relative to the current pen location.
 **/
void
cogl_path_line_to_rel (ClutterFixed x,
		       ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_line_to (ctx->path_pen.x + x,
		     ctx->path_pen.y + y);
}

/**
 * cogl_path_h_line_to:
 * @x: X coordinate of the end line vertex
 *
 * Adds a straight horizontal line segment to the current path that
 * ends at the given X coordinate and current pen Y coordinate.
 **/
void
cogl_path_h_line_to (ClutterFixed x)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_line_to (x,
		     ctx->path_pen.y);
}

/**
 * cogl_path_v_line_to:
 * @y: Y coordinate of the end line vertex
 *
 * Adds a stright vertical line segment to the current path that ends
 * at the current pen X coordinate and the given Y coordinate.
 **/
void
cogl_path_v_line_to (ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_line_to (ctx->path_pen.x,
		     y);
}

/**
 * cogl_path_h_line_to_rel:
 * @x: X offset from the current pen location of the end line vertex
 *
 * Adds a straight horizontal line segment to the current path that
 * ends at the given X coordinate relative to the current pen location
 * and current pen Y coordinate.
 **/
void
cogl_path_h_line_to_rel (ClutterFixed x)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_line_to (ctx->path_pen.x + x,
		     ctx->path_pen.y);
}

/**
 * cogl_path_v_line_to_rel:
 * @y: Y offset from the current pen location of the end line vertex
 *
 * Adds a stright vertical line segment to the current path that ends
 * at the current pen X coordinate and the given Y coordinate relative
 * to the current pen location.
 **/
void
cogl_path_v_line_to_rel (ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_line_to (ctx->path_pen.x,
		     ctx->path_pen.y + y);
}

/**
 * cogl_path_close:
 *
 * Closes the path being constructed by adding a straight line segment
 * to it that ends at the first vertex of the path.
 **/
void
cogl_path_close ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  _cogl_path_add_node (ctx->path_start.x, ctx->path_start.y);
  ctx->path_pen = ctx->path_start;
}

/**
 * cogl_line:
 * @x1: X coordinate of the start line vertex
 * @y1: Y coordinate of the start line vertex
 * @x2: X coordinate of the end line vertex
 * @y2: Y coordinate of the end line vertex
 *
 * Clears the previously constructed shape and constructs a straight
 * line shape start and ending at the given coordinates.
 **/
void
cogl_line (ClutterFixed x1,
	   ClutterFixed y1,
	   ClutterFixed x2,
	   ClutterFixed y2)
{
  cogl_path_move_to (x1, y1);
  cogl_path_line_to (x2, y2);
}

/**
 * cogl_polyline:
 * @coords: A pointer to the first element of an array of fixed-point
 * values that specify the vertex coordinates.
 * @num_points: The total number of vertices.
 *
 * Clears the previously constructed shape and constructs a series of straight
 * line segments, starting from the first given vertex coordinate. Each
 * subsequent segment stars where the previous one ended and ends at the next
 * given vertex coordinate.
 *
 * The coords array must contain 2 * num_points values. The first value
 * represents the X coordinate of the first vertex, the second value
 * represents the Y coordinate of the first vertex, continuing in the same
 * fashion for the rest of the vertices. (num_points - 1) segments will
 * be constructed.
 **/
void
cogl_polyline (ClutterFixed *coords,
	       gint num_points)
{
  gint c = 0;
  
  cogl_path_move_to (coords[0], coords[1]);
  
  for (c = 1; c < num_points; ++c)
    cogl_path_line_to (coords[2*c], coords[2*c+1]);
}


/**
 * cogl_polygon:
 * @coords: A pointer to the first element of an array of fixed-point
 * values that specify the vertex coordinates.
 * @num_points: The total number of vertices.
 *
 * Clears the previously constructed shape and constructs a polygonal
 * shape of the given number of vertices.
 *
 * The coords array must contain 2 * num_points values. The first value
 * represents the X coordinate of the first vertex, the second value
 * represents the Y coordinate of the first vertex, continuing in the same
 * fashion for the rest of the vertices.
 **/
void
cogl_polygon (ClutterFixed *coords,
	      gint num_points)
{
  cogl_polyline (coords, num_points);
  cogl_path_close ();
}

/**
 * cogl_rectangle:
 * @x: X coordinate of the top-left corner.
 * @y: Y coordinate of the top-left corner.
 * @width: Rectangle width.
 * @height: Rectangle height.
 *
 * Clears the previously constructed shape and constructs a rectangular
 * shape at the given coordinates.
 **/
void
cogl_rectangle (ClutterFixed x,
		ClutterFixed y,
		ClutterFixed width,
		ClutterFixed height)
{
  cogl_path_move_to (x,         y);
  cogl_path_line_to (x + width, y);
  cogl_path_line_to (x + width, y + height);
  cogl_path_line_to (x,         y + height);
  cogl_path_close   ();
}

static void
_cogl_arc (ClutterFixed center_x,
	   ClutterFixed center_y,
	   ClutterFixed radius_x,
	   ClutterFixed radius_y,
	   ClutterAngle angle_1,
	   ClutterAngle angle_2,
	   ClutterAngle angle_step,
	   guint        move_first)
{
  ClutterAngle a     = 0x0;
  ClutterAngle temp  = 0x0;
  ClutterFixed cosa  = 0x0;
  ClutterFixed sina  = 0x0;
  ClutterFixed px    = 0x0;
  ClutterFixed py    = 0x0;
  
  /* Fix invalid angles */
  
  if (angle_1 == angle_2 || angle_step == 0x0)
    return;
  
  if (angle_step < 0x0)
    angle_step = -angle_step;
  
  if (angle_2 < angle_1)
    {
      temp = angle_1;
      angle_1 = angle_2;
      angle_2 = temp;
    }
  
  /* Walk the arc by given step */
  
  for (a = angle_1; a < angle_2; a += angle_step)
    {
      cosa = clutter_cosi (a);
      sina = clutter_sini (a);

      px = center_x + CFX_MUL (cosa, radius_x);
      py = center_y + CFX_MUL (sina, radius_y);
      
      if (a == angle_1 && move_first)
	cogl_path_move_to (px, py);
      else
	cogl_path_line_to (px, py);
    }
}

/**
 * cogl_path_arc:
 * @center_x: X coordinate of the elliptical arc center
 * @center_y: Y coordinate of the elliptical arc center
 * @radius_x: X radius of the elliptical arc
 * @radius_y: Y radious of the elliptical arc
 * @angle_1: Angle in the unit-circle at which the arc begin
 * @angle_2: Angle in the unit-circle at which the arc ends
 * @angle_step: Angle increment resolution for subdivision
 *
 * Adds an elliptical arc segment to the current path. A straight line
 * segment will link the current pen location with the first vertex
 * of the arc.
 **/
void
cogl_path_arc (ClutterFixed center_x,
	       ClutterFixed center_y,
	       ClutterFixed radius_x,
	       ClutterFixed radius_y,
	       ClutterAngle angle_1,
	       ClutterAngle angle_2,
	       ClutterAngle angle_step)
{ 
  _cogl_arc (center_x,   center_y,
	     radius_x,   radius_y,
	     angle_1,    angle_2,
	     angle_step, 0 /* no move */);
}

/**
 * cogl_path_arc_rel:
 * @center_x: X offset from the current pen location of the elliptical
 * arc center
 * @center_y: Y offset from the current pen location of the elliptical
 * arc center
 * @radius_x: X radius of the elliptical arc
 * @radius_y: Y radious of the elliptical arc
 * @angle_1: Angle in the unit-circle at which the arc begin
 * @angle_2: Angle in the unit-circle at which the arc ends
 * @angle_step: Angle increment resolution for subdivision
 *
 * Adds an elliptical arc segment to the current path. A straight line
 * segment will link the current pen location with the first vertex
 * of the arc.
 **/
void
cogl_path_arc_rel (ClutterFixed center_x,
		   ClutterFixed center_y,
		   ClutterFixed radius_x,
		   ClutterFixed radius_y,
		   ClutterAngle angle_1,
		   ClutterAngle angle_2,
		   ClutterAngle angle_step)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  _cogl_arc (ctx->path_pen.x + center_x,
	     ctx->path_pen.y + center_y,
	     radius_x,   radius_y,
	     angle_1,    angle_2,
	     angle_step, 0 /* no move */);
}

/**
 * cogl_arc:
 * @center_x: X coordinate of the elliptical arc center
 * @center_y: Y coordinate of the elliptical arc center
 * @radius_x: X radius of the elliptical arc
 * @radius_y: Y radious of the elliptical arc
 * @angle_1: Angle in the unit-circle at which the arc begin
 * @angle_2: Angle in the unit-circle at which the arc ends
 * @angle_step: Angle increment resolution for subdivision
 *
 * Clears the previously constructed shape and constructs and elliptical arc
 * shape.
 **/
void
cogl_arc (ClutterFixed center_x,
	  ClutterFixed center_y,
	  ClutterFixed radius_x,
	  ClutterFixed radius_y,
	  ClutterAngle angle_1,
	  ClutterAngle angle_2,
	  ClutterAngle angle_step)
{
  _cogl_arc (center_x,   center_y,
	     radius_x,   radius_y,
	     angle_1,    angle_2,
	     angle_step, 1 /* move first */);
}

/**
 * cogl_ellipse:
 * @center_x: X coordinate of the ellipse center
 * @center_y: Y coordinate of the ellipse center
 * @radius_x: X radius of the ellipse
 * @radius_y: Y radius of the ellipse
 * @angle_step: Angle increment resolution for subdivision
 *
 * Clears the previously constructed shape and constructs an ellipse
 * shape.
 **/
void
cogl_ellipse (ClutterFixed center_x,
	      ClutterFixed center_y,
	      ClutterFixed radius_x,
	      ClutterFixed radius_y,
	      ClutterAngle angle_step)
{
  
  /* FIXME: if shows to be slow might be optimized
   * by mirroring just a quarter of it */
  
  _cogl_arc (center_x, center_y,
	     radius_x, radius_y,
	     0, CLUTTER_ANGLE_FROM_DEG(360),
	     angle_step, 1 /* move first */);
  
  cogl_path_close();
}

/**
 * cogl_round_rectangle:
 * @x: X coordinate of the top-left corner
 * @y: Y coordinate of the top-left corner
 * @width: Width of the rectangle
 * @height: Height of the rectangle
 * @radius: Radius of the corner arcs.
 * @arc_step: Angle increment resolution for subdivision of
 * the corner arcs.
 *
 * Clears the previously constructed shape and constructs a rectangular
 * shape with rounded corners.
 **/
void
cogl_round_rectangle (ClutterFixed x,
		      ClutterFixed y,
		      ClutterFixed width,
		      ClutterFixed height,
		      ClutterFixed radius,
		      ClutterAngle arc_step)
{
  ClutterFixed inner_width = width  - (radius << 1);
  ClutterFixed inner_height = height - (radius << 1);
  
  cogl_path_move_to (x, y + radius);
  cogl_path_arc_rel (radius, 0,
		     radius, radius,
		     CLUTTER_ANGLE_FROM_DEG (180),
		     CLUTTER_ANGLE_FROM_DEG (270),
		     arc_step);
  
  cogl_path_h_line_to_rel (inner_width);
  cogl_path_arc_rel       (0, radius,
			   radius, radius,
			   CLUTTER_ANGLE_FROM_DEG (-90),
			   CLUTTER_ANGLE_FROM_DEG (0),
			   arc_step);
  
  cogl_path_v_line_to_rel (inner_height);
  cogl_path_arc_rel       (-radius, 0,
			   radius, radius,
			   CLUTTER_ANGLE_FROM_DEG (0),
			   CLUTTER_ANGLE_FROM_DEG (90),
			   arc_step);
  
  cogl_path_h_line_to_rel (-inner_width);
  cogl_path_arc_rel       (0, -radius,
			   radius, radius,
			   CLUTTER_ANGLE_FROM_DEG (90),
			   CLUTTER_ANGLE_FROM_DEG (180),
			   arc_step);
  
  cogl_path_close ();
}

static void
_cogl_path_bezier2_sub (CoglBezQuad *quad)
{
  CoglBezQuad     quads[_COGL_MAX_BEZ_RECURSE_DEPTH];
  CoglBezQuad    *qleft;
  CoglBezQuad    *qright;
  CoglBezQuad    *q;
  CoglFixedVec2   mid;
  CoglFixedVec2   dif;
  CoglFixedVec2   c1;
  CoglFixedVec2   c2;
  CoglFixedVec2   c3;
  gint            qindex;
  
  /* Put first curve on stack */
  quads[0] = *quad;
  qindex   =  0;
  
  /* While stack is not empty */
  while (qindex >= 0)
    {
      
      q = &quads[qindex];
      
      /* Calculate distance of control point from its
       * counterpart on the line between end points */
      mid.x = ((q->p1.x + q->p3.x) >> 1);
      mid.y = ((q->p1.y + q->p3.y) >> 1);
      dif.x = (q->p2.x - mid.x);
      dif.y = (q->p2.y - mid.y);
      if (dif.x < 0) dif.x = -dif.x;
      if (dif.y < 0) dif.y = -dif.y;
      
      /* Cancel if the curve is flat enough */
      if (dif.x + dif.y <= CFX_ONE
	  || qindex == _COGL_MAX_BEZ_RECURSE_DEPTH - 1)
	{
	  /* Add subdivision point (skip last) */
	  if (qindex == 0) return;
	  _cogl_path_add_node (q->p3.x, q->p3.y);
	  --qindex; continue;
	}
      
      /* Left recursion goes on top of stack! */
      qright = q; qleft = &quads[++qindex];
      
      /* Subdivide into 2 sub-curves */
      c1.x = ((q->p1.x + q->p2.x) >> 1);
      c1.y = ((q->p1.y + q->p2.y) >> 1);
      c3.x = ((q->p2.x + q->p3.x) >> 1);
      c3.y = ((q->p2.y + q->p3.y) >> 1);
      c2.x = ((c1.x + c3.x) >> 1);
      c2.y = ((c1.y + c3.y) >> 1);
      
      /* Add left recursion onto stack */
      qleft->p1 = q->p1;
      qleft->p2 = c1;
      qleft->p3 = c2;
      
      /* Add right recursion onto stack */
      qright->p1 = c2;
      qright->p2 = c3;
      qright->p3 = q->p3;
    }
}

static void
_cogl_path_bezier3_sub (CoglBezCubic *cubic)
{
  CoglBezCubic   cubics[_COGL_MAX_BEZ_RECURSE_DEPTH];
  CoglBezCubic  *cleft;
  CoglBezCubic  *cright;
  CoglBezCubic  *c;
  CoglFixedVec2  dif1;
  CoglFixedVec2  dif2;
  CoglFixedVec2  mm;
  CoglFixedVec2  c1;
  CoglFixedVec2  c2;
  CoglFixedVec2  c3;
  CoglFixedVec2  c4;
  CoglFixedVec2  c5;
  gint           cindex;
  
  /* Put first curve on stack */
  cubics[0] = *cubic;
  cindex    =  0;
  
  while (cindex >= 0)
    {
      c = &cubics[cindex];
      
#define CFX_MUL2(x) ((x) << 1)
#define CFX_MUL3(x) (((x) << 1) + (x))
#define CFX_SQ(x) CFX_MUL (x, x)
      
      /* Calculate distance of control points from their
       * counterparts on the line between end points */
      dif1.x = CFX_MUL3 (c->p2.x) - CFX_MUL2 (c->p1.x) - c->p4.x;
      dif1.y = CFX_MUL3 (c->p2.y) - CFX_MUL2 (c->p1.y) - c->p4.y;
      dif2.x = CFX_MUL3 (c->p3.x) - CFX_MUL2 (c->p4.x) - c->p1.x;
      dif2.y = CFX_MUL3 (c->p3.y) - CFX_MUL2 (c->p4.y) - c->p1.y;
      if (dif1.x < 0) dif1.x = -dif1.x;
      if (dif1.y < 0) dif1.y = -dif1.y;
      if (dif2.x < 0) dif2.x = -dif2.x;
      if (dif2.y < 0) dif2.y = -dif2.y;
      
#undef CFX_MUL2
#undef CFX_MUL3
#undef CFX_SQ
      
      /* Pick the greatest of two distances */
      if (dif1.x < dif2.x) dif1.x = dif2.x;
      if (dif1.y < dif2.y) dif1.y = dif2.y;
      
      /* Cancel if the curve is flat enough */
      if (dif1.x + dif1.y <= CFX_ONE
	  || cindex == _COGL_MAX_BEZ_RECURSE_DEPTH-1)
	{
	  /* Add subdivision point (skip last) */
	  if (cindex == 0) return;
	  _cogl_path_add_node (c->p4.x, c->p4.y);
	  --cindex; continue;
	}
      
      /* Left recursion goes on top of stack! */
      cright = c; cleft = &cubics[++cindex];
      
      /* Subdivide into 2 sub-curves */
      c1.x = ((c->p1.x + c->p2.x) >> 1);
      c1.y = ((c->p1.y + c->p2.y) >> 1);
      mm.x = ((c->p2.x + c->p3.x) >> 1);
      mm.y = ((c->p2.y + c->p3.y) >> 1);
      c5.x = ((c->p3.x + c->p4.x) >> 1);
      c5.y = ((c->p3.y + c->p4.y) >> 1);
      
      c2.x = ((c1.x + mm.x) >> 1);
      c2.y = ((c1.y + mm.y) >> 1);
      c4.x = ((mm.x + c5.x) >> 1);
      c4.y = ((mm.y + c5.y) >> 1);
      
      c3.x = ((c2.x + c4.x) >> 1);
      c3.y = ((c2.y + c4.y) >> 1);
      
      /* Add left recursion to stack */
      cleft->p1 = c->p1;
      cleft->p2 = c1;
      cleft->p3 = c2;
      cleft->p4 = c3;
      
      /* Add right recursion to stack */
      cright->p1 = c3;
      cright->p2 = c4;
      cright->p3 = c5;
      cright->p4 = c->p4;
    }
}

/**
 * cogl_path_bezier2_to:
 * @x1: X coordinate of the second bezier control point
 * @y1: Y coordinate of the second bezier control point
 * @x2: X coordinate of the third bezier control point
 * @y2: Y coordinate of the third bezier control point
 *
 * Adds a quadratic bezier curve segment to the current path with the given
 * second and third control points and using current pen location as the
 * first control point.
 **/
void
cogl_path_bezier2_to (ClutterFixed x1,
		      ClutterFixed y1,
		      ClutterFixed x2,
		      ClutterFixed y2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  CoglBezQuad quad;
  
  /* Prepare quadratic curve */
  quad.p1 = ctx->path_pen;
  quad.p2.x = x1;
  quad.p2.y = y1;
  quad.p3.x = x2;
  quad.p3.y = y2;
  
  /* Run subdivision */
  _cogl_path_bezier2_sub (&quad);
  
  /* Add last point */
  _cogl_path_add_node (quad.p3.x, quad.p3.y);
  ctx->path_pen = quad.p3;
}

/**
 * cogl_path_bezier3_to:
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
void
cogl_path_bezier3_to (ClutterFixed x1,
		      ClutterFixed y1,
		      ClutterFixed x2,
		      ClutterFixed y2,
		      ClutterFixed x3,
		      ClutterFixed y3)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  CoglBezCubic cubic;
  
  /* Prepare cubic curve */
  cubic.p1 = ctx->path_pen;
  cubic.p2.x = x1;
  cubic.p2.y = y1;
  cubic.p3.x = x2;
  cubic.p3.y = y2;
  cubic.p4.x = x3;
  cubic.p4.y = y3;
  
  /* Run subdivision */
  _cogl_path_bezier3_sub (&cubic);
  
  /* Add last point */
  _cogl_path_add_node (cubic.p4.x, cubic.p4.y);
  ctx->path_pen = cubic.p4;
}

/**
 * cogl_path_bezier2_to_rel:
 * @x1: X coordinate of the second bezier control point
 * @y1: Y coordinate of the second bezier control point
 * @x2: X coordinate of the third bezier control point
 * @y2: Y coordinate of the third bezier control point
 *
 * Adds a quadratic bezier curve segment to the current path with the given
 * second and third control points and using current pen location as the
 * first control point. The given coordinates are relative to the current
 * pen location.
 **/
void
cogl_path_bezier2_to_rel (ClutterFixed x1,
			  ClutterFixed y1,
			  ClutterFixed x2,
			  ClutterFixed y2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_bezier2_to (ctx->path_pen.x + x1,
			ctx->path_pen.y + y2,
			ctx->path_pen.x + x2,
			ctx->path_pen.y + y2);
}

/**
 * cogl_path_bezier3_to_rel:
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
 **/
void
cogl_path_bezier3_to_rel (ClutterFixed x1,
			  ClutterFixed y1,
			  ClutterFixed x2,
			  ClutterFixed y2,
			  ClutterFixed x3,
			  ClutterFixed y3)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_bezier3_to (ctx->path_pen.x + x1,
			ctx->path_pen.y + y2,
			ctx->path_pen.x + x2,
			ctx->path_pen.y + y2,
			ctx->path_pen.x + x3,
			ctx->path_pen.y + y3);
}
