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

#include "backends/meta-monitor-config-manager.h"

#include "backends/meta-monitor-config-store.h"
#include "backends/meta-monitor-manager-private.h"
#include "core/boxes-private.h"

struct _MetaMonitorConfigManager
{
  GObject parent;

  MetaMonitorManager *monitor_manager;

  MetaMonitorConfigStore *config_store;

  MetaMonitorsConfig *current_config;
  MetaMonitorsConfig *previous_config;
};

G_DEFINE_TYPE (MetaMonitorConfigManager, meta_monitor_config_manager,
               G_TYPE_OBJECT)

G_DEFINE_TYPE (MetaMonitorsConfig, meta_monitors_config,
               G_TYPE_OBJECT)

MetaMonitorConfigManager *
meta_monitor_config_manager_new (MetaMonitorManager *monitor_manager)
{
  MetaMonitorConfigManager *config_manager;

  config_manager = g_object_new (META_TYPE_MONITOR_CONFIG_MANAGER, NULL);
  config_manager->monitor_manager = monitor_manager;
  config_manager->config_store =
    meta_monitor_config_store_new (monitor_manager);

  return config_manager;
}

MetaMonitorConfigStore *
meta_monitor_config_manager_get_store (MetaMonitorConfigManager *config_manager)
{
  return config_manager->config_store;
}

static gboolean
is_crtc_assigned (MetaCrtc  *crtc,
                  GPtrArray *crtc_infos)
{
  unsigned int i;

  for (i = 0; i < crtc_infos->len; i++)
    {
      MetaCrtcInfo *assigned_crtc_info = g_ptr_array_index (crtc_infos, i);

      if (assigned_crtc_info->crtc == crtc)
        return TRUE;
    }

  return FALSE;
}

static MetaCrtc *
find_unassigned_crtc (MetaOutput *output,
                      GPtrArray  *crtc_infos)
{
  unsigned int i;

  for (i = 0; i < output->n_possible_crtcs; i++)
    {
      MetaCrtc *crtc = output->possible_crtcs[i];

      if (is_crtc_assigned (crtc, crtc_infos))
        continue;

      return crtc;
    }

  return NULL;
}

typedef struct
{
  MetaMonitorManager *monitor_manager;
  MetaLogicalMonitorConfig *logical_monitor_config;
  MetaMonitorConfig *monitor_config;
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;
} MonitorAssignmentData;

