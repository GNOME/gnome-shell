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

#ifndef META_MONITOR_CONFIG_MANAGER_H
#define META_MONITOR_CONFIG_MANAGER_H

#include "backends/meta-monitor.h"
#include "backends/meta-monitor-manager-private.h"

#define META_TYPE_MONITOR_CONFIG_MANAGER (meta_monitor_config_manager_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorConfigManager, meta_monitor_config_manager,
                      META, MONITOR_CONFIG_MANAGER, GObject)

typedef struct _MetaMonitorConfig
{
  MetaMonitorSpec *monitor_spec;
  MetaMonitorModeSpec *mode_spec;
  gboolean enable_underscanning;
} MetaMonitorConfig;

typedef struct _MetaLogicalMonitorConfig
{
  MetaRectangle layout;
  GList *monitor_configs;
  MetaMonitorTransform transform;
  float scale;
  gboolean is_primary;
  gboolean is_presentation;
} MetaLogicalMonitorConfig;

typedef struct _MetaMonitorsConfigKey
{
  GList *monitor_specs;
} MetaMonitorsConfigKey;

struct _MetaMonitorsConfig
{
  GObject parent;

  MetaMonitorsConfigKey *key;
  GList *logical_monitor_configs;

  MetaLogicalMonitorLayoutMode layout_mode;
};

#define META_TYPE_MONITORS_CONFIG (meta_monitors_config_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorsConfig, meta_monitors_config,
                      META, MONITORS_CONFIG, GObject)

MetaMonitorConfigManager * meta_monitor_config_manager_new (MetaMonitorManager *monitor_manager);

MetaMonitorConfigStore * meta_monitor_config_manager_get_store (MetaMonitorConfigManager *config_manager);

gboolean meta_monitor_config_manager_assign (MetaMonitorManager *manager,
                                             MetaMonitorsConfig *config,
                                             GPtrArray         **crtc_infos,
                                             GPtrArray         **output_infos,
                                             GError            **error);

MetaMonitorsConfig * meta_monitor_config_manager_get_stored (MetaMonitorConfigManager *config_manager);

MetaMonitorsConfig * meta_monitor_config_manager_create_linear (MetaMonitorConfigManager *config_manager);

MetaMonitorsConfig * meta_monitor_config_manager_create_fallback (MetaMonitorConfigManager *config_manager);

MetaMonitorsConfig * meta_monitor_config_manager_create_suggested (MetaMonitorConfigManager *config_manager);

void meta_monitor_config_manager_set_current (MetaMonitorConfigManager *config_manager,
                                              MetaMonitorsConfig       *config);

MetaMonitorsConfig * meta_monitor_config_manager_get_current (MetaMonitorConfigManager *config_manager);

MetaMonitorsConfig * meta_monitor_config_manager_get_previous (MetaMonitorConfigManager *config_manager);

void meta_monitor_config_manager_save_current (MetaMonitorConfigManager *config_manager);

MetaMonitorsConfig * meta_monitors_config_new (GList                       *logical_monitor_configs,
                                               MetaLogicalMonitorLayoutMode layout_mode);

unsigned int meta_monitors_config_key_hash (gconstpointer config_key);

gboolean meta_monitors_config_key_equal (gconstpointer config_key_a,
                                         gconstpointer config_key_b);

void meta_monitors_config_key_free (MetaMonitorsConfigKey *config_key);

void meta_logical_monitor_config_free (MetaLogicalMonitorConfig *logical_monitor_config);

void meta_monitor_config_free (MetaMonitorConfig *monitor_config);

gboolean meta_verify_monitor_mode_spec (MetaMonitorModeSpec *monitor_mode_spec,
                                        GError             **error);

gboolean meta_verify_monitor_spec (MetaMonitorSpec *monitor_spec,
                                   GError         **error);

gboolean meta_verify_monitor_config (MetaMonitorConfig *monitor_config,
                                     GError           **error);

gboolean meta_verify_logical_monitor_config (MetaLogicalMonitorConfig    *logical_monitor_config,
                                             MetaLogicalMonitorLayoutMode layout_mode,
                                             GError                     **error);

gboolean meta_verify_monitors_config (MetaMonitorsConfig *config,
                                      MetaMonitorManager *monitor_manager,
                                      GError            **error);

#endif /* META_MONITOR_CONFIG_MANAGER_H */
