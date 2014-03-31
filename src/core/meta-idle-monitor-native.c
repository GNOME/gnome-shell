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

#include "config.h"

#include "meta-idle-monitor-native.h"
#include "meta-idle-monitor-private.h"

#include <meta/util.h>
#include "display-private.h"

#include <string.h>

struct _MetaIdleMonitorNative
{
  MetaIdleMonitor parent;
};

struct _MetaIdleMonitorNativeClass
{
  MetaIdleMonitorClass parent_class;
};

typedef struct {
  MetaIdleMonitorWatch base;

  GSource *timeout_source;
} MetaIdleMonitorWatchNative;

G_DEFINE_TYPE (MetaIdleMonitorNative, meta_idle_monitor_native, META_TYPE_IDLE_MONITOR)

static gint64
meta_idle_monitor_native_get_idletime (MetaIdleMonitor *monitor)
{
  return (g_get_monotonic_time () - monitor->last_event_time) / 1000;
}

static guint32
get_next_watch_serial (void)
{
  static guint32 serial = 0;
  g_atomic_int_inc (&serial);
  return serial;
}

static gboolean
native_dispatch_timeout (GSource     *source,
                         GSourceFunc  callback,
                         gpointer     user_data)
{
  MetaIdleMonitorWatchNative *watch_native = user_data;
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) watch;

  _meta_idle_monitor_watch_fire (watch);
  g_source_set_ready_time (watch_native->timeout_source, -1);
  return TRUE;
}

static GSourceFuncs native_source_funcs = {
  NULL, /* prepare */
  NULL, /* check */
  native_dispatch_timeout,
  NULL, /* finalize */
};

static void
free_watch (gpointer data)
{
  MetaIdleMonitorWatchNative *watch_native = data;
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) watch_native;
  MetaIdleMonitor *monitor = watch->monitor;

  g_object_ref (monitor);

  if (watch->idle_source_id)
    {
      g_source_remove (watch->idle_source_id);
      watch->idle_source_id = 0;
    }

  if (watch->notify != NULL)
    watch->notify (watch->user_data);

  if (watch_native->timeout_source != NULL)
    g_source_destroy (watch_native->timeout_source);

  g_object_unref (monitor);
  g_slice_free (MetaIdleMonitorWatchNative, watch_native);
}

static MetaIdleMonitorWatch *
meta_idle_monitor_native_make_watch (MetaIdleMonitor           *monitor,
                                    guint64                    timeout_msec,
                                    MetaIdleMonitorWatchFunc   callback,
                                    gpointer                   user_data,
                                    GDestroyNotify             notify)
{
  MetaIdleMonitorWatchNative *watch_native;
  MetaIdleMonitorWatch *watch;

  watch_native = g_slice_new0 (MetaIdleMonitorWatchNative);
  watch = (MetaIdleMonitorWatch *) watch_native;

  watch = g_slice_new0 (MetaIdleMonitorWatch);
  watch->monitor = monitor;
  watch->id = get_next_watch_serial ();
  watch->callback = callback;
  watch->user_data = user_data;
  watch->notify = notify;
  watch->timeout_msec = timeout_msec;

  if (timeout_msec != 0)
    {
      GSource *source = g_source_new (&native_source_funcs, sizeof (GSource));

      g_source_set_callback (source, NULL, watch, NULL);
      g_source_set_ready_time (source, monitor->last_event_time + timeout_msec * 1000);
      g_source_attach (source, NULL);
      g_source_unref (source);

      watch_native->timeout_source = source;
    }

  return watch;
}

static void
meta_idle_monitor_native_class_init (MetaIdleMonitorNativeClass *klass)
{
  MetaIdleMonitorClass *idle_monitor_class = META_IDLE_MONITOR_CLASS (klass);

  idle_monitor_class->get_idletime = meta_idle_monitor_native_get_idletime;
  idle_monitor_class->make_watch = meta_idle_monitor_native_make_watch;
}

static void
meta_idle_monitor_native_init (MetaIdleMonitorNative *monitor_native)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (monitor_native);

  monitor->watches = g_hash_table_new_full (NULL, NULL, NULL, free_watch);
}

typedef struct {
  MetaIdleMonitor *monitor;
  GList *fired_watches;
} CheckNativeClosure;

static gboolean
check_native_watch (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
  MetaIdleMonitorWatchNative *watch_native = value;
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) watch_native;
  CheckNativeClosure *closure = user_data;
  gboolean steal;

  if (watch->timeout_msec == 0)
    {
      closure->fired_watches = g_list_prepend (closure->fired_watches, watch);
      steal = TRUE;
    }
  else
    {
      g_source_set_ready_time (watch_native->timeout_source,
                               closure->monitor->last_event_time +
                               watch->timeout_msec * 1000);
      steal = FALSE;
    }

  return steal;
}

static void
fire_native_watch (gpointer watch,
                   gpointer data)
{
  _meta_idle_monitor_watch_fire (watch);
}

void
meta_idle_monitor_native_reset_idletime (MetaIdleMonitor *monitor)
{
  CheckNativeClosure closure;

  monitor->last_event_time = g_get_monotonic_time ();

  closure.monitor = monitor;
  closure.fired_watches = NULL;
  g_hash_table_foreach_steal (monitor->watches, check_native_watch, &closure);

  g_list_foreach (closure.fired_watches, fire_native_watch, NULL);
  g_list_free (closure.fired_watches);
}
