/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "meta-monitor-manager-private.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <clutter/clutter.h>

#include <meta/main.h>
#include "util-private.h"
#include <meta/errors.h>
#include "edid.h"
#include "backends/meta-crtc.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor.h"
#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-orientation-manager.h"
#include "backends/meta-output.h"
#include "backends/x11/meta-monitor-manager-xrandr.h"
#include "meta-backend-private.h"

#define DEFAULT_DISPLAY_CONFIGURATION_TIMEOUT 20

enum
{
  PROP_0,

  PROP_BACKEND,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

enum {
  MONITORS_CHANGED_INTERNAL,
  CONFIRM_DISPLAY_CHANGE,
  SIGNALS_LAST
};

/* Array index matches MetaMonitorTransform */
static gfloat transform_matrices[][6] = {
  {  1,  0,  0,  0,  1,  0 }, /* normal */
  {  0, -1,  1,  1,  0,  0 }, /* 90° */
  { -1,  0,  1,  0, -1,  1 }, /* 180° */
  {  0,  1,  0, -1,  0,  1 }, /* 270° */
  { -1,  0,  1,  0,  1,  0 }, /* normal flipped */
  {  0,  1,  0,  1,  0,  0 }, /* 90° flipped */
  {  1,  0,  0,  0, -1,  1 }, /* 180° flipped */
  {  0, -1,  1, -1,  0,  1 }, /* 270° flipped */
};

static int signals[SIGNALS_LAST];

static void meta_monitor_manager_display_config_init (MetaDBusDisplayConfigIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MetaMonitorManager, meta_monitor_manager, META_DBUS_TYPE_DISPLAY_CONFIG_SKELETON,
                                  G_IMPLEMENT_INTERFACE (META_DBUS_TYPE_DISPLAY_CONFIG, meta_monitor_manager_display_config_init));

static void initialize_dbus_interface (MetaMonitorManager *manager);

static gboolean
meta_monitor_manager_is_config_complete (MetaMonitorManager *manager,
                                         MetaMonitorsConfig *config);

MetaBackend *
meta_monitor_manager_get_backend (MetaMonitorManager *manager)
{
  return manager->backend;
}

static void
meta_monitor_manager_init (MetaMonitorManager *manager)
{
}

static void
meta_monitor_manager_set_primary_logical_monitor (MetaMonitorManager *manager,
                                                  MetaLogicalMonitor *logical_monitor)
{
  manager->primary_logical_monitor = logical_monitor;
  if (logical_monitor)
    meta_logical_monitor_make_primary (logical_monitor);
}

static gboolean
is_main_tiled_monitor_output (MetaOutput *output)
{
  return output->tile_info.loc_h_tile == 0 && output->tile_info.loc_v_tile == 0;
}

static MetaLogicalMonitor *
logical_monitor_from_layout (MetaMonitorManager *manager,
                             GList              *logical_monitors,
                             MetaRectangle      *layout)
{
  GList *l;

  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      if (meta_rectangle_equal (layout, &logical_monitor->rect))
        return logical_monitor;
    }

  return NULL;
}

static void
meta_monitor_manager_rebuild_logical_monitors (MetaMonitorManager *manager,
                                               MetaMonitorsConfig *config)
{
  GList *logical_monitor_configs;
  GList *logical_monitors = NULL;
  GList *l;
  int monitor_number = 0;
  MetaLogicalMonitor *primary_logical_monitor = NULL;

  logical_monitor_configs = config ? config->logical_monitor_configs : NULL;
  for (l = logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      MetaLogicalMonitor *logical_monitor;

      logical_monitor = meta_logical_monitor_new (manager,
                                                  logical_monitor_config,
                                                  monitor_number);
      monitor_number++;

      if (logical_monitor_config->is_primary)
        primary_logical_monitor = logical_monitor;

      logical_monitors = g_list_append (logical_monitors, logical_monitor);
    }

  /*
   * If no monitor was marked as primary, fall back on marking the first
   * logical monitor the primary one.
   */
  if (!primary_logical_monitor && logical_monitors)
    primary_logical_monitor = g_list_first (logical_monitors)->data;

  manager->logical_monitors = logical_monitors;
  meta_monitor_manager_set_primary_logical_monitor (manager,
                                                    primary_logical_monitor);
}

static float
derive_configured_global_scale (MetaMonitorManager *manager,
                                MetaMonitorsConfig *config)
{
  MetaLogicalMonitorConfig *logical_monitor_config;

  logical_monitor_config = config->logical_monitor_configs->data;

  return logical_monitor_config->scale;
}

static float
calculate_monitor_scale (MetaMonitorManager *manager,
                         MetaMonitor        *monitor)
{
  MetaMonitorMode *monitor_mode;

  monitor_mode = meta_monitor_get_current_mode (monitor);
  return meta_monitor_manager_calculate_monitor_mode_scale (manager,
                                                            monitor,
                                                            monitor_mode);
}

static float
derive_calculated_global_scale (MetaMonitorManager *manager)
{
  MetaMonitor *primary_monitor;

  primary_monitor = meta_monitor_manager_get_primary_monitor (manager);
  if (!primary_monitor)
    return 1.0;

  return calculate_monitor_scale (manager, primary_monitor);
}

static float
derive_scale_from_config (MetaMonitorManager *manager,
                          MetaMonitorsConfig *config,
                          MetaRectangle      *layout)
{
  GList *l;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;

      if (meta_rectangle_equal (layout, &logical_monitor_config->layout))
        return logical_monitor_config->scale;
    }

  g_warning ("Missing logical monitor, using scale 1");
  return 1.0;
}

static void
meta_monitor_manager_rebuild_logical_monitors_derived (MetaMonitorManager *manager,
                                                       MetaMonitorsConfig *config)
{
  GList *logical_monitors = NULL;
  GList *l;
  int monitor_number;
  MetaLogicalMonitor *primary_logical_monitor = NULL;
  gboolean use_global_scale;
  float global_scale = 0.0;
  MetaMonitorManagerCapability capabilities;

  monitor_number = 0;

  capabilities = meta_monitor_manager_get_capabilities (manager);
  use_global_scale =
    !!(capabilities & META_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED);

  if (use_global_scale)
    {
      if (config)
        global_scale = derive_configured_global_scale (manager, config);
      else
        global_scale = derive_calculated_global_scale (manager);
    }

  for (l = manager->monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaLogicalMonitor *logical_monitor;
      MetaRectangle layout;

      if (!meta_monitor_is_active (monitor))
        continue;

      meta_monitor_derive_layout (monitor, &layout);
      logical_monitor = logical_monitor_from_layout (manager, logical_monitors,
                                                     &layout);
      if (logical_monitor)
        {
          meta_logical_monitor_add_monitor (logical_monitor, monitor);
        }
      else
        {
          float scale;

          if (use_global_scale)
            scale = global_scale;
          else if (config)
            scale = derive_scale_from_config (manager, config, &layout);
          else
            scale = calculate_monitor_scale (manager, monitor);

          g_assert (scale > 0);

          logical_monitor = meta_logical_monitor_new_derived (manager,
                                                              monitor,
                                                              &layout,
                                                              scale,
                                                              monitor_number);
          logical_monitors = g_list_append (logical_monitors, logical_monitor);
          monitor_number++;
        }

      if (meta_monitor_is_primary (monitor))
        primary_logical_monitor = logical_monitor;
    }

  manager->logical_monitors = logical_monitors;

  /*
   * If no monitor was marked as primary, fall back on marking the first
   * logical monitor the primary one.
   */
  if (!primary_logical_monitor && manager->logical_monitors)
    primary_logical_monitor = g_list_first (manager->logical_monitors)->data;

  meta_monitor_manager_set_primary_logical_monitor (manager,
                                                    primary_logical_monitor);
}

static void
power_save_mode_changed (MetaMonitorManager *manager,
                         GParamSpec         *pspec,
                         gpointer            user_data)
{
  MetaMonitorManagerClass *klass;
  int mode = meta_dbus_display_config_get_power_save_mode (META_DBUS_DISPLAY_CONFIG (manager));

  if (mode == META_POWER_SAVE_UNSUPPORTED)
    return;

  /* If DPMS is unsupported, force the property back. */
  if (manager->power_save_mode == META_POWER_SAVE_UNSUPPORTED)
    {
      meta_dbus_display_config_set_power_save_mode (META_DBUS_DISPLAY_CONFIG (manager), META_POWER_SAVE_UNSUPPORTED);
      return;
    }

  klass = META_MONITOR_MANAGER_GET_CLASS (manager);
  if (klass->set_power_save_mode)
    klass->set_power_save_mode (manager, mode);

  manager->power_save_mode = mode;
}

void
meta_monitor_manager_lid_is_closed_changed (MetaMonitorManager *manager)
{
  meta_monitor_manager_ensure_configured (manager);
}

static void
lid_is_closed_changed (UpClient   *client,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  MetaMonitorManager *manager = user_data;

  meta_monitor_manager_lid_is_closed_changed (manager);
}

static gboolean
meta_monitor_manager_real_is_lid_closed (MetaMonitorManager *manager)
{
  if (!manager->up_client)
    return FALSE;

  return up_client_get_lid_is_closed (manager->up_client);
}

gboolean
meta_monitor_manager_is_lid_closed (MetaMonitorManager *manager)
{
  return META_MONITOR_MANAGER_GET_CLASS (manager)->is_lid_closed (manager);
}

gboolean
meta_monitor_manager_is_headless (MetaMonitorManager *manager)
{
  return !manager->logical_monitors;
}

float
meta_monitor_manager_calculate_monitor_mode_scale (MetaMonitorManager *manager,
                                                   MetaMonitor        *monitor,
                                                   MetaMonitorMode    *monitor_mode)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->calculate_monitor_mode_scale (manager,
                                                      monitor,
                                                      monitor_mode);
}

float *
meta_monitor_manager_calculate_supported_scales (MetaMonitorManager          *manager,
                                                 MetaLogicalMonitorLayoutMode layout_mode,
                                                 MetaMonitor                 *monitor,
                                                 MetaMonitorMode             *monitor_mode,
                                                 int                         *n_supported_scales)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->calculate_supported_scales (manager,
                                                    layout_mode,
                                                    monitor,
                                                    monitor_mode,
                                                    n_supported_scales);
}

