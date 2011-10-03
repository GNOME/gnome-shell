/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010,2011  Intel Corporation.
 *               2011 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "clutter-backend-eglnative.h"

/* This is a Cogl based backend */
#include "cogl/clutter-stage-cogl.h"

#ifdef HAVE_EVDEV
#include "clutter-device-manager-evdev.h"
#endif

#ifdef HAVE_TSLIB
#include "clutter-event-tslib.h"
#endif

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#ifdef COGL_HAS_EGL_SUPPORT
#include "clutter-egl.h"
#endif

#ifdef CLUTTER_EGL_BACKEND_CEX100
#include "clutter-cex100.h"
#endif

#ifdef CLUTTER_EGL_BACKEND_CEX100
static gdl_plane_id_t gdl_plane = GDL_PLANE_ID_UPP_C;
static guint gdl_n_buffers = CLUTTER_CEX100_TRIPLE_BUFFERING;
#endif

#define clutter_backend_egl_native_get_type     _clutter_backend_egl_native_get_type

G_DEFINE_TYPE (ClutterBackendEglNative, clutter_backend_egl_native, CLUTTER_TYPE_BACKEND_COGL);

static ClutterDeviceManager *
clutter_backend_egl_native_get_device_manager (ClutterBackend *backend)
{
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (backend);

  if (G_UNLIKELY (backend_egl_native->device_manager == NULL))
    {
#ifdef HAVE_EVDEV
      backend_egl_native->device_manager =
	g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_EVDEV,
		      "backend", backend_egl_native,
		      NULL);
#endif
    }

  return backend_egl_native->device_manager;
}

static void
clutter_backend_egl_native_init_events (ClutterBackend *backend)
{
  const char *input_backend = NULL;

  input_backend = g_getenv ("CLUTTER_INPUT_BACKEND");

#ifdef HAVE_EVDEV
  if (input_backend != NULL &&
      strcmp (input_backend, CLUTTER_EVDEV_INPUT_BACKEND) == 0)
    _clutter_events_evdev_init (CLUTTER_BACKEND (backend));
  else
#endif
#ifdef HAVE_TSLIB
  if (input_backend != NULL &&
      strcmp (input_backend, CLUTTER_TSLIB_INPUT_BACKEND) == 0)
    _clutter_events_tslib_init (CLUTTER_BACKEND (backend));
  else
#endif
  if (input_backend != NULL)
    g_error ("Unrecognized input backend '%s'", input_backend);
  else
    g_error ("Unknown input backend");
}

static void
clutter_backend_egl_native_dispose (GObject *gobject)
{
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (gobject);

  if (backend_egl_native->event_timer != NULL)
    {
      g_timer_destroy (backend_egl_native->event_timer);
      backend_egl_native->event_timer = NULL;
    }

#ifdef HAVE_TSLIB
  _clutter_events_tslib_uninit (CLUTTER_BACKEND (gobject));
#endif

#ifdef HAVE_EVDEV
  _clutter_events_evdev_uninit (CLUTTER_BACKEND (gobject));

  if (backend_egl_native->device_manager != NULL)
    {
      g_object_unref (backend_egl_native->device_manager);
      backend_egl_native->device_manager = NULL;
    }
#endif

  G_OBJECT_CLASS (clutter_backend_egl_native_parent_class)->dispose (gobject);
}

static ClutterStageWindow *
clutter_backend_egl_native_create_stage (ClutterBackend  *backend,
					 ClutterStage    *wrapper,
					 GError         **error)
{
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (backend);
  ClutterStageWindow *stage;

  if (G_UNLIKELY (backend_egl_native->stage != NULL))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "The EglNative backend does not support multiple "
                   "onscreen windows");
      return backend_egl_native->stage;
    }

  stage = g_object_new (CLUTTER_TYPE_STAGE_COGL,
			"backend", backend,
			"wrapper", wrapper,
			NULL);
  backend_egl_native->stage = stage;

  return stage;
}

