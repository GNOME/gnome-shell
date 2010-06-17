#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-stage-egl.h"
#include "clutter-eglx.h"
#include "clutter-backend-egl.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"
#include "../clutter-stage.h"
#include "../clutter-stage-window.h"

#ifdef COGL_HAS_X11_SUPPORT
static ClutterStageWindowIface *clutter_stage_egl_parent_iface = NULL;
#endif

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageEGL,
                         clutter_stage_egl,
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

  CLUTTER_NOTE (BACKEND, "Unrealizing stage");

  clutter_x11_trap_x_errors ();

  if (!stage_x11->is_foreign_xwin && stage_x11->xwin != None)
    {
      XDestroyWindow (backend_x11->xdpy, stage_x11->xwin);
      stage_x11->xwin = None;
    }
  else
    stage_x11->xwin = None;

  if (stage_egl->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (clutter_eglx_display (), stage_egl->egl_surface);
      stage_egl->egl_surface = EGL_NO_SURFACE;
    }

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
  ClutterBackendX11 *backend_x11;
  EGLDisplay         edpy;

  CLUTTER_NOTE (BACKEND, "Realizing main stage");

  backend     = clutter_get_default_backend ();
  backend_egl = CLUTTER_BACKEND_EGL (backend);
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  edpy = clutter_eglx_display ();

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

  if (clutter_x11_has_event_retrieval ())
    {
      if (clutter_x11_has_xinput ())
        {
          XSelectInput (backend_x11->xdpy, stage_x11->xwin,
                        StructureNotifyMask |
                        FocusChangeMask |
                        ExposureMask |
                        EnterWindowMask | LeaveWindowMask |
                        PropertyChangeMask);
#ifdef USE_XINPUT
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
    }

  /* no user resize... */
  clutter_stage_x11_fix_window_size (stage_x11,
                                     stage_x11->xwin_width,
                                     stage_x11->xwin_height);
  clutter_stage_x11_set_wm_protocols (stage_x11);

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
}

#ifdef COGL_HAS_X11_SUPPORT
static void
clutter_stage_egl_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_stage_egl_parent_class)->dispose (gobject);
}

static void
clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_stage_egl_dispose;
}

static void
clutter_stage_egl_init (ClutterStageEGL *stage)
{
  stage->egl_surface = EGL_NO_SURFACE;
}

#else /* COGL_HAS_X11_SUPPORT */

static void
clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
}

static void
clutter_stage_egl_init (ClutterStageEGL *stage)
{
  /* Without X we only support one surface and that is associated
   * with the backend directly instead of the stage */
}

#endif /* COGL_HAS_X11_SUPPORT */

void
clutter_stage_egl_redraw (ClutterStageEGL *stage_egl,
                          ClutterStage    *stage)
{
  ClutterBackend     *backend = clutter_get_default_backend ();
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterActor       *wrapper;
  EGLSurface          egl_surface;
#ifdef COGL_HAS_X11_SUPPORT
  ClutterStageX11    *stage_x11 = CLUTTER_STAGE_X11 (stage_egl);

  wrapper = CLUTTER_ACTOR (stage_x11->wrapper);
  egl_surface = stage_egl->egl_surface;
#else
  wrapper = CLUTTER_ACTOR (stage_egl->wrapper);
  /* Without X we only support one surface and that is associated
   * with the backend directly instead of the stage */
  egl_surface = backend_egl->egl_surface;
#endif

  clutter_actor_paint (wrapper);
  cogl_flush ();

  eglSwapBuffers (backend_egl->edpy, egl_surface);
}
