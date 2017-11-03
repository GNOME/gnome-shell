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

#include "meta-monitor-manager-dummy.h"

#include <stdlib.h>

#include <meta/util.h>
#include "backends/meta-backend-private.h"
#include "backends/meta-crtc.h"
#include "backends/meta-monitor.h"
#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-output.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

#define MAX_MONITORS 5
#define MAX_OUTPUTS (MAX_MONITORS * 2)
#define MAX_CRTCS (MAX_MONITORS * 2)
#define MAX_MODES (MAX_MONITORS * 4)

struct _MetaMonitorManagerDummy
{
  MetaMonitorManager parent_instance;

  MetaGpu *gpu;

  gboolean is_transform_handled;
};

struct _MetaMonitorManagerDummyClass
{
  MetaMonitorManagerClass parent_class;
};

typedef struct _MetaOutputDummy
{
  float scale;
} MetaOutputDummy;

G_DEFINE_TYPE (MetaMonitorManagerDummy, meta_monitor_manager_dummy, META_TYPE_MONITOR_MANAGER);

struct _MetaGpuDummy
{
  MetaGpu parent;
};

G_DEFINE_TYPE (MetaGpuDummy, meta_gpu_dummy, META_TYPE_GPU)

static void
meta_output_dummy_notify_destroy (MetaOutput *output);

typedef struct _CrtcModeSpec
{
  int width;
  int height;
  float refresh_rate;
} CrtcModeSpec;

static MetaCrtcMode *
create_mode (CrtcModeSpec *spec,
             long          mode_id)
{
  MetaCrtcMode *mode;

  mode = g_object_new (META_TYPE_CRTC_MODE, NULL);

  mode->mode_id = mode_id;
  mode->width = spec->width;
  mode->height = spec->height;
  mode->refresh_rate = spec->refresh_rate;

  return mode;
}

static void
append_monitor (MetaMonitorManager *manager,
                GList             **modes,
                GList             **crtcs,
                GList             **outputs,
                float               scale)
{
  MetaMonitorManagerDummy *manager_dummy = META_MONITOR_MANAGER_DUMMY (manager);
  MetaGpu *gpu = manager_dummy->gpu;
  CrtcModeSpec mode_specs[] = {
    {
      .width = 800,
      .height = 600,
      .refresh_rate = 60.0
    },
    {
      .width = 1024,
      .height = 768,
      .refresh_rate = 60.0
    }
  };
  GList *new_modes = NULL;
  MetaCrtc *crtc;
  MetaOutputDummy *output_dummy;
  MetaOutput *output;
  unsigned int i;
  unsigned int number;
  GList *l;

  for (i = 0; i < G_N_ELEMENTS (mode_specs); i++)
    {
      long mode_id;
      MetaCrtcMode *mode;

      mode_id = g_list_length (*modes) + i + 1;
      mode = create_mode (&mode_specs[i], mode_id);

      new_modes = g_list_append (new_modes, mode);
    }
  *modes = g_list_concat (*modes, new_modes);

  crtc = g_object_new (META_TYPE_CRTC, NULL);
  crtc->crtc_id = g_list_length (*crtcs) + 1;
  crtc->all_transforms = ALL_TRANSFORMS;
  *crtcs = g_list_append (*crtcs, crtc);

  output = g_object_new (META_TYPE_OUTPUT, NULL);

  output_dummy = g_new0 (MetaOutputDummy, 1);
  *output_dummy = (MetaOutputDummy) {
    .scale = scale
  };

  number = g_list_length (*outputs) + 1;

  output->gpu = gpu;
  output->winsys_id = number;
  output->name = g_strdup_printf ("LVDS%d", number);
  output->vendor = g_strdup ("MetaProducts Inc.");
  output->product = g_strdup ("MetaMonitor");
  output->serial = g_strdup_printf ("0xC0FFEE-%d", number);
  output->suggested_x = -1;
  output->suggested_y = -1;
  output->width_mm = 222;
  output->height_mm = 125;
  output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
  output->preferred_mode = g_list_last (*modes)->data;
  output->n_possible_clones = 0;
  output->backlight = -1;
  output->connector_type = META_CONNECTOR_TYPE_LVDS;
  output->driver_private = output_dummy;
  output->driver_notify =
    (GDestroyNotify) meta_output_dummy_notify_destroy;

  output->modes = g_new0 (MetaCrtcMode *, G_N_ELEMENTS (mode_specs));
  for (l = new_modes, i = 0; l; l = l->next, i++)
    {
      MetaCrtcMode *mode = l->data;

      output->modes[i] = mode;
    }
  output->n_modes = G_N_ELEMENTS (mode_specs);
  output->possible_crtcs = g_new0 (MetaCrtc *, 1);
  output->possible_crtcs[0] = g_list_last (*crtcs)->data;
  output->n_possible_crtcs = 1;

  *outputs = g_list_append (*outputs, output);
}

