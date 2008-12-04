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

/* these are defined in the particular backend(float in gl vs fixed in gles)*/
void _cogl_path_add_node    (gboolean new_sub_path,
			     CoglFixed x,
                             CoglFixed y);
void _cogl_path_fill_nodes    ();
void _cogl_path_stroke_nodes  ();
void _cogl_rectangle (gint x,
                      gint y,
                      guint width,
                      guint height);
void _cogl_rectanglex (CoglFixed x,
                       CoglFixed y,
                       CoglFixed width,
                       CoglFixed height);
void
cogl_rectangle (gint x,
                gint y,
                guint width,
                guint height)
{
  cogl_clip_ensure ();

  _cogl_rectangle (x, y, width, height);
}

void
cogl_rectanglex (CoglFixed x,
                 CoglFixed y,
                 CoglFixed width,
                 CoglFixed height)
{
  cogl_clip_ensure ();

  _cogl_rectanglex (x, y, width, height);
}


void
cogl_path_fill (void)
{
  cogl_path_fill_preserve ();

  cogl_path_new ();
}

void
cogl_path_fill_preserve (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_clip_ensure ();
  
  if (ctx->path_nodes->len == 0)
    return;  

  _cogl_path_fill_nodes ();
}

void
cogl_path_stroke (void)
{
  cogl_path_stroke_preserve ();

  cogl_path_new ();
}

void
cogl_path_stroke_preserve (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_clip_ensure ();

  if (ctx->path_nodes->len == 0)
    return;
  
  _cogl_path_stroke_nodes();
}

void
cogl_path_move_to (CoglFixed x,
                   CoglFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  /* FIXME: handle multiple contours maybe? */
 
  _cogl_path_add_node (TRUE, x, y);
  
  ctx->path_start.x = x;
  ctx->path_start.y = y;
  
  ctx->path_pen = ctx->path_start;
}

void
cogl_path_rel_move_to (CoglFixed x,
                       CoglFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_move_to (ctx->path_pen.x + x,
                     ctx->path_pen.y + y);
}

void
cogl_path_line_to (CoglFixed x,
                   CoglFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  _cogl_path_add_node (FALSE, x, y);
  
  ctx->path_pen.x = x;
  ctx->path_pen.y = y;
}

void
cogl_path_rel_line_to (CoglFixed x,
                       CoglFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_line_to (ctx->path_pen.x + x,
                     ctx->path_pen.y + y);
}

void
cogl_path_close (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  _cogl_path_add_node (FALSE, ctx->path_start.x, ctx->path_start.y);
  ctx->path_pen = ctx->path_start;
}

void
cogl_path_new (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_array_set_size (ctx->path_nodes, 0);
}

void
cogl_path_line (CoglFixed x1,
	        CoglFixed y1,
	        CoglFixed x2,
	        CoglFixed y2)
{
  cogl_path_move_to (x1, y1);
  cogl_path_line_to (x2, y2);
}

void
cogl_path_polyline (CoglFixed *coords,
	            gint num_points)
{
  gint c = 0;
  
  cogl_path_move_to (coords[0], coords[1]);
  
  for (c = 1; c < num_points; ++c)
    cogl_path_line_to (coords[2*c], coords[2*c+1]);
}

void
cogl_path_polygon (CoglFixed *coords,
	           gint          num_points)
{
  cogl_path_polyline (coords, num_points);
  cogl_path_close ();
}

void
cogl_path_rectangle (CoglFixed x,
                     CoglFixed y,
                     CoglFixed width,
                     CoglFixed height)
{
  cogl_path_move_to (x,         y);
  cogl_path_line_to (x + width, y);
  cogl_path_line_to (x + width, y + height);
  cogl_path_line_to (x,         y + height);
  cogl_path_close   ();
}