MetaMonitorManagerCapability
meta_monitor_manager_get_capabilities (MetaMonitorManager *manager)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->get_capabilities (manager);
}

gboolean
meta_monitor_manager_get_max_screen_size (MetaMonitorManager *manager,
                                          int                *max_width,
                                          int                *max_height)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->get_max_screen_size (manager, max_width, max_height);
}


MetaLogicalMonitorLayoutMode
meta_monitor_manager_get_default_layout_mode (MetaMonitorManager *manager)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->get_default_layout_mode (manager);
}

static void
meta_monitor_manager_ensure_initial_config (MetaMonitorManager *manager)
{
  META_MONITOR_MANAGER_GET_CLASS (manager)->ensure_initial_config (manager);
}

static gboolean
meta_monitor_manager_apply_monitors_config (MetaMonitorManager      *manager,
                                            MetaMonitorsConfig      *config,
                                            MetaMonitorsConfigMethod method,
                                            GError                 **error)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  g_assert (!config ||
            !(config->flags & META_MONITORS_CONFIG_FLAG_MIGRATED));

  if (!manager_class->apply_monitors_config (manager, config, method, error))
    return FALSE;

  switch (method)
    {
    case META_MONITORS_CONFIG_METHOD_TEMPORARY:
    case META_MONITORS_CONFIG_METHOD_PERSISTENT:
      meta_monitor_config_manager_set_current (manager->config_manager, config);
      break;
    case META_MONITORS_CONFIG_METHOD_VERIFY:
      break;
    }

  return TRUE;
}

gboolean
meta_monitor_manager_has_hotplug_mode_update (MetaMonitorManager *manager)
{
  GList *l;

  for (l = manager->gpus; l; l = l->next)
    {
      MetaGpu *gpu = l->data;

      if (meta_gpu_has_hotplug_mode_update (gpu))
        return TRUE;
    }

  return FALSE;
}

static gboolean
should_use_stored_config (MetaMonitorManager *manager)
{
  return (manager->in_init ||
          !meta_monitor_manager_has_hotplug_mode_update (manager));
}

MetaMonitorsConfig *
meta_monitor_manager_ensure_configured (MetaMonitorManager *manager)
{
  MetaMonitorsConfig *config = NULL;
  GError *error = NULL;
  gboolean use_stored_config;
  MetaMonitorsConfigMethod method;
  MetaMonitorsConfigMethod fallback_method =
    META_MONITORS_CONFIG_METHOD_TEMPORARY;

  use_stored_config = should_use_stored_config (manager);
  if (use_stored_config)
    method = META_MONITORS_CONFIG_METHOD_PERSISTENT;
  else
    method = META_MONITORS_CONFIG_METHOD_TEMPORARY;

  if (use_stored_config)
    {
      config = meta_monitor_config_manager_get_stored (manager->config_manager);
      if (config)
        {
          if (!meta_monitor_manager_apply_monitors_config (manager,
                                                           config,
                                                           method,
                                                           &error))
            {
              config = NULL;
              g_warning ("Failed to use stored monitor configuration: %s",
                         error->message);
              g_clear_error (&error);
            }
          else
            {
              g_object_ref (config);
              goto done;
            }
        }
    }

  config = meta_monitor_config_manager_create_suggested (manager->config_manager);
  if (config)
    {
      if (!meta_monitor_manager_apply_monitors_config (manager,
                                                       config,
                                                       method,
                                                       &error))
        {
          g_clear_object (&config);
          g_warning ("Failed to use suggested monitor configuration: %s",
                     error->message);
          g_clear_error (&error);
        }
      else
        {
          goto done;
        }
    }

  config = meta_monitor_config_manager_get_previous (manager->config_manager);
  if (config)
    {
      config = g_object_ref (config);

      if (meta_monitor_manager_is_config_complete (manager, config))
        {
          if (!meta_monitor_manager_apply_monitors_config (manager,
                                                           config,
                                                           method,
                                                           &error))
            {
              g_warning ("Failed to use suggested monitor configuration: %s",
                         error->message);
              g_clear_error (&error);
            }
          else
            {
              goto done;
            }
        }

      g_clear_object (&config);
    }

  config = meta_monitor_config_manager_create_linear (manager->config_manager);
  if (config)
    {
      if (!meta_monitor_manager_apply_monitors_config (manager,
                                                       config,
                                                       method,
                                                       &error))
        {
          g_clear_object (&config);
          g_warning ("Failed to use linear monitor configuration: %s",
                     error->message);
          g_clear_error (&error);
        }
      else
        {
          goto done;
        }
    }

  config = meta_monitor_config_manager_create_fallback (manager->config_manager);
  if (config)
    {
      if (!meta_monitor_manager_apply_monitors_config (manager,
                                                       config,
                                                       fallback_method,
                                                       &error))
        {
          g_clear_object (&config);
          g_warning ("Failed to use fallback monitor configuration: %s",
                 error->message);
          g_clear_error (&error);
        }
      else
        {
          goto done;
        }
    }

done:
  if (!config)
    {
      meta_monitor_manager_apply_monitors_config (manager,
                                                  NULL,
                                                  fallback_method,
                                                  &error);
      return NULL;
    }

  g_object_unref (config);

  return config;
}

static void
orientation_changed (MetaOrientationManager *orientation_manager,
                     MetaMonitorManager     *manager)
{
  MetaMonitorTransform transform;
  GError *error = NULL;
  MetaMonitorsConfig *config;

  switch (meta_orientation_manager_get_orientation (orientation_manager))
    {
    case META_ORIENTATION_NORMAL:
      transform = META_MONITOR_TRANSFORM_NORMAL;
      break;
    case META_ORIENTATION_BOTTOM_UP:
      transform = META_MONITOR_TRANSFORM_180;
      break;
    case META_ORIENTATION_LEFT_UP:
      transform = META_MONITOR_TRANSFORM_90;
      break;
    case META_ORIENTATION_RIGHT_UP:
      transform = META_MONITOR_TRANSFORM_270;
      break;

    case META_ORIENTATION_UNDEFINED:
    default:
      return;
    }

  config =
    meta_monitor_config_manager_create_for_orientation (manager->config_manager,
                                                        transform);
  if (!config)
    return;

  if (!meta_monitor_manager_apply_monitors_config (manager,
                                                   config,
                                                   META_MONITORS_CONFIG_METHOD_TEMPORARY,
                                                   &error))
    {
      g_warning ("Failed to use orientation monitor configuration: %s",
                 error->message);
      g_error_free (error);
    }
  g_object_unref (config);
}

static void
experimental_features_changed (MetaSettings           *settings,
                               MetaExperimentalFeature old_experimental_features,
                               MetaMonitorManager     *manager)
{
  gboolean was_stage_views_scaled;
  gboolean is_stage_views_scaled;
  gboolean should_reconfigure = FALSE;

  was_stage_views_scaled =
    !!(old_experimental_features &
       META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER);
  is_stage_views_scaled =
    meta_settings_is_experimental_feature_enabled (
      settings,
      META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER);

  if (is_stage_views_scaled != was_stage_views_scaled)
    should_reconfigure = TRUE;

  if (should_reconfigure)
    meta_monitor_manager_on_hotplug (manager);

  meta_settings_update_ui_scaling_factor (settings);
}

void
meta_monitor_manager_setup (MetaMonitorManager *manager)
{
  manager->in_init = TRUE;

  manager->config_manager = meta_monitor_config_manager_new (manager);

  meta_monitor_manager_read_current_state (manager);

  meta_monitor_manager_ensure_initial_config (manager);

  manager->in_init = FALSE;
}

static void
meta_monitor_manager_constructed (GObject *object)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (object);
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);
  MetaBackend *backend = manager->backend;
  MetaSettings *settings = meta_backend_get_settings (backend);

  manager->experimental_features_changed_handler_id =
    g_signal_connect (settings,
                      "experimental-features-changed",
                      G_CALLBACK (experimental_features_changed),
                      manager);

  if (manager_class->is_lid_closed == meta_monitor_manager_real_is_lid_closed)
    {
      manager->up_client = up_client_new ();
      g_signal_connect_object (manager->up_client, "notify::lid-is-closed",
                               G_CALLBACK (lid_is_closed_changed), manager, 0);
    }

  g_signal_connect_object (manager, "notify::power-save-mode",
                           G_CALLBACK (power_save_mode_changed), manager, 0);

  g_signal_connect_object (meta_backend_get_orientation_manager (backend),
                           "orientation-changed",
                           G_CALLBACK (orientation_changed),
                           manager, 0);

  manager->current_switch_config = META_MONITOR_SWITCH_CONFIG_UNKNOWN;

  initialize_dbus_interface (manager);
}

static void
meta_monitor_manager_finalize (GObject *object)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (object);

  g_list_free_full (manager->gpus, g_object_unref);
  g_list_free_full (manager->logical_monitors, g_object_unref);

  g_signal_handler_disconnect (manager->backend,
                               manager->experimental_features_changed_handler_id);

  G_OBJECT_CLASS (meta_monitor_manager_parent_class)->finalize (object);
}

static void
meta_monitor_manager_dispose (GObject *object)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (object);

  if (manager->dbus_name_id != 0)
    {
      g_bus_unown_name (manager->dbus_name_id);
      manager->dbus_name_id = 0;
    }

  g_clear_object (&manager->config_manager);
  g_clear_object (&manager->up_client);

  G_OBJECT_CLASS (meta_monitor_manager_parent_class)->dispose (object);
}

static GBytes *
meta_monitor_manager_real_read_edid (MetaMonitorManager *manager,
                                     MetaOutput         *output)
{
  return NULL;
}

static char *
meta_monitor_manager_real_get_edid_file (MetaMonitorManager *manager,
                                         MetaOutput         *output)
{
  return NULL;
}