static void
append_tiled_monitor (MetaMonitorManager *manager,
                      GList             **modes,
                      GList             **crtcs,
                      GList             **outputs,
                      int                 scale)
{
  MetaMonitorManagerDummy *manager_dummy = META_MONITOR_MANAGER_DUMMY (manager);
  MetaGpu *gpu = manager_dummy->gpu;
  CrtcModeSpec mode_specs[] = {
    {
      .width = 800,
      .height = 600,
      .refresh_rate = 60.0
    },
    {
      .width = 512,
      .height = 768,
      .refresh_rate = 60.0
    }
  };
  unsigned int n_tiles = 2;
  GList *new_modes = NULL;
  GList *new_crtcs = NULL;
  MetaOutput *output;
  unsigned int i;
  uint32_t tile_group_id;

  for (i = 0; i < G_N_ELEMENTS (mode_specs); i++)
    {
      long mode_id;
      MetaCrtcMode *mode;

      mode_id = g_list_length (*modes) + i + 1;
      mode = create_mode (&mode_specs[i], mode_id);

      new_modes = g_list_append (new_modes, mode);
    }
  *modes = g_list_concat (*modes, new_modes);

  for (i = 0; i < n_tiles; i++)
    {
      MetaCrtc *crtc;

      crtc = g_object_new (META_TYPE_CRTC, NULL);
      crtc->gpu = gpu;
      crtc->crtc_id = g_list_length (*crtcs) + i + 1;
      crtc->all_transforms = ALL_TRANSFORMS;
      new_crtcs = g_list_append (new_crtcs, crtc);
    }
  *crtcs = g_list_concat (*crtcs, new_crtcs);

  tile_group_id = g_list_length (*outputs) + 1;
  for (i = 0; i < n_tiles; i++)
    {
      MetaOutputDummy *output_dummy;
      MetaCrtcMode *preferred_mode;
      unsigned int j;
      unsigned int number;
      GList *l;

      output_dummy = g_new0 (MetaOutputDummy, 1);
      *output_dummy = (MetaOutputDummy) {
        .scale = scale
      };

      /* Arbitrary ID unique for this output */
      number = g_list_length (*outputs) + 1;

      preferred_mode = g_list_last (*modes)->data;

      output = g_object_new (META_TYPE_OUTPUT, NULL);

      output->gpu = gpu;
      output->winsys_id = number;
      output->name = g_strdup_printf ("LVDS%d", number);
      output->vendor = g_strdup ("MetaProducts Inc.");
      output->product = g_strdup ("MetaMonitor");
      output->serial = g_strdup_printf ("0xC0FFEE-%d", number);
      output->suggested_x = -1;
      output->suggested_y = -1;
      output->width_mm = 222;
      output->height_mm = 125;
      output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
      output->preferred_mode = preferred_mode;
      output->n_possible_clones = 0;
      output->backlight = -1;
      output->connector_type = META_CONNECTOR_TYPE_LVDS;
      output->tile_info = (MetaTileInfo) {
        .group_id = tile_group_id,
        .max_h_tiles = n_tiles,
        .max_v_tiles = 1,
        .loc_h_tile = i,
        .loc_v_tile = 0,
        .tile_w = preferred_mode->width,
        .tile_h = preferred_mode->height
      },
      output->driver_private = output_dummy;
      output->driver_notify =
        (GDestroyNotify) meta_output_dummy_notify_destroy;

      output->modes = g_new0 (MetaCrtcMode *, G_N_ELEMENTS (mode_specs));
      for (l = new_modes, j = 0; l; l = l->next, j++)
        {
          MetaCrtcMode *mode = l->data;

          output->modes[j] = mode;
        }
      output->n_modes = G_N_ELEMENTS (mode_specs);

      output->possible_crtcs = g_new0 (MetaCrtc *, n_tiles);
      for (l = new_crtcs, j = 0; l; l = l->next, j++)
        {
          MetaCrtc *crtc = l->data;

          output->possible_crtcs[j] = crtc;
        }
      output->n_possible_crtcs = n_tiles;

      *outputs = g_list_append (*outputs, output);
    }
}

static void
meta_output_dummy_notify_destroy (MetaOutput *output)
{
  g_clear_pointer (&output->driver_private, g_free);
}

