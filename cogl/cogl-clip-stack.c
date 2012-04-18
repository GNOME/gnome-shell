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

#include <string.h>
#include <math.h>

#include <glib.h>

#include "cogl-clip-stack.h"
#include "cogl-primitives.h"
#include "cogl-context-private.h"
#include "cogl-internal.h"
#include "cogl-framebuffer-private.h"
#include "cogl-journal-private.h"
#include "cogl-util.h"
#include "cogl-path-private.h"
#include "cogl-matrix-private.h"
#include "cogl-primitives-private.h"
#include "cogl-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-attribute-private.h"
#include "cogl-primitive-private.h"
#include "cogl1-context.h"
#include "cogl-offscreen.h"

#ifndef GL_CLIP_PLANE0
#define GL_CLIP_PLANE0 0x3000
#define GL_CLIP_PLANE1 0x3001
#define GL_CLIP_PLANE2 0x3002
#define GL_CLIP_PLANE3 0x3003
#define GL_CLIP_PLANE4 0x3004
#define GL_CLIP_PLANE5 0x3005
#endif

static void
project_vertex (const CoglMatrix *modelview_projection,
		float *vertex)
{
  int i;

  cogl_matrix_transform_point (modelview_projection,
                               &vertex[0], &vertex[1],
                               &vertex[2], &vertex[3]);

  /* Convert from homogenized coordinates */
  for (i = 0; i < 4; i++)
    vertex[i] /= vertex[3];
}

static void
set_clip_plane (CoglFramebuffer *framebuffer,
                GLint plane_num,
		const float *vertex_a,
		const float *vertex_b)
{
  GLfloat planef[4];
  double planed[4];
  GLfloat angle;
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglMatrix inverse_projection;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_matrix_stack_get_inverse (projection_stack, &inverse_projection);

  /* Calculate the angle between the axes and the line crossing the
     two points */
  angle = atan2f (vertex_b[1] - vertex_a[1],
                  vertex_b[0] - vertex_a[0]) * (180.0/G_PI);

  _cogl_matrix_stack_push (modelview_stack);

  /* Load the inverse of the projection matrix so we can specify the plane
   * in screen coordinates */
  _cogl_matrix_stack_set (modelview_stack, &inverse_projection);

  /* Rotate about point a */
  _cogl_matrix_stack_translate (modelview_stack,
                                vertex_a[0], vertex_a[1], vertex_a[2]);
  /* Rotate the plane by the calculated angle so that it will connect
     the two points */
  _cogl_matrix_stack_rotate (modelview_stack, angle, 0.0f, 0.0f, 1.0f);
  _cogl_matrix_stack_translate (modelview_stack,
                                -vertex_a[0], -vertex_a[1], -vertex_a[2]);

  /* Clip planes can only be used when a fixed function backend is in
     use so we know we can directly push this matrix to the builtin
     state */
  _cogl_matrix_stack_flush_to_gl_builtins (ctx,
                                           modelview_stack,
                                           COGL_MATRIX_MODELVIEW,
                                           FALSE /* don't disable flip */);

  planef[0] = 0;
  planef[1] = -1.0;
  planef[2] = 0;
  planef[3] = vertex_a[1];

  switch (ctx->driver)
    {
    default:
      g_assert_not_reached ();
      break;

    case COGL_DRIVER_GLES1:
      GE( ctx, glClipPlanef (plane_num, planef) );
      break;

    case COGL_DRIVER_GL:
      planed[0] = planef[0];
      planed[1] = planef[1];
      planed[2] = planef[2];
      planed[3] = planef[3];
      GE( ctx, glClipPlane (plane_num, planed) );
      break;
    }

  _cogl_matrix_stack_pop (modelview_stack);
}