static void
meta_monitor_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      manager->backend = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_monitor_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      g_value_set_object (value, manager->backend);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_monitor_manager_class_init (MetaMonitorManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = meta_monitor_manager_constructed;
  object_class->dispose = meta_monitor_manager_dispose;
  object_class->finalize = meta_monitor_manager_finalize;
  object_class->get_property = meta_monitor_manager_get_property;
  object_class->set_property = meta_monitor_manager_set_property;

  klass->get_edid_file = meta_monitor_manager_real_get_edid_file;
  klass->read_edid = meta_monitor_manager_real_read_edid;
  klass->is_lid_closed = meta_monitor_manager_real_is_lid_closed;

  signals[MONITORS_CHANGED_INTERNAL] =
    g_signal_new ("monitors-changed-internal",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[CONFIRM_DISPLAY_CHANGE] =
    g_signal_new ("confirm-display-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  0,
                  NULL, NULL, NULL,
		  G_TYPE_NONE, 0);

  obj_props[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "backend",
                         "MetaBackend",
                         META_TYPE_BACKEND,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static const double known_diagonals[] = {
    12.1,
    13.3,
    15.6
};

static char *
diagonal_to_str (double d)
{
  unsigned int i;

  for (i = 0; i < G_N_ELEMENTS (known_diagonals); i++)
    {
      double delta;

      delta = fabs(known_diagonals[i] - d);
      if (delta < 0.1)
        return g_strdup_printf ("%0.1lf\"", known_diagonals[i]);
    }

  return g_strdup_printf ("%d\"", (int) (d + 0.5));
}

static char *
make_display_name (MetaMonitorManager *manager,
                   MetaOutput         *output)
{
  g_autofree char *inches = NULL;
  g_autofree char *vendor_name = NULL;

  if (meta_output_is_laptop (output))
      return g_strdup (_("Built-in display"));

  if (output->width_mm > 0 && output->height_mm > 0)
    {
      double d = sqrt (output->width_mm * output->width_mm +
                       output->height_mm * output->height_mm);
      inches = diagonal_to_str (d / 25.4);
    }

  if (g_strcmp0 (output->vendor, "unknown") != 0)
    {
      if (!manager->pnp_ids)
        manager->pnp_ids = gnome_pnp_ids_new ();

      vendor_name = gnome_pnp_ids_get_pnp_id (manager->pnp_ids,
                                              output->vendor);

      if (!vendor_name)
        vendor_name = g_strdup (output->vendor);
    }
  else
    {
      if (inches != NULL)
        vendor_name = g_strdup (_("Unknown"));
      else
        vendor_name = g_strdup (_("Unknown Display"));
    }

  if (inches != NULL)
    {
      /* TRANSLATORS: this is a monitor vendor name, followed by a
       * size in inches, like 'Dell 15"'
       */
      return g_strdup_printf (_("%s %s"), vendor_name, inches);
    }
  else
    {
      return g_strdup (vendor_name);
    }
}

static const char *
get_connector_type_name (MetaConnectorType connector_type)
{
  switch (connector_type)
    {
    case META_CONNECTOR_TYPE_Unknown: return "Unknown";
    case META_CONNECTOR_TYPE_VGA: return "VGA";
    case META_CONNECTOR_TYPE_DVII: return "DVII";
    case META_CONNECTOR_TYPE_DVID: return "DVID";
    case META_CONNECTOR_TYPE_DVIA: return "DVIA";
    case META_CONNECTOR_TYPE_Composite: return "Composite";
    case META_CONNECTOR_TYPE_SVIDEO: return "SVIDEO";
    case META_CONNECTOR_TYPE_LVDS: return "LVDS";
    case META_CONNECTOR_TYPE_Component: return "Component";
    case META_CONNECTOR_TYPE_9PinDIN: return "9PinDIN";
    case META_CONNECTOR_TYPE_DisplayPort: return "DisplayPort";
    case META_CONNECTOR_TYPE_HDMIA: return "HDMIA";
    case META_CONNECTOR_TYPE_HDMIB: return "HDMIB";
    case META_CONNECTOR_TYPE_TV: return "TV";
    case META_CONNECTOR_TYPE_eDP: return "eDP";
    case META_CONNECTOR_TYPE_VIRTUAL: return "VIRTUAL";
    case META_CONNECTOR_TYPE_DSI: return "DSI";
    default: g_assert_not_reached ();
    }
}

static GList *
combine_gpu_lists (MetaMonitorManager    *manager,
                   GList              * (*list_getter) (MetaGpu *gpu))
{
  GList *list = NULL;
  GList *l;

  for (l = manager->gpus; l; l = l->next)
    {
      MetaGpu *gpu = l->data;

      list = g_list_concat (list, g_list_copy (list_getter (gpu)));
    }

  return list;
}

static gboolean
meta_monitor_manager_handle_get_resources (MetaDBusDisplayConfig *skeleton,
                                           GDBusMethodInvocation *invocation)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (skeleton);
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_GET_CLASS (skeleton);
  GList *combined_modes;
  GList *combined_outputs;
  GList *combined_crtcs;
  GVariantBuilder crtc_builder, output_builder, mode_builder;
  GList *l;
  unsigned int i, j;
  int max_screen_width;
  int max_screen_height;

  combined_modes = combine_gpu_lists (manager, meta_gpu_get_modes);
  combined_outputs = combine_gpu_lists (manager, meta_gpu_get_outputs);
  combined_crtcs = combine_gpu_lists (manager, meta_gpu_get_crtcs);

  g_variant_builder_init (&crtc_builder, G_VARIANT_TYPE ("a(uxiiiiiuaua{sv})"));
  g_variant_builder_init (&output_builder, G_VARIANT_TYPE ("a(uxiausauaua{sv})"));
  g_variant_builder_init (&mode_builder, G_VARIANT_TYPE ("a(uxuudu)"));

  for (l = combined_crtcs, i = 0; l; l = l->next, i++)
    {
      MetaCrtc *crtc = l->data;
      GVariantBuilder transforms;
      int current_mode_index;

      g_variant_builder_init (&transforms, G_VARIANT_TYPE ("au"));
      for (j = 0; j <= META_MONITOR_TRANSFORM_FLIPPED_270; j++)
        if (crtc->all_transforms & (1 << j))
          g_variant_builder_add (&transforms, "u", j);

      if (crtc->current_mode)
        current_mode_index = g_list_index (combined_modes, crtc->current_mode);
      else
        current_mode_index = -1;
      g_variant_builder_add (&crtc_builder, "(uxiiiiiuaua{sv})",
                             i, /* ID */
                             (gint64)crtc->crtc_id,
                             (int)crtc->rect.x,
                             (int)crtc->rect.y,
                             (int)crtc->rect.width,
                             (int)crtc->rect.height,
                             current_mode_index,
                             (guint32)crtc->transform,
                             &transforms,
                             NULL /* properties */);
    }

  for (l = combined_outputs, i = 0; l; l = l->next, i++)
    {
      MetaOutput *output = l->data;
      GVariantBuilder crtcs, modes, clones, properties;
      GBytes *edid;
      char *edid_file;
      int crtc_index;

      g_variant_builder_init (&crtcs, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_possible_crtcs; j++)
        {
          MetaCrtc *possible_crtc = output->possible_crtcs[j];
          unsigned possible_crtc_index;

          possible_crtc_index = g_list_index (combined_crtcs, possible_crtc);
          g_variant_builder_add (&crtcs, "u", possible_crtc_index);
        }

      g_variant_builder_init (&modes, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_modes; j++)
        {
          unsigned mode_index;

          mode_index = g_list_index (combined_modes, output->modes[j]);
          g_variant_builder_add (&modes, "u", mode_index);
        }

      g_variant_builder_init (&clones, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_possible_clones; j++)
        {
          unsigned int possible_clone_index;

          possible_clone_index = g_list_index (combined_outputs,
                                               output->possible_clones[j]);
          g_variant_builder_add (&clones, "u", possible_clone_index);
        }

      g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&properties, "{sv}", "vendor",
                             g_variant_new_string (output->vendor));
      g_variant_builder_add (&properties, "{sv}", "product",
                             g_variant_new_string (output->product));
      g_variant_builder_add (&properties, "{sv}", "serial",
                             g_variant_new_string (output->serial));
      g_variant_builder_add (&properties, "{sv}", "width-mm",
                             g_variant_new_int32 (output->width_mm));
      g_variant_builder_add (&properties, "{sv}", "height-mm",
                             g_variant_new_int32 (output->height_mm));
      g_variant_builder_add (&properties, "{sv}", "display-name",
                             g_variant_new_take_string (make_display_name (manager, output)));
      g_variant_builder_add (&properties, "{sv}", "backlight",
                             g_variant_new_int32 (output->backlight));
      g_variant_builder_add (&properties, "{sv}", "min-backlight-step",
                             g_variant_new_int32 ((output->backlight_max - output->backlight_min) ?
                                                  100 / (output->backlight_max - output->backlight_min) : -1));
      g_variant_builder_add (&properties, "{sv}", "primary",
                             g_variant_new_boolean (output->is_primary));
      g_variant_builder_add (&properties, "{sv}", "presentation",
                             g_variant_new_boolean (output->is_presentation));
      g_variant_builder_add (&properties, "{sv}", "connector-type",
                             g_variant_new_string (get_connector_type_name (output->connector_type)));
      g_variant_builder_add (&properties, "{sv}", "underscanning",
                             g_variant_new_boolean (output->is_underscanning));
      g_variant_builder_add (&properties, "{sv}", "supports-underscanning",
                             g_variant_new_boolean (output->supports_underscanning));

      edid_file = manager_class->get_edid_file (manager, output);
      if (edid_file)
        {
          g_variant_builder_add (&properties, "{sv}", "edid-file",
                                 g_variant_new_take_string (edid_file));
        }
      else
        {
          edid = manager_class->read_edid (manager, output);

          if (edid)
            {
              g_variant_builder_add (&properties, "{sv}", "edid",
                                     g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"),
                                                               edid, TRUE));
              g_bytes_unref (edid);
            }
        }

      if (output->tile_info.group_id)
        {
          g_variant_builder_add (&properties, "{sv}", "tile",
                                 g_variant_new ("(uuuuuuuu)",
                                                output->tile_info.group_id,
                                                output->tile_info.flags,
                                                output->tile_info.max_h_tiles,
                                                output->tile_info.max_v_tiles,
                                                output->tile_info.loc_h_tile,
                                                output->tile_info.loc_v_tile,
                                                output->tile_info.tile_w,
                                                output->tile_info.tile_h));
        }

      crtc_index = output->crtc ? g_list_index (combined_crtcs, output->crtc)
                                : -1;
      g_variant_builder_add (&output_builder, "(uxiausauaua{sv})",
                             i, /* ID */
                             (gint64)output->winsys_id,
                             crtc_index,
                             &crtcs,
                             output->name,
                             &modes,
                             &clones,
                             &properties);
    }

  for (l = combined_modes, i = 0; l; l = l->next, i++)
    {
      MetaCrtcMode *mode = l->data;

      g_variant_builder_add (&mode_builder, "(uxuudu)",
                             i, /* ID */
                             (gint64)mode->mode_id,
                             (guint32)mode->width,
                             (guint32)mode->height,
                             (double)mode->refresh_rate,
                             (guint32)mode->flags);
    }

  if (!meta_monitor_manager_get_max_screen_size (manager,
                                                 &max_screen_width,
                                                 &max_screen_height))
    {
      /* No max screen size, just send something large */
      max_screen_width = 65535;
      max_screen_height = 65535;
    }

  meta_dbus_display_config_complete_get_resources (skeleton,
                                                   invocation,
                                                   manager->serial,
                                                   g_variant_builder_end (&crtc_builder),
                                                   g_variant_builder_end (&output_builder),
                                                   g_variant_builder_end (&mode_builder),
                                                   max_screen_width,
                                                   max_screen_height);

  g_list_free (combined_modes);
  g_list_free (combined_outputs);
  g_list_free (combined_crtcs);

  return TRUE;
}

