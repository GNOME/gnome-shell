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
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

void
_cogl_path_add_node (gboolean new_sub_path,
		     float x,
		     float y)
{
  CoglPathNode new_node;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  new_node.x =  (x);
  new_node.y =  (y);
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
  guint path_start = 0;
  guint sub_path_num = 0;
  float bounds_x;
  float bounds_y;
  float bounds_w;
  float bounds_h;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

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

  while (path_start < path_size)
    {
      gulong      enable_flags = COGL_ENABLE_VERTEX_ARRAY;

      /* Just setup a simple material that doesn't use texturing... */
      cogl_material_flush_gl_state (ctx->stencil_material, NULL);

      enable_flags |=
        cogl_material_get_cogl_enable_flags (ctx->source_material);
      cogl_enable (enable_flags);

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
          cogl_rectangle (bounds_x, bounds_y, bounds_w, bounds_h);

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
      GE( glPushMatrix () );
      GE( glLoadIdentity () );
      GE( glMatrixMode (GL_PROJECTION) );
      GE( glPushMatrix () );
      GE( glLoadIdentity () );
      GE( glRecti (-1, 1, 1, -1) );
      GE( glRecti (-1, 1, 1, -1) );
      GE( glPopMatrix () );
      GE( glMatrixMode (GL_MODELVIEW) );
      GE( glPopMatrix () );
    }

  GE( glStencilMask (~(GLuint) 0) );
  GE( glDepthMask (TRUE) );
  GE( glColorMask (TRUE, TRUE, TRUE, TRUE) );

  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

void
_cogl_path_fill_nodes ()
{
  float bounds_x;
  float bounds_y;
  float bounds_w;
  float bounds_h;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_add_path_to_stencil_buffer (ctx->path_nodes_min,
                                    ctx->path_nodes_max,
                                    ctx->path_nodes->len,
                                    &g_array_index (ctx->path_nodes,
                                                    CoglPathNode, 0),
                                    ctx->clip.stencil_used);

  _cogl_path_get_bounds (ctx->path_nodes_min, ctx->path_nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  cogl_rectangle (bounds_x, bounds_y, bounds_w, bounds_h);

  /* The stencil buffer now contains garbage so the clip area needs to
     be rebuilt */
  ctx->clip.stack_dirty = TRUE;
}