static void
set_clip_planes (CoglFramebuffer *framebuffer,
                 float x_1,
		 float y_1,
		 float x_2,
		 float y_2)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrix modelview_matrix;
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglMatrix projection_matrix;
  CoglMatrix modelview_projection;
  float signed_area;

  float vertex_tl[4] = { x_1, y_1, 0, 1.0 };
  float vertex_tr[4] = { x_2, y_1, 0, 1.0 };
  float vertex_bl[4] = { x_1, y_2, 0, 1.0 };
  float vertex_br[4] = { x_2, y_2, 0, 1.0 };

  _cogl_matrix_stack_get (projection_stack, &projection_matrix);
  _cogl_matrix_stack_get (modelview_stack, &modelview_matrix);

  cogl_matrix_multiply (&modelview_projection,
                        &projection_matrix,
                        &modelview_matrix);

  project_vertex (&modelview_projection, vertex_tl);
  project_vertex (&modelview_projection, vertex_tr);
  project_vertex (&modelview_projection, vertex_bl);
  project_vertex (&modelview_projection, vertex_br);

  /* Calculate the signed area of the polygon formed by the four
     vertices so that we can know its orientation */
  signed_area = (vertex_tl[0] * (vertex_tr[1] - vertex_bl[1])
                 + vertex_tr[0] * (vertex_br[1] - vertex_tl[1])
                 + vertex_br[0] * (vertex_bl[1] - vertex_tr[1])
                 + vertex_bl[0] * (vertex_tl[1] - vertex_br[1]));

  /* Set the clip planes to form lines between all of the vertices
     using the same orientation as we calculated */
  if (signed_area > 0.0f)
    {
      /* counter-clockwise */
      set_clip_plane (framebuffer, GL_CLIP_PLANE0, vertex_tl, vertex_bl);
      set_clip_plane (framebuffer, GL_CLIP_PLANE1, vertex_bl, vertex_br);
      set_clip_plane (framebuffer, GL_CLIP_PLANE2, vertex_br, vertex_tr);
      set_clip_plane (framebuffer, GL_CLIP_PLANE3, vertex_tr, vertex_tl);
    }
  else
    {
      /* clockwise */
      set_clip_plane (framebuffer, GL_CLIP_PLANE0, vertex_tl, vertex_tr);
      set_clip_plane (framebuffer, GL_CLIP_PLANE1, vertex_tr, vertex_br);
      set_clip_plane (framebuffer, GL_CLIP_PLANE2, vertex_br, vertex_bl);
      set_clip_plane (framebuffer, GL_CLIP_PLANE3, vertex_bl, vertex_tl);
    }
}

