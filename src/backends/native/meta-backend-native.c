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

#include "meta-idle-monitor-native.h"

G_DEFINE_TYPE (MetaBackendNative, meta_backend_native, META_TYPE_BACKEND);

static MetaIdleMonitor *
meta_backend_native_create_idle_monitor (MetaBackend *backend,
                                         int          device_id)
{
  return g_object_new (META_TYPE_IDLE_MONITOR_NATIVE,
                       "device-id", device_id,
                       NULL);
}

static void
meta_backend_native_class_init (MetaBackendNativeClass *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);

  backend_class->create_idle_monitor = meta_backend_native_create_idle_monitor;
}

static void
meta_backend_native_init (MetaBackendNative *native)
{
}
