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

#ifdef COGL_HAS_XLIB_SUPPORT

static void
clutter_stage_egl_unrealize (ClutterStageWindow *stage_window)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  CLUTTER_NOTE (BACKEND, "Unrealizing EGL stage [%p]", stage_egl);

  clutter_x11_trap_x_errors ();

  if (stage_egl->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (clutter_egl_get_egl_display (), stage_egl->egl_surface);
      stage_egl->egl_surface = EGL_NO_SURFACE;
    }

  _clutter_stage_x11_destroy_window_untrapped (stage_x11);

  XSync (backend_x11->xdpy, False);

  clutter_x11_untrap_x_errors ();
}

static gboolean
clutter_stage_egl_realize (ClutterStageWindow *stage_window)
{
  ClutterStageEGL   *stage_egl = CLUTTER_STAGE_EGL (stage_window);
  ClutterStageX11   *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackend    *backend;
  ClutterBackendEGL *backend_egl;
  EGLDisplay         edpy;

  CLUTTER_NOTE (BACKEND, "Realizing stage '%s' [%p]",
                G_OBJECT_TYPE_NAME (stage_egl),
                stage_egl);

  backend     = clutter_get_default_backend ();
  backend_egl = CLUTTER_BACKEND_EGL (backend);

  edpy = clutter_egl_get_egl_display ();

  if (!_clutter_stage_x11_create_window (stage_x11))
    return FALSE;

  if (stage_egl->egl_surface == EGL_NO_SURFACE)
    {
      stage_egl->egl_surface =
        eglCreateWindowSurface (edpy,
                                backend_egl->egl_config,
                                (NativeWindowType) stage_x11->xwin,
                                NULL);
    }

  if (stage_egl->egl_surface == EGL_NO_SURFACE)
    g_warning ("Unable to create an EGL surface");

  return clutter_stage_egl_parent_iface->realize (stage_window);
}

#else /* COGL_HAS_XLIB_SUPPORT */

static void
clutter_stage_egl_unrealize (ClutterStageWindow *stage_window)
{
}

static gboolean
clutter_stage_egl_realize (ClutterStageWindow *stage_window)
{
  /* the EGL surface is created by the backend */
  return TRUE;
}

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
  ClutterBackendEGL *backend_egl = stage_egl->backend;

  if (geometry)
    {
      geometry->x = geometry->y = 0;

      geometry->width = backend_egl->surface_width;
      geometry->height = backend_egl->surface_height;
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

static void
clutter_stage_egl_redraw (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterActor *wrapper;
  EGLSurface egl_surface;
  gboolean may_use_clipped_redraw;
  gboolean use_clipped_redraw;
#ifdef COGL_HAS_X11_SUPPORT
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_egl);

  wrapper = CLUTTER_ACTOR (stage_x11->wrapper);
  egl_surface = stage_egl->egl_surface;
#else
  wrapper = CLUTTER_ACTOR (stage_egl->wrapper);
  /* Without X we only support one surface and that is associated
   * with the backend directly instead of the stage */
  egl_surface = backend_egl->egl_surface;
#endif

  if (G_LIKELY (backend_egl->can_blit_sub_buffer) &&
      /* NB: a zero width clip == full stage redraw */
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
    may_use_clipped_redraw = TRUE;
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

  if (clutter_paint_debug_flags & CLUTTER_DEBUG_REDRAWS &&
      may_use_clipped_redraw)
    {
      ClutterGeometry *clip = &stage_egl->bounding_redraw_clip;
      static CoglMaterial *outline = NULL;
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
      _clutter_actor_apply_modelview_transform (wrapper, &modelview);
      cogl_set_modelview_matrix (&modelview);
      cogl_set_source (outline);
      cogl_vertex_buffer_draw (vbo, COGL_VERTICES_MODE_LINE_LOOP,
                               0 , 4);
      cogl_pop_matrix ();
      cogl_object_unref (vbo);
    }

  cogl_flush ();

  /* push on the screen */
  if (use_clipped_redraw)
    {
      ClutterGeometry *clip = &stage_egl->bounding_redraw_clip;
      ClutterGeometry copy_area;

      CLUTTER_NOTE (BACKEND,
                    "_egl_blit_sub_buffer (surface: %p, "
                                          "x: %d, y: %d, "
                                          "width: %d, height: %d)",
                    egl_surface,
                    stage_egl->bounding_redraw_clip.x,
                    stage_egl->bounding_redraw_clip.y,
                    stage_egl->bounding_redraw_clip.width,
                    stage_egl->bounding_redraw_clip.height);

      copy_area.x = clip->x;
      copy_area.y = clip->y;
      copy_area.width = clip->width;
      copy_area.height = clip->height;

      CLUTTER_TIMER_START (_clutter_uprof_context, blit_sub_buffer_timer);
      _clutter_backend_egl_blit_sub_buffer (backend_egl,
                                            egl_surface,
                                            copy_area.x,
                                            copy_area.y,
                                            copy_area.width,
                                            copy_area.height);
      CLUTTER_TIMER_STOP (_clutter_uprof_context, blit_sub_buffer_timer);
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "eglwapBuffers (display: %p, surface: %p)",
                    backend_egl->edpy,
                    egl_surface);

      CLUTTER_TIMER_START (_clutter_uprof_context, swapbuffers_timer);
      eglSwapBuffers (backend_egl->edpy, egl_surface);
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

static void
_clutter_stage_egl_init (ClutterStageEGL *stage)
{
  stage->egl_surface = EGL_NO_SURFACE;
}

#else /* COGL_HAS_X11_SUPPORT */

static void
_clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
}

static void
_clutter_stage_egl_init (ClutterStageEGL *stage)
{
  /* Without X we only support one surface and that is associated
   * with the backend directly instead of the stage */
}

#endif /* COGL_HAS_X11_SUPPORT */
