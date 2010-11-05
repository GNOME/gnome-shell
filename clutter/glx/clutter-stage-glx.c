/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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

#include "clutter-backend-glx.h"
#include "clutter-stage-glx.h"
#include "clutter-glx.h"
#include "clutter-profile.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-event.h"
#include "clutter-enum-types.h"
#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "cogl/cogl.h"

#include <GL/glx.h>
#include <GL/gl.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#ifdef HAVE_DRM
#include <drm.h>
#endif

static void clutter_stage_window_iface_init     (ClutterStageWindowIface     *iface);

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

#define clutter_stage_glx_get_type      _clutter_stage_glx_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterStageGLX,
                         clutter_stage_glx,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_glx_unrealize (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* Note unrealize should free up any backend stage related resources */
  CLUTTER_NOTE (BACKEND, "Unrealizing GLX stage [%p]", stage_glx);

  cogl_object_unref (stage_glx->onscreen);
  stage_glx->onscreen = NULL;
}

static void
handle_swap_complete_cb (CoglFramebuffer *framebuffer,
                         void *user_data)
{
  ClutterStageGLX *stage_glx = user_data;

  /* Early versions of the swap_event implementation in Mesa
   * deliver BufferSwapComplete event when not selected for,
   * so if we get a swap event we aren't expecting, just ignore it.
   *
   * https://bugs.freedesktop.org/show_bug.cgi?id=27962
   *
   * FIXME: This issue can be hidden inside Cogl so we shouldn't
   * need to care about this bug here.
   */
  if (stage_glx->pending_swaps > 0)
    stage_glx->pending_swaps--;
}

static gboolean
clutter_stage_glx_realize (ClutterStageWindow *stage_window)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);
  ClutterBackend *backend;
  ClutterBackendGLX *backend_glx;
  CoglFramebuffer *framebuffer;
  GError *error = NULL;
  gfloat width;
  gfloat height;
  const char *clutter_vblank;

  CLUTTER_NOTE (ACTOR, "Realizing stage '%s' [%p]",
                G_OBJECT_TYPE_NAME (stage_window),
                stage_window);

  backend = CLUTTER_BACKEND (stage_x11->backend);
  backend_glx = CLUTTER_BACKEND_GLX (stage_x11->backend);

  clutter_actor_get_size (CLUTTER_ACTOR (stage_x11->wrapper), &width, &height);

  stage_glx->onscreen = cogl_onscreen_new (backend->cogl_context,
                                           width, height);
  if (stage_x11->xwin != None)
    cogl_onscreen_x11_set_foreign_window_xid (stage_glx->onscreen,
                                              stage_x11->xwin);

  clutter_vblank = _clutter_backend_glx_get_vblank ();
  if (clutter_vblank && strcmp (clutter_vblank, "none") == 0)
    cogl_onscreen_set_swap_throttled (stage_glx->onscreen, FALSE);

  framebuffer = COGL_FRAMEBUFFER (stage_glx->onscreen);
  if (!cogl_framebuffer_allocate (framebuffer, &error))
    {
      g_warning ("Failed to allocate stage: %s", error->message);
      g_error_free (error);
      cogl_object_unref (stage_glx->onscreen);
      stage_glx->onscreen = NULL;
      return FALSE;
    }

  if (stage_x11->xwin == None)
    stage_x11->xwin = cogl_onscreen_x11_get_window_xid (stage_glx->onscreen);

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT))
    {
      stage_glx->swap_callback_id =
        cogl_framebuffer_add_swap_buffers_callback (framebuffer,
                                                    handle_swap_complete_cb,
                                                    stage_glx);
    }

  /* chain up to the StageX11 implementation */
  return clutter_stage_window_parent_iface->realize (stage_window);
}

static int
clutter_stage_glx_get_pending_swaps (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  return stage_glx->pending_swaps;
}

static void
clutter_stage_glx_class_init (ClutterStageGLXClass *klass)
{
}

static void
clutter_stage_glx_init (ClutterStageGLX *stage)
{
}

static gboolean
clutter_stage_glx_has_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* NB: at the start of each new frame there is an implied clip that
   * clips everything (i.e. nothing would be drawn) so we need to make
   * sure we return True in the un-initialized case here.
   *
   * NB: a clip width of 0 means a full stage redraw has been queued
   * so we effectively don't have any redraw clips in that case.
   */
  if (!stage_glx->initialized_redraw_clip ||
      (stage_glx->initialized_redraw_clip &&
       stage_glx->bounding_redraw_clip.width != 0))
    return TRUE;
  else
    return FALSE;
}

