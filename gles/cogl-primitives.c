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
  /* 32-bit integers are not supported as coord types
     in GLES . Fixed type has got 16 bits left of the
     point which is equal to short anyway. */
  
  GLshort rect_verts[8] = {
    (GLshort)  x,          (GLshort)  y,
    (GLshort) (x + width), (GLshort)  y,
    (GLshort)  x,          (GLshort) (y + height),
    (GLshort) (x + width), (GLshort) (y + height)
  };

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
              | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));
  GE ( cogl_wrap_glVertexPointer (2, GL_SHORT, 0, rect_verts ) );
  GE ( cogl_wrap_glDrawArrays (GL_TRIANGLE_STRIP, 0, 4) );
}


void
_cogl_rectanglex (CoglFixed x,
                  CoglFixed y,
                  CoglFixed width,
                  CoglFixed height)
{
  GLfixed rect_verts[8] = {
    x,         y,
    x + width, y,
    x,         y + height,
    x + width, y + height
  };
   
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
	       | (ctx->color_alpha < 255
		  ? COGL_ENABLE_BLEND : 0));
  
  GE( cogl_wrap_glVertexPointer (2, GL_FIXED, 0, rect_verts) );
  GE( cogl_wrap_glDrawArrays (GL_TRIANGLE_STRIP, 0, 4) );

}


void
_cogl_path_clear_nodes ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes)
    g_free(ctx->path_nodes);
  
  ctx->path_nodes = (CoglFixedVec2*) g_malloc (2 * sizeof(CoglFixedVec2));
  ctx->path_nodes_size = 0;
  ctx->path_nodes_cap = 2;
}

void
_cogl_path_add_node (CoglFixed x,
		     CoglFixed y)
{
  CoglFixedVec2   *new_nodes = NULL;
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->path_nodes_size == ctx->path_nodes_cap)
    {
      new_nodes = g_realloc (ctx->path_nodes,
			     2 * ctx->path_nodes_cap
			     * sizeof (CoglFixedVec2));
      
      if (new_nodes == NULL) return;

      ctx->path_nodes = new_nodes;
      ctx->path_nodes_cap *= 2;
    }
  
  ctx->path_nodes [ctx->path_nodes_size].x = x;
  ctx->path_nodes [ctx->path_nodes_size].y = y;
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
  
  GE( cogl_wrap_glVertexPointer (2, GL_FIXED, 0, ctx->path_nodes) );
  GE( cogl_wrap_glDrawArrays (GL_LINE_STRIP, 0, ctx->path_nodes_size) );
}

static gint compare_ints (gconstpointer a,
                          gconstpointer b)
{
  return GPOINTER_TO_INT(a)-GPOINTER_TO_INT(b);
}