static gboolean
clutter_backend_egl_native_create_context (ClutterBackend  *backend,
					   GError         **error)
{
  CoglSwapChain *swap_chain = NULL;
  CoglOnscreenTemplate *onscreen_template = NULL;

  if (backend->cogl_context != NULL)
    return TRUE;

  backend->cogl_renderer = cogl_renderer_new ();
  if (!cogl_renderer_connect (backend->cogl_renderer, error))
    goto error;

  swap_chain = cogl_swap_chain_new ();

#if defined(CLUTTER_EGL_BACKEND_CEX100) && defined(COGL_HAS_GDL_SUPPORT)
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

#if defined(CLUTTER_EGL_BACKEND_CEX100) && defined(COGL_HAS_GDL_SUPPORT)
  cogl_gdl_display_set_plane (backend->cogl_display, gdl_plane);
#endif /* CLUTTER_EGL_BACKEND_CEX100 */

  cogl_object_unref (backend->cogl_renderer);
  cogl_object_unref (onscreen_template);

  if (!cogl_display_setup (backend->cogl_display, error))
    goto error;

  backend->cogl_context = cogl_context_new (backend->cogl_display, error);
  if (backend->cogl_context == NULL)
    goto error;

  return TRUE;

error:
  if (backend->cogl_display != NULL)
    {
      cogl_object_unref (backend->cogl_display);
      backend->cogl_display = NULL;
    }

  if (onscreen_template != NULL)
    cogl_object_unref (onscreen_template);

  if (swap_chain != NULL)
    cogl_object_unref (swap_chain);

  if (backend->cogl_renderer != NULL)
    {
      cogl_object_unref (backend->cogl_renderer);
      backend->cogl_renderer = NULL;
    }

  return FALSE;
}

static void
clutter_backend_egl_native_class_init (ClutterBackendEglNativeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_egl_native_dispose;

  backend_class->get_device_manager = clutter_backend_egl_native_get_device_manager;
  backend_class->init_events = clutter_backend_egl_native_init_events;
  backend_class->create_stage = clutter_backend_egl_native_create_stage;
  backend_class->create_context = clutter_backend_egl_native_create_context;
}

static void
clutter_backend_egl_native_init (ClutterBackendEglNative *backend_egl_native)
{
  backend_egl_native->event_timer = g_timer_new ();
}

#ifdef CLUTTER_EGL_BACKEND_CEX100
/**
 * clutter_cex100_set_plane:
 * @plane: FIXME
 *
 * FIXME
 *
 * Since:
 */
void
clutter_cex100_set_plane (gdl_plane_id_t plane)
{
  g_return_if_fail (plane >= GDL_PLANE_ID_UPP_A && plane <= GDL_PLANE_ID_UPP_E);

  gdl_plane = plane;
}
#endif

#ifdef CLUTTER_EGL_BACKEND_CEX100
/**
 * clutter_cex100_set_plane:
 * @mode: FIXME
 *
 * FIXME
 *
 * Since:
 */
void
clutter_cex100_set_buffering_mode (ClutterCex100BufferingMode mode)
{
  g_return_if_fail (mode == CLUTTER_CEX100_DOUBLE_BUFFERING ||
                    mode == CLUTTER_CEX100_TRIPLE_BUFFERING);

  gdl_n_buffers = mode;
}
#endif

/**
 * clutter_eglx_display:
 *
 * Retrieves the EGL display used by Clutter.
 *
 * Return value: the EGL display, or 0
 *
 * Since: 0.6
 *
 * Deprecated: 1.6: Use clutter_egl_get_egl_display() instead.
 */
EGLDisplay
clutter_eglx_display (void)
{
  return clutter_egl_get_egl_display ();
}

/**
 * clutter_egl_display:
 *
 * Retrieves the EGL display used by Clutter.
 *
 * Return value: the EGL display used by Clutter, or 0
 *
 * Since: 0.6
 *
 * Deprecated: 1.6: Use clutter_egl_get_egl_display() instead.
 */
EGLDisplay
clutter_egl_display (void)
{
  return clutter_egl_get_egl_display ();
}

/**
 * clutter_egl_get_egl_display:
 *
 * Retrieves the EGL display used by Clutter, if it supports the
 * EGL windowing system and if it is running using an EGL backend.
 *
 * Return value: the EGL display used by Clutter, or 0
 *
 * Since: 1.6
 */
EGLDisplay
clutter_egl_get_egl_display (void)
{
  ClutterBackend *backend;

  if (!_clutter_context_is_initialized ())
    {
      g_critical ("The Clutter backend has not been initialized yet");
      return 0;
    }

  backend = clutter_get_default_backend ();

  if (!CLUTTER_IS_BACKEND_EGL_NATIVE (backend))
    {
      g_critical ("The Clutter backend is not an EGL backend");
      return 0;
    }

#if COGL_HAS_EGL_SUPPORT
  return cogl_egl_context_get_egl_display (backend->cogl_context);
#else
  return 0;
#endif
}
