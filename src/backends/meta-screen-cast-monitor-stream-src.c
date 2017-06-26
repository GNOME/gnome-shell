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

#include "config.h"

#include "backends/meta-screen-cast-monitor-stream-src.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-screen-cast-monitor-stream.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor.h"
#include "clutter/clutter.h"
#include "clutter/clutter-mutter.h"

struct _MetaScreenCastMonitorStreamSrc
{
  MetaScreenCastStreamSrc parent;

  gulong stage_painted_handler_id;
};

G_DEFINE_TYPE (MetaScreenCastMonitorStreamSrc,
               meta_screen_cast_monitor_stream_src,
               META_TYPE_SCREEN_CAST_STREAM_SRC)

static ClutterStage *
get_stage (MetaScreenCastMonitorStreamSrc *monitor_src)
{
  MetaScreenCastStreamSrc *src;
  MetaScreenCastStream *stream;
  MetaScreenCastMonitorStream *monitor_stream;

  src = META_SCREEN_CAST_STREAM_SRC (monitor_src);
  stream = meta_screen_cast_stream_src_get_stream (src);
  monitor_stream = META_SCREEN_CAST_MONITOR_STREAM (stream);

  return meta_screen_cast_monitor_stream_get_stage (monitor_stream);
}

static MetaMonitor *
get_monitor (MetaScreenCastMonitorStreamSrc *monitor_src)
{
  MetaScreenCastStreamSrc *src;
  MetaScreenCastStream *stream;
  MetaScreenCastMonitorStream *monitor_stream;

  src = META_SCREEN_CAST_STREAM_SRC (monitor_src);
  stream = meta_screen_cast_stream_src_get_stream (src);
  monitor_stream = META_SCREEN_CAST_MONITOR_STREAM (stream);

  return meta_screen_cast_monitor_stream_get_monitor (monitor_stream);
}

static void
meta_screen_cast_monitor_stream_src_get_specs (MetaScreenCastStreamSrc *src,
                                               int                     *width,
                                               int                     *height,
                                               float                   *frame_rate)
{
  MetaScreenCastMonitorStreamSrc *monitor_src =
    META_SCREEN_CAST_MONITOR_STREAM_SRC (src);
  MetaMonitor *monitor;
  MetaLogicalMonitor *logical_monitor;
  float scale;
  MetaMonitorMode *mode;

  monitor = get_monitor (monitor_src);
  logical_monitor = meta_monitor_get_logical_monitor (monitor);
  mode = meta_monitor_get_current_mode (monitor);

  scale = logical_monitor->scale;
  *width = (int) roundf (logical_monitor->rect.width * scale);
  *height = (int) roundf (logical_monitor->rect.height * scale);
  *frame_rate = meta_monitor_mode_get_refresh_rate (mode);
}

static void
stage_painted (ClutterActor                   *actor,
               MetaScreenCastMonitorStreamSrc *monitor_src)
{
  MetaScreenCastStreamSrc *src = META_SCREEN_CAST_STREAM_SRC (monitor_src);

  meta_screen_cast_stream_src_maybe_record_frame (src);
}

static void
meta_screen_cast_monitor_stream_src_enable (MetaScreenCastStreamSrc *src)
{
  MetaScreenCastMonitorStreamSrc *monitor_src =
    META_SCREEN_CAST_MONITOR_STREAM_SRC (src);
  ClutterStage *stage;

  stage = get_stage (monitor_src);
  monitor_src->stage_painted_handler_id =
    g_signal_connect_after (stage, "paint",
                            G_CALLBACK (stage_painted),
                            monitor_src);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

static void
meta_screen_cast_monitor_stream_src_disable (MetaScreenCastStreamSrc *src)
{
  MetaScreenCastMonitorStreamSrc *monitor_src =
    META_SCREEN_CAST_MONITOR_STREAM_SRC (src);
  ClutterStage *stage;

  stage = get_stage (monitor_src);
  g_signal_handler_disconnect (stage, monitor_src->stage_painted_handler_id);
  monitor_src->stage_painted_handler_id = 0;
}

static void
meta_screen_cast_monitor_stream_src_record_frame (MetaScreenCastStreamSrc *src,
                                                  uint8_t                 *data)
{
  MetaScreenCastMonitorStreamSrc *monitor_src =
    META_SCREEN_CAST_MONITOR_STREAM_SRC (src);
  ClutterStage *stage;
  MetaMonitor *monitor;
  MetaLogicalMonitor *logical_monitor;

  stage = get_stage (monitor_src);
  monitor = get_monitor (monitor_src);
  logical_monitor = meta_monitor_get_logical_monitor (monitor);
  clutter_stage_capture_into (stage, FALSE, &logical_monitor->rect, data);
}

MetaScreenCastMonitorStreamSrc *
meta_screen_cast_monitor_stream_src_new (MetaScreenCastMonitorStream  *monitor_stream,
                                         const char                   *stream_id,
                                         GError                      **error)
{
  return g_initable_new (META_TYPE_SCREEN_CAST_MONITOR_STREAM_SRC,
                         NULL,
                         error,
                         "stream-id", stream_id,
                         "stream", monitor_stream,
                         NULL);
}

static void
meta_screen_cast_monitor_stream_src_init (MetaScreenCastMonitorStreamSrc *monitor_src)
{
}

static void
meta_screen_cast_monitor_stream_src_class_init (MetaScreenCastMonitorStreamSrcClass *klass)
{
  MetaScreenCastStreamSrcClass *src_class =
    META_SCREEN_CAST_STREAM_SRC_CLASS (klass);

  src_class->get_specs = meta_screen_cast_monitor_stream_src_get_specs;
  src_class->enable = meta_screen_cast_monitor_stream_src_enable;
  src_class->disable = meta_screen_cast_monitor_stream_src_disable;
  src_class->record_frame = meta_screen_cast_monitor_stream_src_record_frame;
}