static gboolean
clutter_stage_glx_ignoring_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* NB: a clip width of 0 means a full stage redraw is required */
  if (stage_glx->initialized_redraw_clip &&
      stage_glx->bounding_redraw_clip.width == 0)
    return TRUE;
  else
    return FALSE;
}

/* A redraw clip represents (in stage coordinates) the bounding box of
 * something that needs to be redraw. Typically they are added to the
 * StageWindow as a result of clutter_actor_queue_clipped_redraw() by
 * actors such as ClutterGLXTexturePixmap. All redraw clips are
 * discarded after the next paint.
 *
 * A NULL stage_clip means the whole stage needs to be redrawn.
 *
 * What we do with this information:
 * - we keep track of the bounding box for all redraw clips
 * - when we come to redraw; if the bounding box is smaller than the
 *   stage we scissor the redraw to that box and use
 *   GLX_MESA_copy_sub_buffer to present the redraw to the front
 *   buffer.
 *
 * XXX - In theory, we should have some sort of heuristics to promote
 * a clipped redraw to a full screen redraw; in reality, it turns out
 * that promotion is fairly expensive. See the Clutter bug described
 * at: http://bugzilla.clutter-project.org/show_bug.cgi?id=2136 .
 *
 * TODO - we should use different heuristics depending on whether the
 * framebuffer is on screen and not redirected by a compositor VS
 * offscreen (either due to compositor redirection or because we are
 * rendering to a CoglOffscreen framebuffer)
 *
 * When not redirected glXCopySubBuffer (on intel hardware at least)
 * will block the GPU until the vertical trace is at the optimal point
 * so the copy can be done without tearing. In this case we don't want
 * to copy tall regions because they increase the average time spent
 * blocking the GPU.
 *
 * When rendering offscreen (CoglOffscreen or redirected by
 * compositor) then no extra synchronization is needed before the copy
 * can start.
 *
 * In all cases we need to consider that glXCopySubBuffer implies a
 * blit which may be avoided by promoting to a full stage redraw if:
 * - the framebuffer is redirected offscreen or a CoglOffscreen.
 * - the framebuffer is onscreen and fullscreen.
 * By promoting to a full stage redraw we trade off the cost involved
 * in rasterizing the extra pixels vs avoiding to use a blit to
 * present the back buffer.
 */
static void
clutter_stage_glx_add_redraw_clip (ClutterStageWindow *stage_window,
                                   ClutterGeometry    *stage_clip)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* If we are already forced to do a full stage redraw then bail early */
  if (clutter_stage_glx_ignoring_redraw_clips (stage_window))
    return;

  /* A NULL stage clip means a full stage redraw has been queued and
   * we keep track of this by setting a zero width
   * stage_glx->bounding_redraw_clip */
  if (stage_clip == NULL)
    {
      stage_glx->bounding_redraw_clip.width = 0;
      stage_glx->initialized_redraw_clip = TRUE;
      return;
    }

  /* Ignore requests to add degenerate/empty clip rectangles */
  if (stage_clip->width == 0 || stage_clip->height == 0)
    return;

  if (!stage_glx->initialized_redraw_clip)
    {
      stage_glx->bounding_redraw_clip.x = stage_clip->x;
      stage_glx->bounding_redraw_clip.y = stage_clip->y;
      stage_glx->bounding_redraw_clip.width = stage_clip->width;
      stage_glx->bounding_redraw_clip.height = stage_clip->height;
    }
  else if (stage_glx->bounding_redraw_clip.width > 0)
    {
      clutter_geometry_union (&stage_glx->bounding_redraw_clip,
                              stage_clip,
			      &stage_glx->bounding_redraw_clip);
    }

#if 0
  redraw_area = (stage_glx->bounding_redraw_clip.width *
                 stage_glx->bounding_redraw_clip.height);
  stage_area = stage_x11->xwin_width * stage_x11->xwin_height;

  /* Redrawing and blitting >70% of the stage is assumed to be more
   * expensive than redrawing the additional 30% to avoid the blit.
   *
   * FIXME: This threshold was plucked out of thin air!
   *
   * The threshold has been disabled after verifying that it indeed
   * made redraws more expensive than intended; see bug reference:
   *
   * http://bugzilla.clutter-project.org/show_bug.cgi?id=2136
   */
  if (redraw_area > (stage_area * 0.7f))
    {
      g_print ("DEBUG: clipped redraw too big, forcing full redraw\n");
      /* Set a zero width clip to force a full redraw */
      stage_glx->bounding_redraw_clip.width = 0;
    }