static void
meta_monitor_manager_dummy_read_current (MetaMonitorManager *manager)
{
  MetaMonitorManagerDummy *manager_dummy = META_MONITOR_MANAGER_DUMMY (manager);
  MetaGpu *gpu = manager_dummy->gpu;
  unsigned int num_monitors = 1;
  float *monitor_scales = NULL;
  const char *num_monitors_str;
  const char *monitor_scales_str;
  const char *tiled_monitors_str;
  gboolean tiled_monitors;
  unsigned int i;
  GList *outputs;
  GList *crtcs;
  GList *modes;

  /* To control what monitor configuration is generated, there are two available
   * environmental variables that can be used:
   *
   * MUTTER_DEBUG_NUM_DUMMY_MONITORS
   *
   * Specifies the number of dummy monitors to include in the stage. Every
   * monitor is 1024x786 pixels and they are placed on a horizontal row.
   *
   * MUTTER_DEBUG_DUMMY_MONITOR_SCALES
   *
   * A comma separated list that specifies the scales of the dummy monitors.
   *
   * MUTTER_DEBUG_TILED_DUMMY_MONITORS
   *
   * If set to "1" the dummy monitors will emulate being tiled, i.e. each have a
   * unique tile group id, made up of multiple outputs and CRTCs.
   *
   * For example the following configuration results in two monitors, where the
   * first one has the monitor scale 1, and the other the monitor scale 2.
   *
   * MUTTER_DEBUG_NUM_DUMMY_MONITORS=2
   * MUTTER_DEBUG_DUMMY_MONITOR_SCALES=1,2
   * MUTTER_DEBUG_TILED_DUMMY_MONITORS=1
   */
  num_monitors_str = getenv ("MUTTER_DEBUG_NUM_DUMMY_MONITORS");
  if (num_monitors_str)
    {
      num_monitors = g_ascii_strtoll (num_monitors_str, NULL, 10);
      if (num_monitors <= 0)
        {
          meta_warning ("Invalid number of dummy monitors");
          num_monitors = 1;
        }

      if (num_monitors > MAX_MONITORS)
        {
          meta_warning ("Clamping monitor count to max (%d)",
                        MAX_MONITORS);
          num_monitors = MAX_MONITORS;
        }
    }

  monitor_scales = g_newa (typeof (*monitor_scales), num_monitors);
  for (i = 0; i < num_monitors; i++)
    monitor_scales[i] = 1.0;

  monitor_scales_str = getenv ("MUTTER_DEBUG_DUMMY_MONITOR_SCALES");
  if (monitor_scales_str)
    {
      gchar **scales_str_list;

      scales_str_list = g_strsplit (monitor_scales_str, ",", -1);
      if (g_strv_length (scales_str_list) != num_monitors)
        meta_warning ("Number of specified monitor scales differ from number "
                      "of monitors (defaults to 1).\n");
      for (i = 0; i < num_monitors && scales_str_list[i]; i++)
        {
          float scale = g_ascii_strtod (scales_str_list[i], NULL);

          monitor_scales[i] = scale;
        }
      g_strfreev (scales_str_list);
    }

  tiled_monitors_str = g_getenv ("MUTTER_DEBUG_TILED_DUMMY_MONITORS");
  tiled_monitors = g_strcmp0 (tiled_monitors_str, "1") == 0;

  modes = NULL;
  crtcs = NULL;
  outputs = NULL;

  for (i = 0; i < num_monitors; i++)
    {
      if (tiled_monitors)
        append_tiled_monitor (manager,
                              &modes, &crtcs, &outputs, monitor_scales[i]);
      else
        append_monitor (manager, &modes, &crtcs, &outputs, monitor_scales[i]);
    }

  meta_gpu_take_modes (gpu, modes);
  meta_gpu_take_crtcs (gpu, crtcs);
  meta_gpu_take_outputs (gpu, outputs);
}

static void
meta_monitor_manager_dummy_ensure_initial_config (MetaMonitorManager *manager)
{
  MetaMonitorsConfig *config;

  config = meta_monitor_manager_ensure_configured (manager);

  if (meta_is_stage_views_enabled ())
    meta_monitor_manager_update_logical_state (manager, config);
  else
    meta_monitor_manager_update_logical_state_derived (manager, NULL);
}

