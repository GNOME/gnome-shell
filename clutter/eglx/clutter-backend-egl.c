#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-egl.h"
#include "clutter-stage-egl.h"
#include "../clutter-private.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"
#include "../clutter-version.h"

static ClutterBackendEGL *backend_singleton = NULL;

G_DEFINE_TYPE (ClutterBackendEGL, clutter_backend_egl, CLUTTER_TYPE_BACKEND_X11);

static void
clutter_backend_at_exit (void)
{
  if (backend_singleton)
    g_object_run_dispose (G_OBJECT (backend_singleton));
}

static gboolean
clutter_backend_egl_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (clutter_backend_x11_post_parse (backend, error))
    {
      EGLBoolean status;

      backend_egl->edpy =
        eglGetDisplay ((EGLNativeDisplayType) backend_x11->xdpy);

      status = eglInitialize (backend_egl->edpy,
			      &backend_egl->egl_version_major,
			      &backend_egl->egl_version_minor);

      g_atexit (clutter_backend_at_exit);

      if (status != EGL_TRUE)
	{
	  g_set_error (error, CLUTTER_INIT_ERROR,
		       CLUTTER_INIT_ERROR_BACKEND,
		       "Unable to Initialize EGL");
	  return FALSE;
	}

    }

  CLUTTER_NOTE (BACKEND, "EGL Reports version %i.%i",
		backend_egl->egl_version_major,
		backend_egl->egl_version_minor);

  return TRUE;
}

static gboolean
clutter_backend_egl_create_context (ClutterBackend  *backend,
                                    GError         **error)
{
  ClutterBackendEGL *backend_egl;
  ClutterBackendX11 *backend_x11;
  EGLConfig          config;
  EGLint             config_count = 0;
  EGLBoolean         status;
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
  gint retry_cookie = 0;
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;

  backend     = clutter_get_default_backend ();
  backend_egl = CLUTTER_BACKEND_EGL (backend);

  if (backend_egl->egl_context != EGL_NO_CONTEXT)
    return TRUE;

  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  edpy = clutter_eglx_display ();

retry:
  /* Here we can change the attributes depending on the fallback count... */

  /* Some GLES hardware can't support a stencil buffer: */
  if (retry_cookie == 1)
    {
      g_warning ("Trying with stencil buffer disabled...");
      cfg_attribs[1 /* EGL_STENCIL_SIZE */] = 0;
    }

  /* XXX: at this point we only have one fallback */

  status = eglChooseConfig (edpy,
                            cfg_attribs,
                            &config, 1,
                            &config_count);
  if (status != EGL_TRUE || config_count == 0)
    {
      g_warning ("eglChooseConfig failed");
      goto fail;
    }

  if (G_UNLIKELY (backend_egl->egl_context == EGL_NO_CONTEXT))
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

  /* COGL assumes that there is always a GL context selected; in order
   * to make sure that an EGL context exists and is made current, we use
   * a dummy, offscreen override-redirect window to which we can always
   * fall back if no stage is available */

  xvisinfo = clutter_backend_x11_get_visual_info (backend_x11);
  if (xvisinfo == NULL)
    {
      g_critical ("Unable to find suitable GL visual.");
      return FALSE;
    }

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (backend_x11->xdpy,
                                    backend_x11->xwin_root,
                                    xvisinfo->visual,
                                    AllocNone);
  attrs.border_pixel = 0;

  backend_egl->dummy_xwin = XCreateWindow (backend_x11->xdpy,
                                           backend_x11->xwin_root,
                                           -100, -100, 1, 1,
                                           0,
                                           xvisinfo->depth,
                                           CopyFromParent,
                                           xvisinfo->visual,
                                           CWOverrideRedirect |
                                           CWColormap |
                                           CWBorderPixel,
                                           &attrs);

  XFree (xvisinfo);

  backend_egl->dummy_surface =
    eglCreateWindowSurface (edpy,
                            backend_egl->egl_config,
                            (EGLNativeWindowType) backend_egl->dummy_xwin,
                            NULL);

  if (backend_egl->dummy_surface == EGL_NO_SURFACE)
    {
      g_critical ("Unable to create an EGL surface");
      return FALSE;
    }

  eglMakeCurrent (edpy,
                  backend_egl->dummy_surface,
                  backend_egl->dummy_surface,
                  backend_egl->egl_context);

  return TRUE;

fail:

  /* NB: We currently only support a single fallback option */
  if (retry_cookie == 0)
    {
      retry_cookie = 1;
      goto retry;
    }

  return FALSE;
}

