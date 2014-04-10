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

#include "meta-idle-monitor-xsync.h"
#include "meta-idle-monitor-private.h"

#include <meta/util.h>
#include "display-private.h"

#include <string.h>

struct _MetaIdleMonitorXSync
{
  MetaIdleMonitor parent;

  GHashTable  *alarms;
  Display     *display;
  XSyncCounter counter;
  XSyncAlarm   user_active_alarm;
};

struct _MetaIdleMonitorXSyncClass
{
  MetaIdleMonitorClass parent_class;
};

typedef struct {
  MetaIdleMonitorWatch base;

  XSyncAlarm xalarm;
} MetaIdleMonitorWatchXSync;

G_DEFINE_TYPE (MetaIdleMonitorXSync, meta_idle_monitor_xsync, META_TYPE_IDLE_MONITOR)

static gint64
_xsyncvalue_to_int64 (XSyncValue value)
{
  return ((guint64) XSyncValueHigh32 (value)) << 32
    | (guint64) XSyncValueLow32 (value);
}

#define GUINT64_TO_XSYNCVALUE(value, ret) XSyncIntsToValue (ret, (value) & 0xFFFFFFFF, ((guint64)(value)) >> 32)

static XSyncAlarm
_xsync_alarm_set (MetaIdleMonitorXSync	*monitor_xsync,
		  XSyncTestType          test_type,
		  guint64                interval,
		  gboolean               want_events)
{
  XSyncAlarmAttributes attr;
  XSyncValue	     delta;
  guint		     flags;

  flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType |
    XSyncCAValue | XSyncCADelta | XSyncCAEvents;

  XSyncIntToValue (&delta, 0);
  attr.trigger.counter = monitor_xsync->counter;
  attr.trigger.value_type = XSyncAbsolute;
  attr.delta = delta;
  attr.events = want_events;

  GUINT64_TO_XSYNCVALUE (interval, &attr.trigger.wait_value);
  attr.trigger.test_type = test_type;
  return XSyncCreateAlarm (monitor_xsync->display, flags, &attr);
}

static void
ensure_alarm_rescheduled (Display    *dpy,
			  XSyncAlarm  alarm)
{
  XSyncAlarmAttributes attr;

  /* Some versions of Xorg have an issue where alarms aren't
   * always rescheduled. Calling XSyncChangeAlarm, even
   * without any attributes, will reschedule the alarm. */
  XSyncChangeAlarm (dpy, alarm, 0, &attr);
}

static void
set_alarm_enabled (Display    *dpy,
		   XSyncAlarm  alarm,
		   gboolean    enabled)
{
  XSyncAlarmAttributes attr;
  attr.events = enabled;
  XSyncChangeAlarm (dpy, alarm, XSyncCAEvents, &attr);
}

static void
check_x11_watch (gpointer data,
                 gpointer user_data)
{
  MetaIdleMonitorWatchXSync *watch_xsync = data;
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) watch_xsync;
  XSyncAlarm alarm = (XSyncAlarm) user_data;

  if (watch_xsync->xalarm != alarm)
    return;

  _meta_idle_monitor_watch_fire (watch);
}

static char *
counter_name_for_device (int device_id)
{
  if (device_id > 0)
    return g_strdup_printf ("DEVICEIDLETIME %d", device_id);

  return g_strdup ("IDLETIME");
}

static XSyncCounter
find_idletime_counter (MetaIdleMonitorXSync *monitor_xsync)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (monitor_xsync);
  int		      i;
  int		      ncounters;
  XSyncSystemCounter *counters;
  XSyncCounter        counter = None;
  char               *counter_name;

  counter_name = counter_name_for_device (monitor->device_id);
  counters = XSyncListSystemCounters (monitor_xsync->display, &ncounters);
  for (i = 0; i < ncounters; i++)
    {
      if (counters[i].name != NULL && strcmp (counters[i].name, counter_name) == 0)
        {
          counter = counters[i].counter;
          break;
        }
    }
  XSyncFreeSystemCounterList (counters);
  g_free (counter_name);

  return counter;
}

static void
init_xsync (MetaIdleMonitorXSync *monitor_xsync)
{
  monitor_xsync->counter = find_idletime_counter (monitor_xsync);
  /* IDLETIME counter not found? */
  if (monitor_xsync->counter == None)
    {
      g_warning ("IDLETIME counter not found\n");
      return;
    }

  monitor_xsync->user_active_alarm = _xsync_alarm_set (monitor_xsync, XSyncNegativeTransition, 1, FALSE);
}

static void
meta_idle_monitor_xsync_dispose (GObject *object)
{
  MetaIdleMonitorXSync *monitor_xsync = META_IDLE_MONITOR_XSYNC (object);

  if (monitor_xsync->user_active_alarm != None)
    {
      XSyncDestroyAlarm (monitor_xsync->display, monitor_xsync->user_active_alarm);
      monitor_xsync->user_active_alarm = None;
    }

  g_clear_pointer (&monitor_xsync->alarms, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_idle_monitor_xsync_parent_class)->dispose (object);
}

static void
meta_idle_monitor_xsync_constructed (GObject *object)
{
  MetaIdleMonitorXSync *monitor_xsync = META_IDLE_MONITOR_XSYNC (object);

  g_assert (!meta_is_wayland_compositor ());

  monitor_xsync->display = meta_get_display ()->xdisplay;
  init_xsync (monitor_xsync);

  G_OBJECT_CLASS (meta_idle_monitor_xsync_parent_class)->constructed (object);
}