static gboolean
assign_monitor_crtc (MetaMonitor         *monitor,
                     MetaMonitorMode     *mode,
                     MetaMonitorCrtcMode *monitor_crtc_mode,
                     gpointer             user_data,
                     GError             **error)
{
  MonitorAssignmentData *data = user_data;
  MetaOutput *output;
  MetaCrtc *crtc;
  MetaMonitorTransform transform;
  MetaMonitorTransform crtc_transform;
  int crtc_x, crtc_y;
  MetaCrtcInfo *crtc_info;
  MetaOutputInfo *output_info;
  MetaMonitorConfig *first_monitor_config;
  gboolean assign_output_as_primary;
  gboolean assign_output_as_presentation;

  output = monitor_crtc_mode->output;

  crtc = find_unassigned_crtc (output, data->crtc_infos);
  if (!crtc)
    {
      MetaMonitorSpec *monitor_spec = meta_monitor_get_spec (monitor);

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "No available CRTC for monitor '%s %s' not found",
                   monitor_spec->vendor, monitor_spec->product);
      return FALSE;
    }

  transform = data->logical_monitor_config->transform;
  if (meta_monitor_manager_is_transform_handled (data->monitor_manager,
                                                 crtc,
                                                 transform))
    crtc_transform = transform;
  else
    crtc_transform = META_MONITOR_TRANSFORM_NORMAL;

  meta_monitor_calculate_crtc_pos (monitor, mode, output, crtc_transform,
                                   &crtc_x, &crtc_y);

  crtc_info = g_slice_new0 (MetaCrtcInfo);
  *crtc_info = (MetaCrtcInfo) {
    .crtc = crtc,
    .mode = monitor_crtc_mode->crtc_mode,
    .x = crtc_x,
    .y = crtc_y,
    .transform = crtc_transform,
    .outputs = g_ptr_array_new ()
  };
  g_ptr_array_add (crtc_info->outputs, output);

  /*
   * Currently, MetaCrtcInfo are deliberately offset incorrectly to carry over
   * logical monitor location inside the MetaCrtc struct, when in fact this
   * depends on the framebuffer configuration. This will eventually be negated
   * when setting the actual KMS mode.
   *
   * TODO: Remove this hack when we don't need to rely on MetaCrtc to pass
   * logical monitor state.
   */
  crtc_info->x += data->logical_monitor_config->layout.x;
  crtc_info->y += data->logical_monitor_config->layout.y;

  /*
   * Only one output can be marked as primary (due to Xrandr limitation),
   * so only mark the main output of the first monitor in the logical monitor
   * as such.
   */
  first_monitor_config = data->logical_monitor_config->monitor_configs->data;
  if (data->logical_monitor_config->is_primary &&
      data->monitor_config == first_monitor_config &&
      meta_monitor_get_main_output (monitor) == output)
    assign_output_as_primary = TRUE;
  else
    assign_output_as_primary = FALSE;

  if (data->logical_monitor_config->is_presentation)
    assign_output_as_presentation = TRUE;
  else
    assign_output_as_presentation = FALSE;

  output_info = g_slice_new0 (MetaOutputInfo);
  *output_info = (MetaOutputInfo) {
    .output = output,
    .is_primary = assign_output_as_primary,
    .is_presentation = assign_output_as_presentation,
    .is_underscanning = data->monitor_config->enable_underscanning
  };

  g_ptr_array_add (data->crtc_infos, crtc_info);
  g_ptr_array_add (data->output_infos, output_info);

  return TRUE;
}

static gboolean
assign_monitor_crtcs (MetaMonitorManager       *manager,
                      MetaLogicalMonitorConfig *logical_monitor_config,
                      MetaMonitorConfig        *monitor_config,
                      GPtrArray                *crtc_infos,
                      GPtrArray                *output_infos,
                      GError                  **error)
{
  MetaMonitorSpec *monitor_spec = monitor_config->monitor_spec;
  MetaMonitorModeSpec *monitor_mode_spec = monitor_config->mode_spec;
  MetaMonitor *monitor;
  MetaMonitorMode *monitor_mode;
  MonitorAssignmentData data;

  monitor = meta_monitor_manager_get_monitor_from_spec (manager, monitor_spec);
  if (!monitor)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Configured monitor '%s %s' not found",
                   monitor_spec->vendor, monitor_spec->product);
      return FALSE;
    }

  monitor_mode = meta_monitor_get_mode_from_spec (monitor, monitor_mode_spec);
  if (!monitor_mode)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid mode %dx%d (%f) for monitor '%s %s'",
                   monitor_mode_spec->width, monitor_mode_spec->height,
                   monitor_mode_spec->refresh_rate,
                   monitor_spec->vendor, monitor_spec->product);
      return FALSE;
    }

  data = (MonitorAssignmentData) {
    .monitor_manager = manager,
    .logical_monitor_config = logical_monitor_config,
    .monitor_config = monitor_config,
    .crtc_infos = crtc_infos,
    .output_infos = output_infos
  };
  if (!meta_monitor_mode_foreach_crtc (monitor, monitor_mode,
                                       assign_monitor_crtc,
                                       &data,
                                       error))
    return FALSE;

  return TRUE;
}