static void
restore_previous_config (MetaMonitorManager *manager)
{
  MetaMonitorsConfig *previous_config;
  GError *error = NULL;

  previous_config =
    meta_monitor_config_manager_pop_previous (manager->config_manager);

  if (previous_config)
    {
      MetaMonitorsConfigMethod method;

      method = META_MONITORS_CONFIG_METHOD_TEMPORARY;
      if (meta_monitor_manager_apply_monitors_config (manager,
                                                      previous_config,
                                                      method,
                                                      &error))
        {
          g_object_unref (previous_config);
          return;
        }
      else
        {
          g_object_unref (previous_config);
          g_warning ("Failed to restore previous configuration: %s",
                     error->message);
          g_error_free (error);
        }
    }

  meta_monitor_manager_ensure_configured (manager);
}

gint
meta_monitor_manager_get_display_configuration_timeout (void)
{
  return DEFAULT_DISPLAY_CONFIGURATION_TIMEOUT;
}

static gboolean
save_config_timeout (gpointer user_data)
{
  MetaMonitorManager *manager = user_data;

  restore_previous_config (manager);
  manager->persistent_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
cancel_persistent_confirmation (MetaMonitorManager *manager)
{
  g_source_remove (manager->persistent_timeout_id);
  manager->persistent_timeout_id = 0;
}

static void
request_persistent_confirmation (MetaMonitorManager *manager)
{
  manager->persistent_timeout_id = g_timeout_add_seconds (meta_monitor_manager_get_display_configuration_timeout (),
                                                          save_config_timeout,
                                                          manager);
  g_source_set_name_by_id (manager->persistent_timeout_id,
                           "[mutter] save_config_timeout");

  g_signal_emit (manager, signals[CONFIRM_DISPLAY_CHANGE], 0);
}

#define META_DISPLAY_CONFIG_MODE_FLAGS_PREFERRED (1 << 0)
#define META_DISPLAY_CONFIG_MODE_FLAGS_CURRENT (1 << 1)

#define MODE_FORMAT "(siiddada{sv})"
#define MODES_FORMAT "a" MODE_FORMAT
#define MONITOR_SPEC_FORMAT "(ssss)"
#define MONITOR_FORMAT "(" MONITOR_SPEC_FORMAT MODES_FORMAT "a{sv})"
#define MONITORS_FORMAT "a" MONITOR_FORMAT

#define LOGICAL_MONITOR_MONITORS_FORMAT "a" MONITOR_SPEC_FORMAT
#define LOGICAL_MONITOR_FORMAT "(iidub" LOGICAL_MONITOR_MONITORS_FORMAT "a{sv})"
#define LOGICAL_MONITORS_FORMAT "a" LOGICAL_MONITOR_FORMAT

static gboolean
meta_monitor_manager_handle_get_current_state (MetaDBusDisplayConfig *skeleton,
                                               GDBusMethodInvocation *invocation)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (skeleton);
  MetaSettings *settings = meta_backend_get_settings (manager->backend);
  GVariantBuilder monitors_builder;
  GVariantBuilder logical_monitors_builder;
  GVariantBuilder properties_builder;
  GList *l;
  int i;
  MetaMonitorManagerCapability capabilities;
  int ui_scaling_factor;
  int max_screen_width, max_screen_height;

  g_variant_builder_init (&monitors_builder,
                          G_VARIANT_TYPE (MONITORS_FORMAT));
  g_variant_builder_init (&logical_monitors_builder,
                          G_VARIANT_TYPE (LOGICAL_MONITORS_FORMAT));

  for (l = manager->monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaMonitorSpec *monitor_spec = meta_monitor_get_spec (monitor);
      MetaMonitorMode *current_mode;
      MetaMonitorMode *preferred_mode;
      GVariantBuilder modes_builder;
      GVariantBuilder monitor_properties_builder;
      GList *k;
      gboolean is_builtin;
      MetaOutput *main_output;
      char *display_name;

      current_mode = meta_monitor_get_current_mode (monitor);
      preferred_mode = meta_monitor_get_preferred_mode (monitor);

      g_variant_builder_init (&modes_builder, G_VARIANT_TYPE (MODES_FORMAT));
      for (k = meta_monitor_get_modes (monitor); k; k = k->next)
        {
          MetaMonitorMode *monitor_mode = k->data;
          GVariantBuilder supported_scales_builder;
          const char *mode_id;
          int mode_width, mode_height;
          float refresh_rate;
          float preferred_scale;
          float *supported_scales;
          int n_supported_scales;
          GVariantBuilder mode_properties_builder;
          MetaCrtcModeFlag mode_flags;

          mode_id = meta_monitor_mode_get_id (monitor_mode);
          meta_monitor_mode_get_resolution (monitor_mode,
                                            &mode_width, &mode_height);
          refresh_rate = meta_monitor_mode_get_refresh_rate (monitor_mode);

          preferred_scale =
            meta_monitor_manager_calculate_monitor_mode_scale (manager,
                                                               monitor,
                                                               monitor_mode);

          g_variant_builder_init (&supported_scales_builder,
                                  G_VARIANT_TYPE ("ad"));
          supported_scales =
            meta_monitor_manager_calculate_supported_scales (manager,
                                                             manager->layout_mode,
                                                             monitor,
                                                             monitor_mode,
                                                             &n_supported_scales);
          for (i = 0; i < n_supported_scales; i++)
            g_variant_builder_add (&supported_scales_builder, "d",
                                   (double) supported_scales[i]);
          g_free (supported_scales);

          mode_flags = meta_monitor_mode_get_flags (monitor_mode);

          g_variant_builder_init (&mode_properties_builder,
                                  G_VARIANT_TYPE ("a{sv}"));
          if (monitor_mode == current_mode)
            g_variant_builder_add (&mode_properties_builder, "{sv}",
                                   "is-current",
                                   g_variant_new_boolean (TRUE));
          if (monitor_mode == preferred_mode)
            g_variant_builder_add (&mode_properties_builder, "{sv}",
                                   "is-preferred",
                                   g_variant_new_boolean (TRUE));
          if (mode_flags & META_CRTC_MODE_FLAG_INTERLACE)
            g_variant_builder_add (&mode_properties_builder, "{sv}",
                                   "is-interlaced",
                                   g_variant_new_boolean (TRUE));

          g_variant_builder_add (&modes_builder, MODE_FORMAT,
                                 mode_id,
                                 mode_width,
                                 mode_height,
                                 refresh_rate,
                                 (double) preferred_scale,
                                 &supported_scales_builder,
                                 &mode_properties_builder);
        }

      g_variant_builder_init (&monitor_properties_builder,
                              G_VARIANT_TYPE ("a{sv}"));
      if (meta_monitor_supports_underscanning (monitor))
        {
          gboolean is_underscanning = meta_monitor_is_underscanning (monitor);

          g_variant_builder_add (&monitor_properties_builder, "{sv}",
                                 "is-underscanning",
                                 g_variant_new_boolean (is_underscanning));
        }

      is_builtin = meta_monitor_is_laptop_panel (monitor);
      g_variant_builder_add (&monitor_properties_builder, "{sv}",
                             "is-builtin",
                             g_variant_new_boolean (is_builtin));

      main_output = meta_monitor_get_main_output (monitor);
      display_name = make_display_name (manager, main_output);
      g_variant_builder_add (&monitor_properties_builder, "{sv}",
                             "display-name",
                             g_variant_new_take_string (display_name));

      g_variant_builder_add (&monitors_builder, MONITOR_FORMAT,
                             monitor_spec->connector,
                             monitor_spec->vendor,
                             monitor_spec->product,
                             monitor_spec->serial,
                             &modes_builder,
                             &monitor_properties_builder);
    }

  for (l = manager->logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      GVariantBuilder logical_monitor_monitors_builder;
      GList *k;

      g_variant_builder_init (&logical_monitor_monitors_builder,
                              G_VARIANT_TYPE (LOGICAL_MONITOR_MONITORS_FORMAT));

      for (k = logical_monitor->monitors; k; k = k->next)
        {
          MetaMonitor *monitor = k->data;
          MetaMonitorSpec *monitor_spec = meta_monitor_get_spec (monitor);

          g_variant_builder_add (&logical_monitor_monitors_builder,
                                 MONITOR_SPEC_FORMAT,
                                 monitor_spec->connector,
                                 monitor_spec->vendor,
                                 monitor_spec->product,
                                 monitor_spec->serial);
        }

      g_variant_builder_add (&logical_monitors_builder,
                             LOGICAL_MONITOR_FORMAT,
                             logical_monitor->rect.x,
                             logical_monitor->rect.y,
                             (double) logical_monitor->scale,
                             logical_monitor->transform,
                             logical_monitor->is_primary,
                             &logical_monitor_monitors_builder,
                             NULL);
    }

  g_variant_builder_init (&properties_builder, G_VARIANT_TYPE ("a{sv}"));
  capabilities = meta_monitor_manager_get_capabilities (manager);
  if ((capabilities & META_MONITOR_MANAGER_CAPABILITY_MIRRORING) == 0)
    {
      g_variant_builder_add (&properties_builder, "{sv}",
                             "supports-mirroring",
                             g_variant_new_boolean (FALSE));
    }

  g_variant_builder_add (&properties_builder, "{sv}",
                         "layout-mode",
                         g_variant_new_uint32 (manager->layout_mode));
  if (capabilities & META_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE)
    {
      g_variant_builder_add (&properties_builder, "{sv}",
                             "supports-changing-layout-mode",
                             g_variant_new_boolean (TRUE));
    }

  if (capabilities & META_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED)
    {
      g_variant_builder_add (&properties_builder, "{sv}",
                             "global-scale-required",
                             g_variant_new_boolean (TRUE));
    }

  ui_scaling_factor = meta_settings_get_ui_scaling_factor (settings);
  g_variant_builder_add (&properties_builder, "{sv}",
                         "legacy-ui-scaling-factor",
                         g_variant_new_int32 (ui_scaling_factor));

  if (meta_monitor_manager_get_max_screen_size (manager,
                                                &max_screen_width,
                                                &max_screen_height))
    {
      GVariantBuilder max_screen_size_builder;

      g_variant_builder_init (&max_screen_size_builder,
                              G_VARIANT_TYPE ("(ii)"));
      g_variant_builder_add (&max_screen_size_builder, "i",
                             max_screen_width);
      g_variant_builder_add (&max_screen_size_builder, "i",
                             max_screen_height);

      g_variant_builder_add (&properties_builder, "{sv}",
                             "max-screen-size",
                             g_variant_builder_end (&max_screen_size_builder));
    }

  meta_dbus_display_config_complete_get_current_state (
    skeleton,
    invocation,
    manager->serial,
    g_variant_builder_end (&monitors_builder),
    g_variant_builder_end (&logical_monitors_builder),
    g_variant_builder_end (&properties_builder));

  return TRUE;
}

