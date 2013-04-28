/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2012 Intel Corporation.
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
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-primitives-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-clip-stack-gl-private.h"
#include "cogl-primitive-private.h"

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
                int plane_num,
		const float *vertex_a,
		const float *vertex_b)
{
  CoglContext *ctx = framebuffer->context;
  float planef[4];
  double planed[4];
  float angle;
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglMatrix inverse_projection;

  cogl_matrix_stack_get_inverse (projection_stack, &inverse_projection);

  /* Calculate the angle between the axes and the line crossing the
     two points */
  angle = atan2f (vertex_b[1] - vertex_a[1],
                  vertex_b[0] - vertex_a[0]) * (180.0/G_PI);

  cogl_matrix_stack_push (modelview_stack);

  /* Load the inverse of the projection matrix so we can specify the plane
   * in screen coordinates */
  cogl_matrix_stack_set (modelview_stack, &inverse_projection);

  /* Rotate about point a */
  cogl_matrix_stack_translate (modelview_stack,
                               vertex_a[0], vertex_a[1], vertex_a[2]);
  /* Rotate the plane by the calculated angle so that it will connect
     the two points */
  cogl_matrix_stack_rotate (modelview_stack, angle, 0.0f, 0.0f, 1.0f);
  cogl_matrix_stack_translate (modelview_stack,
                               -vertex_a[0], -vertex_a[1], -vertex_a[2]);

  /* Clip planes can only be used when a fixed function backend is in
     use so we know we can directly push this matrix to the builtin
     state */
  _cogl_matrix_entry_flush_to_gl_builtins (ctx,
                                           modelview_stack->last_entry,
                                           COGL_MATRIX_MODELVIEW,
                                           framebuffer,
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
    case COGL_DRIVER_GL3:
      planed[0] = planef[0];
      planed[1] = planef[1];
      planed[2] = planef[2];
      planed[3] = planef[3];
      GE( ctx, glClipPlane (plane_num, planed) );
      break;
    }

  cogl_matrix_stack_pop (modelview_stack);
}

static void
set_clip_planes (CoglFramebuffer *framebuffer,
                 CoglMatrixEntry *modelview_entry,
                 float x_1,
                 float y_1,
                 float x_2,
                 float y_2)
{
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

  cogl_matrix_stack_get (projection_stack, &projection_matrix);
  cogl_matrix_entry_get (modelview_entry, &modelview_matrix);

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
                            CoglMatrixEntry *modelview_entry,
                            float x_1,
                            float y_1,
                            float x_2,
                            float y_2,
                            CoglBool first)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglContext *ctx = cogl_framebuffer_get_context (framebuffer);

  /* NB: This can be called while flushing the journal so we need
   * to be very conservative with what state we change.
   */

  _cogl_context_set_current_projection_entry (ctx,
                                              projection_stack->last_entry);
  _cogl_context_set_current_modelview_entry (ctx, modelview_entry);

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

      _cogl_context_set_current_projection_entry (ctx, &ctx->identity_entry);
      _cogl_context_set_current_modelview_entry (ctx, &ctx->identity_entry);

      _cogl_rectangle_immediate (framebuffer,
                                 ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);
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
                             CoglMatrixEntry *modelview_entry,
                             float bounds_x1,
                             float bounds_y1,
                             float bounds_x2,
                             float bounds_y2,
                             CoglBool merge,
                             CoglBool need_clear,
                             void *user_data)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglContext *ctx = cogl_framebuffer_get_context (framebuffer);

  /* NB: This can be called while flushing the journal so we need
   * to be very conservative with what state we change.
   */

  _cogl_context_set_current_projection_entry (ctx,
                                              projection_stack->last_entry);
  _cogl_context_set_current_modelview_entry (ctx, modelview_entry);

  _cogl_pipeline_flush_gl_state (ctx, ctx->stencil_pipeline,
                                 framebuffer, FALSE, FALSE);

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

      _cogl_context_set_current_projection_entry (ctx, &ctx->identity_entry);
      _cogl_context_set_current_modelview_entry (ctx, &ctx->identity_entry);

      _cogl_rectangle_immediate (framebuffer, ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);
      _cogl_rectangle_immediate (framebuffer, ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);
    }

  GE (ctx, glStencilMask (~(GLuint) 0));
  GE (ctx, glDepthMask (TRUE));
  GE (ctx, glColorMask (TRUE, TRUE, TRUE, TRUE));

  GE (ctx, glStencilFunc (GL_EQUAL, 0x1, 0x1));
  GE (ctx, glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP));
}

static void
paint_primitive_silhouette (CoglFramebuffer *framebuffer,
                            CoglPipeline *pipeline,
                            void *user_data)
{
  _cogl_primitive_draw (user_data,
                        framebuffer,
                        pipeline,
                        COGL_DRAW_SKIP_JOURNAL_FLUSH |
                        COGL_DRAW_SKIP_PIPELINE_VALIDATION |
                        COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH |
                        COGL_DRAW_SKIP_LEGACY_STATE);
}