static gboolean
assign_logical_monitor_crtcs (MetaMonitorManager       *manager,
                              MetaLogicalMonitorConfig *logical_monitor_config,
                              GPtrArray                *crtc_infos,
                              GPtrArray                *output_infos,
                              GError                  **error)
{
  GList *l;

  for (l = logical_monitor_config->monitor_configs; l; l = l->next)
    {
      MetaMonitorConfig *monitor_config = l->data;

      if (!assign_monitor_crtcs (manager,
                                 logical_monitor_config,
                                 monitor_config,
                                 crtc_infos, output_infos,
                                 error))
        return FALSE;
    }

  return TRUE;
}

gboolean
meta_monitor_config_manager_assign (MetaMonitorManager *manager,
                                    MetaMonitorsConfig *config,
                                    GPtrArray         **out_crtc_infos,
                                    GPtrArray         **out_output_infos,
                                    GError            **error)
{
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;
  GList *l;

  crtc_infos =
    g_ptr_array_new_with_free_func ((GDestroyNotify) meta_crtc_info_free);
  output_infos =
    g_ptr_array_new_with_free_func ((GDestroyNotify) meta_output_info_free);

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;

      if (!assign_logical_monitor_crtcs (manager, logical_monitor_config,
                                         crtc_infos, output_infos,
                                         error))
        {
          g_ptr_array_free (crtc_infos, TRUE);
          g_ptr_array_free (output_infos, TRUE);
          return FALSE;
        }
    }

  *out_crtc_infos = crtc_infos;
  *out_output_infos = output_infos;

  return TRUE;
}

static MetaMonitorsConfigKey *
create_key_for_current_state (MetaMonitorManager *monitor_manager)
{
  MetaMonitorsConfigKey *config_key;
  GList *l;
  GList *monitor_specs;

  monitor_specs = NULL;
  for (l = monitor_manager->monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaMonitorSpec *monitor_spec;

      if (meta_monitor_is_laptop_panel (monitor) &&
          meta_monitor_manager_is_lid_closed (monitor_manager))
        continue;

      monitor_spec = meta_monitor_spec_clone (meta_monitor_get_spec (monitor));
      monitor_specs = g_list_prepend (monitor_specs, monitor_spec);
    }

  if (!monitor_specs)
    return NULL;

  monitor_specs = g_list_sort (monitor_specs,
                               (GCompareFunc) meta_monitor_spec_compare);

  config_key = g_new0 (MetaMonitorsConfigKey, 1);
  *config_key = (MetaMonitorsConfigKey) {
    .monitor_specs = monitor_specs
  };

  return config_key;
}

MetaMonitorsConfig *
meta_monitor_config_manager_get_stored (MetaMonitorConfigManager *config_manager)
{
  MetaMonitorsConfigKey *config_key;
  MetaMonitorsConfig *config;

  config_key = create_key_for_current_state (config_manager->monitor_manager);
  if (!config_key)
    return NULL;

  config = meta_monitor_config_store_lookup (config_manager->config_store,
                                             config_key);
  meta_monitors_config_key_free (config_key);

  return config;
}

typedef enum _MonitorMatchRule
{
  MONITOR_MATCH_ALL = 0,
  MONITOR_MATCH_EXTERNAL = (1 << 0)
} MonitorMatchRule;

static MetaMonitor *
find_monitor_with_highest_preferred_resolution (MetaMonitorManager *monitor_manager,
                                                MonitorMatchRule    match_rule)
{
  GList *monitors;
  GList *l;
  int largest_area = 0;
  MetaMonitor *largest_monitor = NULL;

  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaMonitorMode *mode;
      int width, height;
      int area;

      if (match_rule & MONITOR_MATCH_EXTERNAL)
        {
          if (meta_monitor_is_laptop_panel (monitor))
            continue;
        }

      mode = meta_monitor_get_preferred_mode (monitor);
      meta_monitor_mode_get_resolution (mode, &width, &height);
      area = width * height;

      if (area > largest_area)
        {
          largest_area = area;
          largest_monitor = monitor;
        }
    }

  return largest_monitor;
}

