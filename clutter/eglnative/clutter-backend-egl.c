#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-egl.h"
#include "clutter-stage-egl.h"
#include "../clutter-private.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"

static ClutterBackendEGL *backend_singleton = NULL;


G_DEFINE_TYPE (ClutterBackendEGL, clutter_backend_egl, CLUTTER_TYPE_BACKEND);

static void
clutter_backend_at_exit (void)
{
  if (backend_singleton)
    g_object_run_dispose (G_OBJECT (backend_singleton));
}

static gboolean
clutter_backend_egl_pre_parse (ClutterBackend  *backend,
                               GError         **error)
{
  return TRUE;
}

static gboolean
clutter_backend_egl_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL(backend);
  EGLBoolean status;

  backend_egl->edpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);

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

  CLUTTER_NOTE (BACKEND, "EGL Reports version %i.%i",
		backend_egl->egl_version_major, 
		backend_egl->egl_version_minor);

  return TRUE;
}

static void
clutter_backend_egl_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
  /* not doing anything since we only have one context */
}

static void
clutter_backend_egl_redraw (ClutterBackend *backend,
                            ClutterStage   *stage)
{
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterStageEGL    *stage_egl;
  ClutterStageWindow *impl;

  impl = _clutter_stage_get_window (stage);
  if (!impl)
    return;

  g_assert (CLUTTER_IS_STAGE_EGL (impl));
  stage_egl = CLUTTER_STAGE_EGL (impl);

  eglWaitNative (EGL_CORE_NATIVE_ENGINE);
  clutter_actor_paint (CLUTTER_ACTOR (stage));
  cogl_flush ();

  eglWaitGL();
  eglSwapBuffers (backend_egl->edpy,  stage_egl->egl_surface);
}

static ClutterActor *
clutter_backend_egl_create_stage (ClutterBackend  *backend,
                                  ClutterStage    *wrapper,
                                  GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterStageEGL   *stage_egl;
  ClutterActor      *stage;

  if (backend_egl->stage)
    {
      g_warning ("The EGL native backend does not support multiple stages");
      return backend_egl->stage;
    }

  stage = g_object_new (CLUTTER_TYPE_STAGE_EGL, NULL);

  stage_egl = CLUTTER_STAGE_EGL (stage);
  stage_egl->backend = backend_egl;
  stage_egl->wrapper = wrapper;

  backend_egl->stage = CLUTTER_ACTOR (stage_egl);

  return stage;
}

static void
clutter_backend_egl_init_events (ClutterBackend *backend)
{
  _clutter_events_init (backend);
}

static const GOptionEntry entries[] =
{
  { NULL }
};

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

  _clutter_events_uninit (CLUTTER_BACKEND (backend_egl));

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

  if (backend_egl->event_timer)
    {
      g_timer_destroy (backend_egl->event_timer);
      backend_egl->event_timer = NULL;
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
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);

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

  return CLUTTER_FEATURE_STAGE_STATIC;
}

static void
clutter_backend_egl_class_init (ClutterBackendEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_egl_constructor;
  gobject_class->dispose = clutter_backend_egl_dispose;
  gobject_class->finalize = clutter_backend_egl_finalize;

  backend_class->pre_parse        = clutter_backend_egl_pre_parse;
  backend_class->post_parse       = clutter_backend_egl_post_parse;
  backend_class->init_events      = clutter_backend_egl_init_events;
  backend_class->create_stage     = clutter_backend_egl_create_stage;
  backend_class->ensure_context   = clutter_backend_egl_ensure_context;
  backend_class->redraw           = clutter_backend_egl_redraw;
  backend_class->get_features     = clutter_backend_egl_get_features;
}

static void
clutter_backend_egl_init (ClutterBackendEGL *backend_egl)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_egl);

  clutter_backend_set_resolution (backend, 96.0);
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);

  backend_egl->event_timer = g_timer_new ();
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_egl_get_type ();
}

EGLDisplay
clutter_egl_display (void)
{
  return backend_singleton->edpy;
}