static void
add_stencil_clip_rectangle (CoglFramebuffer *framebuffer,
                            float x_1,
                            float y_1,
                            float x_2,
                            float y_2,
                            CoglBool first)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglContext *ctx = cogl_framebuffer_get_context (framebuffer);

  /* This can be called from the journal code which doesn't flush
     the matrix stacks between calls so we need to ensure they're
     flushed now */
  _cogl_context_set_current_projection (ctx, projection_stack);
  _cogl_context_set_current_modelview (ctx, modelview_stack);

  if (first)
    {
      GE( ctx, glEnable (GL_STENCIL_TEST) );

      /* Initially disallow everything */
      GE( ctx, glClearStencil (0) );
      GE( ctx, glClear (GL_STENCIL_BUFFER_BIT) );

      /* Punch out a hole to allow the rectangle */
      GE( ctx, glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( ctx, glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE) );

      _cogl_rectangle_immediate (framebuffer,
                                 ctx->stencil_pipeline,
                                 x_1, y_1, x_2, y_2);
    }
  else
    {
      /* Add one to every pixel of the stencil buffer in the
	 rectangle */
      GE( ctx, glStencilFunc (GL_NEVER, 0x1, 0x3) );
      GE( ctx, glStencilOp (GL_INCR, GL_INCR, GL_INCR) );
      _cogl_rectangle_immediate (framebuffer,
                                 ctx->stencil_pipeline,
                                 x_1, y_1, x_2, y_2);

      /* Subtract one from all pixels in the stencil buffer so that
	 only pixels where both the original stencil buffer and the
	 rectangle are set will be valid */
      GE( ctx, glStencilOp (GL_DECR, GL_DECR, GL_DECR) );

      _cogl_matrix_stack_push (projection_stack);
      _cogl_matrix_stack_load_identity (projection_stack);

      _cogl_matrix_stack_push (modelview_stack);
      _cogl_matrix_stack_load_identity (modelview_stack);

      _cogl_context_set_current_projection (ctx, projection_stack);
      _cogl_context_set_current_modelview (ctx, modelview_stack);

      _cogl_rectangle_immediate (framebuffer,
                                 ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);

      _cogl_matrix_stack_pop (modelview_stack);
      _cogl_matrix_stack_pop (projection_stack);
    }

  /* Restore the stencil mode */
  GE( ctx, glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( ctx, glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

typedef void (*SilhouettePaintCallback) (CoglFramebuffer *framebuffer,
                                         CoglPipeline *pipeline,
                                         void *user_data);

static void
add_stencil_clip_silhouette (CoglFramebuffer *framebuffer,
                             SilhouettePaintCallback silhouette_callback,
                             float bounds_x1,
                             float bounds_y1,
                             float bounds_x2,
                             float bounds_y2,
                             CoglBool merge,
                             CoglBool need_clear,
                             void *user_data)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglContext *ctx = cogl_framebuffer_get_context (framebuffer);

  /* This can be called from the clip stack code which doesn't flush
     the matrix stacks between calls so we need to ensure they're
     flushed now */
  _cogl_context_set_current_projection (ctx, projection_stack);
  _cogl_context_set_current_modelview (ctx, modelview_stack);

  _cogl_pipeline_flush_gl_state (ctx->stencil_pipeline, FALSE, 0);

  GE( ctx, glEnable (GL_STENCIL_TEST) );

  GE( ctx, glColorMask (FALSE, FALSE, FALSE, FALSE) );
  GE( ctx, glDepthMask (FALSE) );

  if (merge)
    {
      GE (ctx, glStencilMask (2));
      GE (ctx, glStencilFunc (GL_LEQUAL, 0x2, 0x6));
    }
  else
    {
      /* If we're not using the stencil buffer for clipping then we
         don't need to clear the whole stencil buffer, just the area
         that will be drawn */
      if (need_clear)
        /* If this is being called from the clip stack code then it
           will have set up a scissor for the minimum bounding box of
           all of the clips. That box will likely mean that this
           _cogl_clear won't need to clear the entire
           buffer. _cogl_framebuffer_clear_without_flush4f is used instead
           of cogl_clear because it won't try to flush the journal */
        _cogl_framebuffer_clear_without_flush4f (framebuffer,
                                                 COGL_BUFFER_BIT_STENCIL,
                                                 0, 0, 0, 0);
      else
        {
          /* Just clear the bounding box */
          GE( ctx, glStencilMask (~(GLuint) 0) );
          GE( ctx, glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
          _cogl_rectangle_immediate (framebuffer,
                                     ctx->stencil_pipeline,
                                     bounds_x1, bounds_y1,
                                     bounds_x2, bounds_y2);
        }
      GE (ctx, glStencilMask (1));
      GE (ctx, glStencilFunc (GL_LEQUAL, 0x1, 0x3));
    }

  GE (ctx, glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT));

  silhouette_callback (framebuffer, ctx->stencil_pipeline, user_data);

  if (merge)
    {
      /* Now we have the new stencil buffer in bit 1 and the old
         stencil buffer in bit 0 so we need to intersect them */
      GE (ctx, glStencilMask (3));
      GE (ctx, glStencilFunc (GL_NEVER, 0x2, 0x3));
      GE (ctx, glStencilOp (GL_DECR, GL_DECR, GL_DECR));
      /* Decrement all of the bits twice so that only pixels where the
         value is 3 will remain */

      _cogl_matrix_stack_push (projection_stack);
      _cogl_matrix_stack_load_identity (projection_stack);

      _cogl_matrix_stack_push (modelview_stack);
      _cogl_matrix_stack_load_identity (modelview_stack);

      _cogl_rectangle_immediate (framebuffer, ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);
      _cogl_rectangle_immediate (framebuffer, ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);

      _cogl_matrix_stack_pop (modelview_stack);
      _cogl_matrix_stack_pop (projection_stack);
    }

  GE (ctx, glStencilMask (~(GLuint) 0));
  GE (ctx, glDepthMask (TRUE));
  GE (ctx, glColorMask (TRUE, TRUE, TRUE, TRUE));

  GE (ctx, glStencilFunc (GL_EQUAL, 0x1, 0x1));
  GE (ctx, glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP));
}

