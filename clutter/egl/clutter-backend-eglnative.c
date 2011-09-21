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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static gboolean gdl_plane_set = FALSE;
static gboolean gdl_n_buffers_set = FALSE;

G_DEFINE_TYPE (ClutterBackendEglNative, _clutter_backend_egl_native, CLUTTER_TYPE_BACKEND_COGL);

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
#ifdef HAVE_TSLIB
  _clutter_events_tslib_init (CLUTTER_BACKEND_EGL (backend));
#endif

#ifdef HAVE_EVDEV
  _clutter_events_evdev_init (CLUTTER_BACKEND (backend));
#endif
}

static void
clutter_backend_cogl_dispose (GObject *gobject)
{
#ifdef HAVE_TSLIB
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (gobject);

  _clutter_events_tslib_uninit (backend_egl_native);

  if (backend_egl_native->event_timer != NULL)
    {
      g_timer_destroy (backend_egl_native->event_timer);
      backend_egl_native->event_timer = NULL;
    }
#endif

  G_OBJECT_CLASS (_clutter_backend_cogl_parent_class)->dispose (gobject);
}

static ClutterStageWindow *
clutter_backend_egl_native_create_stage (ClutterBackend  *backend,
					 ClutterStage    *wrapper,
					 GError         **error)
{
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (backend);
  ClutterStageWindow *stage;
  ClutterStageCogl *stage_cogl;

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

  if (gdl_n_buffers_set)
    cogl_swap_chain_set_length (swap_chain, gdl_n_buffers);

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

#ifdef CLUTTER_EGL_BACKEND_CEX100
  if (gdl_plane_set)
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
_clutter_backend_egl_native_class_init (ClutterBackendEglNativeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose     = clutter_backend_egl_native_dispose;
  gobject_class->finalize    = clutter_backend_egl_native_finalize;

  backend_class->get_device_manager = clutter_backend_egl_native_get_device_manager;
  backend_class->init_events        = clutter_backend_egl_native_init_events;
  backend_class->create_stage       = clutter_backend_egl_native_create_stage;
  backend_class->create_context     = clutter_backend_egl_native_create_context;
}

static void
_clutter_backend_egl_native_init (ClutterBackendEglNative *backend_egl_native)
{
#ifdef HAVE_TSLIB
  backend_egl_native->event_timer = g_timer_new ();
#endif
}

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
#ifdef CLUTTER_EGL_BACKEND_CEX100
  g_return_if_fail (plane >= GDL_PLANE_ID_UPP_A && plane <= GDL_PLANE_ID_UPP_E);

  gdl_plane = plane;
  gdl_plane_set = TRUE;
#endif
}

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
#ifdef CLUTTER_EGL_BACKEND_CEX100
  g_return_if_fail (mode == CLUTTER_CEX100_DOUBLE_BUFFERING ||
                    mode == CLUTTER_CEX100_TRIPLE_BUFFERING);

  gdl_n_buffers = mode;
  gdl_n_buffers_set = TRUE;
#endif
}
