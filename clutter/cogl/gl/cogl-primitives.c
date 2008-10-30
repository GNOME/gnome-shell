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
#include "cogl-clip-stack.h"

#include <string.h>
#include <gmodule.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

void
_cogl_rectangle (gint x,
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
_cogl_rectanglex (CoglFixed x,
                  CoglFixed y,
                  CoglFixed width,
                  CoglFixed height)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (ctx->color_alpha < 255
	       ? COGL_ENABLE_BLEND : 0);
  
  GE( glRectf (COGL_FIXED_TO_FLOAT (x),
	       COGL_FIXED_TO_FLOAT (y),
	       COGL_FIXED_TO_FLOAT (x + width),
	       COGL_FIXED_TO_FLOAT (y + height)) );
}

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
_cogl_path_add_node (CoglFixed x,
		     CoglFixed y)
{
  CoglFloatVec2   *new_nodes = NULL;
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes_size == ctx->path_nodes_cap)
    {
      new_nodes = g_realloc (ctx->path_nodes,
			     2 * ctx->path_nodes_cap
			     * sizeof (CoglFloatVec2));
      
      if (new_nodes == NULL) return;

      ctx->path_nodes = new_nodes;
      ctx->path_nodes_cap *= 2;
    }
  
  ctx->path_nodes [ctx->path_nodes_size] .x = COGL_FIXED_TO_FLOAT (x);
  ctx->path_nodes [ctx->path_nodes_size] .y = COGL_FIXED_TO_FLOAT (y);
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

void
_cogl_path_fill_nodes ()
{
  guint bounds_x;
  guint bounds_y;
  guint bounds_w;
  guint bounds_h;
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  GE( glClear (GL_STENCIL_BUFFER_BIT) );

  GE( glEnable (GL_STENCIL_TEST) );
  GE( glStencilFunc (GL_NEVER, 0x0, 0x1) );
  GE( glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT) );

  GE( glStencilMask (1) );
  
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
	       | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));
  
  GE( glVertexPointer (2, GL_FLOAT, 0, ctx->path_nodes) );
  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, ctx->path_nodes_size) );
  
  GE( glStencilMask (~(GLuint) 0) );
  
  /* Merge the stencil buffer with any clipping rectangles */
  _cogl_clip_stack_merge ();
  
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );

  bounds_x = COGL_FIXED_FLOOR (ctx->path_nodes_min.x);
  bounds_y = COGL_FIXED_FLOOR (ctx->path_nodes_min.y);
  bounds_w = COGL_FIXED_CEIL (ctx->path_nodes_max.x - ctx->path_nodes_min.x);
  bounds_h = COGL_FIXED_CEIL (ctx->path_nodes_max.y - ctx->path_nodes_min.y);
  
  cogl_rectangle (bounds_x, bounds_y, bounds_w, bounds_h);
  
  /* Rebuild the stencil clip */
  _cogl_clip_stack_rebuild (TRUE);
}