static void
paint_path_silhouette (CoglFramebuffer *framebuffer,
                       CoglPipeline *pipeline,
                       void *user_data)
{
  CoglPath *path = user_data;
  if (path->data->path_nodes->len >= 3)
    _cogl_path_fill_nodes (path,
                           framebuffer,
                           pipeline,
                           COGL_DRAW_SKIP_JOURNAL_FLUSH |
                           COGL_DRAW_SKIP_PIPELINE_VALIDATION |
                           COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH);
}

static void
add_stencil_clip_path (CoglFramebuffer *framebuffer,
                       CoglPath *path,
                       CoglBool merge,
                       CoglBool need_clear)
{
  CoglPathData *data = path->data;
  add_stencil_clip_silhouette (framebuffer,
                               paint_path_silhouette,
                               data->path_nodes_min.x,
                               data->path_nodes_min.y,
                               data->path_nodes_max.x,
                               data->path_nodes_max.y,
                               merge,
                               need_clear,
                               path);
}

static void
paint_primitive_silhouette (CoglFramebuffer *framebuffer,
                            CoglPipeline *pipeline,
                            void *user_data)
{
  _cogl_framebuffer_draw_primitive (framebuffer,
                                    pipeline,
                                    user_data,
                                    COGL_DRAW_SKIP_JOURNAL_FLUSH |
                                    COGL_DRAW_SKIP_PIPELINE_VALIDATION |
                                    COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH |
                                    COGL_DRAW_SKIP_LEGACY_STATE);
}

static void
add_stencil_clip_primitive (CoglFramebuffer *framebuffer,
                            CoglPrimitive *primitive,
                            float bounds_x1,
                            float bounds_y1,
                            float bounds_x2,
                            float bounds_y2,
                            CoglBool merge,
                            CoglBool need_clear)
{
  add_stencil_clip_silhouette (framebuffer,
                               paint_primitive_silhouette,
                               bounds_x1,
                               bounds_y1,
                               bounds_x2,
                               bounds_y2,
                               merge,
                               need_clear,
                               primitive);
}

static void
disable_stencil_buffer (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( ctx, glDisable (GL_STENCIL_TEST) );
}

static void
enable_clip_planes (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( ctx, glEnable (GL_CLIP_PLANE0) );
  GE( ctx, glEnable (GL_CLIP_PLANE1) );
  GE( ctx, glEnable (GL_CLIP_PLANE2) );
  GE( ctx, glEnable (GL_CLIP_PLANE3) );
}

static void
disable_clip_planes (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( ctx, glDisable (GL_CLIP_PLANE3) );
  GE( ctx, glDisable (GL_CLIP_PLANE2) );
  GE( ctx, glDisable (GL_CLIP_PLANE1) );
  GE( ctx, glDisable (GL_CLIP_PLANE0) );
}

static void *
_cogl_clip_stack_push_entry (CoglClipStack *clip_stack,
                             size_t size,
                             CoglClipStackType type)
{
  CoglClipStack *entry = g_slice_alloc (size);

  /* The new entry starts with a ref count of 1 because the stack
     holds a reference to it as it is the top entry */
  entry->ref_count = 1;
  entry->type = type;
  entry->parent = clip_stack;

  /* We don't need to take a reference to the parent from the entry
     because the we are stealing the ref in the new stack top */

  return entry;
}

/* Sets the window-space bounds of the entry based on the projected
   coordinates of the given rectangle */
