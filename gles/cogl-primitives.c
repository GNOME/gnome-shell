/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

void
_cogl_path_add_node (gboolean new_sub_path,
		     float x,
		     float y)
{
  CoglPathNode new_node;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  new_node.x = x;
  new_node.y = y;
  new_node.path_size = 0;

  if (new_sub_path || ctx->path_nodes->len == 0)
    ctx->last_path = ctx->path_nodes->len;

  g_array_append_val (ctx->path_nodes, new_node);

  g_array_index (ctx->path_nodes, CoglPathNode, ctx->last_path).path_size++;

  if (ctx->path_nodes->len == 1)
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
  guint   path_start = 0;
  gulong  enable_flags = COGL_ENABLE_VERTEX_ARRAY;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  enable_flags |= cogl_material_get_cogl_enable_flags (ctx->source_material);
  cogl_enable (enable_flags);

  cogl_material_flush_gl_state (ctx->source_material,
                                COGL_MATERIAL_FLUSH_DISABLE_MASK,
                                (guint32)~0, /* disable all texture layers */
                                NULL);
  _cogl_current_matrix_state_flush ();

  while (path_start < ctx->path_nodes->len)
    {
      CoglPathNode *path = &g_array_index (ctx->path_nodes, CoglPathNode,
                                           path_start);

      GE( glVertexPointer (2, GL_FLOAT, sizeof (CoglPathNode),
                           (guchar *) path
                           + G_STRUCT_OFFSET (CoglPathNode, x)) );
      GE( glDrawArrays (GL_LINE_STRIP, 0, path->path_size) );

      path_start += path->path_size;
    }
}

static void
_cogl_path_get_bounds (floatVec2 nodes_min,
                       floatVec2 nodes_max,
                       float *bounds_x,
                       float *bounds_y,
                       float *bounds_w,
                       float *bounds_h)
{
  *bounds_x = nodes_min.x;
  *bounds_y = nodes_min.y;
  *bounds_w = nodes_max.x - *bounds_x;
  *bounds_h = nodes_max.y - *bounds_y;
}

static gint compare_ints (gconstpointer a,
                          gconstpointer b)
{
  return GPOINTER_TO_INT(a)-GPOINTER_TO_INT(b);
}

void
_cogl_add_path_to_stencil_buffer (floatVec2 nodes_min,
                                  floatVec2 nodes_max,
                                  guint         path_size,
                                  CoglPathNode *path,
                                  gboolean      merge)
{
  guint   path_start = 0;
  guint   sub_path_num = 0;
  float   bounds_x;
  float   bounds_y;
  float   bounds_w;
  float   bounds_h;
  gulong  enable_flags = COGL_ENABLE_VERTEX_ARRAY;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Just setup a simple material that doesn't use texturing... */
  cogl_material_flush_gl_state (ctx->stencil_material, NULL);

  enable_flags |=
    cogl_material_get_cogl_enable_flags (ctx->source_material);
  cogl_enable (enable_flags);

  _cogl_path_get_bounds (nodes_min, nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  if (merge)
    {
      GE( glStencilMask (2) );
      GE( glStencilFunc (GL_LEQUAL, 0x2, 0x6) );
    }
  else
    {
      GE( glClear (GL_STENCIL_BUFFER_BIT) );
      GE( glStencilMask (1) );
      GE( glStencilFunc (GL_LEQUAL, 0x1, 0x3) );
    }

  GE( glEnable (GL_STENCIL_TEST) );
  GE( glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT) );

  GE( glColorMask (FALSE, FALSE, FALSE, FALSE) );
  GE( glDepthMask (FALSE) );

  _cogl_current_matrix_state_flush ();
  while (path_start < path_size)
    {
      GE( glVertexPointer (2, GL_FLOAT, sizeof (CoglPathNode),
                           (guchar *) path
                           + G_STRUCT_OFFSET (CoglPathNode, x)) );
      GE( glDrawArrays (GL_TRIANGLE_FAN, 0, path->path_size) );

      if (sub_path_num > 0)
        {
          /* Union the two stencil buffers bits into the least
             significant bit */
          GE( glStencilMask (merge ? 6 : 3) );
          GE( glStencilOp (GL_ZERO, GL_REPLACE, GL_REPLACE) );
          cogl_rectangle (bounds_x, bounds_y,
                          bounds_x + bounds_w, bounds_y + bounds_h);

          GE( glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT) );
        }

      GE( glStencilMask (merge ? 4 : 2) );

      path_start += path->path_size;
      path += path->path_size;
      sub_path_num++;
    }

  if (merge)
    {
      /* Now we have the new stencil buffer in bit 1 and the old
         stencil buffer in bit 0 so we need to intersect them */
      GE( glStencilMask (3) );
      GE( glStencilFunc (GL_NEVER, 0x2, 0x3) );
      GE( glStencilOp (GL_DECR, GL_DECR, GL_DECR) );
      /* Decrement all of the bits twice so that only pixels where the
         value is 3 will remain */

      _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
      _cogl_current_matrix_push ();
      _cogl_current_matrix_identity ();

      /* Cogl generally assumes the modelview matrix is current, so since
       * cogl_rectangle will be flushing GL state and emitting geometry
       * to OpenGL it will be confused if we leave the projection matrix
       * active... */
      _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
      _cogl_current_matrix_push ();
      _cogl_current_matrix_identity ();

      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);

      _cogl_current_matrix_pop ();

      _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
      _cogl_current_matrix_pop ();

      _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
    }

  GE( glStencilMask (~(GLuint) 0) );
  GE( glDepthMask (TRUE) );
  GE( glColorMask (TRUE, TRUE, TRUE, TRUE) );

  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

