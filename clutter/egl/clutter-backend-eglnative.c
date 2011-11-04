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

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#ifdef COGL_HAS_EGL_SUPPORT
#include "clutter-egl.h"
#endif

#define clutter_backend_egl_native_get_type     _clutter_backend_egl_native_get_type

G_DEFINE_TYPE (ClutterBackendEglNative, clutter_backend_egl_native, CLUTTER_TYPE_BACKEND);

static ClutterDeviceManager *
clutter_backend_egl_native_get_device_manager (ClutterBackend *backend)
{
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (backend);

#ifdef HAVE_EVDEV
  if (G_UNLIKELY (backend_egl_native->device_manager == NULL))
    {
      backend_egl_native->device_manager =
	g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_EVDEV,
		      "backend", backend_egl_native,
		      NULL);
    }
#endif

  return backend_egl_native->device_manager;
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

  if (backend_egl_native->device_manager != NULL)
    {
      g_object_unref (backend_egl_native->device_manager);
      backend_egl_native->device_manager = NULL;
    }

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

static void
clutter_backend_egl_native_class_init (ClutterBackendEglNativeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_egl_native_dispose;

  backend_class->get_device_manager = clutter_backend_egl_native_get_device_manager;
  backend_class->create_stage = clutter_backend_egl_native_create_stage;
}

static void
clutter_backend_egl_native_init (ClutterBackendEglNative *backend_egl_native)
{
  backend_egl_native->event_timer = g_timer_new ();
}

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