#undef MODE_FORMAT
#undef MODES_FORMAT
#undef MONITOR_SPEC_FORMAT
#undef MONITOR_FORMAT
#undef MONITORS_FORMAT
#undef LOGICAL_MONITOR_MONITORS_FORMAT
#undef LOGICAL_MONITOR_FORMAT
#undef LOGICAL_MONITORS_FORMAT

gboolean
meta_monitor_manager_is_scale_supported (MetaMonitorManager          *manager,
                                         MetaLogicalMonitorLayoutMode layout_mode,
                                         MetaMonitor                 *monitor,
                                         MetaMonitorMode             *monitor_mode,
                                         float                        scale)
{
  g_autofree float *supported_scales = NULL;
  int n_supported_scales;
  int i;

  supported_scales =
    meta_monitor_manager_calculate_supported_scales (manager,
                                                     layout_mode,
                                                     monitor,
                                                     monitor_mode,
                                                     &n_supported_scales);
  for (i = 0; i < n_supported_scales; i++)
    {
      if (supported_scales[i] == scale)
        return TRUE;
    }

  return FALSE;
}

static gboolean
meta_monitor_manager_is_config_applicable (MetaMonitorManager *manager,
                                           MetaMonitorsConfig *config,
                                           GError            **error)
{
  GList *l;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      float scale = logical_monitor_config->scale;
      GList *k;

      for (k = logical_monitor_config->monitor_configs; k; k = k->next)
        {
          MetaMonitorConfig *monitor_config = k->data;
          MetaMonitorSpec *monitor_spec = monitor_config->monitor_spec;
          MetaMonitorModeSpec *mode_spec = monitor_config->mode_spec;
          MetaMonitor *monitor;
          MetaMonitorMode *monitor_mode;

          monitor = meta_monitor_manager_get_monitor_from_spec (manager,
                                                                monitor_spec);
          if (!monitor)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Specified monitor not found");
              return FALSE;
            }

          monitor_mode = meta_monitor_get_mode_from_spec (monitor, mode_spec);
          if (!monitor_mode)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Specified monitor mode not available");
              return FALSE;
            }

          if (!meta_monitor_manager_is_scale_supported (manager,
                                                        config->layout_mode,
                                                        monitor,
                                                        monitor_mode,
                                                        scale))
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Scale not supported by backend");
              return FALSE;
            }

          if (meta_monitor_is_laptop_panel (monitor) &&
              meta_monitor_manager_is_lid_closed (manager))
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Refusing to activate a closed laptop panel");
              return FALSE;
            }
        }
    }

  return TRUE;
}

static gboolean
meta_monitor_manager_is_config_complete (MetaMonitorManager *manager,
                                         MetaMonitorsConfig *config)
{
  MetaMonitorsConfigKey *current_state_key;
  gboolean is_config_complete;

  current_state_key =
    meta_create_monitors_config_key_for_current_state (manager);
  if (!current_state_key)
    return FALSE;

  is_config_complete = meta_monitors_config_key_equal (current_state_key,
                                                       config->key);
  meta_monitors_config_key_free (current_state_key);

  if (!is_config_complete)
    return FALSE;

  return meta_monitor_manager_is_config_applicable (manager, config, NULL);
}

static MetaMonitor *
find_monitor_from_connector (MetaMonitorManager *manager,
                             char               *connector)
{
  GList *monitors;
  GList *l;

  if (!connector)
    return NULL;

  monitors = meta_monitor_manager_get_monitors (manager);
  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaMonitorSpec *monitor_spec = meta_monitor_get_spec (monitor);

      if (g_str_equal (connector, monitor_spec->connector))
        return monitor;
    }

  return NULL;
}

#define MONITOR_CONFIG_FORMAT "(ssa{sv})"
#define MONITOR_CONFIGS_FORMAT "a" MONITOR_CONFIG_FORMAT

#define LOGICAL_MONITOR_CONFIG_FORMAT "(iidub" MONITOR_CONFIGS_FORMAT ")"

static MetaMonitorConfig *
create_monitor_config_from_variant (MetaMonitorManager *manager,
                                    GVariant           *monitor_config_variant,
                                    GError            **error)
{

  MetaMonitorConfig *monitor_config = NULL;
  g_autofree char *connector = NULL;
  g_autofree char *mode_id = NULL;
  MetaMonitorMode *monitor_mode;
  MetaMonitor *monitor;
  MetaMonitorSpec *monitor_spec;
  MetaMonitorModeSpec *monitor_mode_spec;
  g_autoptr (GVariant) properties_variant = NULL;
  gboolean enable_underscanning = FALSE;

  g_variant_get (monitor_config_variant, "(ss@a{sv})",
                 &connector,
                 &mode_id,
                 &properties_variant);

  monitor = find_monitor_from_connector (manager, connector);
  if (!monitor)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid connector '%s' specified", connector);
      return NULL;
    }

  monitor_mode = meta_monitor_get_mode_from_id (monitor, mode_id);
  if (!monitor_mode)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid mode '%s' specified", mode_id);
      return NULL;
    }

  g_variant_lookup (properties_variant, "underscanning", "b", &enable_underscanning);

  monitor_spec = meta_monitor_spec_clone (meta_monitor_get_spec (monitor));

  monitor_mode_spec = g_new0 (MetaMonitorModeSpec, 1);
  *monitor_mode_spec = *meta_monitor_mode_get_spec (monitor_mode);

  monitor_config = g_new0 (MetaMonitorConfig, 1);
  *monitor_config = (MetaMonitorConfig) {
    .monitor_spec = monitor_spec,
    .mode_spec = monitor_mode_spec,
    .enable_underscanning = enable_underscanning
  };

  return monitor_config;
}

static gboolean
find_monitor_mode_scale (MetaMonitorManager          *manager,
                         MetaLogicalMonitorLayoutMode layout_mode,
                         MetaMonitorConfig           *monitor_config,
                         float                        scale,
                         float                       *out_scale,
                         GError                     **error)
{
  MetaMonitorSpec *monitor_spec;
  MetaMonitor *monitor;
  MetaMonitorModeSpec *monitor_mode_spec;
  MetaMonitorMode *monitor_mode;
  g_autofree float *supported_scales = NULL;
  int n_supported_scales;
  int i;

  monitor_spec = monitor_config->monitor_spec;
  monitor = meta_monitor_manager_get_monitor_from_spec (manager,
                                                        monitor_spec);
  if (!monitor)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor not found");
      return FALSE;
    }

  monitor_mode_spec = monitor_config->mode_spec;
  monitor_mode = meta_monitor_get_mode_from_spec (monitor,
                                                  monitor_mode_spec);
  if (!monitor_mode)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor mode not found");
      return FALSE;
    }

  supported_scales =
    meta_monitor_manager_calculate_supported_scales (manager, layout_mode,
                                                     monitor, monitor_mode,
                                                     &n_supported_scales);

  for (i = 0; i < n_supported_scales; i++)
    {
      float supported_scale = supported_scales[i];

      if (fabsf (supported_scale - scale) < FLT_EPSILON)
        {
          *out_scale = supported_scale;
          return TRUE;
        }
    }

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Scale %g not valid for resolution %dx%d",
               scale,
               monitor_mode_spec->width,
               monitor_mode_spec->height);
  return FALSE;
}

static gboolean
derive_logical_monitor_size (MetaMonitorConfig           *monitor_config,
                             int                         *out_width,
                             int                         *out_height,
                             float                        scale,
                             MetaMonitorTransform         transform,
                             MetaLogicalMonitorLayoutMode layout_mode,
                             GError                     **error)
{
  int width, height;

  if (meta_monitor_transform_is_rotated (transform))
    {
      width = monitor_config->mode_spec->height;
      height = monitor_config->mode_spec->width;
    }
  else
    {
      width = monitor_config->mode_spec->width;
      height = monitor_config->mode_spec->height;
    }

  switch (layout_mode)
    {
    case META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
      width = roundf (width / scale);
      height = roundf (height / scale);
      break;
    case META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      break;
    }

  *out_width = width;
  *out_height = height;

  return TRUE;
}

