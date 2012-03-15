/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Adapted from gnome-session/gnome-session/gs-idle-monitor.c
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"

#include <time.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include "shell-idle-monitor.h"

#define SHELL_IDLE_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SHELL_TYPE_IDLE_MONITOR, ShellIdleMonitorPrivate))

struct ShellIdleMonitorPrivate
{
        Display     *display;

        GHashTable  *watches;
        int          sync_event_base;
        XSyncCounter counter;
};

typedef struct
{
        Display                  *display;
        guint                     id;
        XSyncValue                interval;
        ShellIdleMonitorWatchFunc callback;
        gpointer                  user_data;
        GDestroyNotify            notify;
        XSyncAlarm                xalarm_positive;
        XSyncAlarm                xalarm_negative;
} ShellIdleMonitorWatch;

static guint32 watch_serial = 1;

G_DEFINE_TYPE (ShellIdleMonitor, shell_idle_monitor, G_TYPE_OBJECT)

static gint64
_xsyncvalue_to_int64 (XSyncValue value)
{
        return ((guint64) XSyncValueHigh32 (value)) << 32
                | (guint64) XSyncValueLow32 (value);
}

static XSyncValue
_int64_to_xsyncvalue (gint64 value)
{
        XSyncValue ret;

        XSyncIntsToValue (&ret, value, ((guint64)value) >> 32);

        return ret;
}

static void
shell_idle_monitor_dispose (GObject *object)
{
        ShellIdleMonitor *monitor;

        monitor = SHELL_IDLE_MONITOR (object);

        if (monitor->priv->watches != NULL) {
                g_hash_table_destroy (monitor->priv->watches);
                monitor->priv->watches = NULL;
        }

        G_OBJECT_CLASS (shell_idle_monitor_parent_class)->dispose (object);
}

static gboolean
_find_alarm (gpointer               key,
             ShellIdleMonitorWatch *watch,
             XSyncAlarm            *alarm)
{
        /* g_debug ("Searching for %d in %d,%d", (int)*alarm, (int)watch->xalarm_positive, (int)watch->xalarm_negative); */
        if (watch->xalarm_positive == *alarm
            || watch->xalarm_negative == *alarm) {
                return TRUE;
        }
        return FALSE;
}

static ShellIdleMonitorWatch *
find_watch_for_alarm (ShellIdleMonitor *monitor,
                      XSyncAlarm        alarm)
{
        ShellIdleMonitorWatch *watch;

        watch = g_hash_table_find (monitor->priv->watches,
                                   (GHRFunc)_find_alarm,
                                   &alarm);
        return watch;
}

static void
handle_alarm_notify_event (ShellIdleMonitor         *monitor,
                           XSyncAlarmNotifyEvent    *alarm_event)
{
        ShellIdleMonitorWatch *watch;
        gboolean               condition;

        if (alarm_event->state == XSyncAlarmDestroyed) {
                return;
        }

        watch = find_watch_for_alarm (monitor, alarm_event->alarm);

        if (watch == NULL) {
                /* g_debug ("Unable to find watch for alarm %d", (int)alarm_event->alarm); */
                return;
        }

        /* g_debug ("Watch %d fired, idle time = %" G_GINT64_FORMAT,
                 watch->id,
                 _xsyncvalue_to_int64 (alarm_event->counter_value)); */

        if (alarm_event->alarm == watch->xalarm_positive) {
                condition = TRUE;
        } else {
                condition = FALSE;
        }

        if (watch->callback != NULL) {
                watch->callback (monitor,
                                 watch->id,
                                 condition,
                                 watch->user_data);
        }
}

static GdkFilterReturn
xevent_filter (GdkXEvent        *xevent,
               GdkEvent         *event,
               ShellIdleMonitor *monitor)
{
        XEvent                *ev;
        XSyncAlarmNotifyEvent *alarm_event;

        ev = xevent;
        if (ev->xany.type != monitor->priv->sync_event_base + XSyncAlarmNotify) {
                return GDK_FILTER_CONTINUE;
        }

        alarm_event = xevent;

        handle_alarm_notify_event (monitor, alarm_event);

        return GDK_FILTER_CONTINUE;
}

static gboolean
init_xsync (ShellIdleMonitor *monitor)
{
        int                 sync_error_base;
        int                 res;
        int                 major;
        int                 minor;
        int                 i;
        int                 ncounters;
        XSyncSystemCounter *counters;

        res = XSyncQueryExtension (monitor->priv->display,
                                   &monitor->priv->sync_event_base,
                                   &sync_error_base);
        if (! res) {
                g_warning ("ShellIdleMonitor: Sync extension not present");
                return FALSE;
        }

        res = XSyncInitialize (monitor->priv->display, &major, &minor);
        if (! res) {
                g_warning ("ShellIdleMonitor: Unable to initialize Sync extension");
                return FALSE;
        }

        counters = XSyncListSystemCounters (monitor->priv->display, &ncounters);
        for (i = 0; i < ncounters; i++) {
                if (counters[i].name != NULL
                    && strcmp (counters[i].name, "IDLETIME") == 0) {
                        monitor->priv->counter = counters[i].counter;
                        break;
                }
        }
        XSyncFreeSystemCounterList (counters);

        if (monitor->priv->counter == None) {
                g_warning ("ShellIdleMonitor: IDLETIME counter not found");
                return FALSE;
        }

        gdk_window_add_filter (NULL, (GdkFilterFunc)xevent_filter, monitor);

        return TRUE;
}

