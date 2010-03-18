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

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"
#include "../clutter-shader.h"
#include "../clutter-group.h"
#include "../clutter-container.h"
#include "../clutter-stage.h"
#include "../clutter-stage-window.h"

#include "cogl/cogl.h"

#include <GL/glx.h>
#include <GL/gl.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __linux__
#define DRM_VBLANK_RELATIVE 0x1;

struct drm_wait_vblank_request {
    int           type;
    unsigned int  sequence;
    unsigned long signal;
};

struct drm_wait_vblank_reply {
    int          type;
    unsigned int sequence;
    long         tval_sec;
    long         tval_usec;
};

typedef union drm_wait_vblank {
    struct drm_wait_vblank_request request;
    struct drm_wait_vblank_reply reply;
} drm_wait_vblank_t;

#define DRM_IOCTL_BASE                  'd'
#define DRM_IOWR(nr,type)               _IOWR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOCTL_WAIT_VBLANK           DRM_IOWR(0x3a, drm_wait_vblank_t)

#endif /* __linux__ */

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

static ClutterStageWindowIface *clutter_stage_glx_parent_iface = NULL;

G_DEFINE_TYPE_WITH_CODE (ClutterStageGLX,
                         clutter_stage_glx,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_glx_unrealize (ClutterStageWindow *stage_window)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* Note unrealize should free up any backend stage related resources */
  CLUTTER_NOTE (BACKEND, "Unrealizing stage");

  clutter_x11_trap_x_errors ();

  if (stage_glx->glxwin != None)
    {
      glXDestroyWindow (backend_x11->xdpy, stage_glx->glxwin);
      stage_glx->glxwin = None;
    }

  if (!stage_x11->is_foreign_xwin && stage_x11->xwin != None)
    {
      XDestroyWindow (backend_x11->xdpy, stage_x11->xwin);
      stage_x11->xwin = None;
    }
  else
    stage_x11->xwin = None;

  XSync (backend_x11->xdpy, False);

  clutter_x11_untrap_x_errors ();

  CLUTTER_MARK ();
}

static gboolean
clutter_stage_glx_realize (ClutterStageWindow *stage_window)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);
  ClutterBackendX11 *backend_x11;
  ClutterBackendGLX *backend_glx;
  ClutterBackend *backend;

  CLUTTER_NOTE (ACTOR, "Realizing stage '%s' [%p]",
                G_OBJECT_TYPE_NAME (stage_window),
                stage_window);

  backend     = clutter_get_default_backend ();
  backend_glx = CLUTTER_BACKEND_GLX (backend);
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (stage_x11->xwin == None)
    {
      XSetWindowAttributes xattr;
      unsigned long mask;
      XVisualInfo *xvisinfo;
      gfloat width, height;

      CLUTTER_NOTE (MISC, "Creating stage X window");

      xvisinfo = clutter_backend_x11_get_visual_info (backend_x11);
      if (xvisinfo == NULL)
        {
          g_critical ("Unable to find suitable GL visual.");
          return FALSE;
        }

      /* window attributes */
      xattr.background_pixel = WhitePixel (backend_x11->xdpy,
                                           backend_x11->xscreen_num);
      xattr.border_pixel = 0;
      xattr.colormap = XCreateColormap (backend_x11->xdpy,
                                        backend_x11->xwin_root,
                                        xvisinfo->visual,
                                        AllocNone);
      mask = CWBorderPixel | CWColormap;

      /* Call get_size - this will either get the geometry size (which
       * before we create the window is set to 640x480), or if a size
       * is set, it will get that. This lets you set a size on the
       * stage before it's realized.
       */
      clutter_actor_get_size (CLUTTER_ACTOR (stage_x11->wrapper),
                              &width,
                              &height);
      stage_x11->xwin_width = (gint)width;
      stage_x11->xwin_height = (gint)height;

      stage_x11->xwin = XCreateWindow (backend_x11->xdpy,
                                       backend_x11->xwin_root,
                                       0, 0,
                                       stage_x11->xwin_width,
                                       stage_x11->xwin_height,
                                       0,
                                       xvisinfo->depth,
                                       InputOutput,
                                       xvisinfo->visual,
                                       mask, &xattr);

      CLUTTER_NOTE (BACKEND, "Stage [%p], window: 0x%x, size: %dx%d",
                    stage_window,
                    (unsigned int) stage_x11->xwin,
                    stage_x11->xwin_width,
                    stage_x11->xwin_height);

      XFree (xvisinfo);
    }

  if (stage_glx->glxwin == None)
    {
      int major;
      int minor;
      GLXFBConfig config;

      /* Try and create a GLXWindow to use with extensions dependent on
       * GLX versions >= 1.3 that don't accept regular X Windows as GLX
       * drawables. */
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

  if (clutter_x11_has_event_retrieval ())
    {
      if (clutter_x11_has_xinput ())
        {
          XSelectInput (backend_x11->xdpy, stage_x11->xwin,
                        StructureNotifyMask |
                        FocusChangeMask |
                        ExposureMask |
                        KeyPressMask | KeyReleaseMask |
                        EnterWindowMask | LeaveWindowMask |
                        PropertyChangeMask);
#ifdef HAVE_XINPUT
          _clutter_x11_select_events (stage_x11->xwin);
#endif
        }
      else
        XSelectInput (backend_x11->xdpy, stage_x11->xwin,
                      StructureNotifyMask |
                      FocusChangeMask |
                      ExposureMask |
                      PointerMotionMask |
                      KeyPressMask | KeyReleaseMask |
                      ButtonPressMask | ButtonReleaseMask |
                      EnterWindowMask | LeaveWindowMask |
                      PropertyChangeMask);

#ifdef GLX_INTEL_swap_event
      if (clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS))
        {
          GLXDrawable drawable =
            stage_glx->glxwin ? stage_glx->glxwin : stage_x11->xwin;
          glXSelectEvent (backend_x11->xdpy,
                          drawable,
                          GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
        }
#endif /* GLX_INTEL_swap_event */
    }

  /* no user resize.. */
  clutter_stage_x11_fix_window_size (stage_x11,
                                     stage_x11->xwin_width,
                                     stage_x11->xwin_height);
  clutter_stage_x11_set_wm_protocols (stage_x11);

  CLUTTER_NOTE (BACKEND, "Successfully realized stage");

  /* chain up to the StageX11 implementation */
  return clutter_stage_glx_parent_iface->realize (stage_window);
}

