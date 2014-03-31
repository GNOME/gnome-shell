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

#ifndef META_IDLE_MONITOR_PRIVATE_H
#define META_IDLE_MONITOR_PRIVATE_H

#include <meta/meta-idle-monitor.h>
#include "display-private.h"

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

typedef struct
{
  MetaIdleMonitor          *monitor;
  guint	                    id;
  MetaIdleMonitorWatchFunc  callback;
  gpointer		    user_data;
  GDestroyNotify            notify;
  guint64                   timeout_msec;
  int                       idle_source_id;
} MetaIdleMonitorWatch;

struct _MetaIdleMonitor
{
  GObject parent_instance;

  GHashTable  *watches;
  int          device_id;

  /* X11 implementation */
  Display     *display;
  int          sync_event_base;
  XSyncCounter counter;
  XSyncAlarm   user_active_alarm;

  /* Wayland implementation */
  guint64      last_event_time;
};

struct _MetaIdleMonitorClass
{
  GObjectClass parent_class;

  gint64 (*get_idletime) (MetaIdleMonitor *monitor);
  MetaIdleMonitorWatch * (*make_watch) (MetaIdleMonitor           *monitor,
                                        guint64                    timeout_msec,
                                        MetaIdleMonitorWatchFunc   callback,
                                        gpointer                   user_data,
                                        GDestroyNotify             notify);
};

void _meta_idle_monitor_watch_fire (MetaIdleMonitorWatch *watch);

#endif /* META_IDLE_MONITOR_PRIVATE_H */
