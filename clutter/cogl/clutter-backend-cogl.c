/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010,2011  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.

 * Authors:
 *  Matthew Allum
 *  Emmanuele Bassi
 *  Robert Bragg
 *  Neil Roberts
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "clutter-backend-cogl.h"
#include "clutter-stage-cogl.h"

#ifdef HAVE_EVDEV
#include "clutter-device-manager-evdev.h"
#endif

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#ifdef COGL_HAS_EGL_SUPPORT
#include "clutter-egl.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
#include "clutter-cex100.h"
#endif

static ClutterBackendCogl *backend_singleton = NULL;

static gchar *clutter_vblank = NULL;

/* FIXME: We should have CLUTTER_ define for this... */
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
static gdl_plane_id_t gdl_plane = GDL_PLANE_ID_UPP_C;
static guint gdl_n_buffers = CLUTTER_CEX100_TRIPLE_BUFFERING;
#endif

#ifdef COGL_HAS_X11_SUPPORT
G_DEFINE_TYPE (ClutterBackendCogl, _clutter_backend_cogl, CLUTTER_TYPE_BACKEND_X11);
#else
G_DEFINE_TYPE (ClutterBackendCogl, _clutter_backend_cogl, CLUTTER_TYPE_BACKEND);
#endif

static void
clutter_backend_at_exit (void)
{
  if (backend_singleton)
    g_object_run_dispose (G_OBJECT (backend_singleton));
}

const gchar*
_clutter_backend_cogl_get_vblank (void)
{
  if (clutter_vblank && strcmp (clutter_vblank, "0") == 0)
    return "none";
  else
    return clutter_vblank;
}

static gboolean
clutter_backend_cogl_pre_parse (ClutterBackend  *backend,
                                GError         **error)
{
  const gchar *env_string;
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendClass *parent_class =
    CLUTTER_BACKEND_CLASS (_clutter_backend_cogl_parent_class);

  if (!parent_class->pre_parse (backend, error))
    return FALSE;
#endif

  env_string = g_getenv ("CLUTTER_VBLANK");
  if (env_string)
    {
      clutter_vblank = g_strdup (env_string);
      env_string = NULL;
    }

  return TRUE;
}

static gboolean
clutter_backend_cogl_post_parse (ClutterBackend  *backend,
                                 GError         **error)
{
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendClass *parent_class =
    CLUTTER_BACKEND_CLASS (_clutter_backend_cogl_parent_class);

  if (!parent_class->post_parse (backend, error))
    return FALSE;

  return TRUE;
#endif

  g_atexit (clutter_backend_at_exit);

  return TRUE;
}

#ifndef COGL_HAS_XLIB_SUPPORT
static ClutterDeviceManager *
clutter_backend_cogl_get_device_manager (ClutterBackend *backend)
{
  ClutterBackendCogl *backend_cogl = CLUTTER_BACKEND_COGL (backend);

  if (G_UNLIKELY (backend_cogl->device_manager == NULL))
    {
#ifdef HAVE_EVDEV
      backend_cogl->device_manager =
	g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_EVDEV,
		      "backend", backend_cogl,
		      NULL);
#endif
    }

  return backend_cogl->device_manager;
}
#endif

static void
clutter_backend_cogl_init_events (ClutterBackend *backend)
{
#ifdef HAVE_TSLIB
  /* XXX: This should be renamed to _clutter_events_tslib_init */
  _clutter_events_tslib_init (CLUTTER_BACKEND_COGL (backend));
#endif
#ifdef HAVE_EVDEV
  _clutter_events_evdev_init (CLUTTER_BACKEND (backend));
#endif
#ifdef COGL_HAS_X11_SUPPORT
  /* Chain up to the X11 backend */
  CLUTTER_BACKEND_CLASS (_clutter_backend_cogl_parent_class)->
    init_events (backend);
#endif
}

static void
clutter_backend_cogl_finalize (GObject *gobject)
{
  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (_clutter_backend_cogl_parent_class)->finalize (gobject);
}

static void
clutter_backend_cogl_dispose (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);
#ifdef HAVE_TSLIB
  ClutterBackendCogl *backend_cogl = CLUTTER_BACKEND_COGL (gobject);
#endif

  /* We chain up before disposing our CoglContext so that we will
   * destroy all of the stages first. Otherwise the actors may try to
   * make Cogl calls during destruction which would cause a crash */
  G_OBJECT_CLASS (_clutter_backend_cogl_parent_class)->dispose (gobject);

  if (backend->cogl_context)
    {
      cogl_object_unref (backend->cogl_context);
      backend->cogl_context = NULL;
    }