static void
_cogl_path_arc (CoglFixed center_x,
	        CoglFixed center_y,
                CoglFixed radius_x,
                CoglFixed radius_y,
                CoglAngle angle_1,
                CoglAngle angle_2,
                CoglAngle angle_step,
                guint        move_first)
{
  CoglAngle a     = 0x0;
  CoglFixed cosa  = 0x0;
  CoglFixed sina  = 0x0;
  CoglFixed px    = 0x0;
  CoglFixed py    = 0x0;
  
  /* Fix invalid angles */
  
  if (angle_1 == angle_2 || angle_step == 0x0)
    return;
  
  if (angle_step < 0x0)
    angle_step = -angle_step;
  
  /* Walk the arc by given step */
  
  a = angle_1;
  while (a != angle_2)
    {
      cosa = cogl_angle_cos (a);
      sina = cogl_angle_sin (a);

      px = center_x + COGL_FIXED_MUL (cosa, radius_x);
      py = center_y + COGL_FIXED_MUL (sina, radius_y);
      
      if (a == angle_1 && move_first)
	cogl_path_move_to (px, py);
      else
	cogl_path_line_to (px, py);
      
      if (G_LIKELY (angle_2 > angle_1))
        {
          a += angle_step;
          if (a > angle_2)
            a = angle_2;
        }
      else
        {
          a -= angle_step;
          if (a < angle_2)
            a = angle_2;
        }
    }

  /* Make sure the final point is drawn */
  
  cosa = cogl_angle_cos (angle_2);
  sina = cogl_angle_sin (angle_2);

  px = center_x + COGL_FIXED_MUL (cosa, radius_x);
  py = center_y + COGL_FIXED_MUL (sina, radius_y);

  cogl_path_line_to (px, py);
}

void
cogl_path_arc (CoglFixed center_x,
               CoglFixed center_y,
               CoglFixed radius_x,
               CoglFixed radius_y,
               CoglAngle angle_1,
               CoglAngle angle_2)
{ 
  CoglAngle angle_step = 10;
  /* it is documented that a move to is needed to create a freestanding
   * arc
   */
  _cogl_path_arc (center_x,   center_y,
	          radius_x,   radius_y,
	          angle_1,    angle_2,
	          angle_step, 0 /* no move */);
}


void
cogl_path_arc_rel (CoglFixed center_x,
		   CoglFixed center_y,
		   CoglFixed radius_x,
		   CoglFixed radius_y,
		   CoglAngle angle_1,
		   CoglAngle angle_2,
		   CoglAngle angle_step)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  _cogl_path_arc (ctx->path_pen.x + center_x,
	          ctx->path_pen.y + center_y,
	          radius_x,   radius_y,
	          angle_1,    angle_2,
	          angle_step, 0 /* no move */);
}

void
cogl_path_ellipse (CoglFixed center_x,
                   CoglFixed center_y,
                   CoglFixed radius_x,
                   CoglFixed radius_y)
{
  CoglAngle angle_step = 10;
  
  /* FIXME: if shows to be slow might be optimized
   * by mirroring just a quarter of it */
  
  _cogl_path_arc (center_x, center_y,
	          radius_x, radius_y,
	          0, COGL_ANGLE_FROM_DEG (360),
	          angle_step, 1 /* move first */);
  
  cogl_path_close();
}

