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

#include "backends/meta-screen-cast-monitor-stream.h"

#include "backends/meta-logical-monitor.h"
#include "backends/meta-screen-cast-monitor-stream-src.h"

struct _MetaScreenCastMonitorStream
{
  MetaScreenCastStream parent;

  ClutterStage *stage;

  MetaMonitor *monitor;
  MetaLogicalMonitor *logical_monitor;
};

G_DEFINE_TYPE (MetaScreenCastMonitorStream,
               meta_screen_cast_monitor_stream,
               META_TYPE_SCREEN_CAST_STREAM)

static gboolean
update_monitor (MetaScreenCastMonitorStream *monitor_stream,
                MetaMonitor                 *new_monitor)
{
  MetaLogicalMonitor *new_logical_monitor;

  new_logical_monitor = meta_monitor_get_logical_monitor (new_monitor);
  if (!new_logical_monitor)
    return FALSE;

  if (!meta_rectangle_equal (&new_logical_monitor->rect,
                             &monitor_stream->logical_monitor->rect))
    return FALSE;

  g_set_object (&monitor_stream->monitor, new_monitor);
  g_set_object (&monitor_stream->logical_monitor, new_logical_monitor);

  return TRUE;
}

static void
on_monitors_changed (MetaMonitorManager          *monitor_manager,
                     MetaScreenCastMonitorStream *monitor_stream)
{
  MetaMonitor *new_monitor = NULL;
  GList *monitors;
  GList *l;

  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *other_monitor = l->data;

      if (meta_monitor_is_same_as (monitor_stream->monitor, other_monitor))
        {
          new_monitor = other_monitor;
          break;
        }
    }

  if (!new_monitor || !update_monitor (monitor_stream, new_monitor))
    meta_screen_cast_stream_close (META_SCREEN_CAST_STREAM (monitor_stream));
}

ClutterStage *
meta_screen_cast_monitor_stream_get_stage (MetaScreenCastMonitorStream *monitor_stream)
{
  return monitor_stream->stage;
}

MetaMonitor *
meta_screen_cast_monitor_stream_get_monitor (MetaScreenCastMonitorStream *monitor_stream)
{
  return monitor_stream->monitor;
}

MetaScreenCastMonitorStream *
meta_screen_cast_monitor_stream_new (GDBusConnection     *connection,
                                     MetaMonitorManager  *monitor_manager,
                                     MetaMonitor         *monitor,
                                     ClutterStage        *stage,
                                     GError             **error)
{
  MetaScreenCastMonitorStream *monitor_stream;
  MetaLogicalMonitor *logical_monitor;

  logical_monitor = meta_monitor_get_logical_monitor (monitor);
  if (!logical_monitor)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Monitor not active");
      return NULL;
    }

  monitor_stream = g_initable_new (META_TYPE_SCREEN_CAST_MONITOR_STREAM,
                                   NULL,
                                   error,
                                   "connection", connection,
                                   NULL);
  if (!monitor_stream)
    return NULL;

  g_set_object (&monitor_stream->monitor, monitor);
  g_set_object (&monitor_stream->logical_monitor, logical_monitor);
  monitor_stream->stage = stage;

  g_signal_connect_object (monitor_manager, "monitors-changed",
                           G_CALLBACK (on_monitors_changed),
                           monitor_stream, 0);

  return monitor_stream;
}

static MetaScreenCastStreamSrc *
meta_screen_cast_monitor_stream_create_src (MetaScreenCastStream  *stream,
                                            GError               **error)
{
  MetaScreenCastMonitorStream *monitor_stream =
    META_SCREEN_CAST_MONITOR_STREAM (stream);
  MetaScreenCastMonitorStreamSrc *monitor_stream_src;

  monitor_stream_src = meta_screen_cast_monitor_stream_src_new (monitor_stream,
                                                                error);
  if (!monitor_stream_src)
    return NULL;

  return META_SCREEN_CAST_STREAM_SRC (monitor_stream_src);
}

static void
meta_screen_cast_monitor_stream_finalize (GObject *object)
{
  MetaScreenCastMonitorStream *monitor_stream =
    META_SCREEN_CAST_MONITOR_STREAM (object);

  g_clear_object (&monitor_stream->monitor);
  g_clear_object (&monitor_stream->logical_monitor);

  G_OBJECT_CLASS (meta_screen_cast_monitor_stream_parent_class)->finalize (object);
}

static void
meta_screen_cast_monitor_stream_init (MetaScreenCastMonitorStream *monitor_stream)
{
}

static void
meta_screen_cast_monitor_stream_class_init (MetaScreenCastMonitorStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaScreenCastStreamClass *stream_class =
    META_SCREEN_CAST_STREAM_CLASS (klass);

  object_class->finalize = meta_screen_cast_monitor_stream_finalize;

  stream_class->create_src = meta_screen_cast_monitor_stream_create_src;
}