static void
_cogl_clip_stack_entry_set_bounds (CoglClipStack *entry,
                                   float x_1,
                                   float y_1,
                                   float x_2,
                                   float y_2,
                                   const CoglMatrix *modelview)
{
  CoglMatrix projection;
  float viewport[4];
  float verts[4 * 2] = { x_1, y_1, x_2, y_1, x_2, y_2, x_1, y_2 };
  float min_x = G_MAXFLOAT, min_y = G_MAXFLOAT;
  float max_x = -G_MAXFLOAT, max_y = -G_MAXFLOAT;
  int i;

  cogl_get_projection_matrix (&projection);
  cogl_get_viewport (viewport);

  for (i = 0; i < 4; i++)
    {
      float *v = verts + i * 2;

      /* Project the coordinates to window space coordinates */
      _cogl_transform_point (modelview, &projection, viewport, v, v + 1);

      if (v[0] > max_x)
        max_x = v[0];
      if (v[0] < min_x)
        min_x = v[0];
      if (v[1] > max_y)
        max_y = v[1];
      if (v[1] < min_y)
        min_y = v[1];
    }

  entry->bounds_x0 = floorf (min_x);
  entry->bounds_x1 = ceilf (max_x);
  entry->bounds_y0 = floorf (min_y);
  entry->bounds_y1 = ceilf (max_y);
}

CoglClipStack *
_cogl_clip_stack_push_window_rectangle (CoglClipStack *stack,
                                        int x_offset,
                                        int y_offset,
                                        int width,
                                        int height)
{
  CoglClipStack *entry;

  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackWindowRect),
                                       COGL_CLIP_STACK_WINDOW_RECT);

  entry->bounds_x0 = x_offset;
  entry->bounds_x1 = x_offset + width;
  entry->bounds_y0 = y_offset;
  entry->bounds_y1 = y_offset + height;

  return entry;
}

CoglClipStack *
_cogl_clip_stack_push_rectangle (CoglClipStack *stack,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2,
                                 const CoglMatrix *modelview_matrix)
{
  CoglClipStackRect *entry;
  CoglMatrix matrix_p;
  float v[4];

  /* Make a new entry */
  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackRect),
                                       COGL_CLIP_STACK_RECT);

  entry->x0 = x_1;
  entry->y0 = y_1;
  entry->x1 = x_2;
  entry->y1 = y_2;

  entry->matrix = *modelview_matrix;

  /* If the modelview meets these constraints then a transformed rectangle
   * should still be a rectangle when it reaches screen coordinates.
   *
   * FIXME: we are are making certain assumptions about the projection
   * matrix a.t.m and should really be looking at the combined modelview
   * and projection matrix.
   * FIXME: we don't consider rotations that are a multiple of 90 degrees
   * which could be quite common.
   */
  if (modelview_matrix->xy != 0 || modelview_matrix->xz != 0 ||
      modelview_matrix->yx != 0 || modelview_matrix->yz != 0 ||
      modelview_matrix->zx != 0 || modelview_matrix->zy != 0)
    {
      entry->can_be_scissor = FALSE;
      _cogl_clip_stack_entry_set_bounds ((CoglClipStack *) entry,
                                         x_1, y_1, x_2, y_2, modelview_matrix);
    }
  else
    {
      CoglClipStack *base_entry = (CoglClipStack *) entry;

      cogl_get_projection_matrix (&matrix_p);
      cogl_get_viewport (v);

      _cogl_transform_point (modelview_matrix, &matrix_p, v, &x_1, &y_1);
      _cogl_transform_point (modelview_matrix, &matrix_p, v, &x_2, &y_2);

      /* Consider that the modelview matrix may flip the rectangle
       * along the x or y axis... */
#define SWAP(A,B) do { float tmp = B; B = A; A = tmp; } while (0)
      if (x_1 > x_2)
        SWAP (x_1, x_2);
      if (y_1 > y_2)
        SWAP (y_1, y_2);
#undef SWAP

      base_entry->bounds_x0 = COGL_UTIL_NEARBYINT (x_1);
      base_entry->bounds_y0 = COGL_UTIL_NEARBYINT (y_1);
      base_entry->bounds_x1 = COGL_UTIL_NEARBYINT (x_2);
      base_entry->bounds_y1 = COGL_UTIL_NEARBYINT (y_2);
      entry->can_be_scissor = TRUE;
    }

  return (CoglClipStack *) entry;
}

