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
#include "evdev/clutter-device-manager-evdev.h"
#endif

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#ifdef COGL_HAS_EGL_SUPPORT
#include "clutter-egl.h"
#endif

#include "clutter-stage-eglnative.h"

#define clutter_backend_egl_native_get_type     _clutter_backend_egl_native_get_type

G_DEFINE_TYPE (ClutterBackendEglNative, clutter_backend_egl_native, CLUTTER_TYPE_BACKEND);

#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
static int _kms_fd = -1;
#endif

static void
clutter_backend_egl_native_dispose (GObject *gobject)
{
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (gobject);

  if (backend_egl_native->event_timer != NULL)
    {
      g_timer_destroy (backend_egl_native->event_timer);
      backend_egl_native->event_timer = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_egl_native_parent_class)->dispose (gobject);
}

static CoglRenderer *
clutter_backend_egl_native_get_renderer (ClutterBackend  *backend,
                                         GError         **error)
{
  CoglRenderer *renderer;

  renderer = cogl_renderer_new ();

#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
  if (_kms_fd > -1)
    {
      cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_EGL_KMS);
      cogl_kms_renderer_set_kms_fd (renderer, _kms_fd);
    }
#endif

  return renderer;
}

static void
clutter_backend_egl_native_class_init (ClutterBackendEglNativeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_egl_native_dispose;

  backend_class->stage_window_type = CLUTTER_TYPE_STAGE_EGL_NATIVE;

  backend_class->get_renderer = clutter_backend_egl_native_get_renderer;
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

#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
/**
 * clutter_egl_set_kms_fd:
 * @fd: The fd to talk to the kms driver with
 *
 * Sets the fd that Cogl should use to talk to the kms driver.
 * Setting this to a negative value effectively reverts this
 * call, making Cogl open the device itself.
 *
 * This can only be called before clutter_init() is called.
 *
 * Since: 1.18
 */
void
clutter_egl_set_kms_fd (int fd)
{
  _kms_fd = fd;
}
#endif

/**
 * clutter_egl_freeze_master_clock:
 *
 * Freezing the master clock makes Clutter stop processing events,
 * redrawing, and advancing timelines. This is necessary when implementing
 * a display server, to ensure that Clutter doesn't keep trying to page
 * flip when DRM master has been dropped, e.g. when VT switched away.
 *
 * The master clock starts out running, so if you are VT switched away on
 * startup, you need to call this immediately.
 *
 * If you're also using the evdev backend, make sure to also use
 * clutter_evdev_release_devices() to make sure that Clutter doesn't also
 * access revoked evdev devices when VT switched away.
 *
 * To unthaw a frozen master clock, use clutter_egl_thaw_master_clock().
 *
 * Since: 1.20
 */
void
clutter_egl_freeze_master_clock (void)
{
  ClutterMasterClock *master_clock;

  g_return_if_fail (CLUTTER_IS_BACKEND_EGL_NATIVE (clutter_get_default_backend ()));

  master_clock = _clutter_master_clock_get_default ();
  _clutter_master_clock_set_paused (master_clock, TRUE);
}

/**
 * clutter_egl_thaw_master_clock:
 *
 * Thaws a master clock that has previously been frozen with
 * clutter_egl_freeze_master_clock(), and start pumping the master clock
 * again at the next iteration. Note that if you're switching back to your
 * own VT, you should probably also queue a stage redraw with
 * clutter_stage_ensure_redraw().
 *
 * Since: 1.20
 */
void
clutter_egl_thaw_master_clock (void)
{
  ClutterMasterClock *master_clock;

  g_return_if_fail (CLUTTER_IS_BACKEND_EGL_NATIVE (clutter_get_default_backend ()));

  master_clock = _clutter_master_clock_get_default ();
  _clutter_master_clock_set_paused (master_clock, FALSE);

  _clutter_master_clock_start_running (master_clock);
}
