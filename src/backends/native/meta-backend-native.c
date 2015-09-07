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

#include "meta-backend-native.h"
#include "meta-backend-native-private.h"

#include <meta/main.h>
#include <clutter/evdev/clutter-evdev.h>
#include <libupower-glib/upower.h>

#include "meta-barrier-native.h"
#include "meta-idle-monitor-native.h"
#include "meta-monitor-manager-kms.h"
#include "meta-cursor-renderer-native.h"
#include "meta-launcher.h"

struct _MetaBackendNativePrivate
{
  MetaLauncher *launcher;
  MetaBarrierManagerNative *barrier_manager;
  UpClient *up_client;
  guint sleep_signal_id;
  GCancellable *cancellable;
  GDBusConnection *system_bus;
};
typedef struct _MetaBackendNativePrivate MetaBackendNativePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaBackendNative, meta_backend_native, META_TYPE_BACKEND);

static void
meta_backend_native_finalize (GObject *object)
{
  MetaBackendNative *native = META_BACKEND_NATIVE (object);
  MetaBackendNativePrivate *priv = meta_backend_native_get_instance_private (native);

  meta_launcher_free (priv->launcher);

  g_object_unref (priv->up_client);
  if (priv->sleep_signal_id)
    g_dbus_connection_signal_unsubscribe (priv->system_bus, priv->sleep_signal_id);
  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->system_bus);

  G_OBJECT_CLASS (meta_backend_native_parent_class)->finalize (object);
}

static void
prepare_for_sleep_cb (GDBusConnection *connection,
                      const gchar     *sender_name,
                      const gchar     *object_path,
                      const gchar     *interface_name,
                      const gchar     *signal_name,
                      GVariant        *parameters,
                      gpointer         user_data)
{
  gboolean suspending;
  g_variant_get (parameters, "(b)", &suspending);
  if (suspending)
    return;
  meta_idle_monitor_native_reset_idletime (meta_idle_monitor_get_core ());
}

static void
system_bus_gotten_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  MetaBackendNativePrivate *priv;
  GDBusConnection *bus;

  bus = g_bus_get_finish (res, NULL);
  if (!bus)
    return;

  priv = meta_backend_native_get_instance_private (META_BACKEND_NATIVE (user_data));
  priv->system_bus = bus;
  priv->sleep_signal_id = g_dbus_connection_signal_subscribe (priv->system_bus,
                                                              "org.freedesktop.login1",
                                                              "org.freedesktop.login1.Manager",
                                                              "PrepareForSleep",
                                                              "/org/freedesktop/login1",
                                                              NULL,
                                                              G_DBUS_SIGNAL_FLAGS_NONE,
                                                              prepare_for_sleep_cb,
                                                              NULL,
                                                              NULL);
}

static void
lid_is_closed_changed_cb (UpClient   *client,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  if (up_client_get_lid_is_closed (client))
    return;

  meta_idle_monitor_native_reset_idletime (meta_idle_monitor_get_core ());
}

static void
constrain_to_barriers (ClutterInputDevice *device,
                       guint32             time,
                       float              *new_x,
                       float              *new_y)
{
  MetaBackendNative *native = META_BACKEND_NATIVE (meta_get_backend ());
  MetaBackendNativePrivate *priv =
    meta_backend_native_get_instance_private (native);

  meta_barrier_manager_native_process (priv->barrier_manager,
                                       device,
                                       time,
                                       new_x, new_y);
}

/*
 * The pointer constrain code is mostly a rip-off of the XRandR code from Xorg.
 * (from xserver/randr/rrcrtc.c, RRConstrainCursorHarder)
 *
 * Copyright © 2006 Keith Packard
 * Copyright 2010 Red Hat, Inc
 *
 */