static void
_cogl_path_fill_nodes_scanlines (CoglPathNode *path,
                                 guint         path_size,
                                 gint          bounds_x,
                                 gint          bounds_y,
                                 guint         bounds_w,
                                 guint         bounds_h)
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

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* clear scanline intersection lists */
  for (i=0; i < bounds_h; i++)
    scanlines[i]=NULL;

  first_x = prev_x =  (path->x);
  first_y = prev_y =  (path->y);

  /* create scanline intersection list */
  for (i=1; i < path_size; i++)
    {
      gint dest_x =  (path[i].x);
      gint dest_y =  (path[i].y);
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
      if (path_size == i+1)
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
    GLfloat *coords;

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
    coords = g_malloc0 (spans * sizeof (GLfloat) * 3 * 2 * 2);

    span_no = 0;
    /* build list of triangles */
    for (i=0; i < bounds_h; i++)
      {
        GSList *iter = scanlines[i];
        while (iter)
          {
            GSList *next = iter->next;
            GLfloat x_0, x_1;
            GLfloat y_0, y_1;
            if (!next)
              break;

            x_0 = GPOINTER_TO_INT (iter->data);
            x_1 = GPOINTER_TO_INT (next->data);
            y_0 = bounds_y + i;
            y_1 = bounds_y + i + 1.0625f;
            /* render scanlines 1.0625 high to avoid gaps when
               transformed */

            coords[span_no * 12 + 0] = x_0;
            coords[span_no * 12 + 1] = y_0;
            coords[span_no * 12 + 2] = x_1;
            coords[span_no * 12 + 3] = y_0;
            coords[span_no * 12 + 4] = x_1;
            coords[span_no * 12 + 5] = y_1;
            coords[span_no * 12 + 6] = x_0;
            coords[span_no * 12 + 7] = y_0;
            coords[span_no * 12 + 8] = x_0;
            coords[span_no * 12 + 9] = y_1;
            coords[span_no * 12 + 10] = x_1;
            coords[span_no * 12 + 11] = y_1;
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
    GE ( glVertexPointer (2, GL_FLOAT, 0, coords ) );
    GE ( glDrawArrays (GL_TRIANGLES, 0, spans * 2 * 3));
    g_free (coords);
  }
}

void
_cogl_path_fill_nodes ()
{
  float bounds_x;
  float bounds_y;
  float bounds_w;
  float bounds_h;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_get_bounds (ctx->path_nodes_min, ctx->path_nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  if (cogl_features_available (COGL_FEATURE_STENCIL_BUFFER))
    {
      _cogl_add_path_to_stencil_buffer (ctx->path_nodes_min,
                                        ctx->path_nodes_max,
                                        ctx->path_nodes->len,
                                        &g_array_index (ctx->path_nodes,
                                                        CoglPathNode, 0),
                                        ctx->clip.stencil_used);

      cogl_rectangle (bounds_x, bounds_y,
                      bounds_x + bounds_w, bounds_y + bounds_h);

      /* The stencil buffer now contains garbage so the clip area needs to
         be rebuilt */
      ctx->clip.stack_dirty = TRUE;
    }
  else
    {
      guint path_start = 0;

      while (path_start < ctx->path_nodes->len)
        {
          CoglPathNode *path = &g_array_index (ctx->path_nodes, CoglPathNode,
                                               path_start);

          _cogl_path_fill_nodes_scanlines (path,
                                           path->path_size,
                                           bounds_x, bounds_y,
                                           bounds_w, bounds_h);

          path_start += path->path_size;
        }
    }
}