#endif

  stage_glx->initialized_redraw_clip = TRUE;
}

static void
clutter_stage_glx_redraw (ClutterStageWindow *stage_window)
{
  ClutterBackendGLX *backend_glx;
  ClutterStageX11 *stage_x11;
  ClutterStageGLX *stage_glx;
  gboolean may_use_clipped_redraw;
  gboolean use_clipped_redraw;

  CLUTTER_STATIC_TIMER (painting_timer,
                        "Redrawing", /* parent */
                        "Painting actors",
                        "The time spent painting actors",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (swapbuffers_timer,
                        "Redrawing", /* parent */
                        "glXSwapBuffers",
                        "The time spent blocked by glXSwapBuffers",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (blit_sub_buffer_timer,
                        "Redrawing", /* parent */
                        "glx_blit_sub_buffer",
                        "The time spent in _glx_blit_sub_buffer",
                        0 /* no application private data */);

  stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  if (stage_x11->xwin == None)
    return;

  stage_glx = CLUTTER_STAGE_GLX (stage_window);

  backend_glx = CLUTTER_BACKEND_GLX (stage_x11->backend);

  CLUTTER_TIMER_START (_clutter_uprof_context, painting_timer);

  if (G_LIKELY (backend_glx->can_blit_sub_buffer) &&
      /* NB: a zero width redraw clip == full stage redraw */
      stage_glx->bounding_redraw_clip.width != 0 &&
      /* some drivers struggle to get going and produce some junk
       * frames when starting up... */
      G_LIKELY (stage_glx->frame_count > 3) &&
      /* While resizing a window clipped redraws are disabled to avoid
       * artefacts. See clutter-event-x11.c:event_translate for a
       * detailed explanation */
      G_LIKELY (stage_x11->clipped_redraws_cool_off == 0))
    {
      may_use_clipped_redraw = TRUE;
    }
  else
    may_use_clipped_redraw = FALSE;

  if (may_use_clipped_redraw &&
      G_LIKELY (!(clutter_paint_debug_flags &
                  CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS)))
    use_clipped_redraw = TRUE;
  else
    use_clipped_redraw = FALSE;

  if (use_clipped_redraw)
    {
      CLUTTER_NOTE (CLIPPING,
                    "Stage clip pushed: x=%d, y=%d, width=%d, height=%d\n",
                    stage_glx->bounding_redraw_clip.x,
                    stage_glx->bounding_redraw_clip.y,
                    stage_glx->bounding_redraw_clip.width,
                    stage_glx->bounding_redraw_clip.height);
      cogl_clip_push_window_rectangle (stage_glx->bounding_redraw_clip.x,
                                       stage_glx->bounding_redraw_clip.y,
                                       stage_glx->bounding_redraw_clip.width,
                                       stage_glx->bounding_redraw_clip.height);
      _clutter_stage_do_paint (stage_x11->wrapper,
                               &stage_glx->bounding_redraw_clip);
      cogl_clip_pop ();
    }
  else
    {
      CLUTTER_NOTE (CLIPPING, "Unclipped stage paint\n");

      /* If we are trying to debug redraw issues then we want to pass
       * the bounding_redraw_clip so it can be visualized */
      if (G_UNLIKELY (clutter_paint_debug_flags &
                      CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS) &&
          may_use_clipped_redraw)
        {
          _clutter_stage_do_paint (stage_x11->wrapper,
                                   &stage_glx->bounding_redraw_clip);
        }
      else
        _clutter_stage_do_paint (stage_x11->wrapper, NULL);
    }

  if (may_use_clipped_redraw &&
      G_UNLIKELY ((clutter_paint_debug_flags & CLUTTER_DEBUG_REDRAWS)))
    {
      static CoglMaterial *outline = NULL;
      ClutterGeometry *clip = &stage_glx->bounding_redraw_clip;
      ClutterActor *actor = CLUTTER_ACTOR (stage_x11->wrapper);
      CoglHandle vbo;
      float x_1 = clip->x;
      float x_2 = clip->x + clip->width;
      float y_1 = clip->y;
      float y_2 = clip->y + clip->height;
      float quad[8] = {
        x_1, y_1,
        x_2, y_1,
        x_2, y_2,
        x_1, y_2
      };
      CoglMatrix modelview;

      if (outline == NULL)
        {
          outline = cogl_material_new ();
          cogl_material_set_color4ub (outline, 0xff, 0x00, 0x00, 0xff);
        }

      vbo = cogl_vertex_buffer_new (4);
      cogl_vertex_buffer_add (vbo,
                              "gl_Vertex",
                              2, /* n_components */
                              COGL_ATTRIBUTE_TYPE_FLOAT,
                              FALSE, /* normalized */
                              0, /* stride */
                              quad);
      cogl_vertex_buffer_submit (vbo);

      cogl_push_matrix ();
      cogl_matrix_init_identity (&modelview);
      _clutter_actor_apply_modelview_transform (actor, &modelview);
      cogl_set_modelview_matrix (&modelview);
      cogl_set_source (outline);
      cogl_vertex_buffer_draw (vbo, COGL_VERTICES_MODE_LINE_LOOP,
                               0 , 4);
      cogl_pop_matrix ();
      cogl_object_unref (vbo);
    }

  CLUTTER_TIMER_STOP (_clutter_uprof_context, painting_timer);

  /* push on the screen */
  if (use_clipped_redraw)
    {
      ClutterGeometry *clip = &stage_glx->bounding_redraw_clip;
      int copy_area[4];
      ClutterActor *actor;

      /* XXX: It seems there will be a race here in that the stage
       * window may be resized before the cogl_framebuffer_swap_region
       * is handled and so we may copy the wrong region. I can't
       * really see how we can handle this with the current state of X
       * but at least in this case a full redraw should be queued by
       * the resize anyway so it should only exhibit temporary
       * artefacts.
       */

      actor = CLUTTER_ACTOR (stage_x11->wrapper);

      copy_area[0] = clip->x;
      copy_area[1] = clutter_actor_get_height (actor) - clip->y - clip->height;
      copy_area[2] = clip->width;
      copy_area[3] = clip->height;

      CLUTTER_NOTE (BACKEND,
                    "cogl_framebuffer_swap_region (onscreen: %p, "
                                                  "x: %d, y: %d, "
                                                  "width: %d, height: %d)",
                    stage_glx->onscreen,
                    copy_area[0], copy_area[1], copy_area[2], copy_area[3]);

      CLUTTER_TIMER_START (_clutter_uprof_context, blit_sub_buffer_timer);

      cogl_framebuffer_swap_region (COGL_FRAMEBUFFER (stage_glx->onscreen),
                                    copy_area, 1);

      CLUTTER_TIMER_STOP (_clutter_uprof_context, blit_sub_buffer_timer);
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "cogl_framebuffer_swap_buffers (onscreen: %p)",
                    stage_glx->onscreen);

      /* If we have swap buffer events then
       * cogl_framebuffer_swap_buffers will return immediately and we
       * need to track that there is a swap in progress... */
      if (clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS))
        stage_glx->pending_swaps++;

      CLUTTER_TIMER_START (_clutter_uprof_context, swapbuffers_timer);
      cogl_framebuffer_swap_buffers (COGL_FRAMEBUFFER (stage_glx->onscreen));
      CLUTTER_TIMER_STOP (_clutter_uprof_context, swapbuffers_timer);
    }

  /* reset the redraw clipping for the next paint... */
  stage_glx->initialized_redraw_clip = FALSE;

  stage_glx->frame_count++;
}

static CoglFramebuffer *
clutter_stage_glx_get_active_framebuffer (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  return COGL_FRAMEBUFFER (stage_glx->onscreen);
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = clutter_stage_glx_realize;
  iface->unrealize = clutter_stage_glx_unrealize;
  iface->get_pending_swaps = clutter_stage_glx_get_pending_swaps;

  iface->add_redraw_clip = clutter_stage_glx_add_redraw_clip;
  iface->has_redraw_clips = clutter_stage_glx_has_redraw_clips;
  iface->ignoring_redraw_clips = clutter_stage_glx_ignoring_redraw_clips;
  iface->redraw = clutter_stage_glx_redraw;
  iface->get_active_framebuffer = clutter_stage_glx_get_active_framebuffer;

  /* the rest is inherited from ClutterStageX11 */
}
