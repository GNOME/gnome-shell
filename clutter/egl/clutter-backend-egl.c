#include "config.h"

#include "clutter-backend-egl.h"
#include "clutter-stage-egl.h"
#include "../clutter-private.h"
#include "../clutter-main.h"

static ClutterBackendEgl *backend_singleton = NULL;

G_DEFINE_TYPE (ClutterBackendEgl, clutter_backend_egl, CLUTTER_TYPE_BACKEND);

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
  return TRUE;
}

static gboolean
clutter_backend_egl_init_stage (ClutterBackend  *backend,
                                GError         **error)
{
  return TRUE;
}

static void
clutter_backend_egl_init_events (ClutterBackend *backend)
{

}

static void
clutter_backend_egl_add_options (ClutterBackend *backend,
                                 GOptionGroup   *group)
{

}

static ClutterActor *
clutter_backend_egl_get_stage (ClutterBackend *backend)
{
  return NULL;
}

static void
clutter_backend_egl_class_init (ClutterBackendEglClass *klass)
{
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  backend_class->pre_parse = clutter_backend_egl_pre_parse;
  backend_class->post_parse = clutter_backend_egl_post_parse;
  backend_class->init_stage = clutter_backend_egl_init_stage;
  backend_class->init_events = clutter_backend_egl_init_events;
  backend_class->get_stage = clutter_backend_egl_get_stage;
  backend_class->add_options = clutter_backend_egl_add_options;
}

static void
clutter_backend_egl_init (ClutterBackendEgl *backend)
{

}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_egl_get_type ();
}