static MetaLogicalMonitorConfig *
create_logical_monitor_config_from_variant (MetaMonitorManager          *manager,
                                            GVariant                    *logical_monitor_config_variant,
                                            MetaLogicalMonitorLayoutMode layout_mode,
                                            GError                     **error)
{
  MetaLogicalMonitorConfig *logical_monitor_config;
  int x, y, width, height;
  double scale_d;
  float scale;
  MetaMonitorTransform transform;
  gboolean is_primary;
  GVariantIter *monitor_configs_iter;
  GList *monitor_configs = NULL;
  MetaMonitorConfig *monitor_config;

  g_variant_get (logical_monitor_config_variant, LOGICAL_MONITOR_CONFIG_FORMAT,
                 &x,
                 &y,
                 &scale_d,
                 &transform,
                 &is_primary,
                 &monitor_configs_iter);
  scale = (float) scale_d;

  while (TRUE)
    {
      GVariant *monitor_config_variant =
        g_variant_iter_next_value (monitor_configs_iter);
      MetaMonitorConfig *monitor_config;

      if (!monitor_config_variant)
        break;

      monitor_config =
        create_monitor_config_from_variant (manager,
                                            monitor_config_variant, error);
      g_variant_unref (monitor_config_variant);

      if (!monitor_config)
        goto err;

      if (!meta_verify_monitor_config (monitor_config, error))
        {
          meta_monitor_config_free (monitor_config);
          goto err;
        }

      monitor_configs = g_list_append (monitor_configs, monitor_config);
    }
  g_variant_iter_free (monitor_configs_iter);

  if (!monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Empty logical monitor");
      goto err;
    }

  monitor_config = monitor_configs->data;
  if (!find_monitor_mode_scale (manager,
                                layout_mode,
                                monitor_config,
                                scale,
                                &scale,
                                error))
    goto err;

  if (!derive_logical_monitor_size (monitor_config, &width, &height,
                                    scale, transform, layout_mode, error))
    goto err;

  logical_monitor_config = g_new0 (MetaLogicalMonitorConfig, 1);
  *logical_monitor_config = (MetaLogicalMonitorConfig) {
    .layout = {
      .x = x,
      .y = y,
      .width = width,
      .height = height
    },
    .transform = transform,
    .scale = scale,
    .is_primary = is_primary,
    .monitor_configs = monitor_configs
  };

  if (!meta_verify_logical_monitor_config (logical_monitor_config,
                                           layout_mode,
                                           manager,
                                           error))
    {
      meta_logical_monitor_config_free (logical_monitor_config);
      return NULL;
    }

  return logical_monitor_config;

err:
  g_list_free_full (monitor_configs, (GDestroyNotify) meta_monitor_config_free);
  return NULL;
}

static gboolean
is_valid_layout_mode (MetaLogicalMonitorLayoutMode layout_mode)
{
  switch (layout_mode)
    {
    case META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
    case META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      return TRUE;
    }

  return FALSE;
}

static gboolean
meta_monitor_manager_handle_apply_monitors_config (MetaDBusDisplayConfig *skeleton,
                                                   GDBusMethodInvocation *invocation,
                                                   guint                  serial,
                                                   guint                  method,
                                                   GVariant              *logical_monitor_configs_variant,
                                                   GVariant              *properties_variant)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (skeleton);
  MetaMonitorManagerCapability capabilities;
  GVariant *layout_mode_variant = NULL;
  MetaLogicalMonitorLayoutMode layout_mode;
  GVariantIter logical_monitor_configs_iter;
  MetaMonitorsConfig *config;
  GList *logical_monitor_configs = NULL;
  GError *error = NULL;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  capabilities = meta_monitor_manager_get_capabilities (manager);

  if (properties_variant)
    layout_mode_variant = g_variant_lookup_value (properties_variant,
                                                  "layout-mode",
                                                  G_VARIANT_TYPE ("u"));

  if (layout_mode_variant &&
      capabilities & META_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE)
    {
      g_variant_get (layout_mode_variant, "u", &layout_mode);
    }
  else if (!layout_mode_variant)
    {
      layout_mode =
        meta_monitor_manager_get_default_layout_mode (manager);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Can't set layout mode");
      return TRUE;
    }

  if (!is_valid_layout_mode (layout_mode))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "Invalid layout mode specified");
      return TRUE;
    }

  g_variant_iter_init (&logical_monitor_configs_iter,
                       logical_monitor_configs_variant);
  while (TRUE)
    {
      GVariant *logical_monitor_config_variant =
        g_variant_iter_next_value (&logical_monitor_configs_iter);
      MetaLogicalMonitorConfig *logical_monitor_config;

      if (!logical_monitor_config_variant)
        break;

      logical_monitor_config =
        create_logical_monitor_config_from_variant (manager,
                                                    logical_monitor_config_variant,
                                                    layout_mode,
                                                    &error);
      g_variant_unref (logical_monitor_config_variant);

      if (!logical_monitor_config)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "%s", error->message);
          g_error_free (error);
          g_list_free_full (logical_monitor_configs,
                            (GDestroyNotify) meta_logical_monitor_config_free);
          return TRUE;
        }

      logical_monitor_configs = g_list_append (logical_monitor_configs,
                                               logical_monitor_config);
    }

  config = meta_monitors_config_new (manager,
                                     logical_monitor_configs,
                                     layout_mode,
                                     META_MONITORS_CONFIG_FLAG_NONE);
  if (!meta_verify_monitors_config (config, manager, &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "%s", error->message);
      g_error_free (error);
      g_object_unref (config);
      return TRUE;
    }

  if (!meta_monitor_manager_is_config_applicable (manager, config, &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "%s", error->message);
      g_error_free (error);
      g_object_unref (config);
      return TRUE;
    }

  if (manager->persistent_timeout_id &&
      method != META_MONITORS_CONFIG_METHOD_VERIFY)
    cancel_persistent_confirmation (manager);

  if (!meta_monitor_manager_apply_monitors_config (manager,
                                                   config,
                                                   method,
                                                   &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "%s", error->message);
      g_error_free (error);
      g_object_unref (config);
      return TRUE;
    }

  if (method == META_MONITORS_CONFIG_METHOD_PERSISTENT)
    request_persistent_confirmation (manager);

  meta_dbus_display_config_complete_apply_monitors_config (skeleton, invocation);

  return TRUE;
}

#undef MONITOR_MODE_SPEC_FORMAT
#undef MONITOR_CONFIG_FORMAT
#undef MONITOR_CONFIGS_FORMAT
#undef LOGICAL_MONITOR_CONFIG_FORMAT

static void
confirm_configuration (MetaMonitorManager *manager,
                       gboolean            confirmed)
{
  if (confirmed)
    meta_monitor_config_manager_save_current (manager->config_manager);
  else
    restore_previous_config (manager);
}

void
meta_monitor_manager_confirm_configuration (MetaMonitorManager *manager,
                                            gboolean            ok)
{
  if (!manager->persistent_timeout_id)
    {
      /* too late */
      return;
    }

  cancel_persistent_confirmation (manager);
  confirm_configuration (manager, ok);
}

static gboolean
meta_monitor_manager_handle_change_backlight  (MetaDBusDisplayConfig *skeleton,
                                               GDBusMethodInvocation *invocation,
                                               guint                  serial,
                                               guint                  output_index,
                                               gint                   value)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (skeleton);
  GList *combined_outputs;
  MetaOutput *output;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  combined_outputs = combine_gpu_lists (manager, meta_gpu_get_outputs);

  if (output_index >= g_list_length (combined_outputs))
    {
      g_list_free (combined_outputs);
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid output id");
      return TRUE;
    }
  output = g_list_nth_data (combined_outputs, output_index);
  g_list_free (combined_outputs);

  if (value < 0 || value > 100)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid backlight value");
      return TRUE;
    }

  if (output->backlight == -1 ||
      (output->backlight_min == 0 && output->backlight_max == 0))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Output does not support changing backlight");
      return TRUE;
    }

  META_MONITOR_MANAGER_GET_CLASS (manager)->change_backlight (manager, output, value);

  meta_dbus_display_config_complete_change_backlight (skeleton, invocation, output->backlight);
  return TRUE;
}

static gboolean
meta_monitor_manager_handle_get_crtc_gamma  (MetaDBusDisplayConfig *skeleton,
                                             GDBusMethodInvocation *invocation,
                                             guint                  serial,
                                             guint                  crtc_id)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (skeleton);
  MetaMonitorManagerClass *klass;
  GList *combined_crtcs;
  MetaCrtc *crtc;
  gsize size;
  unsigned short *red;
  unsigned short *green;
  unsigned short *blue;
  GBytes *red_bytes, *green_bytes, *blue_bytes;
  GVariant *red_v, *green_v, *blue_v;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  combined_crtcs = combine_gpu_lists (manager, meta_gpu_get_crtcs);
  if (crtc_id >= g_list_length (combined_crtcs))
    {
      g_list_free (combined_crtcs);
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = g_list_nth_data (combined_crtcs, crtc_id);
  g_list_free (combined_crtcs);

  klass = META_MONITOR_MANAGER_GET_CLASS (manager);
  if (klass->get_crtc_gamma)
    klass->get_crtc_gamma (manager, crtc, &size, &red, &green, &blue);
  else
    {
      size = 0;
      red = green = blue = NULL;
    }

  red_bytes = g_bytes_new_take (red, size * sizeof (unsigned short));
  green_bytes = g_bytes_new_take (green, size * sizeof (unsigned short));
  blue_bytes = g_bytes_new_take (blue, size * sizeof (unsigned short));

  red_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), red_bytes, TRUE);
  green_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), green_bytes, TRUE);
  blue_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), blue_bytes, TRUE);

  meta_dbus_display_config_complete_get_crtc_gamma (skeleton, invocation,
                                                    red_v, green_v, blue_v);

  g_bytes_unref (red_bytes);
  g_bytes_unref (green_bytes);
  g_bytes_unref (blue_bytes);

  return TRUE;
}

