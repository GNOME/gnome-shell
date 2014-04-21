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

#include "meta-backend-x11.h"

#include "meta-idle-monitor-xsync.h"

G_DEFINE_TYPE (MetaBackendX11, meta_backend_x11, META_TYPE_BACKEND);

static MetaIdleMonitor *
meta_backend_x11_create_idle_monitor (MetaBackend *backend,
                                      int          device_id)
{
  return g_object_new (META_TYPE_IDLE_MONITOR_XSYNC,
                       "device-id", device_id,
                       NULL);
}

static void
meta_backend_x11_class_init (MetaBackendX11Class *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);

  backend_class->create_idle_monitor = meta_backend_x11_create_idle_monitor;
}

static void
meta_backend_x11_init (MetaBackendX11 *x11)
{
}

void
meta_backend_x11_handle_alarm_notify (MetaBackend *backend,
                                      XEvent      *xevent)
{
  int i;

  if (!META_IS_BACKEND_X11 (backend))
    return;

  for (i = 0; i <= backend->device_id_max; i++)
    if (backend->device_monitors[i])
      meta_idle_monitor_xsync_handle_xevent (backend->device_monitors[i], (XSyncAlarmNotifyEvent*)xevent);
}