/*
 * Try to find the primary monitor. The priority of classification is:
 *
 * 1. Find the primary monitor as reported by the underlying system,
 * 2. Find the laptop panel
 * 3. Find the external monitor with highest resolution
 *
 * If the laptop lid is closed, exclude the laptop panel from possible
 * alternatives, except if no other alternatives exist.
 */
static MetaMonitor *
find_primary_monitor (MetaMonitorManager *monitor_manager)
{
  MetaMonitor *monitor;

  if (meta_monitor_manager_is_lid_closed (monitor_manager))
    {
      monitor = meta_monitor_manager_get_primary_monitor (monitor_manager);
      if (monitor && !meta_monitor_is_laptop_panel (monitor))
        return monitor;

      monitor =
        find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                        MONITOR_MATCH_EXTERNAL);
      if (monitor)
        return monitor;

      return find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                             MONITOR_MATCH_ALL);
    }
  else
    {
      monitor = meta_monitor_manager_get_primary_monitor (monitor_manager);
      if (monitor)
        return monitor;

      monitor = meta_monitor_manager_get_laptop_panel (monitor_manager);
      if (monitor)
        return monitor;

      return find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                             MONITOR_MATCH_ALL);
    }
}

static MetaMonitorConfig *
create_monitor_config (MetaMonitor     *monitor,
                       MetaMonitorMode *mode)
{
  MetaMonitorSpec *monitor_spec;
  MetaMonitorModeSpec *mode_spec;
  MetaMonitorConfig *monitor_config;

  monitor_spec = meta_monitor_get_spec (monitor);
  mode_spec = meta_monitor_mode_get_spec (mode);

  monitor_config = g_new0 (MetaMonitorConfig, 1);
  *monitor_config = (MetaMonitorConfig) {
    .monitor_spec = meta_monitor_spec_clone (monitor_spec),
    .mode_spec = g_memdup (mode_spec, sizeof (MetaMonitorModeSpec)),
    .enable_underscanning = meta_monitor_is_underscanning (monitor)
  };

  return monitor_config;
}

static MetaLogicalMonitorConfig *
create_preferred_logical_monitor_config (MetaMonitorManager          *monitor_manager,
                                         MetaMonitor                 *monitor,
                                         int                          x,
                                         int                          y,
                                         MetaLogicalMonitorConfig    *primary_logical_monitor_config,
                                         MetaLogicalMonitorLayoutMode layout_mode)
{
  MetaMonitorMode *mode;
  int width, height;
  float scale;
  MetaMonitorConfig *monitor_config;
  MetaLogicalMonitorConfig *logical_monitor_config;

  mode = meta_monitor_get_preferred_mode (monitor);
  meta_monitor_mode_get_resolution (mode, &width, &height);

  if ((meta_monitor_manager_get_capabilities (monitor_manager) &
       META_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED) &&
      primary_logical_monitor_config)
    scale = primary_logical_monitor_config->scale;
  else
    scale = meta_monitor_manager_calculate_monitor_mode_scale (monitor_manager,
                                                               monitor,
                                                               mode);

  switch (layout_mode)
    {
    case META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
      width /= scale;
      height /= scale;
      break;
    case META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      break;
    }

  monitor_config = create_monitor_config (monitor, mode);

  logical_monitor_config = g_new0 (MetaLogicalMonitorConfig, 1);
  *logical_monitor_config = (MetaLogicalMonitorConfig) {
    .layout = (MetaRectangle) {
      .x = x,
      .y = y,
      .width = width,
      .height = height
    },
    .scale = scale,
    .monitor_configs = g_list_append (NULL, monitor_config)
  };

  return logical_monitor_config;
}

