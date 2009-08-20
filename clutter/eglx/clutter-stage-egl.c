#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-egl.h"
#include "clutter-stage-egl.h"
#include "clutter-eglx.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"
#include "../clutter-container.h"
#include "../clutter-stage.h"
#include "../clutter-stage-window.h"

static ClutterStageWindowIface *clutter_stage_egl_parent_iface = NULL;

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageEGL,
                         clutter_stage_egl,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

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

  if (stage_egl->egl_surface)
    {
      eglDestroySurface (clutter_eglx_display (), stage_egl->egl_surface);
      stage_egl->egl_surface = EGL_NO_SURFACE;
    }

  XSync (backend_x11->xdpy, False);

  clutter_x11_untrap_x_errors ();
}

static gboolean
_clutter_stage_egl_try_realize (ClutterStageWindow *stage_window, int *retry_cookie)
{
  ClutterStageEGL   *stage_egl = CLUTTER_STAGE_EGL (stage_window);
  ClutterStageX11   *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackend    *backend;
  ClutterBackendEGL *backend_egl;
  ClutterBackendX11 *backend_x11;
  EGLConfig          config;
  EGLint             config_count;
  EGLBoolean         status;
  int                i;
  int                num_configs;
  EGLConfig         *all_configs;
  EGLint             cfg_attribs[] = {
    /* NB: This must be the first attribute, since we may
     * try and fallback to no stencil buffer */
    EGL_STENCIL_SIZE,   8,

    EGL_RED_SIZE,       5,
    EGL_GREEN_SIZE,     6,
    EGL_BLUE_SIZE,      5,

    EGL_BUFFER_SIZE,    EGL_DONT_CARE,

#ifdef HAVE_COGL_GLES2
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else /* HAVE_COGL_GLES2 */
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
#endif /* HAVE_COGL_GLES2 */

    EGL_NONE
  };
  EGLDisplay edpy;

  /* Here we can change the attributes depending on the fallback count... */

  /* Some GLES hardware can't support a stencil buffer: */
  if (*retry_cookie == 1)
    {
      g_warning ("Trying with stencil buffer disabled...");
      cfg_attribs[1 /* EGL_STENCIL_SIZE */] = 0;
    }

  /* XXX: at this point we only have one fallback */

  backend     = clutter_get_default_backend ();
  backend_egl = CLUTTER_BACKEND_EGL (backend);
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  edpy = clutter_eglx_display ();

  eglGetConfigs (edpy, NULL, 0, &num_configs);

  all_configs = g_malloc (num_configs * sizeof (EGLConfig));
  eglGetConfigs (clutter_eglx_display (),
                 all_configs,
                 num_configs,
                 &num_configs);

  for (i = 0; i < num_configs; ++i)
    {
      EGLint red = -1, green = -1, blue = -1, alpha = -1, stencil = -1;

      eglGetConfigAttrib (edpy,
                          all_configs[i],
                          EGL_RED_SIZE, &red);
      eglGetConfigAttrib (edpy,
                          all_configs[i],
                          EGL_GREEN_SIZE, &green);
      eglGetConfigAttrib (edpy,
                          all_configs[i],
                          EGL_BLUE_SIZE, &blue);
      eglGetConfigAttrib (edpy,
                          all_configs[i],
                          EGL_ALPHA_SIZE, &alpha);
      eglGetConfigAttrib (edpy,
                          all_configs[i],
                          EGL_STENCIL_SIZE, &stencil);
      CLUTTER_NOTE (BACKEND, "EGLConfig == R:%d G:%d B:%d A:%d S:%d \n",
                    red, green, blue, alpha, stencil);
    }

  g_free (all_configs);

  status = eglChooseConfig (edpy,
                            cfg_attribs,
                            &config, 1,
                            &config_count);
  if (status != EGL_TRUE)
    {
      g_warning ("eglChooseConfig failed");
      goto fail;
    }

  if (stage_x11->xwin == None)
    stage_x11->xwin =
      XCreateSimpleWindow (backend_x11->xdpy,
                           backend_x11->xwin_root,
                           0, 0,
                           stage_x11->xwin_width,
                           stage_x11->xwin_height,
                           0, 0,
                           WhitePixel (backend_x11->xdpy,
                                       backend_x11->xscreen_num));

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

  clutter_stage_x11_fix_window_size (stage_x11, -1, -1);
  clutter_stage_x11_set_wm_protocols (stage_x11);

  if (stage_egl->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (edpy, stage_egl->egl_surface);
      stage_egl->egl_surface = EGL_NO_SURFACE;
    }

  stage_egl->egl_surface =
    eglCreateWindowSurface (edpy,
                            config,
                            (NativeWindowType) stage_x11->xwin,
                            NULL);

  if (stage_egl->egl_surface == EGL_NO_SURFACE)
    {
      g_warning ("Unable to create an EGL surface");
      goto fail;
    }

  if (G_UNLIKELY (backend_egl->egl_context == None))
    {
#ifdef HAVE_COGL_GLES2
      static const EGLint attribs[3]
        = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

      backend_egl->egl_context = eglCreateContext (edpy,
                                                   config,
                                                   EGL_NO_CONTEXT,
                                                   attribs);
#else
      /* Seems some GLES implementations 1.x do not like attribs... */
      backend_egl->egl_context = eglCreateContext (edpy,
                                                   config,
                                                   EGL_NO_CONTEXT,
                                                   NULL);
#endif
      if (backend_egl->egl_context == EGL_NO_CONTEXT)
        {
          g_warning ("Unable to create a suitable EGL context");
          goto fail;
        }

      backend_egl->egl_config = config;
      CLUTTER_NOTE (GL, "Created EGL Context");
    }

  *retry_cookie = 0;
  return TRUE;

fail:

  if (stage_egl->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (backend_egl->edpy, stage_egl->egl_surface);
      stage_egl->egl_surface = EGL_NO_SURFACE;
    }
  if (stage_x11->xwin != None)
    {
      XDestroyWindow (backend_x11->xdpy, stage_x11->xwin);
      stage_x11->xwin = None;
    }

  /* NB: We currently only support a single fallback option */
  if (*retry_cookie == 0)
    *retry_cookie = 1; /* tell the caller to try again */
  else
    *retry_cookie = 0; /* tell caller not to try again! */

  return FALSE;
}

static gboolean
clutter_stage_egl_realize (ClutterStageWindow *stage_window)
{
  int retry_cookie = 0;

  CLUTTER_NOTE (BACKEND, "Realizing main stage");

  while (1)
    {
      /* _clutter_stage_egl_try_realize supports fallbacks, and the number of
       * fallbacks already tried is tracked in the retry_cookie, so what we are
       * doing here is re-trying until we get told there are no more fallback
       * options... */
      if (_clutter_stage_egl_try_realize (stage_window, &retry_cookie))
        {
          gboolean ret = clutter_stage_egl_parent_iface->realize (stage_window);
          if (G_LIKELY (ret))
            CLUTTER_NOTE (BACKEND, "Successfully realized stage");

          return ret;
        }
      if (retry_cookie == 0)
        return FALSE; /* we've been told not to try again! */

      g_warning ("%s: Trying fallback", G_STRFUNC);
    }

  g_return_val_if_reached (FALSE);
}

static void
clutter_stage_egl_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_stage_egl_parent_class)->dispose (gobject);
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_egl_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = clutter_stage_egl_realize;
  iface->unrealize = clutter_stage_egl_unrealize;

  /* the rest is inherited from ClutterStageX11 */
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
}
