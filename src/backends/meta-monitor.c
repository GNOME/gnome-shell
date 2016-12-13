/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat
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
 */

#include "config.h"

#include "backends/meta-monitor.h"

#include "backends/meta-monitor-manager-private.h"

typedef struct _MetaMonitorPrivate
{
  GList *outputs;

  /*
   * The primary or first output for this monitor, 0 if we can't figure out.
   * It can be matched to a winsys_id of a MetaOutput.
   *
   * This is used as an opaque token on reconfiguration when switching from
   * clone to extened, to decide on what output the windows should go next
   * (it's an attempt to keep windows on the same monitor, and preferably on
   * the primary one).
   */
  long winsys_id;
} MetaMonitorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaMonitor, meta_monitor, G_TYPE_OBJECT)

struct _MetaMonitorNormal
{
  MetaMonitor parent;
};

G_DEFINE_TYPE (MetaMonitorNormal, meta_monitor_normal, META_TYPE_MONITOR)

struct _MetaMonitorTiled
{
  MetaMonitor parent;

  uint32_t tile_group_id;

  MetaOutput *main_output;
};

G_DEFINE_TYPE (MetaMonitorTiled, meta_monitor_tiled, META_TYPE_MONITOR)

GList *
meta_monitor_get_outputs (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  return priv->outputs;
}

static MetaOutput *
meta_monitor_get_main_output (MetaMonitor *monitor)
{
  return META_MONITOR_GET_CLASS (monitor)->get_main_output (monitor);
}

gboolean
meta_monitor_is_active (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);

  return output->crtc && output->crtc->current_mode;
}

void
meta_monitor_get_dimensions (MetaMonitor   *monitor,
                             int           *width,
                             int           *height)
{
  META_MONITOR_GET_CLASS (monitor)->get_dimensions (monitor, width, height);
}

void
meta_monitor_get_physical_dimensions (MetaMonitor *monitor,
                                      int         *width_mm,
                                      int         *height_mm)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  *width_mm = output->width_mm;
  *height_mm = output->height_mm;
}

static void
meta_monitor_finalize (GObject *object)
{
  MetaMonitor *monitor = META_MONITOR (object);
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  g_clear_pointer (&priv->outputs, g_list_free);
}

static void
meta_monitor_init (MetaMonitor *monitor)
{
}

static void
meta_monitor_class_init (MetaMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_monitor_finalize;
}

MetaMonitorNormal *
meta_monitor_normal_new (MetaOutput *output)
{
  MetaMonitorNormal *monitor_normal;
  MetaMonitorPrivate *monitor_priv;

  monitor_normal = g_object_new (META_TYPE_MONITOR_NORMAL, NULL);
  monitor_priv =
    meta_monitor_get_instance_private (META_MONITOR (monitor_normal));

  monitor_priv->outputs = g_list_append (NULL, output);
  monitor_priv->winsys_id = output->winsys_id;

  return monitor_normal;
}

static MetaOutput *
meta_monitor_normal_get_main_output (MetaMonitor *monitor)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);

  return monitor_priv->outputs->data;
}

static void
meta_monitor_normal_get_dimensions (MetaMonitor   *monitor,
                                    int           *width,
                                    int           *height)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  *width = output->crtc->rect.width;
  *height = output->crtc->rect.height;
}

static void
meta_monitor_normal_init (MetaMonitorNormal *monitor)
{
}

static void
meta_monitor_normal_class_init (MetaMonitorNormalClass *klass)
{
  MetaMonitorClass *monitor_class = META_MONITOR_CLASS (klass);

  monitor_class->get_main_output = meta_monitor_normal_get_main_output;
  monitor_class->get_dimensions = meta_monitor_normal_get_dimensions;
}

static void
add_tiled_monitor_outputs (MetaMonitorManager *monitor_manager,
                           MetaMonitorTiled   *monitor_tiled)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (META_MONITOR (monitor_tiled));
  unsigned int i;

  for (i = 0; i < monitor_manager->n_outputs; i++)
    {
      MetaOutput *output = &monitor_manager->outputs[i];

      if (output->tile_info.group_id != monitor_tiled->tile_group_id)
        continue;

      monitor_priv->outputs = g_list_append (monitor_priv->outputs, output);
    }
}

MetaMonitorTiled *
meta_monitor_tiled_new (MetaMonitorManager *monitor_manager,
                        MetaOutput         *output)
{
  MetaMonitorTiled *monitor_tiled;
  MetaMonitorPrivate *monitor_priv;

  monitor_tiled = g_object_new (META_TYPE_MONITOR_TILED, NULL);
  monitor_priv =
    meta_monitor_get_instance_private (META_MONITOR (monitor_tiled));

  monitor_tiled->tile_group_id = output->tile_info.group_id;
  monitor_priv->winsys_id = output->winsys_id;

  add_tiled_monitor_outputs (monitor_manager, monitor_tiled);
  monitor_tiled->main_output = output;

  return monitor_tiled;
}

static MetaOutput *
meta_monitor_tiled_get_main_output (MetaMonitor *monitor)
{
  MetaMonitorTiled *monitor_tiled = META_MONITOR_TILED (monitor);

  return monitor_tiled->main_output;
}

static void
meta_monitor_tiled_get_dimensions (MetaMonitor   *monitor,
                                   int           *out_width,
                                   int           *out_height)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  GList *l;
  int width;
  int height;

  width = 0;
  height = 0;
  for (l = monitor_priv->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->tile_info.loc_v_tile == 0)
        width += output->tile_info.tile_w;

      if (output->tile_info.loc_h_tile == 0)
        height += output->tile_info.tile_h;
    }

  *out_width = width;
  *out_height = height;
}

static void
meta_monitor_tiled_init (MetaMonitorTiled *monitor)
{
}

static void
meta_monitor_tiled_class_init (MetaMonitorTiledClass *klass)
{
  MetaMonitorClass *monitor_class = META_MONITOR_CLASS (klass);

  monitor_class->get_main_output = meta_monitor_tiled_get_main_output;
  monitor_class->get_dimensions = meta_monitor_tiled_get_dimensions;
}