static void
constrain_all_screen_monitors (ClutterInputDevice *device,
			       MetaMonitorInfo    *monitors,
			       unsigned            n_monitors,
			       float              *x,
			       float              *y)
{
  ClutterPoint current;
  unsigned int i;
  float cx, cy;

  clutter_input_device_get_coords (device, NULL, &current);

  cx = current.x;
  cy = current.y;

  /* if we're trying to escape, clamp to the CRTC we're coming from */
  for (i = 0; i < n_monitors; i++)
    {
      MetaMonitorInfo *monitor = &monitors[i];
      int left, right, top, bottom;

      left = monitor->rect.x;
      right = left + monitor->rect.width;
      top = monitor->rect.y;
      bottom = top + monitor->rect.height;

      if ((cx >= left) && (cx < right) && (cy >= top) && (cy < bottom))
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

  /* Constrain to barriers */
  constrain_to_barriers (device, time, new_x, new_y);

  monitor_manager = meta_monitor_manager_get ();
  monitors = meta_monitor_manager_get_monitor_infos (monitor_manager, &n_monitors);

  /* if we're moving inside a monitor, we're fine */
  if (meta_monitor_manager_get_monitor_at_point (monitor_manager, *new_x, *new_y) >= 0)
    return;

  /* if we're trying to escape, clamp to the CRTC we're coming from */
  constrain_all_screen_monitors(device, monitors, n_monitors, new_x, new_y);
}

static void
meta_backend_native_post_init (MetaBackend *backend)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();

  META_BACKEND_CLASS (meta_backend_native_parent_class)->post_init (backend);

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
meta_backend_native_warp_pointer (MetaBackend *backend,
                                  int          x,
                                  int          y)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  ClutterInputDevice *device = clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);

  /* XXX */
  guint32 time_ = 0;

  clutter_evdev_warp_pointer (device, time_, x, y);
}

static void
meta_backend_native_set_keymap (MetaBackend *backend,
                                const char  *layouts,
                                const char  *variants,
                                const char  *options)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  struct xkb_rule_names names;
  struct xkb_keymap *keymap;
  struct xkb_context *context;

  names.rules = DEFAULT_XKB_RULES_FILE;
  names.model = DEFAULT_XKB_MODEL;
  names.layout = layouts;
  names.variant = variants;
  names.options = options;

  context = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
  keymap = xkb_keymap_new_from_names (context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  xkb_context_unref (context);

  clutter_evdev_set_keyboard_map (manager, keymap);

  g_signal_emit_by_name (backend, "keymap-changed", 0);

  xkb_keymap_unref (keymap);
}

static struct xkb_keymap *
meta_backend_native_get_keymap (MetaBackend *backend)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  return clutter_evdev_get_keyboard_map (manager);
}

static void
meta_backend_native_lock_layout_group (MetaBackend *backend,
                                       guint        idx)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  clutter_evdev_set_keyboard_layout_index (manager, idx);
  g_signal_emit_by_name (backend, "keymap-layout-group-changed", idx, 0);
}

static void
meta_backend_native_class_init (MetaBackendNativeClass *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_backend_native_finalize;

  backend_class->post_init = meta_backend_native_post_init;
  backend_class->create_idle_monitor = meta_backend_native_create_idle_monitor;
  backend_class->create_monitor_manager = meta_backend_native_create_monitor_manager;
  backend_class->create_cursor_renderer = meta_backend_native_create_cursor_renderer;

  backend_class->warp_pointer = meta_backend_native_warp_pointer;
  backend_class->set_keymap = meta_backend_native_set_keymap;
  backend_class->get_keymap = meta_backend_native_get_keymap;
  backend_class->lock_layout_group = meta_backend_native_lock_layout_group;
}

static void
meta_backend_native_init (MetaBackendNative *native)
{
  MetaBackendNativePrivate *priv = meta_backend_native_get_instance_private (native);

  priv->launcher = meta_launcher_new ();
  priv->barrier_manager = meta_barrier_manager_native_new ();

  priv->up_client = up_client_new ();
  g_signal_connect (priv->up_client, "notify::lid-is-closed",
                    G_CALLBACK (lid_is_closed_changed_cb), NULL);

  priv->cancellable = g_cancellable_new ();
  g_bus_get (G_BUS_TYPE_SYSTEM,
             priv->cancellable,
             system_bus_gotten_cb,
             native);
}

gboolean
meta_activate_vt (int vt, GError **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaBackendNative *native = META_BACKEND_NATIVE (backend);
  MetaBackendNativePrivate *priv = meta_backend_native_get_instance_private (native);

  return meta_launcher_activate_vt (priv->launcher, vt, error);
}

MetaBarrierManagerNative *
meta_backend_native_get_barrier_manager (MetaBackendNative *native)
{
  MetaBackendNativePrivate *priv =
    meta_backend_native_get_instance_private (native);

  return priv->barrier_manager;
}

/**
 * meta_activate_session:
 *
 * Tells mutter to activate the session. When mutter is a
 * display server, this tells logind to switch over to
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

  if (!meta_launcher_activate_session (priv->launcher, &error))
    {
      g_warning ("Could not activate session: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}
