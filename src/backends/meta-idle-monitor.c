/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright 2013 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Adapted from gnome-session/gnome-session/gs-idle-monitor.c and
 *         from gnome-desktop/libgnome-desktop/gnome-idle-monitor.c
 */

/**
 * SECTION:idle-monitor
 * @title: MetaIdleMonitor
 * @short_description: Mutter idle counter (similar to X's IDLETIME)
 */

#include "config.h"

#include <string.h>
#include <clutter/clutter.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include <meta/util.h>
#include <meta/main.h>
#include <meta/meta-idle-monitor.h>
#include "meta-idle-monitor-private.h"
#include "meta-idle-monitor-dbus.h"
#include "meta-backend-private.h"

G_STATIC_ASSERT(sizeof(unsigned long) == sizeof(gpointer));

enum
{
  PROP_0,
  PROP_DEVICE_ID,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (MetaIdleMonitor, meta_idle_monitor, G_TYPE_OBJECT)

void
_meta_idle_monitor_watch_fire (MetaIdleMonitorWatch *watch)
{
  MetaIdleMonitor *monitor;
  guint id;
  gboolean is_user_active_watch;

  monitor = watch->monitor;
  g_object_ref (monitor);

  if (watch->idle_source_id)
    {
      g_source_remove (watch->idle_source_id);
      watch->idle_source_id = 0;
    }

  id = watch->id;
  is_user_active_watch = (watch->timeout_msec == 0);

  if (watch->callback)
    watch->callback (monitor, id, watch->user_data);

  if (is_user_active_watch)
    meta_idle_monitor_remove_watch (monitor, id);

  g_object_unref (monitor);
}

static void
meta_idle_monitor_dispose (GObject *object)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (object);

  g_clear_pointer (&monitor->watches, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_idle_monitor_parent_class)->dispose (object);
}

static void
meta_idle_monitor_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_int (value, monitor->device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_idle_monitor_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (object);
  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      monitor->device_id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_idle_monitor_class_init (MetaIdleMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_idle_monitor_dispose;
  object_class->get_property = meta_idle_monitor_get_property;
  object_class->set_property = meta_idle_monitor_set_property;

  /**
   * MetaIdleMonitor:device_id:
   *
   * The device to listen to idletime on.
   */
  obj_props[PROP_DEVICE_ID] =
    g_param_spec_int ("device-id",
                      "Device ID",
                      "The device to listen to idletime on",
                      0, 255, 0,
                      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_DEVICE_ID, obj_props[PROP_DEVICE_ID]);
}

static void
free_watch (gpointer data)
{
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) data;
  MetaIdleMonitor *monitor = watch->monitor;

  g_object_ref (monitor);

  if (watch->idle_source_id)
    {
      g_source_remove (watch->idle_source_id);
      watch->idle_source_id = 0;
    }

  if (watch->notify != NULL)
    watch->notify (watch->user_data);

  if (watch->timeout_source != NULL)
    g_source_destroy (watch->timeout_source);

  g_object_unref (monitor);
  g_slice_free (MetaIdleMonitorWatch, watch);
}

static void
meta_idle_monitor_init (MetaIdleMonitor *monitor)
{
  monitor->watches = g_hash_table_new_full (NULL, NULL, NULL, free_watch);
  monitor->last_event_time = g_get_monotonic_time ();
}

/**
 * meta_idle_monitor_get_core:
 *
 * Returns: (transfer none): the #MetaIdleMonitor that tracks the server-global
 * idletime for all devices. To track device-specific idletime,
 * use meta_idle_monitor_get_for_device().
 */
MetaIdleMonitor *
meta_idle_monitor_get_core (void)
{
  MetaBackend *backend = meta_get_backend ();
  return meta_backend_get_idle_monitor (backend, 0);
}

/**
 * meta_idle_monitor_get_for_device:
 * @device_id: the device to get the idle time for.
 *
 * Returns: (transfer none): a new #MetaIdleMonitor that tracks the
 * device-specific idletime for @device. To track server-global idletime
 * for all devices, use meta_idle_monitor_get_core().
 */
MetaIdleMonitor *
meta_idle_monitor_get_for_device (int device_id)
{
  MetaBackend *backend = meta_get_backend ();
  return meta_backend_get_idle_monitor (backend, device_id);
}

static guint32
get_next_watch_serial (void)
{
  static guint32 serial = 0;

  g_atomic_int_inc (&serial);

  return serial;
}

static gboolean
idle_monitor_dispatch_timeout (GSource     *source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) user_data;

  _meta_idle_monitor_watch_fire (watch);
  g_source_set_ready_time (watch->timeout_source, -1);

  return TRUE;
}

static GSourceFuncs idle_monitor_source_funcs = {
  .prepare = NULL,
  .check = NULL,
  .dispatch = idle_monitor_dispatch_timeout,
  .finalize = NULL,
};

