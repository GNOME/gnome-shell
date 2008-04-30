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
  
  /* 32-bit integers are not supported as coord types
     in GLES . Fixed type has got 16 bits left of the
     point which is equal to short anyway. */
  
  GLshort rect_verts[8] = {
    (GLshort)  x,          (GLshort)  y,
    (GLshort) (x + width), (GLshort)  y,
    (GLshort)  x,          (GLshort) (y + height),
    (GLshort) (x + width), (GLshort) (y + height)
  };

  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
              | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));
  GE ( glVertexPointer (2, GL_SHORT, 0, rect_verts ) );
  GE ( glDrawArrays (GL_TRIANGLE_STRIP, 0, 4) );
}


void
cogl_rectanglex (ClutterFixed x,
                 ClutterFixed y,
                 ClutterFixed width,
                 ClutterFixed height)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  GLfixed rect_verts[8] = {
    x,         y,
    x + width, y,
    x,         y + height,
    x + width, y + height
  };
   
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
	       | (ctx->color_alpha < 255
		  ? COGL_ENABLE_BLEND : 0));
  
  GE( glVertexPointer (2, GL_FIXED, 0, rect_verts) );
  GE( glDrawArrays (GL_TRIANGLE_STRIP, 0, 4) );

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
  
  ctx->path_nodes = (CoglFixedVec2*) g_malloc (2 * sizeof(CoglFixedVec2));
  ctx->path_nodes_size = 0;
  ctx->path_nodes_cap = 2;
}

void
_cogl_path_add_node (ClutterFixed x,
		     ClutterFixed y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  CoglFixedVec2   *new_nodes = NULL;
  
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

static void
_cogl_path_stroke_nodes ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
	       | (ctx->color_alpha < 255
		  ? COGL_ENABLE_BLEND : 0));
  
  GE( glVertexPointer (2, GL_FIXED, 0, ctx->path_nodes) );
  GE( glDrawArrays (GL_LINE_STRIP, 0, ctx->path_nodes_size) );
}

static gint compare_ints (gconstpointer a,
                          gconstpointer b)
{
  return GPOINTER_TO_INT(a)-GPOINTER_TO_INT(b);
}

static void
_cogl_path_fill_nodes ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  guint bounds_x;
  guint bounds_y;
  guint bounds_w;
  guint bounds_h;

  bounds_x = CLUTTER_FIXED_FLOOR (ctx->path_nodes_min.x);
  bounds_y = CLUTTER_FIXED_FLOOR (ctx->path_nodes_min.y);
  bounds_w = CLUTTER_FIXED_CEIL (ctx->path_nodes_max.x - ctx->path_nodes_min.x);
  bounds_h = CLUTTER_FIXED_CEIL (ctx->path_nodes_max.y - ctx->path_nodes_min.y);

#if GOT_WORKING_STENCIL_BUFFER
  
  GE( glClear (GL_STENCIL_BUFFER_BIT) );

  GE( glEnable (GL_STENCIL_TEST) );
  GE( glStencilFunc (GL_ALWAYS, 0x0, 0x0) );
  GE( glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT) );
  GE( glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE) );
  
  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
	       | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));
  
  GE( glVertexPointer (2, GL_FIXED, 0, ctx->path_nodes) );
  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, ctx->path_nodes_size) );
  
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
  GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );
  
  
  cogl_rectangle (bounds_x, bounds_y, bounds_w, bounds_h);
  
  GE( glDisable (GL_STENCIL_TEST) );
#endif
  {
    GSList *scanlines[bounds_h];
    /* This is our edge list it stores intersections between our curve and
     * scanlines */

    gint i;
    gint prev_x;
    gint prev_y;
    gint first_x;
    gint first_y;

    /* clear scanline intersection lists */
    for (i=0; i < bounds_h; i++) 
      scanlines[i]=NULL;

    first_x = prev_x = CLUTTER_FIXED_TO_INT (ctx->path_nodes[0].x);
    first_y = prev_y = CLUTTER_FIXED_TO_INT (ctx->path_nodes[0].y);

    /* saturate scanline intersection list */
    for (i=1; i<ctx->path_nodes_size; i++)
      {
        gint dest_x = CLUTTER_FIXED_TO_INT (ctx->path_nodes[i].x);
        gint dest_y = CLUTTER_FIXED_TO_INT (ctx->path_nodes[i].y);
        gint ydir;
        gint dx;
        gint dy;
        gint y;

fill_close:
        dx = dest_x - prev_x;
        dy = dest_y - prev_y;

        if (dy < 0)
          ydir = -1;
        else
          ydir = 1;

        /* do linear interpolation between vertexes */
        for (y=prev_y; y!= dest_y; y += ydir)
          {
            if (y-bounds_y >= 0 &&
                y-bounds_y < bounds_h)
              {
                gint x = prev_x + (dx * (y-prev_y)) / dy;

                scanlines[ y - bounds_y ]=
                  g_slist_insert_sorted (scanlines[ y - bounds_y],
                                         GINT_TO_POINTER(x),
                                         compare_ints);
              }
          }

        prev_x = dest_x;
        prev_y = dest_y;

        /* if we're on the last knot, fake the first vertex being a next one */
        if (ctx->path_nodes_size == i+1)
          {
            dest_x = first_x;
            dest_y = first_y;
            i++; /* to make the loop finally end */
            goto fill_close;
          }
      }

    /* for each scanline */
    for (i=0; i < bounds_h; i++)
      {
        GSList *iter = scanlines[i];
        while (iter)
          {
            GSList *next = iter->next;
            gint startx, endx;
            if (!next)
              break;

            startx = GPOINTER_TO_INT (iter->data);
            endx   = GPOINTER_TO_INT (next->data);

            /* draw the segments that should be visible */

            cogl_rectangle (startx, i + bounds_y, endx - startx, 1);
            iter = next->next;
          }
        if (scanlines[i])
          g_slist_free (scanlines[i]);
      }
  }
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
