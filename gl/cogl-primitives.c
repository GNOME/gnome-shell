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

void
cogl_rectangle (gint x,
                gint y,
                guint width,
                guint height)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (ctx->color_alpha < 255
	       ? COGL_ENABLE_BLEND : 0);
  
  GE( glRecti (x, y, x + width, y + height) );
}


void
cogl_rectanglex (ClutterFixed x,
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

#if 0
void
cogl_trapezoid (gint y1,
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

void
cogl_trapezoidx (ClutterFixed y1,
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
#endif

void
_cogl_path_clear_nodes ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes)
    g_free(ctx->path_nodes);
  
  ctx->path_nodes = (CoglFloatVec2*) g_malloc (2 * sizeof(CoglFloatVec2));
  ctx->path_nodes_size = 0;
  ctx->path_nodes_cap = 2;
}

void
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

void
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
	       | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));
  
  GE( glVertexPointer (2, GL_FLOAT, 0, ctx->path_nodes) );
  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, ctx->path_nodes_size) );
  
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
  GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );
  
  bounds_x = CLUTTER_FIXED_FLOOR (ctx->path_nodes_min.x);
  bounds_y = CLUTTER_FIXED_FLOOR (ctx->path_nodes_min.y);
  bounds_w = CLUTTER_FIXED_CEIL (ctx->path_nodes_max.x - ctx->path_nodes_min.x);
  bounds_h = CLUTTER_FIXED_CEIL (ctx->path_nodes_max.y - ctx->path_nodes_min.y);
  
  cogl_rectangle (bounds_x, bounds_y, bounds_w, bounds_h);
  
  GE( glDisable (GL_STENCIL_TEST) );
}

void
cogl_fill ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes_size == 0)
    return;
  
  _cogl_path_fill_nodes();
  
}

void
cogl_stroke ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes_size == 0)
    return;
  
  _cogl_path_stroke_nodes();
}