static gint64
meta_idle_monitor_xsync_get_idletime (MetaIdleMonitor *monitor)
{
  MetaIdleMonitorXSync *monitor_xsync = META_IDLE_MONITOR_XSYNC (monitor);
  XSyncValue value;

  if (!XSyncQueryCounter (monitor_xsync->display, monitor_xsync->counter, &value))
    return -1;

  return _xsyncvalue_to_int64 (value);
}

static gboolean
fire_watch_idle (gpointer data)
{
  MetaIdleMonitorWatch *watch = data;

  watch->idle_source_id = 0;
  _meta_idle_monitor_watch_fire (watch);

  return FALSE;
}

static guint32
get_next_watch_serial (void)
{
  static guint32 serial = 0;
  g_atomic_int_inc (&serial);
  return serial;
}

static void
free_watch (gpointer data)
{
  MetaIdleMonitorWatchXSync *watch_xsync = data;
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) watch_xsync;
  MetaIdleMonitor *monitor = watch->monitor;
  MetaIdleMonitorXSync *monitor_xsync = META_IDLE_MONITOR_XSYNC (monitor);

  g_object_ref (monitor);

  if (watch->idle_source_id)
    {
      g_source_remove (watch->idle_source_id);
      watch->idle_source_id = 0;
    }

  if (watch->notify != NULL)
    watch->notify (watch->user_data);

  if (watch_xsync->xalarm != monitor_xsync->user_active_alarm &&
      watch_xsync->xalarm != None)
    {
      XSyncDestroyAlarm (monitor_xsync->display, watch_xsync->xalarm);
      g_hash_table_remove (monitor_xsync->alarms, (gpointer) watch_xsync->xalarm);
    }

  g_object_unref (monitor);
  g_slice_free (MetaIdleMonitorWatchXSync, watch_xsync);
}

static MetaIdleMonitorWatch *
meta_idle_monitor_xsync_make_watch (MetaIdleMonitor           *monitor,
                                    guint64                    timeout_msec,
                                    MetaIdleMonitorWatchFunc   callback,
                                    gpointer                   user_data,
                                    GDestroyNotify             notify)
{
  MetaIdleMonitorXSync *monitor_xsync = META_IDLE_MONITOR_XSYNC (monitor);
  MetaIdleMonitorWatchXSync *watch_xsync;
  MetaIdleMonitorWatch *watch;

  watch_xsync = g_slice_new0 (MetaIdleMonitorWatchXSync);
  watch = (MetaIdleMonitorWatch *) watch_xsync;

  watch->monitor = monitor;
  watch->id = get_next_watch_serial ();
  watch->callback = callback;
  watch->user_data = user_data;
  watch->notify = notify;
  watch->timeout_msec = timeout_msec;

  if (monitor_xsync->user_active_alarm != None)
    {
      if (timeout_msec != 0)
        {
          watch_xsync->xalarm = _xsync_alarm_set (monitor_xsync, XSyncPositiveTransition, timeout_msec, TRUE);

          g_hash_table_add (monitor_xsync->alarms, (gpointer) watch_xsync->xalarm);

          if (meta_idle_monitor_get_idletime (monitor) > (gint64)timeout_msec)
            {
              watch->idle_source_id = g_idle_add (fire_watch_idle, watch);
              g_source_set_name_by_id (watch->idle_source_id, "[mutter] fire_watch_idle");
            }
        }
      else
        {
          watch_xsync->xalarm = monitor_xsync->user_active_alarm;

          set_alarm_enabled (monitor_xsync->display, monitor_xsync->user_active_alarm, TRUE);
        }
    }

  return watch;
}

static void
meta_idle_monitor_xsync_class_init (MetaIdleMonitorXSyncClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaIdleMonitorClass *idle_monitor_class = META_IDLE_MONITOR_CLASS (klass);

  object_class->dispose = meta_idle_monitor_xsync_dispose;
  object_class->constructed = meta_idle_monitor_xsync_constructed;

  idle_monitor_class->get_idletime = meta_idle_monitor_xsync_get_idletime;
  idle_monitor_class->make_watch = meta_idle_monitor_xsync_make_watch;
}

static void
meta_idle_monitor_xsync_init (MetaIdleMonitorXSync *monitor_xsync)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (monitor_xsync);

  monitor->watches = g_hash_table_new_full (NULL, NULL, NULL, free_watch);
  monitor_xsync->alarms = g_hash_table_new (NULL, NULL);
}

void
meta_idle_monitor_xsync_handle_xevent (MetaIdleMonitor       *monitor,
                                       XSyncAlarmNotifyEvent *alarm_event)
{
  MetaIdleMonitorXSync *monitor_xsync = META_IDLE_MONITOR_XSYNC (monitor);
  XSyncAlarm alarm;
  GList *watches;
  gboolean has_alarm;

  if (alarm_event->state != XSyncAlarmActive)
    return;

  alarm = alarm_event->alarm;

  has_alarm = FALSE;

  if (alarm == monitor_xsync->user_active_alarm)
    {
      set_alarm_enabled (monitor_xsync->display,
                         alarm,
                         FALSE);
      has_alarm = TRUE;
    }
  else if (g_hash_table_contains (monitor_xsync->alarms, (gpointer) alarm))
    {
      ensure_alarm_rescheduled (monitor_xsync->display,
                                alarm);
      has_alarm = TRUE;
    }

  if (has_alarm)
    {
      watches = g_hash_table_get_values (monitor->watches);

      g_list_foreach (watches, check_x11_watch, (gpointer) alarm);
      g_list_free (watches);
    }
}
