/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat Inc.
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
 */

#ifndef META_SCREEN_CAST_MONITOR_STREAM_H
#define META_SCREEN_CAST_MONITOR_STREAM_H

#include <glib-object.h>

#include "backends/meta-monitor-manager-private.h"
#include "backends/meta-screen-cast-stream.h"

#define META_TYPE_SCREEN_CAST_MONITOR_STREAM (meta_screen_cast_monitor_stream_get_type ())
G_DECLARE_FINAL_TYPE (MetaScreenCastMonitorStream,
                      meta_screen_cast_monitor_stream,
                      META, SCREEN_CAST_MONITOR_STREAM,
                      MetaScreenCastStream)

MetaScreenCastMonitorStream * meta_screen_cast_monitor_stream_new (GDBusConnection    *connection,
								   MetaMonitorManager *monitor_manager,
                                                                   MetaMonitor        *monitor,
                                                                   ClutterStage       *stage,
                                                                   GError            **error);

ClutterStage * meta_screen_cast_monitor_stream_get_stage (MetaScreenCastMonitorStream *monitor_stream);

MetaMonitor * meta_screen_cast_monitor_stream_get_monitor (MetaScreenCastMonitorStream *monitor_stream);

#endif /* META_SCREEN_CAST_MONITOR_STREAM_H */