static GObject *
shell_idle_monitor_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
        ShellIdleMonitor *monitor;

        monitor = SHELL_IDLE_MONITOR (G_OBJECT_CLASS (shell_idle_monitor_parent_class)->constructor (type,
                                                                                                     n_construct_properties,
                                                                                                     construct_properties));

        monitor->priv->display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

        if (! init_xsync (monitor)) {
                g_object_unref (monitor);
                return NULL;
        }

        return G_OBJECT (monitor);
}

static void
shell_idle_monitor_class_init (ShellIdleMonitorClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = shell_idle_monitor_dispose;
        object_class->constructor = shell_idle_monitor_constructor;

        g_type_class_add_private (klass, sizeof (ShellIdleMonitorPrivate));
}

static guint32
get_next_watch_serial (void)
{
        guint32 serial;

        serial = watch_serial++;

        if ((gint32)watch_serial < 0) {
                watch_serial = 1;
        }

        /* FIXME: make sure it isn't in the hash */

        return serial;
}

static ShellIdleMonitorWatch *
idle_monitor_watch_new (guint interval)
{
        ShellIdleMonitorWatch *watch;

        watch = g_slice_new0 (ShellIdleMonitorWatch);
        watch->interval = _int64_to_xsyncvalue ((gint64)interval);
        watch->id = get_next_watch_serial ();
        watch->xalarm_positive = None;
        watch->xalarm_negative = None;

        return watch;
}

static void
idle_monitor_watch_free (ShellIdleMonitorWatch *watch)
{
        if (watch == NULL) {
                return;
        }

        if (watch->notify != NULL) {
            watch->notify (watch->user_data);
        }

        if (watch->xalarm_positive != None) {
                XSyncDestroyAlarm (watch->display, watch->xalarm_positive);
        }
        if (watch->xalarm_negative != None) {
                XSyncDestroyAlarm (watch->display, watch->xalarm_negative);
        }
        g_slice_free (ShellIdleMonitorWatch, watch);
}

static void
shell_idle_monitor_init (ShellIdleMonitor *monitor)
{
        monitor->priv = SHELL_IDLE_MONITOR_GET_PRIVATE (monitor);

        monitor->priv->watches = g_hash_table_new_full (NULL,
                                                        NULL,
                                                        NULL,
                                                        (GDestroyNotify)idle_monitor_watch_free);

        monitor->priv->counter = None;
}

ShellIdleMonitor *
shell_idle_monitor_new (void)
{
        GObject *idle_monitor;

        idle_monitor = g_object_new (SHELL_TYPE_IDLE_MONITOR,
                                     NULL);

        return SHELL_IDLE_MONITOR (idle_monitor);
}

static gboolean
_xsync_alarm_set (ShellIdleMonitor      *monitor,
                  ShellIdleMonitorWatch *watch)
{
        XSyncAlarmAttributes attr;
        XSyncValue           delta;
        guint                flags;

        flags = XSyncCACounter
                | XSyncCAValueType
                | XSyncCATestType
                | XSyncCAValue
                | XSyncCADelta
                | XSyncCAEvents;

        XSyncIntToValue (&delta, 0);
        attr.trigger.counter = monitor->priv->counter;
        attr.trigger.value_type = XSyncAbsolute;
        attr.trigger.wait_value = watch->interval;
        attr.delta = delta;
        attr.events = TRUE;

        attr.trigger.test_type = XSyncPositiveTransition;
        if (watch->xalarm_positive != None) {
                /* g_debug ("ShellIdleMonitor: updating alarm for positive transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value)); */
                XSyncChangeAlarm (monitor->priv->display, watch->xalarm_positive, flags, &attr);
        } else {
                /* g_debug ("ShellIdleMonitor: creating new alarm for positive transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value)); */
                watch->xalarm_positive = XSyncCreateAlarm (monitor->priv->display, flags, &attr);
        }

        attr.trigger.wait_value = _int64_to_xsyncvalue (_xsyncvalue_to_int64 (watch->interval) - 1);
        attr.trigger.test_type = XSyncNegativeTransition;
        if (watch->xalarm_negative != None) {
                /* g_debug ("ShellIdleMonitor: updating alarm for negative transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value)); */
                XSyncChangeAlarm (monitor->priv->display, watch->xalarm_negative, flags, &attr);
        } else {
                /* g_debug ("ShellIdleMonitor: creating new alarm for negative transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value)); */
                watch->xalarm_negative = XSyncCreateAlarm (monitor->priv->display, flags, &attr);
        }

        return TRUE;
}

guint
shell_idle_monitor_add_watch (ShellIdleMonitor         *monitor,
                              guint                     interval,
                              ShellIdleMonitorWatchFunc callback,
                              gpointer                  user_data,
                              GDestroyNotify            notify)
{
        ShellIdleMonitorWatch *watch;

        g_return_val_if_fail (SHELL_IS_IDLE_MONITOR (monitor), 0);
        g_return_val_if_fail (callback != NULL, 0);

        watch = idle_monitor_watch_new (interval);
        watch->display = monitor->priv->display;
        watch->callback = callback;
        watch->user_data = user_data;
        watch->notify = notify;

        _xsync_alarm_set (monitor, watch);

        g_hash_table_insert (monitor->priv->watches,
                             GUINT_TO_POINTER (watch->id),
                             watch);
        return watch->id;
}

void
shell_idle_monitor_remove_watch (ShellIdleMonitor *monitor,
                                 guint          id)
{
        g_return_if_fail (SHELL_IS_IDLE_MONITOR (monitor));

        g_hash_table_remove (monitor->priv->watches,
                             GUINT_TO_POINTER (id));
}
