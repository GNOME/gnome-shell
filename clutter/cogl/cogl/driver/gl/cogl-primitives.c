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
#include "cogl-material-private.h"
#include "cogl-clip-stack.h"
#include "cogl-draw-buffer-private.h"

#include <string.h>
#include <gmodule.h>
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture

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
_cogl_path_stroke_nodes (void)
{
  guint   path_start = 0;
  gulong  enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  CoglMaterialFlushOptions options;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_journal_flush ();

  /* NB: _cogl_draw_buffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_draw_buffer_flush_state (_cogl_get_draw_buffer (), 0);

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

void
_cogl_add_path_to_stencil_buffer (floatVec2 nodes_min,
                                  floatVec2 nodes_max,
                                  guint         path_size,
                                  CoglPathNode *path,
                                  gboolean      merge)
{
  guint            path_start = 0;
  guint            sub_path_num = 0;
  float            bounds_x;
  float            bounds_y;
  float            bounds_w;
  float            bounds_h;
  gulong           enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  CoglHandle       prev_source;
  int              i;
  CoglHandle       draw_buffer = _cogl_get_draw_buffer ();
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (draw_buffer);
  CoglMatrixStack *projection_stack =
    _cogl_draw_buffer_get_projection_stack (draw_buffer);


  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't track changes to the stencil buffer in the journal
   * so we need to flush any batched geometry first */
  _cogl_journal_flush ();

  /* NB: _cogl_draw_buffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_draw_buffer_flush_state (draw_buffer, 0);

  /* Just setup a simple material that doesn't use texturing... */
  prev_source = cogl_handle_ref (ctx->source_material);
  cogl_set_source (ctx->stencil_material);

  _cogl_material_flush_gl_state (ctx->source_material, NULL);

  enable_flags |=
    _cogl_material_get_cogl_enable_flags (ctx->source_material);
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
      cogl_clear (NULL, COGL_BUFFER_BIT_STENCIL);
      GE( glStencilMask (1) );
      GE( glStencilFunc (GL_LEQUAL, 0x1, 0x3) );
    }

  GE( glEnable (GL_STENCIL_TEST) );
  GE( glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT) );

  GE( glColorMask (FALSE, FALSE, FALSE, FALSE) );
  GE( glDepthMask (FALSE) );

  for (i = 0; i < ctx->n_texcoord_arrays_enabled; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }
  ctx->n_texcoord_arrays_enabled = 0;

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
          /* Make sure the rectangle hits the stencil buffer before
           * directly changing other GL state. */
          _cogl_journal_flush ();
          /* NB: The journal flushing may trash the modelview state and
           * enable flags */
          _cogl_matrix_stack_flush_to_gl (modelview_stack,
                                          COGL_MATRIX_MODELVIEW);
          cogl_enable (enable_flags);

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

  GE( glStencilMask (~(GLuint) 0) );
  GE( glDepthMask (TRUE) );
  GE( glColorMask (TRUE, TRUE, TRUE, TRUE) );

  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );

  /* restore the original material */
  cogl_set_source (prev_source);
  cogl_handle_unref (prev_source);
}

void
_cogl_path_fill_nodes (void)
{
  CoglHandle draw_buffer;
  CoglClipStackState *clip_state;
  float bounds_x;
  float bounds_y;
  float bounds_w;
  float bounds_h;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_journal_flush ();

  draw_buffer = _cogl_get_draw_buffer ();
  clip_state = _cogl_draw_buffer_get_clip_state (draw_buffer);

  _cogl_path_get_bounds (ctx->path_nodes_min, ctx->path_nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  _cogl_add_path_to_stencil_buffer (ctx->path_nodes_min,
                                    ctx->path_nodes_max,
                                    ctx->path_nodes->len,
                                    &g_array_index (ctx->path_nodes,
                                                    CoglPathNode, 0),
                                    clip_state->stencil_used);

  cogl_rectangle (bounds_x, bounds_y,
                  bounds_x + bounds_w, bounds_y + bounds_h);

  /* The stencil buffer now contains garbage so the clip area needs to
     be rebuilt */
  _cogl_clip_stack_state_dirty (clip_state);
}

