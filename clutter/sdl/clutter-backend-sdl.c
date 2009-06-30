#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-sdl.h"
#include "clutter-stage-sdl.h"
#include "../clutter-private.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"

static ClutterBackendSDL *backend_singleton = NULL;


G_DEFINE_TYPE (ClutterBackendSDL, clutter_backend_sdl, CLUTTER_TYPE_BACKEND);

static gboolean
clutter_backend_sdl_pre_parse (ClutterBackend  *backend,
                               GError         **error)
{
  return TRUE;
}

static gboolean
clutter_backend_sdl_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  int err;

  if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Unable to Initialize SDL");
      return FALSE;
    }

#if defined(WIN32)
  err = SDL_GL_LoadLibrary ("opengl32.dll");
#elif defined(__linux__) || defined(__FreeBSD__)
  err = SDL_GL_LoadLibrary ("libGL.so");
#else
#error Your platform is not supported
  err = 1;
#endif

  if (err != 0)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "%s", SDL_GetError ());
      return FALSE;
    }

  CLUTTER_NOTE (BACKEND, "SDL successfully initialized");

  return TRUE;
}

static void
clutter_backend_sdl_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
  /* no context to ensure */
}

static void
clutter_backend_sdl_redraw (ClutterBackend *backend,
                            ClutterStage   *stage)
{
  clutter_actor_paint (CLUTTER_ACTOR (stage));
  cogl_flush ();

  SDL_GL_SwapBuffers();
}

static ClutterActor *
clutter_backend_sdl_create_stage (ClutterBackend  *backend,
                                  ClutterStage    *wrapper,
                                  GError         **error)
{
  ClutterBackendSDL *backend_sdl = CLUTTER_BACKEND_SDL (backend);
  ClutterStageSDL *stage_sdl;
  ClutterActor *stage;

  if (backend_sdl->stage)
    {
      g_warning ("The SDL backend does not support multiple stages");
      return CLUTTER_ACTOR (backend_sdl->stage);
    }

  stage = g_object_new (CLUTTER_TYPE_STAGE_SDL, NULL);

  /* copy backend data into the stage */
  stage_sdl = CLUTTER_STAGE_SDL (stage);
  stage_sdl->wrapper = wrapper;

  backend_sdl->stage = stage_sdl;

  return stage;
}

static void
clutter_backend_sdl_init_events (ClutterBackend *backend)
{
  _clutter_events_init (backend);
}

static void
clutter_backend_sdl_finalize (GObject *gobject)
{
  SDL_Quit();

  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_sdl_parent_class)->finalize (gobject);
}

static void
clutter_backend_sdl_dispose (GObject *gobject)
{
  ClutterBackendSDL *backend_sdl = CLUTTER_BACKEND_SDL (gobject);

  _clutter_events_uninit (CLUTTER_BACKEND (backend_sdl));

  if (backend_sdl->stage)
    {
      clutter_actor_destroy (CLUTTER_ACTOR (backend_sdl->stage));
      backend_sdl->stage = NULL;
    }

  if (backend_sdl->timer)
    {
      g_timer_destroy (backend_sdl->timer);
      backend_sdl->timer = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_sdl_parent_class)->dispose (gobject);
}

static GObject *
clutter_backend_sdl_constructor (GType                  gtype,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_sdl_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_SDL (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");
  
  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_sdl_get_features (ClutterBackend *backend)
{
  return CLUTTER_FEATURE_STAGE_CURSOR;
}

static void
clutter_backend_sdl_class_init (ClutterBackendSDLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_sdl_constructor;
  gobject_class->dispose = clutter_backend_sdl_dispose;
  gobject_class->finalize = clutter_backend_sdl_finalize;

  backend_class->pre_parse        = clutter_backend_sdl_pre_parse;
  backend_class->post_parse       = clutter_backend_sdl_post_parse;
  backend_class->init_events      = clutter_backend_sdl_init_events;
  backend_class->create_stage     = clutter_backend_sdl_create_stage;
  backend_class->ensure_context   = clutter_backend_sdl_ensure_context;
  backend_class->redraw           = clutter_backend_sdl_redraw;
  backend_class->get_features     = clutter_backend_sdl_get_features;
}

static void
clutter_backend_sdl_init (ClutterBackendSDL *backend_sdl)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_sdl);

  clutter_backend_set_resolution (backend, 96.0);
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);

  backend_sdl->timer = g_timer_new ();
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_sdl_get_type ();
}

