#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-egl.h"
#include "clutter-stage-egl.h"
#include "../clutter-private.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"

static ClutterBackendEGL *backend_singleton = NULL;

G_DEFINE_TYPE (ClutterBackendEGL, clutter_backend_egl, CLUTTER_TYPE_BACKEND_X11);

static gboolean
clutter_backend_egl_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (clutter_backend_x11_post_parse (backend, error))
    {
      EGLBoolean status;

      backend_egl->edpy = eglGetDisplay((NativeDisplayType)backend_x11->xdpy);

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

    }

  CLUTTER_NOTE (BACKEND, "EGL Reports version %i.%i",
		backend_egl->egl_version_major,
		backend_egl->egl_version_minor);

  return TRUE;
}

static void
clutter_backend_egl_redraw (ClutterBackend *backend)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterStageEGL   *stage_egl;
  ClutterStageX11   *stage_x11;

  stage_x11 = CLUTTER_STAGE_X11(backend_x11->stage);
  stage_egl = CLUTTER_STAGE_EGL(backend_x11->stage);

  clutter_actor_paint (CLUTTER_ACTOR(stage_egl));

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

  if (backend_egl->edpy)
    {
      eglTerminate (backend_egl->edpy);
      backend_egl->edpy = NULL;
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
  /* We can actually resize too */
  return CLUTTER_FEATURE_STAGE_CURSOR;
}

static gboolean
clutter_backend_egl_init_stage (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (!backend_x11->stage)
    {
      ClutterStageX11 *stage_x11;
      ClutterActor *stage;

      stage = g_object_new (CLUTTER_TYPE_STAGE_EGL, NULL);

      /* copy backend data into the stage */
      stage_x11 = CLUTTER_STAGE_X11 (stage);
      stage_x11->xdpy = backend_x11->xdpy;
      stage_x11->xwin_root = backend_x11->xwin_root;
      stage_x11->xscreen = backend_x11->xscreen_num;
      stage_x11->backend = backend_x11;

      CLUTTER_NOTE (MISC, "X11 stage created (display:%p, screen:%d, root:%u)",
                    stage_x11->xdpy,
                    stage_x11->xscreen,
                    (unsigned int) stage_x11->xwin_root);

      g_object_set_data (G_OBJECT (stage), "clutter-backend", backend);

      backend_x11->stage = g_object_ref_sink (stage);
    }

  clutter_actor_realize (backend_x11->stage);

  if (!CLUTTER_ACTOR_IS_REALIZED (backend_x11->stage))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_INTERNAL,
                   "Unable to realize the main stage");
      return FALSE;
    }

  return TRUE;
}

static void
clutter_backend_egl_class_init (ClutterBackendEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_egl_constructor;
  gobject_class->dispose = clutter_backend_egl_dispose;
  gobject_class->finalize = clutter_backend_egl_finalize;

  backend_class->post_parse  = clutter_backend_egl_post_parse;
  backend_class->redraw      = clutter_backend_egl_redraw;
  backend_class->get_features = clutter_backend_egl_get_features;
  backend_class->init_stage = clutter_backend_egl_init_stage;
}

static void
clutter_backend_egl_init (ClutterBackendEGL *backend_egl)
{
  ;
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_egl_get_type ();
}

/**
 * clutter_egl_display
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