static int
clutter_stage_glx_get_pending_swaps (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  return stage_glx->pending_swaps;
}

static void
clutter_stage_glx_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_stage_glx_parent_class)->dispose (gobject);
}

static void
clutter_stage_glx_class_init (ClutterStageGLXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_stage_glx_dispose;
}

static void
clutter_stage_glx_init (ClutterStageGLX *stage)
{
}

static gboolean
clutter_stage_glx_has_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* NB: a degenerate clip means a full stage redraw is required */
  if (stage_glx->initialized_redraw_clip &&
      stage_glx->bounding_redraw_clip.width != 0)
    return TRUE;
  else
    return FALSE;
}

static gboolean
clutter_stage_glx_ignoring_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* NB: a degenerate clip means a full stage redraw is required */
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
 *   buffer. Some heuristics are used to decide when a clipped redraw
 *   should be promoted into a full stage redraw.
 *
 * Currently we simply check that the bounding box height is < 300
 * pixels.
 *
 * XXX: we don't have any empirical data telling us what a sensible
 * thresholds is!
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
 *
 */
static void
clutter_stage_glx_add_redraw_clip (ClutterStageWindow *stage_window,
                                   ClutterGeometry    *stage_clip)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* If we are already forced to do a full stage redraw then bail early */
  if (clutter_stage_glx_ignoring_redraw_clips (stage_window))
    return;

  /* Do nothing on an empty clip, to avoid confusing with the flag degenerate clip */
  if (stage_clip->width == 0)
    return;

  /* A NULL stage clip means a full stage redraw has been queued and
   * we keep track of this by setting a degenerate
   * stage_glx->bounding_redraw_clip */
  if (!stage_clip)
    {
      stage_glx->bounding_redraw_clip.width = 0;
      return;
    }

  if (!stage_glx->initialized_redraw_clip)
    {
      stage_glx->bounding_redraw_clip.x = stage_clip->x;
      stage_glx->bounding_redraw_clip.y = stage_clip->y;
      stage_glx->bounding_redraw_clip.width = stage_clip->width;
      stage_glx->bounding_redraw_clip.height = stage_clip->height;
    }
  else if (stage_glx->bounding_redraw_clip.width > 0)
    {
      clutter_geometry_union (&stage_glx->bounding_redraw_clip, stage_clip,
			      &stage_glx->bounding_redraw_clip);
    }

  /* FIXME: This threshold was plucked out of thin air! */
  if (stage_glx->bounding_redraw_clip.height > 300)
    {
      /* Set a degenerate clip to force a full redraw */
      stage_glx->bounding_redraw_clip.width = 0;
    }