CoglClipStack *
_cogl_clip_stack_push_from_path (CoglClipStack *stack,
                                 CoglPath *path,
                                 const CoglMatrix *modelview_matrix)
{
  float x_1, y_1, x_2, y_2;

  _cogl_path_get_bounds (path, &x_1, &y_1, &x_2, &y_2);

  /* If the path is a simple rectangle then we can divert to pushing a
     rectangle clip instead which usually won't involve the stencil
     buffer */
  if (_cogl_path_is_rectangle (path))
    return _cogl_clip_stack_push_rectangle (stack,
                                            x_1, y_1,
                                            x_2, y_2,
                                            modelview_matrix);
  else
    {
      CoglClipStackPath *entry;

      entry = _cogl_clip_stack_push_entry (stack,
                                           sizeof (CoglClipStackPath),
                                           COGL_CLIP_STACK_PATH);

      entry->path = cogl_path_copy (path);

      entry->matrix = *modelview_matrix;

      _cogl_clip_stack_entry_set_bounds ((CoglClipStack *) entry,
                                         x_1, y_1, x_2, y_2, modelview_matrix);

      return (CoglClipStack *) entry;
    }
}

CoglClipStack *
_cogl_clip_stack_push_primitive (CoglClipStack *stack,
                                 CoglPrimitive *primitive,
                                 float bounds_x1,
                                 float bounds_y1,
                                 float bounds_x2,
                                 float bounds_y2,
                                 const CoglMatrix *modelview_matrix)
{
  CoglClipStackPrimitive *entry;

  entry = _cogl_clip_stack_push_entry (stack,
                                       sizeof (CoglClipStackPrimitive),
                                       COGL_CLIP_STACK_PRIMITIVE);

  entry->primitive = cogl_object_ref (primitive);

  entry->matrix = *modelview_matrix;

  entry->bounds_x1 = bounds_x1;
  entry->bounds_y1 = bounds_y1;
  entry->bounds_x2 = bounds_x2;
  entry->bounds_y2 = bounds_y2;

  /* NB: this is referring to the bounds in window coordinates as opposed
   * to the bounds above in primitive local coordinates. */
  _cogl_clip_stack_entry_set_bounds ((CoglClipStack *) entry,
                                     bounds_x1, bounds_y1, bounds_x2, bounds_y2,
                                     modelview_matrix);

  return (CoglClipStack *) entry;
}

CoglClipStack *
_cogl_clip_stack_ref (CoglClipStack *entry)
{
  /* A NULL pointer is considered a valid stack so we should accept
     that as an argument */
  if (entry)
    entry->ref_count++;

  return entry;
}

void
_cogl_clip_stack_unref (CoglClipStack *entry)
{
  /* Unref all of the entries until we hit the root of the list or the
     entry still has a remaining reference */
  while (entry && --entry->ref_count <= 0)
    {
      CoglClipStack *parent = entry->parent;

      switch (entry->type)
        {
        case COGL_CLIP_STACK_RECT:
          g_slice_free1 (sizeof (CoglClipStackRect), entry);
          break;

        case COGL_CLIP_STACK_WINDOW_RECT:
          g_slice_free1 (sizeof (CoglClipStackWindowRect), entry);
          break;

        case COGL_CLIP_STACK_PATH:
          cogl_object_unref (((CoglClipStackPath *) entry)->path);
          g_slice_free1 (sizeof (CoglClipStackPath), entry);
          break;

        case COGL_CLIP_STACK_PRIMITIVE:
          cogl_object_unref (((CoglClipStackPrimitive *) entry)->primitive);
          g_slice_free1 (sizeof (CoglClipStackPrimitive), entry);
          break;

        default:
          g_assert_not_reached ();
        }

      entry = parent;
    }
}

