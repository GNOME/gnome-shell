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
static void clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface);

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;
static ClutterEventTranslatorIface *clutter_event_translator_parent_iface = NULL;

#define clutter_stage_glx_get_type      _clutter_stage_glx_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterStageGLX,
                         clutter_stage_glx,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_TRANSLATOR,
                                                clutter_event_translator_iface_init));

static void
clutter_stage_glx_unrealize (ClutterStageWindow *stage_window)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  /* Note unrealize should free up any backend stage related resources */
  CLUTTER_NOTE (BACKEND, "Unrealizing GLX stage [%p]", stage_glx);

  clutter_x11_trap_x_errors ();

  if (stage_glx->glxwin != None)
    {
      glXDestroyWindow (backend_x11->xdpy, stage_glx->glxwin);
      stage_glx->glxwin = None;
    }

  _clutter_stage_x11_destroy_window_untrapped (stage_x11);

  XSync (backend_x11->xdpy, False);

  clutter_x11_untrap_x_errors ();
}

static gboolean
clutter_stage_glx_realize (ClutterStageWindow *stage_window)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);
  ClutterBackendX11 *backend_x11;
  ClutterBackendGLX *backend_glx;

  CLUTTER_NOTE (ACTOR, "Realizing stage '%s' [%p]",
                G_OBJECT_TYPE_NAME (stage_window),
                stage_window);

  if (!_clutter_stage_x11_create_window (stage_x11))
    return FALSE;

  backend_x11 = stage_x11->backend;
  backend_glx = CLUTTER_BACKEND_GLX (backend_x11);

  if (stage_glx->glxwin == None)
    {
      int major;
      int minor;
      GLXFBConfig config;

      /* Try and create a GLXWindow to use with extensions dependent on
       * GLX versions >= 1.3 that don't accept regular X Windows as GLX
       * drawables.
       */
      if (glXQueryVersion (backend_x11->xdpy, &major, &minor) &&
          major == 1 && minor >= 3 &&
          _clutter_backend_glx_get_fbconfig (backend_glx, &config))
        {
          stage_glx->glxwin = glXCreateWindow (backend_x11->xdpy,
                                               config,
                                               stage_x11->xwin,
                                               NULL);
        }
    }