#ifdef HAVE_TSLIB
  /* XXX: This should be renamed to _clutter_events_tslib_uninit */
  _clutter_events_egl_uninit (backend_cogl);

  if (backend_cogl->event_timer != NULL)
    {
      g_timer_destroy (backend_cogl->event_timer);
      backend_cogl->event_timer = NULL;
    }
#endif
}

static GObject *
clutter_backend_cogl_constructor (GType                  gtype,
                                  guint                  n_params,
                                  GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (_clutter_backend_cogl_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_COGL (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_cogl_get_features (ClutterBackend *backend)
{
  ClutterBackendCogl *backend_cogl = CLUTTER_BACKEND_COGL (backend);
#ifdef COGL_HAS_XLIB_SUPPORT
  ClutterBackendClass *parent_class;
#endif
  ClutterFeatureFlags flags = 0;

#ifdef COGL_HAS_XLIB_SUPPORT
  parent_class = CLUTTER_BACKEND_CLASS (_clutter_backend_cogl_parent_class);

  flags = parent_class->get_features (backend);
#endif

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports multiple onscreen framebuffers");
      flags |= CLUTTER_FEATURE_STAGE_MULTIPLE;
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "Cogl only supports one onscreen framebuffer");
      flags |= CLUTTER_FEATURE_STAGE_STATIC;
    }

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_THROTTLE))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swap buffers throttling");
      flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
    }
  else
    CLUTTER_NOTE (BACKEND, "Cogl doesn't support swap buffers throttling");

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swap buffers complete events");
      flags |= CLUTTER_FEATURE_SWAP_EVENTS;
    }

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swapping buffer regions");
      backend_cogl->can_blit_sub_buffer = TRUE;
    }

  return flags;
}

#ifdef COGL_HAS_XLIB_SUPPORT
static XVisualInfo *
clutter_backend_cogl_get_visual_info (ClutterBackendX11 *backend_x11)
{
  return cogl_clutter_winsys_xlib_get_visual_info ();
}
#endif

static gboolean
clutter_backend_cogl_create_context (ClutterBackend  *backend,
                                     GError         **error)
{
#ifdef COGL_HAS_XLIB_SUPPORT
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
#endif
  CoglSwapChain *swap_chain = NULL;
  CoglOnscreenTemplate *onscreen_template = NULL;

  if (backend->cogl_context)
    return TRUE;

  backend->cogl_renderer = cogl_renderer_new ();
#ifdef COGL_HAS_XLIB_SUPPORT
  cogl_xlib_renderer_set_foreign_display (backend->cogl_renderer,
                                          backend_x11->xdpy);
#endif
  if (!cogl_renderer_connect (backend->cogl_renderer, error))
    goto error;

  swap_chain = cogl_swap_chain_new ();
#ifdef COGL_HAS_XLIB_SUPPORT
  cogl_swap_chain_set_has_alpha (swap_chain,
                                 clutter_x11_get_use_argb_visual ());
#endif

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  cogl_swap_chain_set_length (swap_chain, gdl_n_buffers);
#endif

  onscreen_template = cogl_onscreen_template_new (swap_chain);
  cogl_object_unref (swap_chain);

  /* XXX: I have some doubts that this is a good design.
   * Conceptually should we be able to check an onscreen_template
   * without more details about the CoglDisplay configuration?
   */
  if (!cogl_renderer_check_onscreen_template (backend->cogl_renderer,
                                              onscreen_template,
                                              error))
    goto error;

  backend->cogl_display = cogl_display_new (backend->cogl_renderer,
                                            onscreen_template);

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  cogl_gdl_display_set_plane (backend->cogl_display, gdl_plane);
#endif

  cogl_object_unref (backend->cogl_renderer);
  cogl_object_unref (onscreen_template);

  if (!cogl_display_setup (backend->cogl_display, error))
    goto error;

  backend->cogl_context = cogl_context_new (backend->cogl_display, error);
  if (!backend->cogl_context)
    goto error;

  return TRUE;

error:
  if (backend->cogl_display)
    {
      cogl_object_unref (backend->cogl_display);
      backend->cogl_display = NULL;
    }

  if (onscreen_template)
    cogl_object_unref (onscreen_template);
  if (swap_chain)
    cogl_object_unref (swap_chain);

  if (backend->cogl_renderer)
    {
      cogl_object_unref (backend->cogl_renderer);
      backend->cogl_renderer = NULL;
    }
  return FALSE;
}

