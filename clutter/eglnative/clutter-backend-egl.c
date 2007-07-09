#include "config.h"

#include "clutter-backend-egl.h"
#include "clutter-stage-egl.h"
#include "../clutter-private.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"

static ClutterBackendEGL *backend_singleton = NULL;


G_DEFINE_TYPE (ClutterBackendEGL, clutter_backend_egl, CLUTTER_TYPE_BACKEND);

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

  backend_egl->edpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  status = eglInitialize(backend_egl->edpy, 
			 &backend_egl->egl_version_major, 
			 &backend_egl->egl_version_minor);
  
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

static gboolean
clutter_backend_egl_init_stage (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);

  if (!backend_egl->stage)
    {
      ClutterActor *stage;

      stage = g_object_new (CLUTTER_TYPE_STAGE_EGL, NULL);

      g_object_set_data (G_OBJECT (stage), "clutter-backend", backend);

      backend_egl->stage = g_object_ref_sink (stage);
    }

  clutter_actor_realize (backend_egl->stage);
  if (!CLUTTER_ACTOR_IS_REALIZED (backend_egl->stage))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_INTERNAL,
                   "Unable to realize the main stage");
      return FALSE;
    }

  return TRUE;
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
clutter_backend_egl_redraw (ClutterBackend *backend)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterStageEGL   *stage_egl;

  stage_egl = CLUTTER_STAGE_EGL(backend_egl->stage);

  clutter_actor_paint (CLUTTER_ACTOR(stage_egl));

  /* Why this paint is done in backend as likely GL windowing system
   * specific calls, like swapping buffers.
  */
  /* clutter_feature_wait_for_vblank (); */
  eglSwapBuffers (backend_egl->edpy,  stage_egl->egl_surface);
}

static ClutterActor *
clutter_backend_egl_get_stage (ClutterBackend *backend)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);

  return backend_egl->stage;
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

  _clutter_events_uninit (CLUTTER_BACKEND (backend_egl));

  if (backend_egl->stage)
    {
      g_object_unref (backend_egl->stage);
      backend_egl->stage = NULL;
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

static void
clutter_backend_egl_class_init (ClutterBackendEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_egl_constructor;
  gobject_class->dispose = clutter_backend_egl_dispose;
  gobject_class->finalize = clutter_backend_egl_finalize;

  backend_class->pre_parse   = clutter_backend_egl_pre_parse;
  backend_class->post_parse  = clutter_backend_egl_post_parse;
  backend_class->init_stage  = clutter_backend_egl_init_stage;
  backend_class->init_events = clutter_backend_egl_init_events;
  backend_class->get_stage   = clutter_backend_egl_get_stage;
  backend_class->redraw      = clutter_backend_egl_redraw;
}

static void
clutter_backend_egl_init (ClutterBackendEGL *backend_egl)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_egl);

  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
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
