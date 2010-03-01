/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-journal-private.h"
#include "cogl-material-private.h"
#include "cogl-framebuffer-private.h"

#include <string.h>
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

#ifdef HAVE_COGL_GL
#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture
#endif

static void
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

static void
_cogl_path_stroke_nodes (void)
{
  unsigned int   path_start = 0;
  unsigned long  enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  CoglMaterialFlushOptions options;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (_cogl_get_framebuffer (), 0);

  enable_flags |= _cogl_material_get_cogl_enable_flags (ctx->source_material);
  cogl_enable (enable_flags);

  options.flags = COGL_MATERIAL_FLUSH_DISABLE_MASK;
  /* disable all texture layers */
  options.disable_layers = (guint32)~0;

  _cogl_material_flush_gl_state (ctx->source_material, &options);

  while (path_start < ctx->path_nodes->len)
    {
      CoglPathNode *path = &g_array_index (ctx->path_nodes, CoglPathNode,
                                           path_start);

      GE( glVertexPointer (2, GL_FLOAT, sizeof (CoglPathNode),
                           (guint8 *) path
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

void
_cogl_add_path_to_stencil_buffer (floatVec2 nodes_min,
                                  floatVec2 nodes_max,
                                  unsigned int  path_size,
                                  CoglPathNode *path,
                                  gboolean      merge,
                                  gboolean      need_clear)
{
  unsigned int     path_start = 0;
  unsigned int     sub_path_num = 0;
  float            bounds_x;
  float            bounds_y;
  float            bounds_w;
  float            bounds_h;
  unsigned long    enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  CoglHandle       prev_source;
  int              i;
  CoglHandle       framebuffer = _cogl_get_framebuffer ();
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);


  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't track changes to the stencil buffer in the journal
   * so we need to flush any batched geometry first */
  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (framebuffer, 0);

  /* Just setup a simple material that doesn't use texturing... */
  prev_source = cogl_handle_ref (ctx->source_material);
  cogl_set_source (ctx->stencil_material);

  _cogl_material_flush_gl_state (ctx->source_material, NULL);

  enable_flags |=
    _cogl_material_get_cogl_enable_flags (ctx->source_material);
  cogl_enable (enable_flags);

  _cogl_path_get_bounds (nodes_min, nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  GE( glEnable (GL_STENCIL_TEST) );

  GE( glColorMask (FALSE, FALSE, FALSE, FALSE) );
  GE( glDepthMask (FALSE) );

  if (merge)
    {
      GE (glStencilMask (2));
      GE (glStencilFunc (GL_LEQUAL, 0x2, 0x6));
    }
  else
    {
      /* If we're not using the stencil buffer for clipping then we
         don't need to clear the whole stencil buffer, just the area
         that will be drawn */
      if (need_clear)
        cogl_clear (NULL, COGL_BUFFER_BIT_STENCIL);
      else
        {
          /* Just clear the bounding box */
          GE( glStencilMask (~(GLuint) 0) );
          GE( glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
          cogl_rectangle (bounds_x, bounds_y,
                          bounds_x + bounds_w, bounds_y + bounds_h);
          /* Make sure the rectangle hits the stencil buffer before
           * directly changing other GL state. */
          _cogl_journal_flush ();
          /* NB: The journal flushing may trash the modelview state and
           * enable flags */
          _cogl_matrix_stack_flush_to_gl (modelview_stack,
                                          COGL_MATRIX_MODELVIEW);
          cogl_enable (enable_flags);
        }
      GE (glStencilMask (1));
      GE (glStencilFunc (GL_LEQUAL, 0x1, 0x3));
    }

  GE (glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT));

  for (i = 0; i < ctx->n_texcoord_arrays_enabled; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }
  ctx->n_texcoord_arrays_enabled = 0;

  while (path_start < path_size)
    {
      GE (glVertexPointer (2, GL_FLOAT, sizeof (CoglPathNode),
                           (guint8 *) path
                           + G_STRUCT_OFFSET (CoglPathNode, x)));
      GE (glDrawArrays (GL_TRIANGLE_FAN, 0, path->path_size));

      if (sub_path_num > 0)
        {
          /* Union the two stencil buffers bits into the least
             significant bit */
          GE (glStencilMask (merge ? 6 : 3));
          GE (glStencilOp (GL_ZERO, GL_REPLACE, GL_REPLACE));
          cogl_rectangle (bounds_x, bounds_y,
                          bounds_x + bounds_w, bounds_y + bounds_h);
          /* Make sure the rectangle hits the stencil buffer before
           * directly changing other GL state. */
          _cogl_journal_flush ();
          /* NB: The journal flushing may trash the modelview state and
           * enable flags */
          _cogl_matrix_stack_flush_to_gl (modelview_stack,
                                          COGL_MATRIX_MODELVIEW);
          cogl_enable (enable_flags);

          GE (glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT));
        }

      GE (glStencilMask (merge ? 4 : 2));

      path_start += path->path_size;
      path += path->path_size;
      sub_path_num++;
    }

  if (merge)
    {
      /* Now we have the new stencil buffer in bit 1 and the old
         stencil buffer in bit 0 so we need to intersect them */
      GE (glStencilMask (3));
      GE (glStencilFunc (GL_NEVER, 0x2, 0x3));
      GE (glStencilOp (GL_DECR, GL_DECR, GL_DECR));
      /* Decrement all of the bits twice so that only pixels where the
         value is 3 will remain */

      _cogl_matrix_stack_push (projection_stack);
      _cogl_matrix_stack_load_identity (projection_stack);
      _cogl_matrix_stack_flush_to_gl (projection_stack,
                                      COGL_MATRIX_PROJECTION);

      _cogl_matrix_stack_push (modelview_stack);
      _cogl_matrix_stack_load_identity (modelview_stack);
      _cogl_matrix_stack_flush_to_gl (modelview_stack,
                                      COGL_MATRIX_MODELVIEW);

      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
      /* Make sure these rectangles hit the stencil buffer before we
       * restore the stencil op/func. */
      _cogl_journal_flush ();

      _cogl_matrix_stack_pop (modelview_stack);
      _cogl_matrix_stack_pop (projection_stack);
    }

  GE (glStencilMask (~(GLuint) 0));
  GE (glDepthMask (TRUE));
  GE (glColorMask (TRUE, TRUE, TRUE, TRUE));

  GE (glStencilFunc (GL_EQUAL, 0x1, 0x1));
  GE (glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP));

  /* restore the original material */
  cogl_set_source (prev_source);
  cogl_handle_unref (prev_source);
}

static int
compare_ints (gconstpointer a,
              gconstpointer b)
{
  return GPOINTER_TO_INT(a)-GPOINTER_TO_INT(b);
}

static void
_cogl_path_fill_nodes_scanlines (CoglPathNode *path,
                                 unsigned int  path_size,
                                 int           bounds_x,
                                 int           bounds_y,
                                 unsigned int  bounds_w,
                                 unsigned int  bounds_h)
{
  /* This is our edge list it stores intersections between our
   * curve and scanlines, it should probably be implemented with a
   * data structure that has smaller overhead for inserting the
   * curve/scanline intersections.
   */
  GSList **scanlines = g_alloca (bounds_h * sizeof (GSList *));

  int i;
  int prev_x;
  int prev_y;
  int first_x;
  int first_y;
  int lastdir = -2; /* last direction we vere moving */
  int lastline = -1; /* the previous scanline we added to */

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We are going to use GL to draw directly so make sure any
   * previously batched geometry gets to GL before we start...
   */
  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (_cogl_get_framebuffer (), 0);

  _cogl_material_flush_gl_state (ctx->source_material, NULL);

  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
               | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));

  /* clear scanline intersection lists */
  for (i = 0; i < bounds_h; i++)
    scanlines[i]=NULL;

  first_x = prev_x = path->x;
  first_y = prev_y = path->y;

  /* create scanline intersection list */
  for (i=1; i < path_size; i++)
    {
      int dest_x = path[i].x;
      int dest_y = path[i].y;
      int ydir;
      int dx;
      int dy;
      int y;

    fill_close:
      dx = dest_x - prev_x;
      dy = dest_y - prev_y;

      if (dy < 0)
        ydir = -1;
      else if (dy > 0)
        ydir = 1;
      else
        ydir = 0;

      /* do linear interpolation between vertices */
      for (y = prev_y; y != dest_y; y += ydir)
        {

          /* only add a point if the scanline has changed and we're
           * within bounds.
           */
          if (y - bounds_y >= 0 &&
              y - bounds_y < bounds_h &&
              lastline != y)
            {
              int x = prev_x + (dx * (y-prev_y)) / dy;

              scanlines[ y - bounds_y ]=
                g_slist_insert_sorted (scanlines[ y - bounds_y],
                                       GINT_TO_POINTER(x),
                                       compare_ints);

              if (ydir != lastdir &&  /* add a double entry when changing */
                  lastdir != -2)        /* vertical direction */
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
    int spans = 0;
    int span_no;
    GLfloat *coords;

    /* count number of spans */
    for (i = 0; i < bounds_h; i++)
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
    for (i = 0; i < bounds_h; i++)
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
    for (i = 0; i < bounds_h; i++)
      g_slist_free (scanlines[i]);

    /* render triangles */
    GE (glVertexPointer (2, GL_FLOAT, 0, coords ));
    GE (glDrawArrays (GL_TRIANGLES, 0, spans * 2 * 3));
    g_free (coords);
  }
}

static void
_cogl_path_fill_nodes (void)
{
  float bounds_x;
  float bounds_y;
  float bounds_w;
  float bounds_h;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_get_bounds (ctx->path_nodes_min, ctx->path_nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  if (G_LIKELY (!(cogl_debug_flags & COGL_DEBUG_FORCE_SCANLINE_PATHS)) &&
      cogl_features_available (COGL_FEATURE_STENCIL_BUFFER))
    {
      CoglHandle framebuffer;
      CoglClipStackState *clip_state;

      _cogl_journal_flush ();

      framebuffer = _cogl_get_framebuffer ();
      clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

      _cogl_add_path_to_stencil_buffer (ctx->path_nodes_min,
                                        ctx->path_nodes_max,
                                        ctx->path_nodes->len,
                                        &g_array_index (ctx->path_nodes,
                                                        CoglPathNode, 0),
                                        clip_state->stencil_used,
                                        FALSE);

      cogl_rectangle (bounds_x, bounds_y,
                      bounds_x + bounds_w, bounds_y + bounds_h);

      /* The stencil buffer now contains garbage so the clip area needs to
       * be rebuilt.
       *
       * NB: We only ever try and update the clip state during
       * _cogl_journal_init (when we flush the framebuffer state) which is
       * only called when the journal first gets something logged in it; so
       * we call cogl_flush() to emtpy the journal.
       */
      cogl_flush ();
      _cogl_clip_stack_state_dirty (clip_state);
    }
  else
    {
      unsigned int path_start = 0;

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

  if (ctx->path_nodes->len == 0)
    return;

  _cogl_path_stroke_nodes ();
}

void
cogl_path_move_to (float x,
                   float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* FIXME: handle multiple contours maybe? */

  _cogl_path_add_node (TRUE, x, y);

  ctx->path_start.x = x;
  ctx->path_start.y = y;

  ctx->path_pen = ctx->path_start;
}

void
cogl_path_rel_move_to (float x,
                       float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_move_to (ctx->path_pen.x + x,
                     ctx->path_pen.y + y);
}

void
cogl_path_line_to (float x,
                   float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_add_node (FALSE, x, y);

  ctx->path_pen.x = x;
  ctx->path_pen.y = y;
}

void
cogl_path_rel_line_to (float x,
                       float y)
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
cogl_path_line (float x_1,
	        float y_1,
	        float x_2,
	        float y_2)
{
  cogl_path_move_to (x_1, y_1);
  cogl_path_line_to (x_2, y_2);
}

void
cogl_path_polyline (float *coords,
	            int num_points)
{
  int c = 0;

  cogl_path_move_to (coords[0], coords[1]);

  for (c = 1; c < num_points; ++c)
    cogl_path_line_to (coords[2*c], coords[2*c+1]);
}

void
cogl_path_polygon (float *coords,
	           int    num_points)
{
  cogl_path_polyline (coords, num_points);
  cogl_path_close ();
}

void
cogl_path_rectangle (float x_1,
                     float y_1,
                     float x_2,
                     float y_2)
{
  cogl_path_move_to (x_1, y_1);
  cogl_path_line_to (x_2, y_1);
  cogl_path_line_to (x_2, y_2);
  cogl_path_line_to (x_1, y_2);
  cogl_path_close   ();
}

static void
_cogl_path_arc (float center_x,
	        float center_y,
                float radius_x,
                float radius_y,
                float angle_1,
                float angle_2,
                float angle_step,
                unsigned int move_first)
{
  float a     = 0x0;
  float cosa  = 0x0;
  float sina  = 0x0;
  float px    = 0x0;
  float py    = 0x0;

  /* Fix invalid angles */

  if (angle_1 == angle_2 || angle_step == 0x0)
    return;

  if (angle_step < 0x0)
    angle_step = -angle_step;

  /* Walk the arc by given step */

  a = angle_1;
  while (a != angle_2)
    {
      cosa = cosf (a * (G_PI/180.0));
      sina = sinf (a * (G_PI/180.0));

      px = center_x + (cosa * radius_x);
      py = center_y + (sina * radius_y);

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

  cosa = cosf (angle_2 * (G_PI/180.0));
  sina = sinf (angle_2 * (G_PI/180.0));

  px = center_x + (cosa * radius_x);
  py = center_y + (sina * radius_y);

  cogl_path_line_to (px, py);
}

void
cogl_path_arc (float center_x,
               float center_y,
               float radius_x,
               float radius_y,
               float angle_1,
               float angle_2)
{
  float angle_step = 10;
  /* it is documented that a move to is needed to create a freestanding
   * arc
   */
  _cogl_path_arc (center_x,   center_y,
	          radius_x,   radius_y,
	          angle_1,    angle_2,
	          angle_step, 0 /* no move */);
}


void
cogl_path_arc_rel (float center_x,
		   float center_y,
		   float radius_x,
		   float radius_y,
		   float angle_1,
		   float angle_2,
		   float angle_step)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_arc (ctx->path_pen.x + center_x,
	          ctx->path_pen.y + center_y,
	          radius_x,   radius_y,
	          angle_1,    angle_2,
	          angle_step, 0 /* no move */);
}

void
cogl_path_ellipse (float center_x,
                   float center_y,
                   float radius_x,
                   float radius_y)
{
  float angle_step = 10;

  /* FIXME: if shows to be slow might be optimized
   * by mirroring just a quarter of it */

  _cogl_path_arc (center_x, center_y,
	          radius_x, radius_y,
	          0, 360,
	          angle_step, 1 /* move first */);

  cogl_path_close();
}

void
cogl_path_round_rectangle (float x_1,
                           float y_1,
                           float x_2,
                           float y_2,
                           float radius,
                           float arc_step)
{
  float inner_width = x_2 - x_1 - radius * 2;
  float inner_height = y_2 - y_1 - radius * 2;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_move_to (x_1, y_1 + radius);
  cogl_path_arc_rel (radius, 0,
		     radius, radius,
		     180,
		     270,
		     arc_step);

  cogl_path_line_to       (ctx->path_pen.x + inner_width,
                           ctx->path_pen.y);
  cogl_path_arc_rel       (0, radius,
			   radius, radius,
			   -90,
			   0,
			   arc_step);

  cogl_path_line_to       (ctx->path_pen.x,
                           ctx->path_pen.y + inner_height);

  cogl_path_arc_rel       (-radius, 0,
			   radius, radius,
			   0,
			   90,
			   arc_step);

  cogl_path_line_to       (ctx->path_pen.x - inner_width,
                           ctx->path_pen.y);
  cogl_path_arc_rel       (0, -radius,
			   radius, radius,
			   90,
			   180,
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
  floatVec2  dif1;
  floatVec2  dif2;
  floatVec2  mm;
  floatVec2  c1;
  floatVec2  c2;
  floatVec2  c3;
  floatVec2  c4;
  floatVec2  c5;
  int           cindex;

  /* Put first curve on stack */
  cubics[0] = *cubic;
  cindex    =  0;

  while (cindex >= 0)
    {
      c = &cubics[cindex];


      /* Calculate distance of control points from their
       * counterparts on the line between end points */
      dif1.x = (c->p2.x * 3) - (c->p1.x * 2) - c->p4.x;
      dif1.y = (c->p2.y * 3) - (c->p1.y * 2) - c->p4.y;
      dif2.x = (c->p3.x * 3) - (c->p4.x * 2) - c->p1.x;
      dif2.y = (c->p3.y * 3) - (c->p4.y * 2) - c->p1.y;

      if (dif1.x < 0)
        dif1.x = -dif1.x;
      if (dif1.y < 0)
        dif1.y = -dif1.y;
      if (dif2.x < 0)
        dif2.x = -dif2.x;
      if (dif2.y < 0)
        dif2.y = -dif2.y;


      /* Pick the greatest of two distances */
      if (dif1.x < dif2.x) dif1.x = dif2.x;
      if (dif1.y < dif2.y) dif1.y = dif2.y;

      /* Cancel if the curve is flat enough */
      if (dif1.x + dif1.y <= 1.0 ||
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
      c1.x = ((c->p1.x + c->p2.x) / 2);
      c1.y = ((c->p1.y + c->p2.y) / 2);
      mm.x = ((c->p2.x + c->p3.x) / 2);
      mm.y = ((c->p2.y + c->p3.y) / 2);
      c5.x = ((c->p3.x + c->p4.x) / 2);
      c5.y = ((c->p3.y + c->p4.y) / 2);

      c2.x = ((c1.x + mm.x) / 2);
      c2.y = ((c1.y + mm.y) / 2);
      c4.x = ((mm.x + c5.x) / 2);
      c4.y = ((mm.y + c5.y) / 2);

      c3.x = ((c2.x + c4.x) / 2);
      c3.y = ((c2.y + c4.y) / 2);

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
cogl_path_curve_to (float x_1,
                    float y_1,
                    float x_2,
                    float y_2,
                    float x_3,
                    float y_3)
{
  CoglBezCubic cubic;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Prepare cubic curve */
  cubic.p1 = ctx->path_pen;
  cubic.p2.x = x_1;
  cubic.p2.y = y_1;
  cubic.p3.x = x_2;
  cubic.p3.y = y_2;
  cubic.p4.x = x_3;
  cubic.p4.y = y_3;

  /* Run subdivision */
  _cogl_path_bezier3_sub (&cubic);

  /* Add last point */
  _cogl_path_add_node (FALSE, cubic.p4.x, cubic.p4.y);
  ctx->path_pen = cubic.p4;
}

void
cogl_path_rel_curve_to (float x_1,
                        float y_1,
                        float x_2,
                        float y_2,
                        float x_3,
                        float y_3)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_curve_to (ctx->path_pen.x + x_1,
                      ctx->path_pen.y + y_1,
                      ctx->path_pen.x + x_2,
                      ctx->path_pen.y + y_2,
                      ctx->path_pen.x + x_3,
                      ctx->path_pen.y + y_3);
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
  floatVec2   mid;
  floatVec2   dif;
  floatVec2   c1;
  floatVec2   c2;
  floatVec2   c3;
  int            qindex;

  /* Put first curve on stack */
  quads[0] = *quad;
  qindex   =  0;

  /* While stack is not empty */
  while (qindex >= 0)
    {

      q = &quads[qindex];

      /* Calculate distance of control point from its
       * counterpart on the line between end points */
      mid.x = ((q->p1.x + q->p3.x) / 2);
      mid.y = ((q->p1.y + q->p3.y) / 2);
      dif.x = (q->p2.x - mid.x);
      dif.y = (q->p2.y - mid.y);
      if (dif.x < 0) dif.x = -dif.x;
      if (dif.y < 0) dif.y = -dif.y;

      /* Cancel if the curve is flat enough */
      if (dif.x + dif.y <= 1.0 ||
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
      c1.x = ((q->p1.x + q->p2.x) / 2);
      c1.y = ((q->p1.y + q->p2.y) / 2);
      c3.x = ((q->p2.x + q->p3.x) / 2);
      c3.y = ((q->p2.y + q->p3.y) / 2);
      c2.x = ((c1.x + c3.x) / 2);
      c2.y = ((c1.y + c3.y) / 2);

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
cogl_path_curve2_to (float x_1,
                     float y_1,
                     float x_2,
                     float y_2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  CoglBezQuad quad;

  /* Prepare quadratic curve */
  quad.p1 = ctx->path_pen;
  quad.p2.x = x_1;
  quad.p2.y = y_1;
  quad.p3.x = x_2;
  quad.p3.y = y_2;

  /* Run subdivision */
  _cogl_path_bezier2_sub (&quad);

  /* Add last point */
  _cogl_path_add_node (FALSE, quad.p3.x, quad.p3.y);
  ctx->path_pen = quad.p3;
}

void
cogl_rel_curve2_to (float x_1,
                    float y_1,
                    float x_2,
                    float y_2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_curve2_to (ctx->path_pen.x + x_1,
                       ctx->path_pen.y + y_1,
                       ctx->path_pen.x + x_2,
                       ctx->path_pen.y + y_2);
}
#endif
