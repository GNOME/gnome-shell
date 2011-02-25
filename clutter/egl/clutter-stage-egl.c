#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-stage-egl.h"
#include "clutter-egl.h"
#include "clutter-backend-egl.h"

#include "clutter-debug.h"
#include "clutter-event.h"
#include "clutter-enum-types.h"
#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-actor-private.h"
#include "clutter-stage-private.h"
#include "clutter-util.h"

#ifdef COGL_HAS_X11_SUPPORT
static ClutterStageWindowIface *clutter_stage_egl_parent_iface = NULL;
#endif

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageEGL,
                         _clutter_stage_egl,
#ifdef COGL_HAS_X11_SUPPORT
                         CLUTTER_TYPE_STAGE_X11,
#else
                         G_TYPE_OBJECT,
#endif
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_egl_unrealize (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  CLUTTER_NOTE (BACKEND, "Unrealizing EGL stage [%p]", stage_egl);

#ifdef COGL_HAS_XLIB_SUPPORT
  /* chain up to the StageX11 implementation */
  clutter_stage_window_parent_iface->unrealize (stage_window);
#endif

  cogl_object_unref (stage_egl->onscreen);
  stage_egl->onscreen = NULL;
}

static gboolean
clutter_stage_egl_realize (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);
#ifdef COGL_HAS_XLIB_SUPPORT
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
#endif
  ClutterBackend *backend;
  ClutterBackendEGL *backend_egl;
  CoglFramebuffer *framebuffer;
  GError *error = NULL;
  gfloat width = 800;
  gfloat height = 600;
  const char *clutter_vblank;

  CLUTTER_NOTE (BACKEND, "Realizing stage '%s' [%p]",
                G_OBJECT_TYPE_NAME (stage_egl),
                stage_egl);

  backend     = clutter_get_default_backend ();
  backend_egl = CLUTTER_BACKEND_EGL (backend);

#ifdef COGL_HAS_XLIB_SUPPORT
  clutter_actor_get_size (CLUTTER_ACTOR (stage_x11->wrapper), &width, &height);
#endif

  stage_egl->onscreen = cogl_onscreen_new (backend->cogl_context,
                                           width, height);
#ifdef COGL_HAS_XLIB_SUPPORT
  if (stage_x11->xwin != None)
    cogl_onscreen_x11_set_foreign_window_xid (stage_egl->onscreen,
                                              stage_x11->xwin);
#endif

  clutter_vblank = _clutter_backend_egl_get_vblank ();
  if (clutter_vblank && strcmp (clutter_vblank, "none") == 0)
    cogl_onscreen_set_swap_throttled (stage_egl->onscreen, FALSE);

  framebuffer = COGL_FRAMEBUFFER (stage_egl->onscreen);
  if (!cogl_framebuffer_allocate (framebuffer, &error))
    {
      g_warning ("Failed to allocate stage: %s", error->message);
      g_error_free (error);
      cogl_object_unref (stage_egl->onscreen);
      stage_egl->onscreen = NULL;
      return FALSE;
    }
  /* FIXME: for fullscreen EGL platforms then the size we gave above
   * will be ignored, so we need to make sure the stage size is
   * updated to this size. */

#ifdef COGL_HAS_XLIB_SUPPORT
  if (stage_x11->xwin == None)
    stage_x11->xwin = cogl_onscreen_x11_get_window_xid (stage_egl->onscreen);

  return clutter_stage_egl_parent_iface->realize (stage_window);
#else
  return TRUE;
#endif
}

#ifndef COGL_HAS_XLIB_SUPPORT

/* FIXME: Move this warnings up into clutter-stage.c */

static void
clutter_stage_egl_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            fullscreen)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_fullscreen",
             G_OBJECT_TYPE_NAME (stage_window));
}

static void
clutter_stage_egl_set_title (ClutterStageWindow *stage_window,
                             const gchar        *title)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_title",
             G_OBJECT_TYPE_NAME (stage_window));
}

static void
clutter_stage_egl_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            cursor_visible)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_cursor_visible",
             G_OBJECT_TYPE_NAME (stage_window));
}

static ClutterActor *
clutter_stage_egl_get_wrapper (ClutterStageWindow *stage_window)
{
  return CLUTTER_ACTOR (CLUTTER_STAGE_EGL (stage_window)->wrapper);
}

static void
clutter_stage_egl_show (ClutterStageWindow *stage_window,
                        gboolean            do_raise)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  clutter_actor_map (CLUTTER_ACTOR (stage_egl->wrapper));
}

static void
clutter_stage_egl_hide (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  clutter_actor_unmap (CLUTTER_ACTOR (stage_egl->wrapper));
}