static ClutterStageWindow *
clutter_backend_cogl_create_stage (ClutterBackend  *backend,
                                   ClutterStage    *wrapper,
                                   GError         **error)
{
#ifdef COGL_HAS_XLIB_SUPPORT
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterEventTranslator *translator;
  ClutterStageWindow *stage;
  ClutterStageX11 *stage_x11;

  stage = g_object_new (CLUTTER_TYPE_STAGE_COGL, NULL);

  /* copy backend data into the stage */
  stage_x11 = CLUTTER_STAGE_X11 (stage);
  stage_x11->wrapper = wrapper;
  stage_x11->backend = backend_x11;

  translator = CLUTTER_EVENT_TRANSLATOR (stage_x11);
  _clutter_backend_add_event_translator (backend, translator);

  CLUTTER_NOTE (MISC, "Cogl stage created (display:%p, screen:%d, root:%u)",
                backend_x11->xdpy,
                backend_x11->xscreen_num,
                (unsigned int) backend_x11->xwin_root);

#else /* COGL_HAS_XLIB_SUPPORT */

  ClutterBackendCogl *backend_cogl = CLUTTER_BACKEND_COGL (backend);
  ClutterStageWindow *stage;
  ClutterStageCogl *stage_cogl;

  if (G_UNLIKELY (backend_cogl->stage != NULL))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "The Cogl backend does not support multiple "
                   "onscreen windows");
      return backend_cogl->stage;
    }

  stage = g_object_new (CLUTTER_TYPE_STAGE_COGL, NULL);

  stage_cogl = CLUTTER_STAGE_COGL (stage);
  stage_cogl->backend = backend_cogl;
  stage_cogl->wrapper = wrapper;

  backend_cogl->stage = stage;

#endif /* COGL_HAS_XLIB_SUPPORT */

  return stage;
}

static void
clutter_backend_cogl_ensure_context (ClutterBackend *backend,
                                     ClutterStage   *stage)
{
  ClutterStageCogl *stage_cogl;

  /* ignore ensuring the context on an empty stage */
  if (stage == NULL)
    return;

  stage_cogl =
    CLUTTER_STAGE_COGL (_clutter_stage_get_window (stage));

  cogl_set_framebuffer (COGL_FRAMEBUFFER (stage_cogl->onscreen));
}

static void
_clutter_backend_cogl_class_init (ClutterBackendCoglClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendX11Class *backendx11_class = CLUTTER_BACKEND_X11_CLASS (klass);
#endif

  gobject_class->constructor = clutter_backend_cogl_constructor;
  gobject_class->dispose     = clutter_backend_cogl_dispose;
  gobject_class->finalize    = clutter_backend_cogl_finalize;

  backend_class->pre_parse          = clutter_backend_cogl_pre_parse;
  backend_class->post_parse         = clutter_backend_cogl_post_parse;
  backend_class->get_features       = clutter_backend_cogl_get_features;
#ifndef COGL_HAS_XLIB_SUPPORT
  backend_class->get_device_manager = clutter_backend_cogl_get_device_manager;
#endif
  backend_class->init_events        = clutter_backend_cogl_init_events;
  backend_class->create_stage       = clutter_backend_cogl_create_stage;
  backend_class->create_context     = clutter_backend_cogl_create_context;
  backend_class->ensure_context     = clutter_backend_cogl_ensure_context;

#ifdef COGL_HAS_XLIB_SUPPORT
  backendx11_class->get_visual_info = clutter_backend_cogl_get_visual_info;
#endif
}

static void
_clutter_backend_cogl_init (ClutterBackendCogl *backend_cogl)
{
#ifdef HAVE_TSLIB
  backend_cogl->event_timer = g_timer_new ();
#endif
}

GType
_clutter_backend_impl_get_type (void)
{
  return _clutter_backend_cogl_get_type ();
}

#ifdef COGL_HAS_EGL_SUPPORT
EGLDisplay
clutter_eglx_display (void)
{
  return clutter_egl_get_egl_display ();
}

EGLDisplay
clutter_egl_display (void)
{
  return clutter_egl_get_egl_display ();
}

EGLDisplay
clutter_egl_get_egl_display (void)
{
  if (backend_singleton == NULL)
    {
      g_critical ("%s has been called before clutter_init()", G_STRFUNC);
      return 0;
    }

  return cogl_egl_context_get_egl_display (backend_singleton->cogl_context);
}
#endif

/* FIXME we should have a CLUTTER_ define for this */
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
void
clutter_cex100_set_plane (gdl_plane_id_t plane)
{
  g_return_if_fail (plane >= GDL_PLANE_ID_UPP_A && plane <= GDL_PLANE_ID_UPP_E);

  gdl_plane = plane;
}

void
clutter_cex100_set_buffering_mode (ClutterCex100BufferingMode mode)
{
  g_return_if_fail (mode == CLUTTER_CEX100_DOUBLE_BUFFERING ||
                    mode == CLUTTER_CEX100_TRIPLE_BUFFERING);

  gdl_n_buffers = mode;
}
#endif
