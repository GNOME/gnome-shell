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
        eglGetDisplay ((NativeDisplayType) backend_x11->xdpy);

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

static void
clutter_backend_egl_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);

  if (stage == NULL)
    {
      CLUTTER_NOTE (BACKEND, "Clearing EGL context");
      eglMakeCurrent (backend_egl->edpy,
                      EGL_NO_SURFACE,
                      EGL_NO_SURFACE,
                      EGL_NO_CONTEXT);
    }
  else
    {
      ClutterStageWindow *impl;
      ClutterStageEGL    *stage_egl;
      ClutterStageX11    *stage_x11;

      impl = _clutter_stage_get_window (stage);
      g_assert (impl != NULL);

      CLUTTER_NOTE (MULTISTAGE, "Setting context for stage of type %s [%p]",
                    g_type_name (G_OBJECT_TYPE (impl)),
                    impl);

      stage_egl = CLUTTER_STAGE_EGL (impl);
      stage_x11 = CLUTTER_STAGE_X11 (impl);

      if (!backend_egl->egl_context)
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

          eglMakeCurrent (backend_egl->edpy,
                          EGL_NO_SURFACE,
                          EGL_NO_SURFACE,
                          EGL_NO_CONTEXT);
        }
      else
        eglMakeCurrent (backend_egl->edpy,
                        stage_egl->egl_surface,
                        stage_egl->egl_surface,
                        backend_egl->egl_context);

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
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterStageEGL    *stage_egl;
  ClutterStageX11    *stage_x11;
  ClutterStageWindow *impl;

  impl = _clutter_stage_get_window (stage);
  if (!impl)
    return;

  g_assert (CLUTTER_IS_STAGE_EGL (impl));

  stage_x11 = CLUTTER_STAGE_X11 (impl);
  stage_egl = CLUTTER_STAGE_EGL (impl);

  /* this will cause the stage implementation to be painted as well */
  clutter_actor_paint (CLUTTER_ACTOR (stage));
  cogl_flush ();

  /* Why this paint is done in backend as likely GL windowing system
   * specific calls, like swapping buffers.
  */
  if (stage_x11->xwin)
    {
      /* clutter_feature_wait_for_vblank (); */
      eglSwapBuffers (backend_egl->edpy,  stage_egl->egl_surface);
    }
  else
    {
      eglWaitGL ();
      CLUTTER_GLERR ();
    }
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

  /* We can actually resize too */
  return CLUTTER_FEATURE_STAGE_CURSOR|CLUTTER_FEATURE_STAGE_MULTIPLE;
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
  
  stage = g_object_new (CLUTTER_STAGE_TYPE, NULL);
  
  /* copy backend data into the stage */
  stage_x11 = CLUTTER_STAGE_X11 (stage);
  stage_x11->xdpy = backend_x11->xdpy;
  stage_x11->xwin_root = backend_x11->xwin_root;
  stage_x11->xscreen = backend_x11->xscreen_num;
  stage_x11->backend = backend_x11;
  stage_x11->wrapper = wrapper;
  
  CLUTTER_NOTE (MISC, "EGLX stage created (display:%p, screen:%d, root:%u)",
                stage_x11->xdpy,
                stage_x11->xscreen,
                (unsigned int) stage_x11->xwin_root);
  
  return stage;
}

static XVisualInfo *
clutter_backend_egl_get_visual_info (ClutterBackendX11 *backend_x11)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend_x11);
  EGLint visualid;
  XVisualInfo visinfo_template;
  XVisualInfo *visinfo = None;
  int visinfos_count;

  eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                      EGL_NATIVE_VISUAL_ID, &visualid);

  visinfo_template.screen = backend_x11->xscreen_num;
  visinfo_template.visualid = visualid;
  visinfo = XGetVisualInfo (backend_x11->xdpy,
                            VisualScreenMask | VisualIDMask,
                            &visinfo_template,
                            &visinfos_count);
  if (!visinfo)
    return None;

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
  backendx11_class->get_visual_info = clutter_backend_egl_get_visual_info;
}

static void
clutter_backend_egl_init (ClutterBackendEGL *backend_egl)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_egl);

  clutter_backend_set_resolution (backend, 96.0);
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
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
