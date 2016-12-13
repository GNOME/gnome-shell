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

#ifndef META_MONITOR_H
#define META_MONITOR_H

#include <glib-object.h>

#include "backends/meta-monitor-manager-private.h"

#define META_TYPE_MONITOR (meta_monitor_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaMonitor, meta_monitor, META, MONITOR, GObject)

struct _MetaMonitorClass
{
  GObjectClass parent_class;

  MetaOutput * (* get_main_output) (MetaMonitor *monitor);
  void (* get_dimensions) (MetaMonitor   *monitor,
                           int           *width,
                           int           *height);
};

#define META_TYPE_MONITOR_NORMAL (meta_monitor_normal_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorNormal, meta_monitor_normal,
                      META, MONITOR_NORMAL,
                      MetaMonitor)

#define META_TYPE_MONITOR_TILED (meta_monitor_tiled_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorTiled, meta_monitor_tiled,
                      META, MONITOR_TILED,
                      MetaMonitor)

MetaMonitorTiled * meta_monitor_tiled_new (MetaMonitorManager *monitor_manager,
                                           MetaOutput         *main_output);

MetaMonitorNormal * meta_monitor_normal_new (MetaOutput *output);

gboolean meta_monitor_is_active (MetaMonitor *monitor);

MetaOutput * meta_monitor_get_main_output (MetaMonitor *monitor);

gboolean meta_monitor_is_primary (MetaMonitor *monitor);

GList * meta_monitor_get_outputs (MetaMonitor *monitor);

void meta_monitor_get_dimensions (MetaMonitor   *monitor,
                                  int           *width,
                                  int           *height);

void meta_monitor_get_physical_dimensions (MetaMonitor *monitor,
                                           int         *width_mm,
                                           int         *height_mm);

const char * meta_monitor_get_product (MetaMonitor *monitor);

uint32_t meta_monitor_tiled_get_tile_group_id (MetaMonitorTiled *monitor_tiled);

#endif /* META_MONITOR_H */
