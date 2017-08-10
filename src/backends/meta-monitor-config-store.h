/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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

#ifndef META_MONITOR_CONFIG_STORE_H
#define META_MONITOR_CONFIG_STORE_H

#include <glib-object.h>

#include "backends/meta-monitor-config-manager.h"

#define META_TYPE_MONITOR_CONFIG_STORE (meta_monitor_config_store_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorConfigStore, meta_monitor_config_store,
                      META, MONITOR_CONFIG_STORE, GObject)

MetaMonitorConfigStore * meta_monitor_config_store_new (MetaMonitorManager *monitor_manager);

MetaMonitorsConfig * meta_monitor_config_store_lookup (MetaMonitorConfigStore *config_store,
                                                       MetaMonitorsConfigKey  *key);

void meta_monitor_config_store_add (MetaMonitorConfigStore *config_store,
                                    MetaMonitorsConfig     *config);

void meta_monitor_config_store_remove (MetaMonitorConfigStore *config_store,
                                       MetaMonitorsConfig     *config);

gboolean meta_monitor_config_store_set_custom (MetaMonitorConfigStore *config_store,
                                               const char             *read_path,
                                               const char             *write_path,
                                               GError                **error);

int meta_monitor_config_store_get_config_count (MetaMonitorConfigStore *config_store);

MetaMonitorManager * meta_monitor_config_store_get_monitor_manager (MetaMonitorConfigStore *config_store);

#endif /* META_MONITOR_CONFIG_STORE_H */