static void
clutter_backend_egl_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterStageWindow *impl;

  if (stage == NULL ||
      (CLUTTER_PRIVATE_FLAGS (stage) & CLUTTER_ACTOR_IN_DESTRUCTION) ||
      ((impl = _clutter_stage_get_window (stage)) == NULL))
    {
      CLUTTER_NOTE (BACKEND, "Clearing EGL context");
      eglMakeCurrent (backend_egl->edpy,
                      EGL_NO_SURFACE,
                      EGL_NO_SURFACE,
                      EGL_NO_CONTEXT);
    }
  else
    {
      ClutterStageEGL    *stage_egl;
      ClutterStageX11    *stage_x11;

      g_assert (impl != NULL);

      CLUTTER_NOTE (MULTISTAGE, "Setting context for stage of type %s [%p]",
                    g_type_name (G_OBJECT_TYPE (impl)),
                    impl);

      stage_egl = CLUTTER_STAGE_EGL (impl);
      stage_x11 = CLUTTER_STAGE_X11 (impl);

      if (backend_egl->egl_context == EGL_NO_CONTEXT)
        return;

      clutter_x11_trap_x_errors ();

      /* we might get here inside the final dispose cycle, so we
       * need to handle this gracefully
       */
      if (stage_x11->xwin == None ||
          stage_egl->egl_surface == EGL_NO_SURFACE)
        {
          CLUTTER_NOTE (MULTISTAGE,
                        "Received a stale stage, clearing all context");

          if (backend_egl->dummy_surface == EGL_NO_SURFACE)
            eglMakeCurrent (backend_egl->edpy,
                            EGL_NO_SURFACE,
                            EGL_NO_SURFACE,
                            EGL_NO_CONTEXT);
          else
            eglMakeCurrent (backend_egl->edpy,
                            backend_egl->dummy_surface,
                            backend_egl->dummy_surface,
                            backend_egl->egl_context);
        }
      else
        {
          CLUTTER_NOTE (MULTISTAGE, "Setting real surface current");
          eglMakeCurrent (backend_egl->edpy,
                          stage_egl->egl_surface,
                          stage_egl->egl_surface,
                          backend_egl->egl_context);
        }

      if (clutter_x11_untrap_x_errors ())
        g_critical ("Unable to make the stage window 0x%x the current "
                    "EGLX drawable",
                    (int) stage_x11->xwin);
    }
}

static void
clutter_backend_egl_redraw (ClutterBackend *backend,
                            ClutterStage   *stage)
{
  ClutterStageWindow *impl;

  impl = _clutter_stage_get_window (stage);
  if (!impl)
    return;

  g_assert (CLUTTER_IS_STAGE_EGL (impl));

  clutter_stage_egl_redraw (CLUTTER_STAGE_EGL (impl), stage);
}

static void
clutter_backend_egl_finalize (GObject *gobject)
{
  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_egl_parent_class)->finalize (gobject);
}

static void
clutter_backend_egl_dispose (GObject *gobject)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (gobject);
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (gobject);

  if (backend_egl->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (backend_egl->edpy, backend_egl->dummy_surface);
      backend_egl->dummy_surface = EGL_NO_SURFACE;
    }

  if (backend_egl->dummy_xwin)
    {
      XDestroyWindow (backend_x11->xdpy, backend_egl->dummy_xwin);
      backend_egl->dummy_xwin = None;
    }

  if (backend_egl->egl_context)
    {
      eglDestroyContext (backend_egl->edpy, backend_egl->egl_context);
      backend_egl->egl_context = NULL;
    }

  if (backend_egl->edpy)
    {
      eglTerminate (backend_egl->edpy);
      backend_egl->edpy = 0;
    }

  G_OBJECT_CLASS (clutter_backend_egl_parent_class)->dispose (gobject);
}