static void
apply_crtc_assignments (MetaMonitorManager *manager,
                        MetaCrtcInfo      **crtcs,
                        unsigned int        n_crtcs,
                        MetaOutputInfo    **outputs,
                        unsigned int        n_outputs)
{
  MetaMonitorManagerDummy *manager_dummy = META_MONITOR_MANAGER_DUMMY (manager);
  GList *l;
  unsigned i;

  for (i = 0; i < n_crtcs; i++)
    {
      MetaCrtcInfo *crtc_info = crtcs[i];
      MetaCrtc *crtc = crtc_info->crtc;
      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        {
          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
      else
        {
          MetaCrtcMode *mode;
          MetaOutput *output;
          unsigned int j;
          int width, height;

          mode = crtc_info->mode;

          if (meta_monitor_transform_is_rotated (crtc_info->transform))
            {
              width = mode->height;
              height = mode->width;
            }
          else
            {
              width = mode->width;
              height = mode->height;
            }

          crtc->rect.x = crtc_info->x;
          crtc->rect.y = crtc_info->y;
          crtc->rect.width = width;
          crtc->rect.height = height;
          crtc->current_mode = mode;
          crtc->transform = crtc_info->transform;

          for (j = 0; j < crtc_info->outputs->len; j++)
            {
              output = ((MetaOutput**)crtc_info->outputs->pdata)[j];

              output->is_dirty = TRUE;
              meta_output_assign_crtc (output, crtc);
            }
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutputInfo *output_info = outputs[i];
      MetaOutput *output = output_info->output;

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
    }

  /* Disable CRTCs not mentioned in the list */
  for (l = meta_gpu_get_crtcs (manager_dummy->gpu); l; l = l->next)
    {
      MetaCrtc *crtc = l->data;

      crtc->logical_monitor = NULL;

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }

      crtc->rect.x = 0;
      crtc->rect.y = 0;
      crtc->rect.width = 0;
      crtc->rect.height = 0;
      crtc->current_mode = NULL;
    }

  /* Disable outputs not mentioned in the list */
  for (l = meta_gpu_get_outputs (manager_dummy->gpu); l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      meta_output_unassign_crtc (output);
      output->is_primary = FALSE;
    }
}

static void
update_screen_size (MetaMonitorManager *manager,
                    MetaMonitorsConfig *config)
{
  GList *l;
  int screen_width = 0;
  int screen_height = 0;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      int right_edge;
      int bottom_edge;

      right_edge = (logical_monitor_config->layout.width +
                    logical_monitor_config->layout.x);
      if (right_edge > screen_width)
        screen_width = right_edge;

      bottom_edge = (logical_monitor_config->layout.height +
                     logical_monitor_config->layout.y);
      if (bottom_edge > screen_height)
        screen_height = bottom_edge;
    }

  manager->screen_width = screen_width;
  manager->screen_height = screen_height;
}

static gboolean
meta_monitor_manager_dummy_apply_monitors_config (MetaMonitorManager      *manager,
                                                  MetaMonitorsConfig      *config,
                                                  MetaMonitorsConfigMethod method,
                                                  GError                 **error)
{
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;

  if (!config)
    {
      manager->screen_width = META_MONITOR_MANAGER_MIN_SCREEN_WIDTH;
      manager->screen_height = META_MONITOR_MANAGER_MIN_SCREEN_HEIGHT;

      meta_monitor_manager_rebuild (manager, NULL);
      return TRUE;
    }

  if (!meta_monitor_config_manager_assign (manager, config,
                                           &crtc_infos, &output_infos,
                                           error))
    return FALSE;

  if (method == META_MONITORS_CONFIG_METHOD_VERIFY)
    {
      g_ptr_array_free (crtc_infos, TRUE);
      g_ptr_array_free (output_infos, TRUE);
      return TRUE;
    }

  apply_crtc_assignments (manager,
                          (MetaCrtcInfo **) crtc_infos->pdata,
                          crtc_infos->len,
                          (MetaOutputInfo **) output_infos->pdata,
                          output_infos->len);

  g_ptr_array_free (crtc_infos, TRUE);
  g_ptr_array_free (output_infos, TRUE);

  update_screen_size (manager, config);
  meta_monitor_manager_rebuild (manager, config);

  return TRUE;
}

static gboolean
meta_monitor_manager_dummy_is_transform_handled (MetaMonitorManager  *manager,
                                                 MetaCrtc            *crtc,
                                                 MetaMonitorTransform transform)
{
  MetaMonitorManagerDummy *manager_dummy = META_MONITOR_MANAGER_DUMMY (manager);

  return manager_dummy->is_transform_handled;
}

static float
meta_monitor_manager_dummy_calculate_monitor_mode_scale (MetaMonitorManager *manager,
                                                         MetaMonitor        *monitor,
                                                         MetaMonitorMode    *monitor_mode)
{
  MetaOutput *output;
  MetaOutputDummy *output_dummy;

  output = meta_monitor_get_main_output (monitor);
  output_dummy = output->driver_private;

  return output_dummy->scale;
}