MetaMonitorsConfig *
meta_monitor_config_manager_create_linear (MetaMonitorConfigManager *config_manager)
{
  MetaMonitorManager *monitor_manager = config_manager->monitor_manager;
  GList *logical_monitor_configs;
  MetaMonitor *primary_monitor;
  MetaLogicalMonitorLayoutMode layout_mode;
  MetaLogicalMonitorConfig *primary_logical_monitor_config;
  int x;
  GList *monitors;
  GList *l;

  primary_monitor = find_primary_monitor (monitor_manager);
  if (!primary_monitor)
    return NULL;

  layout_mode = meta_monitor_manager_get_default_layout_mode (monitor_manager);

  primary_logical_monitor_config =
    create_preferred_logical_monitor_config (monitor_manager,
                                             primary_monitor,
                                             0, 0,
                                             NULL,
                                             layout_mode);
  primary_logical_monitor_config->is_primary = TRUE;
  logical_monitor_configs = g_list_append (NULL,
                                           primary_logical_monitor_config);

  x = primary_logical_monitor_config->layout.width;
  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaLogicalMonitorConfig *logical_monitor_config;

      if (monitor == primary_monitor)
        continue;

      if (meta_monitor_is_laptop_panel (monitor) &&
          meta_monitor_manager_is_lid_closed (monitor_manager))
        continue;

      logical_monitor_config =
        create_preferred_logical_monitor_config (monitor_manager,
                                                 monitor,
                                                 x, 0,
                                                 primary_logical_monitor_config,
                                                 layout_mode);
      logical_monitor_configs = g_list_append (logical_monitor_configs,
                                               logical_monitor_config);

      x += logical_monitor_config->layout.width;
    }

  return meta_monitors_config_new (logical_monitor_configs, layout_mode);
}

MetaMonitorsConfig *
meta_monitor_config_manager_create_fallback (MetaMonitorConfigManager *config_manager)
{
  MetaMonitorManager *monitor_manager = config_manager->monitor_manager;
  MetaMonitor *primary_monitor;
  GList *logical_monitor_configs;
  MetaLogicalMonitorLayoutMode layout_mode;
  MetaLogicalMonitorConfig *primary_logical_monitor_config;

  primary_monitor = find_primary_monitor (monitor_manager);
  if (!primary_monitor)
    return NULL;

  layout_mode = meta_monitor_manager_get_default_layout_mode (monitor_manager);

  primary_logical_monitor_config =
    create_preferred_logical_monitor_config (monitor_manager,
                                             primary_monitor,
                                             0, 0,
                                             NULL,
                                             layout_mode);
  primary_logical_monitor_config->is_primary = TRUE;
  logical_monitor_configs = g_list_append (NULL,
                                           primary_logical_monitor_config);

  return meta_monitors_config_new (logical_monitor_configs, layout_mode);
}

MetaMonitorsConfig *
meta_monitor_config_manager_create_suggested (MetaMonitorConfigManager *config_manager)
{
  MetaMonitorManager *monitor_manager = config_manager->monitor_manager;
  MetaLogicalMonitorConfig *primary_logical_monitor_config = NULL;
  MetaMonitor *primary_monitor;
  MetaLogicalMonitorLayoutMode layout_mode;
  GList *logical_monitor_configs;
  GList *region;
  int x, y;
  GList *monitors;
  GList *l;

  primary_monitor = find_primary_monitor (monitor_manager);
  if (!primary_monitor)
    return NULL;

  if (!meta_monitor_get_suggested_position (primary_monitor, &x, &y))
    return NULL;

  layout_mode = meta_monitor_manager_get_default_layout_mode (monitor_manager);

  primary_logical_monitor_config =
    create_preferred_logical_monitor_config (monitor_manager,
                                             primary_monitor,
                                             x, y,
                                             NULL,
                                             layout_mode);
  primary_logical_monitor_config->is_primary = TRUE;
  logical_monitor_configs = g_list_append (NULL,
                                           primary_logical_monitor_config);
  region = g_list_prepend (NULL, &primary_logical_monitor_config->layout);

  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaLogicalMonitorConfig *logical_monitor_config;

      if (monitor == primary_monitor)
        continue;

      if (!meta_monitor_get_suggested_position (monitor, &x, &y))
        continue;

      logical_monitor_config =
        create_preferred_logical_monitor_config (monitor_manager,
                                                 monitor,
                                                 x, y,
                                                 primary_logical_monitor_config,
                                                 layout_mode);
      logical_monitor_configs = g_list_append (logical_monitor_configs,
                                               logical_monitor_config);

      if (meta_rectangle_overlaps_with_region (region,
                                               &logical_monitor_config->layout))
        {
          g_warning ("Suggested monitor config has overlapping region, rejecting");
          g_list_free (region);
          g_list_free_full (logical_monitor_configs,
                            (GDestroyNotify) meta_logical_monitor_config_free);
          return NULL;
        }

      region = g_list_prepend (region, &logical_monitor_config->layout);
    }

  g_list_free (region);

  if (!logical_monitor_configs)
    return NULL;

  return meta_monitors_config_new (logical_monitor_configs, layout_mode);
}