#if 0
  redraw_area = (stage_glx->bounding_redraw_clip.width *
                 stage_glx->bounding_redraw_clip.height);
  stage_area = stage_x11->xwin_width * stage_x11->xwin_height;

  /* Redrawing and blitting >70% of the stage is assumed to be more
   * expensive than redrawing the additional 30% to avoid the blit.
   *
   * FIXME: This threshold was plucked out of thin air!
   */
  if (redraw_area > (stage_area * 0.7f))
    {
      g_print ("DEBUG: clipped redraw too big, forcing full redraw\n");
      /* Set a degenerate clip to force a full redraw */
      stage_glx->bounding_redraw_clip.width = 0;
    }
#endif

  stage_glx->initialized_redraw_clip = TRUE;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_glx_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = clutter_stage_glx_realize;
  iface->unrealize = clutter_stage_glx_unrealize;
  iface->get_pending_swaps = clutter_stage_glx_get_pending_swaps;

  iface->add_redraw_clip = clutter_stage_glx_add_redraw_clip;
  iface->has_redraw_clips = clutter_stage_glx_has_redraw_clips;
  iface->ignoring_redraw_clips = clutter_stage_glx_ignoring_redraw_clips;

  /* the rest is inherited from ClutterStageX11 */
}

#ifdef __linux__
static int
drm_wait_vblank(int fd, drm_wait_vblank_t *vbl)
{
    int ret, rc;

    do
      {
        ret = ioctl(fd, DRM_IOCTL_WAIT_VBLANK, vbl);
        vbl->request.type &= ~DRM_VBLANK_RELATIVE;
        rc = errno;
      }
    while (ret && rc == EINTR);

    return rc;
}
#endif /* __linux__ */

static void
glx_wait_for_vblank (ClutterBackendGLX *backend_glx)
{
  /* If we are going to wait for VBLANK manually, we not only need
   * to flush out pending drawing to the GPU before we sleep, we
   * need to wait for it to finish. Otherwise, we may end up with
   * the situation:
   *
   *        - We finish drawing      - GPU drawing continues
   *        - We go to sleep         - GPU drawing continues
   * VBLANK - We call glXSwapBuffers - GPU drawing continues
   *                                 - GPU drawing continues
   *                                 - Swap buffers happens
   *
   * Producing a tear. Calling glFinish() first will cause us to properly
   * wait for the next VBLANK before we swap. This obviously does not
   * happen when we use GLX_SWAP and let the driver do the right thing
   */

  switch (backend_glx->vblank_type)
    {
    case CLUTTER_VBLANK_GLX_SWAP:
      CLUTTER_NOTE (BACKEND, "Waiting for vblank (swap)");
      break;

    case CLUTTER_VBLANK_GLX:
      {
        unsigned int retraceCount;

        glFinish ();

        CLUTTER_NOTE (BACKEND, "Waiting for vblank (wait_video_sync)");
        backend_glx->get_video_sync (&retraceCount);
        backend_glx->wait_video_sync (2,
                                      (retraceCount + 1) % 2,
                                      &retraceCount);
      }
      break;

    case CLUTTER_VBLANK_DRI:
#ifdef __linux__
      {
        drm_wait_vblank_t blank;

        glFinish ();

        CLUTTER_NOTE (BACKEND, "Waiting for vblank (drm)");
        blank.request.type     = DRM_VBLANK_RELATIVE;
        blank.request.sequence = 1;
        blank.request.signal   = 0;
        drm_wait_vblank (backend_glx->dri_fd, &blank);
      }
#endif
      break;

    case CLUTTER_VBLANK_NONE:
    default:
      break;
    }
}