static float *
meta_monitor_manager_dummy_calculate_supported_scales (MetaMonitorManager          *manager,
                                                       MetaLogicalMonitorLayoutMode layout_mode,
                                                       MetaMonitor                 *monitor,
                                                       MetaMonitorMode             *monitor_mode,
                                                       int                         *n_supported_scales)
{
  MetaMonitorScalesConstraint constraints =
    META_MONITOR_SCALES_CONSTRAINT_NONE;

  switch (layout_mode)
    {
    case META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
      break;
    case META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      constraints |= META_MONITOR_SCALES_CONSTRAINT_NO_FRAC;
      break;
    }

  return meta_monitor_calculate_supported_scales (monitor, monitor_mode,
                                                  constraints,
                                                  n_supported_scales);
}

static gboolean
is_monitor_framebuffers_scaled (void)
{
  MetaBackend *backend = meta_get_backend ();
  MetaSettings *settings = meta_backend_get_settings (backend);

  return meta_settings_is_experimental_feature_enabled (
    settings,
    META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER);
}

static MetaMonitorManagerCapability
meta_monitor_manager_dummy_get_capabilities (MetaMonitorManager *manager)
{
  MetaBackend *backend = meta_get_backend ();
  MetaSettings *settings = meta_backend_get_settings (backend);
  MetaMonitorManagerCapability capabilities =
    META_MONITOR_MANAGER_CAPABILITY_NONE;

  capabilities |= META_MONITOR_MANAGER_CAPABILITY_MIRRORING;

  if (meta_settings_is_experimental_feature_enabled (
        settings,
        META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER))
    capabilities |= META_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE;

  return capabilities;
}

static gboolean
meta_monitor_manager_dummy_get_max_screen_size (MetaMonitorManager *manager,
                                                int                *max_width,
                                                int                *max_height)
{
  if (meta_is_stage_views_enabled ())
    return FALSE;

  *max_width = 65535;
  *max_height = 65535;

  return TRUE;
}

static MetaLogicalMonitorLayoutMode
meta_monitor_manager_dummy_get_default_layout_mode (MetaMonitorManager *manager)
{
  if (!meta_is_stage_views_enabled ())
    return META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;

  if (is_monitor_framebuffers_scaled ())
    return META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL;
  else
    return META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
meta_monitor_manager_dummy_class_init (MetaMonitorManagerDummyClass *klass)
{
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_CLASS (klass);

  manager_class->ensure_initial_config = meta_monitor_manager_dummy_ensure_initial_config;
  manager_class->apply_monitors_config = meta_monitor_manager_dummy_apply_monitors_config;
  manager_class->is_transform_handled = meta_monitor_manager_dummy_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = meta_monitor_manager_dummy_calculate_monitor_mode_scale;
  manager_class->calculate_supported_scales = meta_monitor_manager_dummy_calculate_supported_scales;
  manager_class->get_capabilities = meta_monitor_manager_dummy_get_capabilities;
  manager_class->get_max_screen_size = meta_monitor_manager_dummy_get_max_screen_size;
  manager_class->get_default_layout_mode = meta_monitor_manager_dummy_get_default_layout_mode;
}

static void
meta_monitor_manager_dummy_init (MetaMonitorManagerDummy *manager_dummy)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_dummy);
  const char *nested_offscreen_transform;

  nested_offscreen_transform =
    g_getenv ("MUTTER_DEBUG_NESTED_OFFSCREEN_TRANSFORM");
  if (g_strcmp0 (nested_offscreen_transform, "1") == 0)
    manager_dummy->is_transform_handled = FALSE;
  else
    manager_dummy->is_transform_handled = TRUE;

  manager_dummy->gpu = g_object_new (META_TYPE_GPU_DUMMY,
                                     "monitor-manager", manager,
                                     NULL);
  meta_monitor_manager_add_gpu (manager, manager_dummy->gpu);
}

static gboolean
meta_gpu_dummy_read_current (MetaGpu  *gpu,
                             GError  **error)
{
  MetaMonitorManager *manager = meta_gpu_get_monitor_manager (gpu);

  meta_monitor_manager_dummy_read_current (manager);

  return TRUE;
}

static void
meta_gpu_dummy_init (MetaGpuDummy *gpu_dummy)
{
}

static void
meta_gpu_dummy_class_init (MetaGpuDummyClass *klass)
{
  MetaGpuClass *gpu_class = META_GPU_CLASS (klass);

  gpu_class->read_current = meta_gpu_dummy_read_current;
}