static MetaIdleMonitorWatch *
make_watch (MetaIdleMonitor           *monitor,
            guint64                    timeout_msec,
            MetaIdleMonitorWatchFunc   callback,
            gpointer                   user_data,
            GDestroyNotify             notify)
{
  MetaIdleMonitorWatch *watch;

  watch = g_slice_new0 (MetaIdleMonitorWatch);

  watch->monitor = monitor;
  watch->id = get_next_watch_serial ();
  watch->callback = callback;
  watch->user_data = user_data;
  watch->notify = notify;
  watch->timeout_msec = timeout_msec;

  if (timeout_msec != 0)
    {
      GSource *source = g_source_new (&idle_monitor_source_funcs,
                                      sizeof (GSource));

      g_source_set_callback (source, NULL, watch, NULL);
      g_source_set_ready_time (source,
                               monitor->last_event_time + timeout_msec * 1000);
      g_source_attach (source, NULL);
      g_source_unref (source);

      watch->timeout_source = source;
    }

  g_hash_table_insert (monitor->watches,
                       GUINT_TO_POINTER (watch->id),
                       watch);
  return watch;
}

/**
 * meta_idle_monitor_add_idle_watch:
 * @monitor: A #MetaIdleMonitor
 * @interval_msec: The idletime interval, in milliseconds
 * @callback: (nullable): The callback to call when the user has
 *     accumulated @interval_msec milliseconds of idle time.
 * @user_data: (nullable): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 *
 * Returns: a watch id
 *
 * Adds a watch for a specific idle time. The callback will be called
 * when the user has accumulated @interval_msec milliseconds of idle time.
 * This function will return an ID that can either be passed to
 * meta_idle_monitor_remove_watch(), or can be used to tell idle time
 * watches apart if you have more than one.
 *
 * Also note that this function will only care about positive transitions
 * (user's idle time exceeding a certain time). If you want to know about
 * when the user has become active, use
 * meta_idle_monitor_add_user_active_watch().
 */
guint
meta_idle_monitor_add_idle_watch (MetaIdleMonitor	       *monitor,
                                  guint64	                interval_msec,
                                  MetaIdleMonitorWatchFunc      callback,
                                  gpointer			user_data,
                                  GDestroyNotify		notify)
{
  MetaIdleMonitorWatch *watch;

  g_return_val_if_fail (META_IS_IDLE_MONITOR (monitor), 0);
  g_return_val_if_fail (interval_msec > 0, 0);

  watch = make_watch (monitor,
                      interval_msec,
                      callback,
                      user_data,
                      notify);

  return watch->id;
}

/**
 * meta_idle_monitor_add_user_active_watch:
 * @monitor: A #MetaIdleMonitor
 * @callback: (nullable): The callback to call when the user is
 *     active again.
 * @user_data: (nullable): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 *
 * Returns: a watch id
 *
 * Add a one-time watch to know when the user is active again.
 * Note that this watch is one-time and will de-activate after the
 * function is called, for efficiency purposes. It's most convenient
 * to call this when an idle watch, as added by
 * meta_idle_monitor_add_idle_watch(), has triggered.
 */
guint
meta_idle_monitor_add_user_active_watch (MetaIdleMonitor          *monitor,
                                         MetaIdleMonitorWatchFunc  callback,
                                         gpointer		   user_data,
                                         GDestroyNotify	           notify)
{
  MetaIdleMonitorWatch *watch;

  g_return_val_if_fail (META_IS_IDLE_MONITOR (monitor), 0);

  watch = make_watch (monitor,
                      0,
                      callback,
                      user_data,
                      notify);

  return watch->id;
}

/**
 * meta_idle_monitor_remove_watch:
 * @monitor: A #MetaIdleMonitor
 * @id: A watch ID
 *
 * Removes an idle time watcher, previously added by
 * meta_idle_monitor_add_idle_watch() or
 * meta_idle_monitor_add_user_active_watch().
 */
void
meta_idle_monitor_remove_watch (MetaIdleMonitor *monitor,
                                guint	         id)
{
  g_return_if_fail (META_IS_IDLE_MONITOR (monitor));

  g_object_ref (monitor);
  g_hash_table_remove (monitor->watches,
                       GUINT_TO_POINTER (id));
  g_object_unref (monitor);
}

/**
 * meta_idle_monitor_get_idletime:
 * @monitor: A #MetaIdleMonitor
 *
 * Returns: The current idle time, in milliseconds, or -1 for not supported
 */
gint64
meta_idle_monitor_get_idletime (MetaIdleMonitor *monitor)
{
  return (g_get_monotonic_time () - monitor->last_event_time) / 1000;
}

void
meta_idle_monitor_reset_idletime (MetaIdleMonitor *monitor)
{
  GList *node, *watch_ids;

  monitor->last_event_time = g_get_monotonic_time ();

  watch_ids = g_hash_table_get_keys (monitor->watches);

  for (node = watch_ids; node != NULL; node = node->next)
    {
      guint watch_id = GPOINTER_TO_UINT (node->data);
      MetaIdleMonitorWatch *watch;

      watch = g_hash_table_lookup (monitor->watches,
                                   GUINT_TO_POINTER (watch_id));
      if (!watch)
        continue;

      if (watch->timeout_msec == 0)
        {
          _meta_idle_monitor_watch_fire ((MetaIdleMonitorWatch *) watch);
        }
      else
        {
          g_source_set_ready_time (watch->timeout_source,
                                   monitor->last_event_time +
                                   watch->timeout_msec * 1000);
        }
    }

  g_list_free (watch_ids);
}