void
meta_monitor_config_manager_set_current (MetaMonitorConfigManager *config_manager,
                                         MetaMonitorsConfig       *config)
{
  g_set_object (&config_manager->previous_config,
                config_manager->current_config);
  g_set_object (&config_manager->current_config, config);
}

void
meta_monitor_config_manager_save_current (MetaMonitorConfigManager *config_manager)
{
  g_return_if_fail (config_manager->current_config);

  meta_monitor_config_store_add (config_manager->config_store,
                                 config_manager->current_config);
}

MetaMonitorsConfig *
meta_monitor_config_manager_get_current (MetaMonitorConfigManager *config_manager)
{
  return config_manager->current_config;
}

MetaMonitorsConfig *
meta_monitor_config_manager_get_previous (MetaMonitorConfigManager *config_manager)
{
  return config_manager->previous_config;
}

static void
meta_monitor_config_manager_dispose (GObject *object)
{
  MetaMonitorConfigManager *config_manager =
    META_MONITOR_CONFIG_MANAGER (object);

  g_clear_object (&config_manager->current_config);
  g_clear_object (&config_manager->previous_config);

  G_OBJECT_CLASS (meta_monitor_config_manager_parent_class)->dispose (object);
}

static void
meta_monitor_config_manager_init (MetaMonitorConfigManager *config_manager)
{
}

static void
meta_monitor_config_manager_class_init (MetaMonitorConfigManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_monitor_config_manager_dispose;
}

void
meta_monitor_config_free (MetaMonitorConfig *monitor_config)
{
  meta_monitor_spec_free (monitor_config->monitor_spec);
  g_free (monitor_config->mode_spec);
  g_free (monitor_config);
}

void
meta_logical_monitor_config_free (MetaLogicalMonitorConfig *logical_monitor_config)
{
  g_list_free_full (logical_monitor_config->monitor_configs,
                    (GDestroyNotify) meta_monitor_config_free);
  g_free (logical_monitor_config);
}

static MetaMonitorsConfigKey *
meta_monitors_config_key_new (GList *logical_monitor_configs)
{
  MetaMonitorsConfigKey *config_key;
  GList *monitor_specs;
  GList *l;

  monitor_specs = NULL;
  for (l = logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      GList *k;

      for (k = logical_monitor_config->monitor_configs; k; k = k->next)
        {
          MetaMonitorConfig *monitor_config = k->data;
          MetaMonitorSpec *monitor_spec;

          monitor_spec = meta_monitor_spec_clone (monitor_config->monitor_spec);
          monitor_specs = g_list_prepend (monitor_specs, monitor_spec);
        }
    }

  monitor_specs = g_list_sort (monitor_specs,
                               (GCompareFunc) meta_monitor_spec_compare);

  config_key = g_new0 (MetaMonitorsConfigKey, 1);
  *config_key = (MetaMonitorsConfigKey) {
    .monitor_specs = monitor_specs
  };

  return config_key;
}

void
meta_monitors_config_key_free (MetaMonitorsConfigKey *config_key)
{
  g_list_free_full (config_key->monitor_specs,
                    (GDestroyNotify) meta_monitor_spec_free);
  g_free (config_key);
}