static void
clutter_stage_egl_get_geometry (ClutterStageWindow *stage_window,
                                ClutterGeometry    *geometry)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  if (geometry)
    {
      if (stage_egl->onscreen)
        {
          CoglFramebuffer *framebuffer =
            COGL_FRAMEBUFFER (stage_egl->onscreen);

          geometry->x = geometry->y = 0;

          geometry->width = cogl_framebuffer_get_width (framebuffer);
          geometry->height = cogl_framebuffer_get_height (framebuffer);
        }
      else
        {
          geometry->x = geometry->y = 0;
          geometry->width = 800;
          geometry->height = 600;
        }
    }
}

static void
clutter_stage_egl_resize (ClutterStageWindow *stage_window,
                          gint                width,
                          gint                height)
{
}

#endif /* COGL_HAS_XLIB_SUPPORT */

static gboolean
clutter_stage_egl_has_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  /* NB: at the start of each new frame there is an implied clip that
   * clips everything (i.e. nothing would be drawn) so we need to make
   * sure we return True in the un-initialized case here.
   *
   * NB: a clip width of 0 means a full stage redraw has been queued
   * so we effectively don't have any redraw clips in that case.
   */
  if (!stage_egl->initialized_redraw_clip ||
      (stage_egl->initialized_redraw_clip &&
       stage_egl->bounding_redraw_clip.width != 0))
    return TRUE;
  else
    return FALSE;
}

static gboolean
clutter_stage_egl_ignoring_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  /* NB: a clip width of 0 means a full stage redraw is required */
  if (stage_egl->initialized_redraw_clip &&
      stage_egl->bounding_redraw_clip.width == 0)
    return TRUE;
  else
    return FALSE;
}

/* A redraw clip represents (in stage coordinates) the bounding box of
 * something that needs to be redraw. Typically they are added to the
 * StageWindow as a result of clutter_actor_queue_clipped_redraw() by
 * actors such as ClutterEGLTexturePixmap. All redraw clips are
 * discarded after the next paint.
 *
 * A NULL stage_clip means the whole stage needs to be redrawn.
 *
 * What we do with this information:
 * - we keep track of the bounding box for all redraw clips
 * - when we come to redraw; we scissor the redraw to that box and use
 *   glBlitFramebuffer to present the redraw to the front
 *   buffer.
 */
static void
clutter_stage_egl_add_redraw_clip (ClutterStageWindow *stage_window,
                                   ClutterGeometry    *stage_clip)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  /* If we are already forced to do a full stage redraw then bail early */
  if (clutter_stage_egl_ignoring_redraw_clips (stage_window))
    return;

  /* A NULL stage clip means a full stage redraw has been queued and
   * we keep track of this by setting a zero width
   * stage_egl->bounding_redraw_clip */
  if (stage_clip == NULL)
    {
      stage_egl->bounding_redraw_clip.width = 0;
      stage_egl->initialized_redraw_clip = TRUE;
      return;
    }

  /* Ignore requests to add degenerate/empty clip rectangles */
  if (stage_clip->width == 0 || stage_clip->height == 0)
    return;

  if (!stage_egl->initialized_redraw_clip)
    {
      stage_egl->bounding_redraw_clip.x = stage_clip->x;
      stage_egl->bounding_redraw_clip.y = stage_clip->y;
      stage_egl->bounding_redraw_clip.width = stage_clip->width;
      stage_egl->bounding_redraw_clip.height = stage_clip->height;
    }
  else if (stage_egl->bounding_redraw_clip.width > 0)
    {
      clutter_geometry_union (&stage_egl->bounding_redraw_clip, stage_clip,
			      &stage_egl->bounding_redraw_clip);
    }

  stage_egl->initialized_redraw_clip = TRUE;
}

