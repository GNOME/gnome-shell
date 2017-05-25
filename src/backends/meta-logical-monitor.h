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

#ifndef META_LOGICAL_MONITOR_H
#define META_LOGICAL_MONITOR_H

#include <glib-object.h>

#include "backends/meta-monitor.h"
#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-monitor-manager-private.h"
#include "meta/boxes.h"

#define META_MAX_OUTPUTS_PER_MONITOR 4

struct _MetaLogicalMonitor
{
  GObject parent;

  int number;
  MetaRectangle rect;
  gboolean is_primary;
  gboolean is_presentation; /* XXX: not yet used */
  gboolean in_fullscreen;
  float scale;
  MetaMonitorTransform transform;

  /* The primary or first output for this monitor, 0 if we can't figure out.
     It can be matched to a winsys_id of a MetaOutput.

     This is used as an opaque token on reconfiguration when switching from
     clone to extened, to decide on what output the windows should go next
     (it's an attempt to keep windows on the same monitor, and preferably on
     the primary one).
  */
  glong winsys_id;

  GList *monitors;
};

#define META_TYPE_LOGICAL_MONITOR (meta_logical_monitor_get_type ())
G_DECLARE_FINAL_TYPE (MetaLogicalMonitor, meta_logical_monitor,
                      META, LOGICAL_MONITOR,
                      GObject)

MetaLogicalMonitor * meta_logical_monitor_new (MetaMonitorManager       *monitor_manager,
                                               MetaLogicalMonitorConfig *logical_monitor_config,
                                               int                       monitor_number);

MetaLogicalMonitor * meta_logical_monitor_new_derived (MetaMonitorManager *monitor_manager,
                                                       MetaMonitor        *monitor,
                                                       MetaRectangle      *layout,
                                                       float               scale,
                                                       int                 monitor_number);

void meta_logical_monitor_add_monitor (MetaLogicalMonitor *logical_monitor,
                                       MetaMonitor        *monitor);

gboolean meta_logical_monitor_is_primary (MetaLogicalMonitor *logical_monitor);

void meta_logical_monitor_make_primary (MetaLogicalMonitor *logical_monitor);

float meta_logical_monitor_get_scale (MetaLogicalMonitor *logical_monitor);

GList * meta_logical_monitor_get_monitors (MetaLogicalMonitor *logical_monitor);

gboolean meta_logical_monitor_has_neighbor (MetaLogicalMonitor  *logical_monitor,
                                            MetaLogicalMonitor  *neighbor,
                                            MetaScreenDirection  neighbor_dir);

#endif /* META_LOGICAL_MONITOR_H */