unsigned int
meta_monitors_config_key_hash (gconstpointer data)
{
  const MetaMonitorsConfigKey *config_key = data;
  GList *l;
  unsigned long hash;

  hash = 0;
  for (l = config_key->monitor_specs; l; l = l->next)
    {
      MetaMonitorSpec *monitor_spec = l->data;

      hash ^= (g_str_hash (monitor_spec->connector) ^
               g_str_hash (monitor_spec->vendor) ^
               g_str_hash (monitor_spec->product) ^
               g_str_hash (monitor_spec->serial));
    }

  return hash;
}

gboolean
meta_monitors_config_key_equal (gconstpointer data_a,
                                gconstpointer data_b)
{
  const MetaMonitorsConfigKey *config_key_a = data_a;
  const MetaMonitorsConfigKey *config_key_b = data_b;
  GList *l_a, *l_b;

  for (l_a = config_key_a->monitor_specs, l_b = config_key_b->monitor_specs;
       l_a && l_b;
       l_a = l_a->next, l_b = l_b->next)
    {
      MetaMonitorSpec *monitor_spec_a = l_a->data;
      MetaMonitorSpec *monitor_spec_b = l_b->data;

      if (!meta_monitor_spec_equals (monitor_spec_a, monitor_spec_b))
        return FALSE;
    }

  if (l_a || l_b)
    return FALSE;

  return TRUE;
}

MetaMonitorsConfig *
meta_monitors_config_new (GList                       *logical_monitor_configs,
                          MetaLogicalMonitorLayoutMode layout_mode)
{
  MetaMonitorsConfig *config;

  config = g_object_new (META_TYPE_MONITORS_CONFIG, NULL);
  config->logical_monitor_configs = logical_monitor_configs;
  config->key = meta_monitors_config_key_new (logical_monitor_configs);
  config->layout_mode = layout_mode;

  return config;
}

static void
meta_monitors_config_finalize (GObject *object)
{
  MetaMonitorsConfig *config = META_MONITORS_CONFIG (object);

  meta_monitors_config_key_free (config->key);
  g_list_free_full (config->logical_monitor_configs,
                    (GDestroyNotify) meta_logical_monitor_config_free);
}

static void
meta_monitors_config_init (MetaMonitorsConfig *config)
{
}

static void
meta_monitors_config_class_init (MetaMonitorsConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_monitors_config_finalize;
}

gboolean
meta_verify_monitor_mode_spec (MetaMonitorModeSpec *monitor_mode_spec,
                               GError             **error)
{
  if (monitor_mode_spec->width > 0 &&
      monitor_mode_spec->height > 0 &&
      monitor_mode_spec->refresh_rate > 0.0f)
    {
      return TRUE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor mode invalid");
      return FALSE;
    }
}

gboolean
meta_verify_monitor_spec (MetaMonitorSpec *monitor_spec,
                          GError         **error)
{
  if (monitor_spec->connector &&
      monitor_spec->vendor &&
      monitor_spec->product &&
      monitor_spec->serial)
    {
      return TRUE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor spec incomplete");
      return FALSE;
    }
}

gboolean
meta_verify_monitor_config (MetaMonitorConfig *monitor_config,
                            GError           **error)
{
  if (monitor_config->monitor_spec && monitor_config->mode_spec)
    {
      return TRUE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor config incomplete");
      return FALSE;
    }
}