static GObject *
clutter_backend_egl_constructor (GType                  gtype,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_egl_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_EGL (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_egl_get_features (ClutterBackend *backend)
{
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterFeatureFlags flags;

  flags = clutter_backend_x11_get_features (backend);
  flags |= CLUTTER_FEATURE_STAGE_MULTIPLE;

  CLUTTER_NOTE (BACKEND, "Checking features\n"
                "GL_VENDOR: %s\n"
                "GL_RENDERER: %s\n"
                "GL_VERSION: %s\n"
                "EGL_VENDOR: %s\n"
                "EGL_VERSION: %s\n"
                "EGL_EXTENSIONS: %s\n",
                glGetString (GL_VENDOR),
                glGetString (GL_RENDERER),
                glGetString (GL_VERSION),
                eglQueryString (backend_egl->edpy, EGL_VENDOR),
                eglQueryString (backend_egl->edpy, EGL_VERSION),
                eglQueryString (backend_egl->edpy, EGL_EXTENSIONS));

  return flags;
}

static ClutterStageWindow *
clutter_backend_egl_create_stage (ClutterBackend  *backend,
                                  ClutterStage    *wrapper,
                                  GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterStageX11 *stage_x11;
  ClutterStageWindow *stage;

  CLUTTER_NOTE (BACKEND, "Creating stage of type '%s'",
                g_type_name (CLUTTER_STAGE_TYPE));
  
  stage = g_object_new (CLUTTER_TYPE_STAGE_EGL, NULL);
  
  /* copy backend data into the stage */
  stage_x11 = CLUTTER_STAGE_X11 (stage);
  stage_x11->wrapper = wrapper;
  
  CLUTTER_NOTE (MISC, "EGLX stage created (display:%p, screen:%d, root:%u)",
                backend_x11->xdpy,
                backend_x11->xscreen_num,
                (unsigned int) backend_x11->xwin_root);
  
  return stage;
}

static XVisualInfo *
clutter_backend_egl_get_visual_info (ClutterBackendX11 *backend_x11)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend_x11);
  XVisualInfo visinfo_template;
  int template_mask = 0;
  XVisualInfo *visinfo = NULL;
  int visinfos_count;
  EGLint visualid, red_size, green_size, blue_size, alpha_size;

  if (!clutter_backend_egl_create_context (CLUTTER_BACKEND (backend_x11), NULL))
    return NULL;

  visinfo_template.screen = backend_x11->xscreen_num;
  template_mask |= VisualScreenMask;

  eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                      EGL_NATIVE_VISUAL_ID, &visualid);

  if (visualid != 0)
    {
      visinfo_template.visualid = visualid;
      template_mask |= VisualIDMask;
    }
  else
    {
      /* some EGL drivers don't implement the EGL_NATIVE_VISUAL_ID
       * attribute, so attempt to find the closest match. */

      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_RED_SIZE, &red_size);
      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_GREEN_SIZE, &green_size);
      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_BLUE_SIZE, &blue_size);
      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_ALPHA_SIZE, &alpha_size);

      visinfo_template.depth = red_size + green_size + blue_size + alpha_size;
      template_mask |= VisualDepthMask;
    }

  visinfo = XGetVisualInfo (backend_x11->xdpy,
                            template_mask,
                            &visinfo_template,
                            &visinfos_count);

  return visinfo;
}

static void
clutter_backend_egl_class_init (ClutterBackendEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);
  ClutterBackendX11Class *backendx11_class = CLUTTER_BACKEND_X11_CLASS (klass);

  gobject_class->constructor = clutter_backend_egl_constructor;
  gobject_class->dispose     = clutter_backend_egl_dispose;
  gobject_class->finalize    = clutter_backend_egl_finalize;

  backend_class->post_parse     = clutter_backend_egl_post_parse;
  backend_class->redraw         = clutter_backend_egl_redraw;
  backend_class->get_features   = clutter_backend_egl_get_features;
  backend_class->create_stage   = clutter_backend_egl_create_stage;
  backend_class->ensure_context = clutter_backend_egl_ensure_context;
  backend_class->create_context = clutter_backend_egl_create_context;
  backendx11_class->get_visual_info = clutter_backend_egl_get_visual_info;
}

static void
clutter_backend_egl_init (ClutterBackendEGL *backend_egl)
{
  backend_egl->egl_context = EGL_NO_CONTEXT;
  backend_egl->dummy_surface = EGL_NO_SURFACE;
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_egl_get_type ();
}

/**
 * clutter_eglx_display:
 *
 * Gets the current EGLDisplay.
 *
 * Return value: an EGLDisplay
 *
 * Since: 0.4
 */
EGLDisplay
clutter_eglx_display (void)
{
  return backend_singleton->edpy;
}