CoglClipStack *
_cogl_clip_stack_pop (CoglClipStack *stack)
{
  CoglClipStack *new_top;

  _COGL_RETURN_VAL_IF_FAIL (stack != NULL, NULL);

  /* To pop we are moving the top of the stack to the old top's parent
     node. The stack always needs to have a reference to the top entry
     so we must take a reference to the new top. The stack would have
     previously had a reference to the old top so we need to decrease
     the ref count on that. We need to ref the new head first in case
     this stack was the only thing referencing the old top. In that
     case the call to _cogl_clip_stack_entry_unref will unref the
     parent. */
  new_top = stack->parent;

  _cogl_clip_stack_ref (new_top);

  _cogl_clip_stack_unref (stack);

  return new_top;
}

void
_cogl_clip_stack_get_bounds (CoglClipStack *stack,
                             int *scissor_x0,
                             int *scissor_y0,
                             int *scissor_x1,
                             int *scissor_y1)
{
  CoglClipStack *entry;

  *scissor_x0 = 0;
  *scissor_y0 = 0;
  *scissor_x1 = G_MAXINT;
  *scissor_y1 = G_MAXINT;

  for (entry = stack; entry; entry = entry->parent)
    {
      /* Get the intersection of the current scissor and the bounding
         box of this clip */
      *scissor_x0 = MAX (*scissor_x0, entry->bounds_x0);
      *scissor_y0 = MAX (*scissor_y0, entry->bounds_y0);
      *scissor_x1 = MIN (*scissor_x1, entry->bounds_x1);
      *scissor_y1 = MIN (*scissor_y1, entry->bounds_y1);
    }
}