gboolean
meta_verify_logical_monitor_config (MetaLogicalMonitorConfig    *logical_monitor_config,
                                    MetaLogicalMonitorLayoutMode layout_mode,
                                    MetaMonitorManager          *monitor_manager,
                                    GError                     **error)
{
  GList *l;
  int expected_mode_width = 0;
  int expected_mode_height = 0;

  if (!meta_monitor_manager_is_scale_supported (monitor_manager,
                                                layout_mode,
                                                logical_monitor_config->scale))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid logical monitor config scale %g",
                   logical_monitor_config->scale);
      return FALSE;
    }

  if (logical_monitor_config->layout.x < 0 ||
      logical_monitor_config->layout.y < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid logical monitor position (%d, %d)",
                   logical_monitor_config->layout.x,
                   logical_monitor_config->layout.y);
      return FALSE;
    }

  if (!logical_monitor_config->monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Logical monitor is empty");
      return FALSE;
    }

  if (meta_monitor_transform_is_rotated (logical_monitor_config->transform))
    {
      expected_mode_width = logical_monitor_config->layout.height;
      expected_mode_height = logical_monitor_config->layout.width;
    }
  else
    {
      expected_mode_width = logical_monitor_config->layout.width;
      expected_mode_height = logical_monitor_config->layout.height;
    }

  switch (layout_mode)
    {
    case META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
      expected_mode_width *= logical_monitor_config->scale;
      expected_mode_height *= logical_monitor_config->scale;
      break;
    case META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      break;
    }

  for (l = logical_monitor_config->monitor_configs; l; l = l->next)
    {
      MetaMonitorConfig *monitor_config = l->data;

      if (monitor_config->mode_spec->width != expected_mode_width ||
          monitor_config->mode_spec->height != expected_mode_height)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Monitor modes in logical monitor conflict");
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
has_adjecent_neighbour (MetaMonitorsConfig       *config,
                        MetaLogicalMonitorConfig *logical_monitor_config)
{
  GList *l;

  if (!config->logical_monitor_configs->next)
    {
      g_assert (config->logical_monitor_configs->data ==
                logical_monitor_config);
      return TRUE;
    }

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *other_logical_monitor_config = l->data;

      if (logical_monitor_config == other_logical_monitor_config)
        continue;

      if (meta_rectangle_is_adjecent_to (&logical_monitor_config->layout,
                                         &other_logical_monitor_config->layout))
        return TRUE;
    }

  return FALSE;
}

gboolean
meta_verify_monitors_config (MetaMonitorsConfig *config,
                             MetaMonitorManager *monitor_manager,
                             GError            **error)
{
  int min_x, min_y;
  gboolean has_primary;
  GList *region;
  GList *l;
  gboolean global_scale_required;

  if (!config->logical_monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitors config incomplete");
      return FALSE;
    }

  global_scale_required =
    !!(meta_monitor_manager_get_capabilities (monitor_manager) &
       META_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED);

  min_x = INT_MAX;
  min_y = INT_MAX;
  region = NULL;
  has_primary = FALSE;
  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;

      if (global_scale_required)
        {
          MetaLogicalMonitorConfig *prev_logical_monitor_config =
            l->prev ? l->prev->data : NULL;

          if (prev_logical_monitor_config &&
              (prev_logical_monitor_config->scale !=
               logical_monitor_config->scale))
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Logical monitor scales must be identical");
              return FALSE;
            }
        }

      if (meta_rectangle_overlaps_with_region (region,
                                               &logical_monitor_config->layout))
        {
          g_list_free (region);
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Logical monitors overlap");
          return FALSE;
        }

      if (has_primary && logical_monitor_config->is_primary)
        {
          g_list_free (region);
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Config contains multiple primary logical monitors");
          return FALSE;
        }
      else if (logical_monitor_config->is_primary)
        {
          has_primary = TRUE;
        }

      if (!has_adjecent_neighbour (config, logical_monitor_config))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Logical monitors not adjecent");
          return FALSE;
        }

      min_x = MIN (logical_monitor_config->layout.x, min_x);
      min_y = MIN (logical_monitor_config->layout.y, min_y);

      region = g_list_prepend (region, &logical_monitor_config->layout);
    }

  g_list_free (region);

  if (min_x != 0 || min_y != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Logical monitors positions are offset");
      return FALSE;
    }

  if (!has_primary)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Config is missing primary logical");
      return FALSE;
    }

  return TRUE;
}