#ifdef GLX_INTEL_swap_event
  if (clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS))
    {
      GLXDrawable drawable = stage_glx->glxwin
                           ? stage_glx->glxwin
                           : stage_x11->xwin;

      /* we unconditionally select this event because we rely on it to
       * advance the master clock, and drive redraw/relayout, animations
       * and event handling.
       */
      glXSelectEvent (backend_x11->xdpy,
                      drawable,
                      GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }
#endif /* GLX_INTEL_swap_event */

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

#ifdef HAVE_DRM
static int
drm_wait_vblank(int fd, drm_wait_vblank_t *vbl)
{
    int ret, rc;

    do
      {
        ret = ioctl(fd, DRM_IOCTL_WAIT_VBLANK, vbl);
        vbl->request.type &= ~_DRM_VBLANK_RELATIVE;
        rc = errno;
      }
    while (ret && rc == EINTR);

    return rc;
}
#endif /* HAVE_DRM */

static void
wait_for_vblank (ClutterBackendGLX *backend_glx)
{
  if (backend_glx->vblank_type == CLUTTER_VBLANK_NONE)
    return;

  if (backend_glx->wait_video_sync)
    {
      unsigned int retraceCount;

      CLUTTER_NOTE (BACKEND, "Waiting for vblank (wait_video_sync)");
      backend_glx->get_video_sync (&retraceCount);
      backend_glx->wait_video_sync (2,
                                    (retraceCount + 1) % 2,
                                    &retraceCount);
    }
  else
    {
#ifdef HAVE_DRM
      drm_wait_vblank_t blank;

      CLUTTER_NOTE (BACKEND, "Waiting for vblank (drm)");
      blank.request.type     = _DRM_VBLANK_RELATIVE;
      blank.request.sequence = 1;
      blank.request.signal   = 0;
      drm_wait_vblank (backend_glx->dri_fd, &blank);
#else
      CLUTTER_NOTE (BACKEND, "No vblank mechanism found");
#endif /* HAVE_DRM */
    }
}

static void
clutter_stage_glx_redraw (ClutterStageWindow *stage_window)
{
  ClutterBackendX11 *backend_x11;
  ClutterBackendGLX *backend_glx;
  ClutterStageX11 *stage_x11;
  ClutterStageGLX *stage_glx;
  GLXDrawable drawable;
  unsigned int video_sync_count;
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

  backend_x11 = stage_x11->backend;
  backend_glx = CLUTTER_BACKEND_GLX (backend_x11);

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

  cogl_flush ();
  CLUTTER_TIMER_STOP (_clutter_uprof_context, painting_timer);

  drawable = stage_glx->glxwin
           ? stage_glx->glxwin
           : stage_x11->xwin;

  /* If we might ever use _clutter_backend_glx_blit_sub_buffer then we
   * always need to keep track of the video_sync_count so that we can
   * throttle blits.
   *
   * Note: we get the count *before* we issue any glXCopySubBuffer or
   * blit_sub_buffer request in case the count would go up before
   * returning control to us.
   */
  if (backend_glx->can_blit_sub_buffer && backend_glx->get_video_sync)
    backend_glx->get_video_sync (&video_sync_count);

  /* push on the screen */
  if (use_clipped_redraw)
    {
      ClutterGeometry *clip = &stage_glx->bounding_redraw_clip;
      ClutterGeometry copy_area;
      ClutterActor *actor;

      CLUTTER_NOTE (BACKEND,
                    "_glx_blit_sub_buffer (window: 0x%lx, "
                                          "x: %d, y: %d, "
                                          "width: %d, height: %d)",
                    (unsigned long) drawable,
                    stage_glx->bounding_redraw_clip.x,
                    stage_glx->bounding_redraw_clip.y,
                    stage_glx->bounding_redraw_clip.width,
                    stage_glx->bounding_redraw_clip.height);

      /* XXX: It seems there will be a race here in that the stage
       * window may be resized before glXCopySubBufferMESA is handled
       * and so we may copy the wrong region. I can't really see how
       * we can handle this with the current state of X but at least
       * in this case a full redraw should be queued by the resize
       * anyway so it should only exhibit temporary artefacts.
       */
      actor = CLUTTER_ACTOR (stage_x11->wrapper);
      copy_area.y = clutter_actor_get_height (actor)
                  - clip->y
                  - clip->height;
      copy_area.x = clip->x;
      copy_area.width = clip->width;
      copy_area.height = clip->height;

      /* We need to ensure that all the rendering is done, otherwise
       * redraw operations that are slower than the framerate can
       * queue up in the pipeline during a heavy animation, causing a
       * larger and larger backlog of rendering visible as lag to the
       * user.
       *
       * Note: since calling glFinish() and sycnrhonizing the CPU with
       * the GPU is far from ideal, we hope that this is only a short
       * term solution.
       * - One idea is to using sync objects to track render
       *   completion so we can throttle the backlog (ideally with an
       *   additional extension that lets us get notifications in our
       *   mainloop instead of having to busy wait for the
       *   completion.)
       * - Another option is to support clipped redraws by reusing the
       *   contents of old back buffers such that we can flip instead
       *   of using a blit and then we can use GLX_INTEL_swap_events
       *   to throttle. For this though we would still probably want an
       *   additional extension so we can report the limited region of
       *   the window damage to X/compositors.
       */
      glFinish ();

      /* glXCopySubBufferMESA and glBlitFramebuffer are not integrated
       * with the glXSwapIntervalSGI mechanism which we usually use to
       * throttle the Clutter framerate to the vertical refresh and so
       * we have to manually wait for the vblank period...
       */

      /* Here 'is_synchronized' only means that the blit won't cause a
       * tear, ie it won't prevent multiple blits per retrace if they
       * can all be performed in the blanking period. If that's the
       * case then we still want to use the vblank sync menchanism but
       * we only need it to throttle redraws.
       */
      if (!backend_glx->blit_sub_buffer_is_synchronized)
        {
          /* XXX: note that glXCopySubBuffer, at least for Intel, is
           * synchronized with the vblank but glBlitFramebuffer may
           * not be so we use the same scheme we do when calling
           * glXSwapBuffers without the swap_control extension and
           * call glFinish () before waiting for the vblank period.
           *
           * See where we call glXSwapBuffers for more details.
           */
          wait_for_vblank (backend_glx);
        }
      else if (backend_glx->get_video_sync)
        {
          /* If we have the GLX_SGI_video_sync extension then we can
           * be a bit smarter about how we throttle blits by avoiding
           * any waits if we can see that the video sync count has
           * already progressed. */
          if (backend_glx->last_video_sync_count == video_sync_count)
            wait_for_vblank (backend_glx);
        }
      else
        wait_for_vblank (backend_glx);

      CLUTTER_TIMER_START (_clutter_uprof_context, blit_sub_buffer_timer);
      _clutter_backend_glx_blit_sub_buffer (backend_glx,
                                            drawable,
                                            copy_area.x,
                                            copy_area.y,
                                            copy_area.width,
                                            copy_area.height);
      CLUTTER_TIMER_STOP (_clutter_uprof_context, blit_sub_buffer_timer);
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "glXSwapBuffers (display: %p, window: 0x%lx)",
                    backend_x11->xdpy,
                    (unsigned long) drawable);

      /* If we have GLX swap buffer events then glXSwapBuffers will return
       * immediately and we need to track that there is a swap in
       * progress... */
      if (clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS))
        stage_glx->pending_swaps++;

      if (backend_glx->vblank_type != CLUTTER_VBLANK_GLX_SWAP &&
          backend_glx->vblank_type != CLUTTER_VBLANK_NONE)
        {
          /* If we are going to wait for VBLANK manually, we not only
           * need to flush out pending drawing to the GPU before we
           * sleep, we need to wait for it to finish. Otherwise, we
           * may end up with the situation:
           *
           *        - We finish drawing      - GPU drawing continues
           *        - We go to sleep         - GPU drawing continues
           * VBLANK - We call glXSwapBuffers - GPU drawing continues
           *                                 - GPU drawing continues
           *                                 - Swap buffers happens
           *
           * Producing a tear. Calling glFinish() first will cause us
           * to properly wait for the next VBLANK before we swap. This
           * obviously does not happen when we use _GLX_SWAP and let
           * the driver do the right thing
           */
          glFinish ();

          wait_for_vblank (backend_glx);
        }

      CLUTTER_TIMER_START (_clutter_uprof_context, swapbuffers_timer);
      glXSwapBuffers (backend_x11->xdpy, drawable);
      CLUTTER_TIMER_STOP (_clutter_uprof_context, swapbuffers_timer);

      _cogl_swap_buffers_notify ();
    }

  backend_glx->last_video_sync_count = video_sync_count;

  /* reset the redraw clipping for the next paint... */
  stage_glx->initialized_redraw_clip = FALSE;

  stage_glx->frame_count++;
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

  /* the rest is inherited from ClutterStageX11 */
}