void
_cogl_clip_stack_flush (CoglClipStack *stack,
                        CoglFramebuffer *framebuffer)
{
  int has_clip_planes;
  CoglBool using_clip_planes = FALSE;
  CoglBool using_stencil_buffer = FALSE;
  int scissor_x0;
  int scissor_y0;
  int scissor_x1;
  int scissor_y1;
  CoglMatrixStack *modelview_stack;
  CoglClipStack *entry;
  int scissor_y_start;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If we have already flushed this state then we don't need to do
     anything */
  if (ctx->current_clip_stack_valid)
    {
      if (ctx->current_clip_stack == stack)
        return;

      _cogl_clip_stack_unref (ctx->current_clip_stack);
    }

  ctx->current_clip_stack_valid = TRUE;
  ctx->current_clip_stack = _cogl_clip_stack_ref (stack);

  modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);

  has_clip_planes =
    ctx->private_feature_flags & COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES;

  if (has_clip_planes)
    disable_clip_planes ();
  disable_stencil_buffer ();

  /* If the stack is empty then there's nothing else to do */
  if (stack == NULL)
    {
      COGL_NOTE (CLIPPING, "Flushed empty clip stack");

      ctx->current_clip_stack_uses_stencil = FALSE;
      GE (ctx, glDisable (GL_SCISSOR_TEST));
      return;
    }

  /* Calculate the scissor rect first so that if we eventually have to
     clear the stencil buffer then the clear will be clipped to the
     intersection of all of the bounding boxes. This saves having to
     clear the whole stencil buffer */
  _cogl_clip_stack_get_bounds (stack,
                               &scissor_x0, &scissor_y0,
                               &scissor_x1, &scissor_y1);

  /* Enable scissoring as soon as possible */
  if (scissor_x0 >= scissor_x1 || scissor_y0 >= scissor_y1)
    scissor_x0 = scissor_y0 = scissor_x1 = scissor_y1 = scissor_y_start = 0;
  else
    {
      /* We store the entry coordinates in Cogl coordinate space
       * but OpenGL requires the window origin to be the bottom
       * left so we may need to convert the incoming coordinates.
       *
       * NB: Cogl forces all offscreen rendering to be done upside
       * down so in this case no conversion is needed.
       */

      if (cogl_is_offscreen (framebuffer))
        scissor_y_start = scissor_y0;
      else
        {
          int framebuffer_height =
            cogl_framebuffer_get_height (framebuffer);

          scissor_y_start = framebuffer_height - scissor_y1;
        }
    }

  COGL_NOTE (CLIPPING, "Flushing scissor to (%i, %i, %i, %i)",
             scissor_x0, scissor_y0,
             scissor_x1, scissor_y1);

  GE (ctx, glEnable (GL_SCISSOR_TEST));
  GE (ctx, glScissor (scissor_x0, scissor_y_start,
                      scissor_x1 - scissor_x0,
                      scissor_y1 - scissor_y0));

  /* Add all of the entries. This will end up adding them in the
     reverse order that they were specified but as all of the clips
     are intersecting it should work out the same regardless of the
     order */
  for (entry = stack; entry; entry = entry->parent)
    {
      switch (entry->type)
        {
        case COGL_CLIP_STACK_PATH:
            {
              CoglClipStackPath *path_entry = (CoglClipStackPath *) entry;

              COGL_NOTE (CLIPPING, "Adding stencil clip for path");

              _cogl_matrix_stack_push (modelview_stack);
              _cogl_matrix_stack_set (modelview_stack, &path_entry->matrix);

              add_stencil_clip_path (framebuffer,
                                     path_entry->path,
                                     using_stencil_buffer,
                                     TRUE);

              _cogl_matrix_stack_pop (modelview_stack);

              using_stencil_buffer = TRUE;
              break;
            }
        case COGL_CLIP_STACK_PRIMITIVE:
            {
              CoglClipStackPrimitive *primitive_entry =
                (CoglClipStackPrimitive *) entry;

              COGL_NOTE (CLIPPING, "Adding stencil clip for primitive");

              _cogl_matrix_stack_push (modelview_stack);
              _cogl_matrix_stack_set (modelview_stack, &primitive_entry->matrix);

              add_stencil_clip_primitive (framebuffer,
                                          primitive_entry->primitive,
                                          primitive_entry->bounds_x1,
                                          primitive_entry->bounds_y1,
                                          primitive_entry->bounds_x2,
                                          primitive_entry->bounds_y2,
                                          using_stencil_buffer,
                                          TRUE);

              _cogl_matrix_stack_pop (modelview_stack);

              using_stencil_buffer = TRUE;
              break;
            }
        case COGL_CLIP_STACK_RECT:
            {
              CoglClipStackRect *rect = (CoglClipStackRect *) entry;

              /* We don't need to do anything extra if the clip for this
                 rectangle was entirely described by its scissor bounds */
              if (!rect->can_be_scissor)
                {
                  _cogl_matrix_stack_push (modelview_stack);
                  _cogl_matrix_stack_set (modelview_stack, &rect->matrix);

                  /* If we support clip planes and we haven't already used
                     them then use that instead */
                  if (has_clip_planes)
                    {
                      COGL_NOTE (CLIPPING,
                                 "Adding clip planes clip for rectangle");

                      set_clip_planes (framebuffer,
                                       rect->x0,
                                       rect->y0,
                                       rect->x1,
                                       rect->y1);
                      using_clip_planes = TRUE;
                      /* We can't use clip planes a second time */
                      has_clip_planes = FALSE;
                    }
                  else
                    {
                      COGL_NOTE (CLIPPING, "Adding stencil clip for rectangle");

                      add_stencil_clip_rectangle (framebuffer,
                                                  rect->x0,
                                                  rect->y0,
                                                  rect->x1,
                                                  rect->y1,
                                                  !using_stencil_buffer);
                      using_stencil_buffer = TRUE;
                    }

                  _cogl_matrix_stack_pop (modelview_stack);
                }
              break;
            }
        case COGL_CLIP_STACK_WINDOW_RECT:
          break;
          /* We don't need to do anything for window space rectangles because
           * their functionality is entirely implemented by the entry bounding
           * box */
        }
    }

  /* Enabling clip planes is delayed to now so that they won't affect
     setting up the stencil buffer */
  if (using_clip_planes)
    enable_clip_planes ();

  ctx->current_clip_stack_uses_stencil = using_stencil_buffer;
}
