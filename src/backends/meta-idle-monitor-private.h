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

typedef struct
{
  MetaIdleMonitor          *monitor;
  guint	                    id;
  MetaIdleMonitorWatchFunc  callback;
  gpointer		    user_data;
  GDestroyNotify            notify;
  guint64                   timeout_msec;
  int                       idle_source_id;
  GSource                  *timeout_source;
} MetaIdleMonitorWatch;

struct _MetaIdleMonitor
{
  GObject parent_instance;

  GDBusProxy *session_proxy;
  gboolean inhibited;
  GHashTable *watches;
  int device_id;
  guint64 last_event_time;
};

struct _MetaIdleMonitorClass
{
  GObjectClass parent_class;
};

void _meta_idle_monitor_watch_fire (MetaIdleMonitorWatch *watch);
void meta_idle_monitor_reset_idletime (MetaIdleMonitor *monitor);

#endif /* META_IDLE_MONITOR_PRIVATE_H */