void
_cogl_path_fill_nodes ()
{
  guint bounds_x;
  guint bounds_y;
  guint bounds_w;
  guint bounds_h;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  bounds_x = COGL_FIXED_FLOOR (ctx->path_nodes_min.x);
  bounds_y = COGL_FIXED_FLOOR (ctx->path_nodes_min.y);
  bounds_w = COGL_FIXED_CEIL (ctx->path_nodes_max.x - ctx->path_nodes_min.x);
  bounds_h = COGL_FIXED_CEIL (ctx->path_nodes_max.y - ctx->path_nodes_min.y);

  if (cogl_features_available (COGL_FEATURE_STENCIL_BUFFER))
    {
      GE( glClear (GL_STENCIL_BUFFER_BIT) );

      GE( cogl_wrap_glEnable (GL_STENCIL_TEST) );
      GE( glStencilFunc (GL_NEVER, 0x0, 0x1) );
      GE( glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT) );
  
      GE( glStencilMask (1) );

      cogl_enable (COGL_ENABLE_VERTEX_ARRAY
		   | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));
  
      GE( cogl_wrap_glVertexPointer (2, GL_FIXED, 0, ctx->path_nodes) );
      GE( cogl_wrap_glDrawArrays (GL_TRIANGLE_FAN, 0, ctx->path_nodes_size) );
  
      GE( glStencilMask (~(GLuint) 0) );
  
      /* Merge the stencil buffer with any clipping rectangles */
      _cogl_clip_stack_merge ();
  
      GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
      GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
  
      cogl_rectangle (bounds_x, bounds_y, bounds_w, bounds_h);
  
      /* Rebuild the stencil clip */
      _cogl_clip_stack_rebuild (TRUE);
    }
  else
    {
      /* This is our edge list it stores intersections between our
       * curve and scanlines, it should probably be implemented with a
       * data structure that has smaller overhead for inserting the
       * curve/scanline intersections.
       */
      GSList *scanlines[bounds_h];

      gint i;
      gint prev_x;
      gint prev_y;
      gint first_x;
      gint first_y;
      gint lastdir=-2; /* last direction we vere moving */
      gint lastline=-1; /* the previous scanline we added to */

      /* clear scanline intersection lists */
      for (i=0; i < bounds_h; i++) 
	scanlines[i]=NULL;

      first_x = prev_x = COGL_FIXED_TO_INT (ctx->path_nodes[0].x);
      first_y = prev_y = COGL_FIXED_TO_INT (ctx->path_nodes[0].y);

      /* create scanline intersection list */
      for (i=1; i<ctx->path_nodes_size; i++)
	{
	  gint dest_x = COGL_FIXED_TO_INT (ctx->path_nodes[i].x);
	  gint dest_y = COGL_FIXED_TO_INT (ctx->path_nodes[i].y);
	  gint ydir;
	  gint dx;
	  gint dy;
	  gint y;

	fill_close:
	  dx = dest_x - prev_x;
	  dy = dest_y - prev_y;

	  if (dy < 0)
	    ydir = -1;
	  else if (dy > 0)
	    ydir = 1;
	  else
	    ydir = 0;

	  /* do linear interpolation between vertexes */
	  for (y=prev_y; y!= dest_y; y += ydir)
	    {

	      /* only add a point if the scanline has changed and we're
	       * within bounds.
	       */
	      if (y-bounds_y >= 0 &&
		  y-bounds_y < bounds_h &&
		  lastline != y)
		{
		  gint x = prev_x + (dx * (y-prev_y)) / dy;

		  scanlines[ y - bounds_y ]=
		    g_slist_insert_sorted (scanlines[ y - bounds_y],
					   GINT_TO_POINTER(x),
					   compare_ints);

		  if (ydir != lastdir &&  /* add a double entry when changing */
		      lastdir!=-2)        /* vertical direction */
		    scanlines[ y - bounds_y ]=
		      g_slist_insert_sorted (scanlines[ y - bounds_y],
					     GINT_TO_POINTER(x),
					     compare_ints);
		  lastdir = ydir;
		  lastline = y;
		}
	    }

	  prev_x = dest_x;
	  prev_y = dest_y;

	  /* if we're on the last knot, fake the first vertex being a
	     next one */
	  if (ctx->path_nodes_size == i+1)
	    {
	      dest_x = first_x;
	      dest_y = first_y;
	      i++; /* to make the loop finally end */
	      goto fill_close;
	    }
	}

      {
	gint spans = 0;
	gint span_no;
	GLfixed *coords;

	/* count number of spans */
	for (i=0; i < bounds_h; i++)
	  {
	    GSList *iter = scanlines[i];
	    while (iter)
	      {
		GSList *next = iter->next;
		if (!next)
		  {
		    break;
		  }
		/* draw the segments that should be visible */
		spans ++;
		iter = next->next;
	      }
	  }
	coords = g_malloc0 (spans * sizeof (GLfixed) * 3 * 2 * 2);

	span_no = 0;
	/* build list of triangles */
	for (i=0; i < bounds_h; i++)
	  {
	    GSList *iter = scanlines[i];
	    while (iter)
	      {
		GSList *next = iter->next;
		GLfixed x0, x1;
		GLfixed y0, y1;
		if (!next)
		  break;

		x0 = COGL_FIXED_FROM_INT (GPOINTER_TO_INT (iter->data));
		x1 = COGL_FIXED_FROM_INT (GPOINTER_TO_INT (next->data));
		y0 = COGL_FIXED_FROM_INT (bounds_y + i);
		y1 = COGL_FIXED_FROM_INT (bounds_y + i + 1) + 2048;
		/* render scanlines 1.0625 high to avoid gaps when
		   transformed */

		coords[span_no * 12 + 0] = x0;
		coords[span_no * 12 + 1] = y0;
		coords[span_no * 12 + 2] = x1;
		coords[span_no * 12 + 3] = y0;
		coords[span_no * 12 + 4] = x1;
		coords[span_no * 12 + 5] = y1;
		coords[span_no * 12 + 6] = x0;
		coords[span_no * 12 + 7] = y0;
		coords[span_no * 12 + 8] = x0;
		coords[span_no * 12 + 9] = y1;
		coords[span_no * 12 + 10] = x1;
		coords[span_no * 12 + 11] = y1;
		span_no ++;
		iter = next->next;
	      }
	  }
	for (i=0; i < bounds_h; i++)
	  {
	    g_slist_free (scanlines[i]);
	  }

        /* render triangles */
        cogl_enable (COGL_ENABLE_VERTEX_ARRAY
		     | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));
        GE ( cogl_wrap_glVertexPointer (2, GL_FIXED, 0, coords ) );
        GE ( cogl_wrap_glDrawArrays (GL_TRIANGLES, 0, spans * 2 * 3));
        g_free (coords);
      }
    }
}

