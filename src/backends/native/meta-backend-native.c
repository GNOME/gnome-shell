/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include <meta/main.h>
#include <clutter/evdev/clutter-evdev.h>
#include "meta-backend-native.h"

#include "meta-idle-monitor-native.h"
#include "meta-monitor-manager-kms.h"
#include "meta-cursor-renderer-native.h"
#include "meta-launcher.h"

struct _MetaBackendNativePrivate
{
  MetaLauncher *launcher;
};
typedef struct _MetaBackendNativePrivate MetaBackendNativePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaBackendNative, meta_backend_native, META_TYPE_BACKEND);

/*
 * The pointer constrain code is mostly a rip-off of the XRandR code from Xorg.
 * (from xserver/randr/rrcrtc.c, RRConstrainCursorHarder)
 *
 * Copyright © 2006 Keith Packard
 * Copyright 2010 Red Hat, Inc
 *
 */

static gboolean
check_all_screen_monitors(MetaMonitorInfo *monitors,
			  unsigned         n_monitors,
			  float            x,
			  float            y)
{
  unsigned int i;

  for (i = 0; i < n_monitors; i++)
    {
      MetaMonitorInfo *monitor = &monitors[i];
      int left, right, top, bottom;

      left = monitor->rect.x;
      right = left + monitor->rect.width;
      top = monitor->rect.y;
      bottom = left + monitor->rect.height;

      if ((x >= left) && (x < right) && (y >= top) && (y < bottom))
	return TRUE;
    }

  return FALSE;
}

static void
constrain_all_screen_monitors (ClutterInputDevice *device,
			       MetaMonitorInfo    *monitors,
			       unsigned            n_monitors,
			       float              *x,
			       float              *y)
{
  ClutterPoint current;
  unsigned int i;

  clutter_input_device_get_coords (device, NULL, &current);

  /* if we're trying to escape, clamp to the CRTC we're coming from */
  for (i = 0; i < n_monitors; i++)
    {
      MetaMonitorInfo *monitor = &monitors[i];
      int left, right, top, bottom;
      float nx, ny;

      left = monitor->rect.x;
      right = left + monitor->rect.width;
      top = monitor->rect.y;
      bottom = left + monitor->rect.height;

      nx = current.x;
      ny = current.y;

      if ((nx >= left) && (nx < right) && (ny >= top) && (ny < bottom))
	{
	  if (*x < left)
	    *x = left;
	  if (*x >= right)
	    *x = right - 1;
	  if (*y < top)
	    *y = top;
	  if (*y >= bottom)
	    *y = bottom - 1;

	  return;
        }
    }
}

static void
pointer_constrain_callback (ClutterInputDevice *device,
			    guint32             time,
			    float              *new_x,
			    float              *new_y,
			    gpointer            user_data)
{
  MetaMonitorManager *monitor_manager;
  MetaMonitorInfo *monitors;
  unsigned int n_monitors;
  gboolean ret;

  monitor_manager = meta_monitor_manager_get ();
  monitors = meta_monitor_manager_get_monitor_infos (monitor_manager, &n_monitors);

  /* if we're moving inside a monitor, we're fine */
  ret = check_all_screen_monitors(monitors, n_monitors, *new_x, *new_y);
  if (ret == TRUE)
    return;

  /* if we're trying to escape, clamp to the CRTC we're coming from */
  constrain_all_screen_monitors(device, monitors, n_monitors, new_x, new_y);
}

static void
meta_backend_native_post_init (MetaBackend *backend)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  clutter_evdev_set_pointer_constrain_callback (manager, pointer_constrain_callback,
                                                NULL, NULL);
}

static MetaIdleMonitor *
meta_backend_native_create_idle_monitor (MetaBackend *backend,
                                         int          device_id)
{
  return g_object_new (META_TYPE_IDLE_MONITOR_NATIVE,
                       "device-id", device_id,
                       NULL);
}

static MetaMonitorManager *
meta_backend_native_create_monitor_manager (MetaBackend *backend)
{
  return g_object_new (META_TYPE_MONITOR_MANAGER_KMS, NULL);
}

static MetaCursorRenderer *
meta_backend_native_create_cursor_renderer (MetaBackend *backend)
{
  return g_object_new (META_TYPE_CURSOR_RENDERER_NATIVE, NULL);
}

static void
meta_backend_native_class_init (MetaBackendNativeClass *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);

  backend_class->post_init = meta_backend_native_post_init;
  backend_class->create_idle_monitor = meta_backend_native_create_idle_monitor;
  backend_class->create_monitor_manager = meta_backend_native_create_monitor_manager;
  backend_class->create_cursor_renderer = meta_backend_native_create_cursor_renderer;
}

static void
meta_backend_native_init (MetaBackendNative *native)
{
  MetaBackendNativePrivate *priv = meta_backend_native_get_instance_private (native);

  /* We're a display server, so start talking to weston-launch. */
  priv->launcher = meta_launcher_new ();
}

gboolean
meta_activate_vt (int vt, GError **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaBackendNative *native = META_BACKEND_NATIVE (backend);
  MetaBackendNativePrivate *priv = meta_backend_native_get_instance_private (native);

  return meta_launcher_activate_vt (priv->launcher, vt, error);
}

/**
 * meta_activate_session:
 *
 * Tells mutter to activate the session. When mutter is a
 * Wayland compositor, this tells logind to switch over to
 * the new session.
 */
gboolean
meta_activate_session (void)
{
  GError *error = NULL;
  MetaBackend *backend = meta_get_backend ();

  /* Do nothing. */
  if (!META_IS_BACKEND_NATIVE (backend))
    return TRUE;

  MetaBackendNative *native = META_BACKEND_NATIVE (backend);
  MetaBackendNativePrivate *priv = meta_backend_native_get_instance_private (native);

  if (!meta_launcher_activate_vt (priv->launcher, -1, &error))
    {
      g_warning ("Could not activate session: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}