void
clutter_stage_glx_redraw (ClutterStageGLX *stage_glx,
                          ClutterStage *stage)
{
  ClutterBackend    *backend;
  ClutterBackendX11 *backend_x11;
  ClutterBackendGLX *backend_glx;
  ClutterStageX11   *stage_x11;
  GLXDrawable        drawable;
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
  CLUTTER_STATIC_TIMER (copy_sub_buffer_timer,
                        "Redrawing", /* parent */
                        "glXCopySubBufferMESA",
                        "The time spent blocked by glXCopySubBufferMESA",
                        0 /* no application private data */);

  backend     = clutter_get_default_backend ();
  backend_x11 = CLUTTER_BACKEND_X11 (backend);
  backend_glx = CLUTTER_BACKEND_GLX (backend);

  stage_x11 = CLUTTER_STAGE_X11 (stage_glx);

  CLUTTER_TIMER_START (_clutter_uprof_context, painting_timer);

  if (backend_glx->copy_sub_buffer &&
      /* NB: a degenerate redraw clip width == full stage redraw */
      (stage_glx->bounding_redraw_clip.width != 0) &&
      G_LIKELY (!(clutter_paint_debug_flags &
                  CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS)))
    {
      cogl_clip_push_window_rectangle (stage_glx->bounding_redraw_clip.x,
                                       stage_glx->bounding_redraw_clip.y,
                                       stage_glx->bounding_redraw_clip.width,
                                       stage_glx->bounding_redraw_clip.height);
      clutter_actor_paint (CLUTTER_ACTOR (stage));
      cogl_clip_pop ();
    }
  else
    clutter_actor_paint (CLUTTER_ACTOR (stage));

  cogl_flush ();
  CLUTTER_TIMER_STOP (_clutter_uprof_context, painting_timer);

  if (stage_x11->xwin == None)
    return;

  drawable = stage_glx->glxwin ? stage_glx->glxwin : stage_x11->xwin;

  /* wait for the next vblank */
  glx_wait_for_vblank (CLUTTER_BACKEND_GLX (backend));

  /* push on the screen */
  if (backend_glx->copy_sub_buffer &&
      /* NB: a degenerate redraw clip width == full stage redraw */
      (stage_glx->bounding_redraw_clip.width != 0) &&
      G_LIKELY (!(clutter_paint_debug_flags &
                  CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS)))
    {
      ClutterGeometry *clip = &stage_glx->bounding_redraw_clip;
      ClutterGeometry copy_area;

      CLUTTER_NOTE (BACKEND,
                    "glXCopySubBufferMESA (display: %p, "
                                          "window: 0x%lx, "
                                          "x: %d, y: %d, "
                                          "width: %d, height: %d)",
                    backend_x11->xdpy,
                    (unsigned long) drawable,
                    stage_glx->bounding_redraw_clip.x,
                    stage_glx->bounding_redraw_clip.y,
                    stage_glx->bounding_redraw_clip.width,
                    stage_glx->bounding_redraw_clip.height);

      if (clutter_paint_debug_flags & CLUTTER_DEBUG_REDRAWS)
        {
          static CoglHandle outline = COGL_INVALID_HANDLE;
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

          if (outline == COGL_INVALID_HANDLE)
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

          cogl_set_source (outline);
          cogl_vertex_buffer_draw (vbo, COGL_VERTICES_MODE_LINE_LOOP,
                                   0 , 4);
          cogl_flush ();
        }

      /* XXX: It seems there will be a race here in that the stage
       * window may be resized before glXCopySubBufferMESA is handled
       * and so we may copy the wrong region. I can't really see how
       * we can handle this with the current state of X but at least
       * in this case a full redraw should be queued by the resize
       * anyway so it should only exhibit temporary artefacts.
       */
      copy_area.y = clutter_actor_get_height (CLUTTER_ACTOR (stage))
        - clip->y - clip->height;
      copy_area.x = clip->x;
      copy_area.width = clip->width;
      copy_area.height = clip->height;

      CLUTTER_TIMER_START (_clutter_uprof_context, copy_sub_buffer_timer);
      backend_glx->copy_sub_buffer (backend_x11->xdpy,
                                    drawable,
                                    copy_area.x,
                                    copy_area.y,
                                    copy_area.width,
                                    copy_area.height);
      CLUTTER_TIMER_STOP (_clutter_uprof_context, copy_sub_buffer_timer);
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

      CLUTTER_TIMER_START (_clutter_uprof_context, swapbuffers_timer);
      glXSwapBuffers (backend_x11->xdpy, drawable);
      CLUTTER_TIMER_STOP (_clutter_uprof_context, swapbuffers_timer);
    }

  /* reset the redraw clipping for the next paint... */
  stage_glx->initialized_redraw_clip = FALSE;
}