static void
add_stencil_clip_primitive (CoglFramebuffer *framebuffer,
                            CoglMatrixEntry *modelview_entry,
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
                               modelview_entry,
                               bounds_x1,
                               bounds_y1,
                               bounds_x2,
                               bounds_y2,
                               merge,
                               need_clear,
                               primitive);
}

static void
enable_clip_planes (CoglContext *ctx)
{
  GE( ctx, glEnable (GL_CLIP_PLANE0) );
  GE( ctx, glEnable (GL_CLIP_PLANE1) );
  GE( ctx, glEnable (GL_CLIP_PLANE2) );
  GE( ctx, glEnable (GL_CLIP_PLANE3) );
}

static void
disable_clip_planes (CoglContext *ctx)
{
  GE( ctx, glDisable (GL_CLIP_PLANE3) );
  GE( ctx, glDisable (GL_CLIP_PLANE2) );
  GE( ctx, glDisable (GL_CLIP_PLANE1) );
  GE( ctx, glDisable (GL_CLIP_PLANE0) );
}

void
_cogl_clip_stack_gl_flush (CoglClipStack *stack,
                           CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  int has_clip_planes;
  CoglBool using_clip_planes = FALSE;
  CoglBool using_stencil_buffer = FALSE;
  int scissor_x0;
  int scissor_y0;
  int scissor_x1;
  int scissor_y1;
  CoglClipStack *entry;
  int scissor_y_start;

  /* If we have already flushed this state then we don't need to do
     anything */
  if (ctx->current_clip_stack_valid)
    {
      if (ctx->current_clip_stack == stack &&
          (ctx->needs_viewport_scissor_workaround == FALSE ||
           (framebuffer->viewport_age ==
            framebuffer->viewport_age_for_scissor_workaround &&
            ctx->viewport_scissor_workaround_framebuffer ==
            framebuffer)))
        return;

      _cogl_clip_stack_unref (ctx->current_clip_stack);
    }

  ctx->current_clip_stack_valid = TRUE;
  ctx->current_clip_stack = _cogl_clip_stack_ref (stack);

  has_clip_planes =
    ctx->private_feature_flags & COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES;

  if (has_clip_planes)
    disable_clip_planes (ctx);
  GE( ctx, glDisable (GL_STENCIL_TEST) );

  /* If the stack is empty then there's nothing else to do
   *
   * See comment below about ctx->needs_viewport_scissor_workaround
   */
  if (stack == NULL && !ctx->needs_viewport_scissor_workaround)
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

  /* XXX: ONGOING BUG: Intel viewport scissor
   *
   * Intel gen6 drivers don't correctly handle offset viewports, since
   * primitives aren't clipped within the bounds of the viewport.  To
   * workaround this we push our own clip for the viewport that will
   * use scissoring to ensure we clip as expected.
   *
   * TODO: file a bug upstream!
   */
  if (ctx->needs_viewport_scissor_workaround)
    {
      _cogl_util_scissor_intersect (framebuffer->viewport_x,
                                    framebuffer->viewport_y,
                                    framebuffer->viewport_x +
                                      framebuffer->viewport_width,
                                    framebuffer->viewport_y +
                                      framebuffer->viewport_height,
                                    &scissor_x0, &scissor_y0,
                                    &scissor_x1, &scissor_y1);
      framebuffer->viewport_age_for_scissor_workaround =
        framebuffer->viewport_age;
      ctx->viewport_scissor_workaround_framebuffer =
        framebuffer;
    }

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
        case COGL_CLIP_STACK_PRIMITIVE:
            {
              CoglClipStackPrimitive *primitive_entry =
                (CoglClipStackPrimitive *) entry;

              COGL_NOTE (CLIPPING, "Adding stencil clip for primitive");

              add_stencil_clip_primitive (framebuffer,
                                          primitive_entry->matrix_entry,
                                          primitive_entry->primitive,
                                          primitive_entry->bounds_x1,
                                          primitive_entry->bounds_y1,
                                          primitive_entry->bounds_x2,
                                          primitive_entry->bounds_y2,
                                          using_stencil_buffer,
                                          TRUE);

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
                  /* If we support clip planes and we haven't already used
                     them then use that instead */
                  if (has_clip_planes)
                    {
                      COGL_NOTE (CLIPPING,
                                 "Adding clip planes clip for rectangle");

                      set_clip_planes (framebuffer,
                                       rect->matrix_entry,
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
                                                  rect->matrix_entry,
                                                  rect->x0,
                                                  rect->y0,
                                                  rect->x1,
                                                  rect->y1,
                                                  !using_stencil_buffer);
                      using_stencil_buffer = TRUE;
                    }
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
    enable_clip_planes (ctx);

  ctx->current_clip_stack_uses_stencil = using_stencil_buffer;
}