static gboolean
meta_monitor_manager_handle_set_crtc_gamma  (MetaDBusDisplayConfig *skeleton,
                                             GDBusMethodInvocation *invocation,
                                             guint                  serial,
                                             guint                  crtc_id,
                                             GVariant              *red_v,
                                             GVariant              *green_v,
                                             GVariant              *blue_v)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (skeleton);
  MetaMonitorManagerClass *klass;
  GList *combined_crtcs;
  MetaCrtc *crtc;
  gsize size, dummy;
  unsigned short *red;
  unsigned short *green;
  unsigned short *blue;
  GBytes *red_bytes, *green_bytes, *blue_bytes;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  combined_crtcs = combine_gpu_lists (manager, meta_gpu_get_crtcs);

  if (crtc_id >= g_list_length (combined_crtcs))
    {
      g_list_free (combined_crtcs);
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = g_list_nth_data (combined_crtcs, crtc_id);
  g_list_free (combined_crtcs);

  red_bytes = g_variant_get_data_as_bytes (red_v);
  green_bytes = g_variant_get_data_as_bytes (green_v);
  blue_bytes = g_variant_get_data_as_bytes (blue_v);

  size = g_bytes_get_size (red_bytes) / sizeof (unsigned short);
  red = (unsigned short*) g_bytes_get_data (red_bytes, &dummy);
  green = (unsigned short*) g_bytes_get_data (green_bytes, &dummy);
  blue = (unsigned short*) g_bytes_get_data (blue_bytes, &dummy);

  klass = META_MONITOR_MANAGER_GET_CLASS (manager);
  if (klass->set_crtc_gamma)
    klass->set_crtc_gamma (manager, crtc, size, red, green, blue);
  meta_dbus_display_config_complete_set_crtc_gamma (skeleton, invocation);

  g_bytes_unref (red_bytes);
  g_bytes_unref (green_bytes);
  g_bytes_unref (blue_bytes);

  return TRUE;
}

static void
meta_monitor_manager_display_config_init (MetaDBusDisplayConfigIface *iface)
{
  iface->handle_get_resources = meta_monitor_manager_handle_get_resources;
  iface->handle_change_backlight = meta_monitor_manager_handle_change_backlight;
  iface->handle_get_crtc_gamma = meta_monitor_manager_handle_get_crtc_gamma;
  iface->handle_set_crtc_gamma = meta_monitor_manager_handle_set_crtc_gamma;
  iface->handle_get_current_state = meta_monitor_manager_handle_get_current_state;
  iface->handle_apply_monitors_config = meta_monitor_manager_handle_apply_monitors_config;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  MetaMonitorManager *manager = user_data;

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (manager),
                                    connection,
                                    "/org/gnome/Mutter/DisplayConfig",
                                    NULL);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  meta_topic (META_DEBUG_DBUS, "Acquired name %s\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
  meta_topic (META_DEBUG_DBUS, "Lost or failed to acquire name %s\n", name);
}

static void
initialize_dbus_interface (MetaMonitorManager *manager)
{
  manager->dbus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                          "org.gnome.Mutter.DisplayConfig",
                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                          (meta_get_replace_current_wm () ?
                                           G_BUS_NAME_OWNER_FLAGS_REPLACE : 0),
                                          on_bus_acquired,
                                          on_name_acquired,
                                          on_name_lost,
                                          g_object_ref (manager),
                                          g_object_unref);
}

/**
 * meta_monitor_manager_get:
 *
 * Accessor for the singleton MetaMonitorManager.
 *
 * Returns: (transfer none): The only #MetaMonitorManager there is.
 */
MetaMonitorManager *
meta_monitor_manager_get (void)
{
  MetaBackend *backend = meta_get_backend ();

  return meta_backend_get_monitor_manager (backend);
}

int
meta_monitor_manager_get_num_logical_monitors (MetaMonitorManager *manager)
{
  return g_list_length (manager->logical_monitors);
}

GList *
meta_monitor_manager_get_logical_monitors (MetaMonitorManager *manager)
{
  return manager->logical_monitors;
}

MetaLogicalMonitor *
meta_monitor_manager_get_logical_monitor_from_number (MetaMonitorManager *manager,
                                                      int                 number)
{
  g_return_val_if_fail ((unsigned int) number < g_list_length (manager->logical_monitors), NULL);

  return g_list_nth (manager->logical_monitors, number)->data;
}

MetaLogicalMonitor *
meta_monitor_manager_get_primary_logical_monitor (MetaMonitorManager *manager)
{
  return manager->primary_logical_monitor;
}

static MetaMonitor *
find_monitor (MetaMonitorManager *monitor_manager,
              gboolean (*match_func) (MetaMonitor *monitor))
{
  GList *monitors;
  GList *l;

  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;

      if (match_func (monitor))
        return monitor;
    }

  return NULL;
}

MetaMonitor *
meta_monitor_manager_get_primary_monitor (MetaMonitorManager *manager)
{
  return find_monitor (manager, meta_monitor_is_primary);
}

MetaMonitor *
meta_monitor_manager_get_laptop_panel (MetaMonitorManager *manager)
{
  return find_monitor (manager, meta_monitor_is_laptop_panel);
}

MetaMonitor *
meta_monitor_manager_get_monitor_from_connector (MetaMonitorManager *manager,
                                                 const char         *connector)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;

      if (g_str_equal (meta_monitor_get_connector (monitor),
                       connector))
        return monitor;
    }

  return NULL;
}

MetaMonitor *
meta_monitor_manager_get_monitor_from_spec (MetaMonitorManager *manager,
                                            MetaMonitorSpec    *monitor_spec)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;

      if (meta_monitor_spec_equals (meta_monitor_get_spec (monitor),
                                    monitor_spec))
        return monitor;
    }

  return NULL;
}

MetaLogicalMonitor *
meta_monitor_manager_get_logical_monitor_at (MetaMonitorManager *manager,
                                             float               x,
                                             float               y)
{
  GList *l;

  for (l = manager->logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      if (POINT_IN_RECT (x, y, logical_monitor->rect))
        return logical_monitor;
    }

  return NULL;
}

MetaLogicalMonitor *
meta_monitor_manager_get_logical_monitor_from_rect (MetaMonitorManager *manager,
                                                    MetaRectangle      *rect)
{
  MetaLogicalMonitor *best_logical_monitor;
  int best_logical_monitor_area;
  GList *l;

  best_logical_monitor = NULL;
  best_logical_monitor_area = 0;

  for (l = manager->logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      MetaRectangle intersection;
      int intersection_area;

      if (!meta_rectangle_intersect (&logical_monitor->rect,
                                     rect,
                                     &intersection))
        continue;

      intersection_area = meta_rectangle_area (&intersection);

      if (intersection_area > best_logical_monitor_area)
        {
          best_logical_monitor = logical_monitor;
          best_logical_monitor_area = intersection_area;
        }
    }

  if (!best_logical_monitor && (rect->width == 0 || rect->height == 0))
    best_logical_monitor =
      meta_monitor_manager_get_logical_monitor_at (manager, rect->x, rect->y);

  if (!best_logical_monitor)
    best_logical_monitor = manager->primary_logical_monitor;

  return best_logical_monitor;
}

MetaLogicalMonitor *
meta_monitor_manager_get_logical_monitor_neighbor (MetaMonitorManager *manager,
                                                   MetaLogicalMonitor *logical_monitor,
                                                   MetaScreenDirection direction)
{
  GList *l;

  for (l = manager->logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *other = l->data;

      if (meta_logical_monitor_has_neighbor (logical_monitor, other, direction))
        return other;
    }

  return NULL;
}

GList *
meta_monitor_manager_get_monitors (MetaMonitorManager *manager)
{
  return manager->monitors;
}

void
meta_monitor_manager_add_gpu (MetaMonitorManager *manager,
                              MetaGpu            *gpu)
{
  manager->gpus = g_list_append (manager->gpus, gpu);
}

GList *
meta_monitor_manager_get_gpus (MetaMonitorManager *manager)
{
  return manager->gpus;
}

void
meta_monitor_manager_get_screen_size (MetaMonitorManager *manager,
                                      int                *width,
                                      int                *height)
{
  *width = manager->screen_width;
  *height = manager->screen_height;
}

static void
rebuild_monitors (MetaMonitorManager *manager)
{
  GList *l;

  if (manager->monitors)
    {
      g_list_free_full (manager->monitors, g_object_unref);
      manager->monitors = NULL;
    }

  for (l = manager->gpus; l; l = l->next)
    {
      MetaGpu *gpu = l->data;
      GList *k;

      for (k = meta_gpu_get_outputs (gpu); k; k = k->next)
        {
          MetaOutput *output = k->data;

          if (output->tile_info.group_id)
            {
              if (is_main_tiled_monitor_output (output))
                {
                  MetaMonitorTiled *monitor_tiled;

                  monitor_tiled = meta_monitor_tiled_new (gpu, output);
                  manager->monitors = g_list_append (manager->monitors,
                                                     monitor_tiled);
                }
            }
          else
            {
              MetaMonitorNormal *monitor_normal;

              monitor_normal = meta_monitor_normal_new (gpu, output);
              manager->monitors = g_list_append (manager->monitors,
                                                 monitor_normal);
            }
        }
    }
}

void
meta_monitor_manager_tiled_monitor_added (MetaMonitorManager *manager,
                                          MetaMonitor        *monitor)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  if (manager_class->tiled_monitor_added)
    manager_class->tiled_monitor_added (manager, monitor);
}

void
meta_monitor_manager_tiled_monitor_removed (MetaMonitorManager *manager,
                                            MetaMonitor        *monitor)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  if (manager_class->tiled_monitor_removed)
    manager_class->tiled_monitor_removed (manager, monitor);
}

gboolean
meta_monitor_manager_is_transform_handled (MetaMonitorManager  *manager,
                                           MetaCrtc            *crtc,
                                           MetaMonitorTransform transform)
{
  MetaMonitorManagerClass *manager_class =
    META_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->is_transform_handled (manager, crtc, transform);
}

void
meta_monitor_manager_read_current_state (MetaMonitorManager *manager)
{
  GList *l;

  manager->serial++;

  for (l = manager->gpus; l; l = l->next)
    {
      MetaGpu *gpu = l->data;
      GError *error = NULL;

      if (!meta_gpu_read_current (gpu, &error))
        {
          g_warning ("Failed to read current KMS state: %s", error->message);
          g_clear_error (&error);
        }
    }

  rebuild_monitors (manager);
}