/* XXX: This is basically identical to clutter_stage_glx_redraw */
static void
clutter_stage_egl_redraw (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);
  ClutterActor *wrapper;
  ClutterBackend *backend;
  ClutterBackendEGL *backend_egl;
  gboolean may_use_clipped_redraw;
  gboolean use_clipped_redraw;

  CLUTTER_STATIC_TIMER (painting_timer,
                        "Redrawing", /* parent */
                        "Painting actors",
                        "The time spent painting actors",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (swapbuffers_timer,
                        "Redrawing", /* parent */
                        "eglSwapBuffers",
                        "The time spent blocked by eglSwapBuffers",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (blit_sub_buffer_timer,
                        "Redrawing", /* parent */
                        "egl_blit_sub_buffer",
                        "The time spent in _egl_blit_sub_buffer",
                        0 /* no application private data */);

#ifdef COGL_HAS_X11_SUPPORT
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_egl);

  wrapper = CLUTTER_ACTOR (stage_x11->wrapper);
#else
  wrapper = CLUTTER_ACTOR (stage_egl->wrapper);
#endif

  if (!stage_egl->onscreen)
    return;

  backend = clutter_get_default_backend ();
  backend_egl = CLUTTER_BACKEND_EGL (backend);

  CLUTTER_TIMER_START (_clutter_uprof_context, painting_timer);

  if (G_LIKELY (backend_egl->can_blit_sub_buffer) &&
      /* NB: a zero width redraw clip == full stage redraw */
      stage_egl->bounding_redraw_clip.width != 0 &&
      /* some drivers struggle to get going and produce some junk
       * frames when starting up... */
      G_LIKELY (stage_egl->frame_count > 3)
#ifdef COGL_HAS_X11_SUPPORT
      /* While resizing a window clipped redraws are disabled to avoid
       * artefacts. See clutter-event-x11.c:event_translate for a
       * detailed explanation */
      && G_LIKELY (stage_x11->clipped_redraws_cool_off == 0)
#endif
      )
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
      cogl_clip_push_window_rectangle (stage_egl->bounding_redraw_clip.x,
                                       stage_egl->bounding_redraw_clip.y,
                                       stage_egl->bounding_redraw_clip.width,
                                       stage_egl->bounding_redraw_clip.height);
      _clutter_stage_do_paint (CLUTTER_STAGE (wrapper),
                               &stage_egl->bounding_redraw_clip);
      cogl_clip_pop ();
    }
  else
    _clutter_stage_do_paint (CLUTTER_STAGE (wrapper), NULL);

  if (may_use_clipped_redraw &&
      G_UNLIKELY ((clutter_paint_debug_flags & CLUTTER_DEBUG_REDRAWS)))
    {
      static CoglMaterial *outline = NULL;
      ClutterGeometry *clip = &stage_egl->bounding_redraw_clip;
      ClutterActor *actor = CLUTTER_ACTOR (wrapper);
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
      ClutterGeometry *clip = &stage_egl->bounding_redraw_clip;
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

      actor = CLUTTER_ACTOR (wrapper);
      copy_area[0] = clip->x;
      copy_area[1] = clutter_actor_get_height (actor) - clip->y - clip->height;
      copy_area[2] = clip->width;
      copy_area[3] = clip->height;

      CLUTTER_NOTE (BACKEND,
                    "cogl_framebuffer_swap_region (onscreen: %p, "
                                                  "x: %d, y: %d, "
                                                  "width: %d, height: %d)",
                    stage_egl->onscreen,
                    copy_area[0], copy_area[1], copy_area[2], copy_area[3]);


      CLUTTER_TIMER_START (_clutter_uprof_context, blit_sub_buffer_timer);

      cogl_framebuffer_swap_region (COGL_FRAMEBUFFER (stage_egl->onscreen),
                                    copy_area, 1);

      CLUTTER_TIMER_STOP (_clutter_uprof_context, blit_sub_buffer_timer);
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "cogl_framebuffer_swap_buffers (onscreen: %p)",
                    stage_egl->onscreen);

      CLUTTER_TIMER_START (_clutter_uprof_context, swapbuffers_timer);
      cogl_framebuffer_swap_buffers (COGL_FRAMEBUFFER (stage_egl->onscreen));
      CLUTTER_TIMER_STOP (_clutter_uprof_context, swapbuffers_timer);
    }

  /* reset the redraw clipping for the next paint... */
  stage_egl->initialized_redraw_clip = FALSE;

  stage_egl->frame_count++;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
#ifdef COGL_HAS_X11_SUPPORT
  clutter_stage_egl_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = clutter_stage_egl_realize;
  iface->unrealize = clutter_stage_egl_unrealize;

  /* the rest is inherited from ClutterStageX11 */

#else /* COGL_HAS_X11_SUPPORT */

  iface->realize = clutter_stage_egl_realize;
  iface->unrealize = clutter_stage_egl_unrealize;
  iface->set_fullscreen = clutter_stage_egl_set_fullscreen;
  iface->set_title = clutter_stage_egl_set_title;
  iface->set_cursor_visible = clutter_stage_egl_set_cursor_visible;
  iface->get_wrapper = clutter_stage_egl_get_wrapper;
  iface->get_geometry = clutter_stage_egl_get_geometry;
  iface->resize = clutter_stage_egl_resize;
  iface->show = clutter_stage_egl_show;
  iface->hide = clutter_stage_egl_hide;

#endif /* COGL_HAS_X11_SUPPORT */

  iface->add_redraw_clip = clutter_stage_egl_add_redraw_clip;
  iface->has_redraw_clips = clutter_stage_egl_has_redraw_clips;
  iface->ignoring_redraw_clips = clutter_stage_egl_ignoring_redraw_clips;
  iface->redraw = clutter_stage_egl_redraw;
}

#ifdef COGL_HAS_X11_SUPPORT
static void
clutter_stage_egl_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (_clutter_stage_egl_parent_class)->dispose (gobject);
}

static void
_clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_stage_egl_dispose;
}
#else
static void
_clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
}
#endif /* COGL_HAS_X11_SUPPORT */

static void
_clutter_stage_egl_init (ClutterStageEGL *stage)
{
}