void
cogl_path_round_rectangle (CoglFixed x,
                           CoglFixed y,
                           CoglFixed width,
                           CoglFixed height,
                           CoglFixed radius,
                           CoglAngle arc_step)
{
  CoglFixed inner_width = width  - (radius << 1);
  CoglFixed inner_height = height - (radius << 1);
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_move_to (x, y + radius);
  cogl_path_arc_rel (radius, 0,
		     radius, radius,
		     COGL_ANGLE_FROM_DEG (180),
		     COGL_ANGLE_FROM_DEG (270),
		     arc_step);
  
  cogl_path_line_to       (ctx->path_pen.x + inner_width,
                           ctx->path_pen.y);
  cogl_path_arc_rel       (0, radius,
			   radius, radius,
			   COGL_ANGLE_FROM_DEG (-90),
			   COGL_ANGLE_FROM_DEG (0),
			   arc_step);
  
  cogl_path_line_to       (ctx->path_pen.x,
                           ctx->path_pen.y + inner_height);

  cogl_path_arc_rel       (-radius, 0,
			   radius, radius,
			   COGL_ANGLE_FROM_DEG (0),
			   COGL_ANGLE_FROM_DEG (90),
			   arc_step);
  
  cogl_path_line_to       (ctx->path_pen.x - inner_width,
                           ctx->path_pen.y);
  cogl_path_arc_rel       (0, -radius,
			   radius, radius,
			   COGL_ANGLE_FROM_DEG (90),
			   COGL_ANGLE_FROM_DEG (180),
			   arc_step);
  
  cogl_path_close ();
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
#define CFX_SQ(x) COGL_FIXED_MUL (x, x)
      
      /* Calculate distance of control points from their
       * counterparts on the line between end points */
      dif1.x = CFX_MUL3 (c->p2.x) - CFX_MUL2 (c->p1.x) - c->p4.x;
      dif1.y = CFX_MUL3 (c->p2.y) - CFX_MUL2 (c->p1.y) - c->p4.y;
      dif2.x = CFX_MUL3 (c->p3.x) - CFX_MUL2 (c->p4.x) - c->p1.x;
      dif2.y = CFX_MUL3 (c->p3.y) - CFX_MUL2 (c->p4.y) - c->p1.y;

      if (dif1.x < 0)
        dif1.x = -dif1.x;
      if (dif1.y < 0)
        dif1.y = -dif1.y;
      if (dif2.x < 0)
        dif2.x = -dif2.x;
      if (dif2.y < 0)
        dif2.y = -dif2.y;
      
#undef CFX_MUL2
#undef CFX_MUL3
#undef CFX_SQ
      
      /* Pick the greatest of two distances */
      if (dif1.x < dif2.x) dif1.x = dif2.x;
      if (dif1.y < dif2.y) dif1.y = dif2.y;
      
      /* Cancel if the curve is flat enough */
      if (dif1.x + dif1.y <= COGL_FIXED_1 ||
	  cindex == _COGL_MAX_BEZ_RECURSE_DEPTH-1)
	{
	  /* Add subdivision point (skip last) */
	  if (cindex == 0)
            return;

	  _cogl_path_add_node (FALSE, c->p4.x, c->p4.y);

	  --cindex;

          continue;
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

void
cogl_path_curve_to (CoglFixed x1,
                    CoglFixed y1,
                    CoglFixed x2,
                    CoglFixed y2,
                    CoglFixed x3,
                    CoglFixed y3)
{
  CoglBezCubic cubic;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

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
  _cogl_path_add_node (FALSE, cubic.p4.x, cubic.p4.y);
  ctx->path_pen = cubic.p4;
}

void
cogl_path_rel_curve_to (CoglFixed x1,
                        CoglFixed y1,
                        CoglFixed x2,
                        CoglFixed y2,
                        CoglFixed x3,
                        CoglFixed y3)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_curve_to (ctx->path_pen.x + x1,
                      ctx->path_pen.y + y1,
                      ctx->path_pen.x + x2,
                      ctx->path_pen.y + y2,
                      ctx->path_pen.x + x3,
                      ctx->path_pen.y + y3);
}


/* If second order beziers were needed the following code could
 * be re-enabled:
 */
#if 0

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
      if (dif.x + dif.y <= COGL_FIXED_1 ||
          qindex == _COGL_MAX_BEZ_RECURSE_DEPTH - 1)
	{
	  /* Add subdivision point (skip last) */
	  if (qindex == 0) return;
	  _cogl_path_add_node (FALSE, q->p3.x, q->p3.y);
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

void
cogl_path_curve2_to (CoglFixed x1,
                     CoglFixed y1,
                     CoglFixed x2,
                     CoglFixed y2)
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
  _cogl_path_add_node (FALSE, quad.p3.x, quad.p3.y);
  ctx->path_pen = quad.p3;
}

void
cogl_rel_curve2_to (CoglFixed x1,
                    CoglFixed y1,
                    CoglFixed x2,
                    CoglFixed y2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_path_curve2_to (ctx->path_pen.x + x1,
                       ctx->path_pen.y + y2,
                       ctx->path_pen.x + x2,
                       ctx->path_pen.y + y2);
}
#endif