static void
meta_monitor_manager_notify_monitors_changed (MetaMonitorManager *manager)
{
  manager->current_switch_config = META_MONITOR_SWITCH_CONFIG_UNKNOWN;

  meta_backend_monitors_changed (manager->backend);

  g_signal_emit (manager, signals[MONITORS_CHANGED_INTERNAL], 0);
  g_signal_emit_by_name (manager, "monitors-changed");
}

static void
set_logical_monitor_modes (MetaMonitorManager       *manager,
                           MetaLogicalMonitorConfig *logical_monitor_config)
{
  GList *l;

  for (l = logical_monitor_config->monitor_configs; l; l = l->next)
    {
      MetaMonitorConfig *monitor_config = l->data;
      MetaMonitorSpec *monitor_spec;
      MetaMonitor *monitor;
      MetaMonitorModeSpec *monitor_mode_spec;
      MetaMonitorMode *monitor_mode;

      monitor_spec = monitor_config->monitor_spec;
      monitor = meta_monitor_manager_get_monitor_from_spec (manager,
                                                            monitor_spec);
      monitor_mode_spec = monitor_config->mode_spec;
      monitor_mode = meta_monitor_get_mode_from_spec (monitor,
                                                      monitor_mode_spec);

      meta_monitor_set_current_mode (monitor, monitor_mode);
    }
}

static void
meta_monitor_manager_update_monitor_modes (MetaMonitorManager *manager,
                                           MetaMonitorsConfig *config)
{
  GList *logical_monitor_configs;
  GList *l;

  g_list_foreach (manager->monitors,
                  (GFunc) meta_monitor_set_current_mode,
                  NULL);

  logical_monitor_configs = config ? config->logical_monitor_configs : NULL;
  for (l = logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;

      set_logical_monitor_modes (manager, logical_monitor_config);
    }
}

void
meta_monitor_manager_update_logical_state (MetaMonitorManager *manager,
                                           MetaMonitorsConfig *config)
{
  if (config)
    manager->layout_mode = config->layout_mode;
  else
    manager->layout_mode =
      meta_monitor_manager_get_default_layout_mode (manager);

  meta_monitor_manager_rebuild_logical_monitors (manager, config);
}

void
meta_monitor_manager_rebuild (MetaMonitorManager *manager,
                              MetaMonitorsConfig *config)
{
  GList *old_logical_monitors;

  meta_monitor_manager_update_monitor_modes (manager, config);

  if (manager->in_init)
    return;

  old_logical_monitors = manager->logical_monitors;

  meta_monitor_manager_update_logical_state (manager, config);

  meta_monitor_manager_notify_monitors_changed (manager);

  g_list_free_full (old_logical_monitors, g_object_unref);
}

static void
meta_monitor_manager_update_monitor_modes_derived (MetaMonitorManager *manager)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;

      meta_monitor_derive_current_mode (monitor);
    }
}

void
meta_monitor_manager_update_logical_state_derived (MetaMonitorManager *manager,
                                                   MetaMonitorsConfig *config)
{
  manager->layout_mode = META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;

  meta_monitor_manager_rebuild_logical_monitors_derived (manager, config);
}

void
meta_monitor_manager_rebuild_derived (MetaMonitorManager *manager,
                                      MetaMonitorsConfig *config)
{
  GList *old_logical_monitors;

  meta_monitor_manager_update_monitor_modes_derived (manager);

  if (manager->in_init)
    return;

  old_logical_monitors = manager->logical_monitors;

  meta_monitor_manager_update_logical_state_derived (manager, config);

  meta_monitor_manager_notify_monitors_changed (manager);

  g_list_free_full (old_logical_monitors, g_object_unref);
}

void
meta_output_parse_edid (MetaOutput *output,
                        GBytes     *edid)
{
  MonitorInfo *parsed_edid;
  gsize len;

  if (!edid)
    goto out;

  parsed_edid = decode_edid (g_bytes_get_data (edid, &len));

  if (parsed_edid)
    {
      output->vendor = g_strndup (parsed_edid->manufacturer_code, 4);
      if (!g_utf8_validate (output->vendor, -1, NULL))
        g_clear_pointer (&output->vendor, g_free);

      output->product = g_strndup (parsed_edid->dsc_product_name, 14);
      if (!g_utf8_validate (output->product, -1, NULL) ||
          output->product[0] == '\0')
        {
          g_clear_pointer (&output->product, g_free);
          output->product = g_strdup_printf ("0x%04x", (unsigned) parsed_edid->product_code);
        }

      output->serial = g_strndup (parsed_edid->dsc_serial_number, 14);
      if (!g_utf8_validate (output->serial, -1, NULL) ||
          output->serial[0] == '\0')
        {
          g_clear_pointer (&output->serial, g_free);
          output->serial = g_strdup_printf ("0x%08x", parsed_edid->serial_number);
        }

      g_free (parsed_edid);
    }

 out:
  if (!output->vendor)
    output->vendor = g_strdup ("unknown");
  if (!output->product)
    output->product = g_strdup ("unknown");
  if (!output->serial)
    output->serial = g_strdup ("unknown");
}

gboolean
meta_output_is_laptop (MetaOutput *output)
{
  /* FIXME: extend with better heuristics */
  switch (output->connector_type)
    {
    case META_CONNECTOR_TYPE_eDP:
    case META_CONNECTOR_TYPE_LVDS:
    case META_CONNECTOR_TYPE_DSI:
      return TRUE;
    default:
      return FALSE;
    }
}

void
meta_monitor_manager_on_hotplug (MetaMonitorManager *manager)
{
  meta_monitor_manager_ensure_configured (manager);
}

static gboolean
calculate_viewport_matrix (MetaMonitorManager *manager,
                           MetaLogicalMonitor *logical_monitor,
                           gfloat              viewport[6])
{
  gfloat x, y, width, height;

  x = (float) logical_monitor->rect.x / manager->screen_width;
  y = (float) logical_monitor->rect.y / manager->screen_height;
  width  = (float) logical_monitor->rect.width / manager->screen_width;
  height = (float) logical_monitor->rect.height / manager->screen_height;

  viewport[0] = width;
  viewport[1] = 0.0f;
  viewport[2] = x;
  viewport[3] = 0.0f;
  viewport[4] = height;
  viewport[5] = y;

  return TRUE;
}

static inline void
multiply_matrix (float a[6],
		 float b[6],
		 float res[6])
{
  res[0] = a[0] * b[0] + a[1] * b[3];
  res[1] = a[0] * b[1] + a[1] * b[4];
  res[2] = a[0] * b[2] + a[1] * b[5] + a[2];
  res[3] = a[3] * b[0] + a[4] * b[3];
  res[4] = a[3] * b[1] + a[4] * b[4];
  res[5] = a[3] * b[2] + a[4] * b[5] + a[5];
}

gboolean
meta_monitor_manager_get_monitor_matrix (MetaMonitorManager *manager,
                                         MetaMonitor        *monitor,
                                         MetaLogicalMonitor *logical_monitor,
                                         gfloat              matrix[6])
{
  MetaMonitorTransform transform;
  gfloat viewport[9];

  if (!calculate_viewport_matrix (manager, logical_monitor, viewport))
    return FALSE;

  /* Get transform corrected for LCD panel-orientation. */
  transform = logical_monitor->transform;
  transform = meta_monitor_logical_to_crtc_transform (monitor, transform);
  multiply_matrix (viewport, transform_matrices[transform],
                   matrix);
  return TRUE;
}

/**
 * meta_monitor_manager_get_monitor_for_connector:
 * @manager: A #MetaMonitorManager
 * @connector: A valid connector name
 *
 * Returns: The monitor index or -1 if @id isn't valid or the connector
 * isn't associated with a logical monitor.
 */
gint
meta_monitor_manager_get_monitor_for_connector (MetaMonitorManager *manager,
                                                const char         *connector)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;

      if (meta_monitor_is_active (monitor) &&
          g_str_equal (connector, meta_monitor_get_connector (monitor)))
        {
          MetaOutput *main_output = meta_monitor_get_main_output (monitor);

          return main_output->crtc->logical_monitor->number;
        }
    }

  return -1;
}

gboolean
meta_monitor_manager_get_is_builtin_display_on (MetaMonitorManager *manager)
{
  MetaMonitor *laptop_panel;

  g_return_val_if_fail (META_IS_MONITOR_MANAGER (manager), FALSE);

  laptop_panel = meta_monitor_manager_get_laptop_panel (manager);
  if (!laptop_panel)
    return FALSE;

  return meta_monitor_is_active (laptop_panel);
}

void
meta_monitor_manager_rotate_monitor (MetaMonitorManager *manager)
{
  GError *error = NULL;
  MetaMonitorsConfig *config =
    meta_monitor_config_manager_create_for_rotate_monitor (manager->config_manager);

  if (!config)
    return;

  if (!meta_monitor_manager_apply_monitors_config (manager,
                                                   config,
                                                   META_MONITORS_CONFIG_METHOD_TEMPORARY,
                                                   &error))
    {
      g_warning ("Failed to use rotate monitor configuration: %s",
                 error->message);
      g_error_free (error);
    }
  g_object_unref (config);
}

void
meta_monitor_manager_switch_config (MetaMonitorManager          *manager,
                                    MetaMonitorSwitchConfigType  config_type)
{
  GError *error = NULL;
  MetaMonitorsConfig *config;

  g_return_if_fail (config_type != META_MONITOR_SWITCH_CONFIG_UNKNOWN);

  config =
    meta_monitor_config_manager_create_for_switch_config (manager->config_manager,
                                                          config_type);
  if (!config)
    return;

  if (!meta_monitor_manager_apply_monitors_config (manager,
                                                   config,
                                                   META_MONITORS_CONFIG_METHOD_TEMPORARY,
                                                   &error))
    {
      g_warning ("Failed to use switch monitor configuration: %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      manager->current_switch_config = config_type;
    }
  g_object_unref (config);
}

gboolean
meta_monitor_manager_can_switch_config (MetaMonitorManager *manager)
{
  return (!meta_monitor_manager_is_lid_closed (manager) &&
          g_list_length (manager->monitors) > 1);
}

MetaMonitorSwitchConfigType
meta_monitor_manager_get_switch_config (MetaMonitorManager *manager)
{
  return manager->current_switch_config;
}

MetaMonitorConfigManager *
meta_monitor_manager_get_config_manager (MetaMonitorManager *manager)
{
  return manager->config_manager;
}