static ClutterTranslateReturn
clutter_stage_glx_translate_event (ClutterEventTranslator *translator,
                                   gpointer                native,
                                   ClutterEvent           *event)
{
#ifdef GLX_INTEL_swap_event
  ClutterBackendGLX *backend_glx;
  XEvent *xevent = native;

  backend_glx = CLUTTER_BACKEND_GLX (clutter_get_default_backend ());

  if (xevent->type == (backend_glx->event_base + GLX_BufferSwapComplete))
    {
      ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (translator);
      ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (translator);
      GLXBufferSwapComplete *swap_complete_event;

      swap_complete_event = (GLXBufferSwapComplete *) xevent;

      if (stage_x11->xwin == swap_complete_event->drawable)
        {
	  /* Early versions of the swap_event implementation in Mesa
	   * deliver BufferSwapComplete event when not selected for,
	   * so if we get a swap event we aren't expecting, just ignore it.
	   *
	   * https://bugs.freedesktop.org/show_bug.cgi?id=27962
	   */
          if (stage_glx->pending_swaps > 0)
            stage_glx->pending_swaps--;

          return CLUTTER_TRANSLATE_REMOVE;
        }
    }
#endif

  /* chain up to the common X11 implementation */
  return clutter_event_translator_parent_iface->translate_event (translator,
                                                                 native,
                                                                 event);
}

static void
clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface)
{
  clutter_event_translator_parent_iface = g_type_interface_peek_parent (iface);

  iface->translate_event = clutter_stage_glx_translate_event;
}
