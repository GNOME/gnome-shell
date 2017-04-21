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
#include "backends/meta-monitor-config-manager.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

#define MAX_MONITORS 5
#define MAX_OUTPUTS (MAX_MONITORS * 2)
#define MAX_CRTCS (MAX_MONITORS * 2)
#define MAX_MODES (MAX_MONITORS * 4)

static float supported_scales_dummy[] = {
  1.0,
  2.0
};

struct _MetaMonitorManagerDummy
{
  MetaMonitorManager parent_instance;

  gboolean is_transform_handled;
};

struct _MetaMonitorManagerDummyClass
{
  MetaMonitorManagerClass parent_class;
};

typedef struct _MetaOutputDummy
{
  int scale;
} MetaOutputDummy;

G_DEFINE_TYPE (MetaMonitorManagerDummy, meta_monitor_manager_dummy, META_TYPE_MONITOR_MANAGER);

static void
meta_output_dummy_notify_destroy (MetaOutput *output);

#define array_last(a, t) \
  g_array_index (a, t, a->len - 1)

static void
append_monitor (GArray *modes,
                GArray *crtcs,
                GArray *outputs,
                int     scale)
{
  MetaCrtcMode modes_decl[] = {
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
  MetaCrtc crtc;
  MetaOutputDummy *output_dummy;
  MetaOutput output;
  unsigned int i;

  for (i = 0; i < G_N_ELEMENTS (modes_decl); i++)
    modes_decl[i].mode_id = modes->len + i;
  g_array_append_vals (modes, modes_decl, G_N_ELEMENTS (modes_decl));

  crtc = (MetaCrtc) {
    .crtc_id = crtcs->len + 1,
    .all_transforms = ALL_TRANSFORMS,
  };
  g_array_append_val (crtcs, crtc);

  output_dummy = g_new0 (MetaOutputDummy, 1);
  *output_dummy = (MetaOutputDummy) {
    .scale = scale
  };

  output = (MetaOutput) {
    .winsys_id = outputs->len + 1,
    .name = g_strdup_printf ("LVDS%d", outputs->len + 1),
    .vendor = g_strdup ("MetaProducts Inc."),
    .product = g_strdup ("MetaMonitor"),
    .serial = g_strdup_printf ("0xC0FFEE-%d", outputs->len + 1),
    .suggested_x = -1,
    .suggested_y = -1,
    .width_mm = 222,
    .height_mm = 125,
    .subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN,
    .preferred_mode = &array_last (modes, MetaCrtcMode),
    .n_possible_clones = 0,
    .backlight = -1,
    .connector_type = META_CONNECTOR_TYPE_LVDS,
    .driver_private = output_dummy,
    .driver_notify =
      (GDestroyNotify) meta_output_dummy_notify_destroy
  };

  output.modes = g_new0 (MetaCrtcMode *, G_N_ELEMENTS (modes_decl));
  for (i = 0; i < G_N_ELEMENTS (modes_decl); i++)
    output.modes[i] = &g_array_index (modes, MetaCrtcMode,
                                      modes->len - (i + 1));
  output.n_modes = G_N_ELEMENTS (modes_decl);
  output.possible_crtcs = g_new0 (MetaCrtc *, 1);
  output.possible_crtcs[0] = &array_last (crtcs, MetaCrtc);
  output.n_possible_crtcs = 1;

  g_array_append_val (outputs, output);
}

static void
append_tiled_monitor (GArray *modes,
                      GArray *crtcs,
                      GArray *outputs,
                      int     scale)
{
  MetaCrtcMode modes_decl[] = {
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
  MetaCrtc crtcs_decl[] = {
    {
      .all_transforms = ALL_TRANSFORMS,
    },
    {
      .all_transforms = ALL_TRANSFORMS,
    },
  };
  MetaOutput output;
  unsigned int i;
  uint32_t tile_group_id;

  for (i = 0; i < G_N_ELEMENTS (modes_decl); i++)
    modes_decl[i].mode_id = modes->len + i;
  g_array_append_vals (modes, modes_decl, G_N_ELEMENTS (modes_decl));

  for (i = 0; i < G_N_ELEMENTS (crtcs_decl); i++)
    crtcs_decl[i].crtc_id = crtcs->len + i + 1;
  g_array_append_vals (crtcs, crtcs_decl, G_N_ELEMENTS (crtcs_decl));

  tile_group_id = outputs->len + 1;
  for (i = 0; i < G_N_ELEMENTS (crtcs_decl); i++)
    {
      MetaOutputDummy *output_dummy;
      MetaCrtcMode *preferred_mode;
      unsigned int j;

      output_dummy = g_new0 (MetaOutputDummy, 1);
      *output_dummy = (MetaOutputDummy) {
        .scale = scale
      };

      preferred_mode = &array_last (modes, MetaCrtcMode),
      output = (MetaOutput) {
        .winsys_id = outputs->len + 1,
        .name = g_strdup_printf ("LVDS%d", outputs->len + 1),
        .vendor = g_strdup ("MetaProducts Inc."),
        .product = g_strdup ("MetaMonitor"),
        .serial = g_strdup_printf ("0xC0FFEE-%d", outputs->len + 1),
        .suggested_x = -1,
        .suggested_y = -1,
        .width_mm = 222,
        .height_mm = 125,
        .subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN,
        .preferred_mode = preferred_mode,
        .n_possible_clones = 0,
        .backlight = -1,
        .connector_type = META_CONNECTOR_TYPE_LVDS,
        .tile_info = (MetaTileInfo) {
          .group_id = tile_group_id,
          .max_h_tiles = G_N_ELEMENTS (crtcs_decl),
          .max_v_tiles = 1,
          .loc_h_tile = i,
          .loc_v_tile = 0,
          .tile_w = preferred_mode->width,
          .tile_h = preferred_mode->height
        },
        .driver_private = output_dummy,
        .driver_notify =
          (GDestroyNotify) meta_output_dummy_notify_destroy
      };

      output.modes = g_new0 (MetaCrtcMode *, G_N_ELEMENTS (modes_decl));
      for (j = 0; j < G_N_ELEMENTS (modes_decl); j++)
        output.modes[j] = &g_array_index (modes, MetaCrtcMode,
                                          modes->len - (j + 1));
      output.n_modes = G_N_ELEMENTS (modes_decl);

      output.possible_crtcs = g_new0 (MetaCrtc *, G_N_ELEMENTS (crtcs_decl));
      for (j = 0; j < G_N_ELEMENTS (crtcs_decl); j++)
        output.possible_crtcs[j] = &g_array_index (crtcs, MetaCrtc,
                                                   crtcs->len - (j + 1));
      output.n_possible_crtcs = G_N_ELEMENTS (crtcs_decl);

      g_array_append_val (outputs, output);
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
  unsigned int num_monitors = 1;
  int *monitor_scales = NULL;
  const char *num_monitors_str;
  const char *monitor_scales_str;
  const char *tiled_monitors_str;
  gboolean tiled_monitors;
  unsigned int i;
  GArray *outputs;
  GArray *crtcs;
  GArray *modes;

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

  monitor_scales = g_newa (int, num_monitors);
  for (i = 0; i < num_monitors; i++)
    monitor_scales[i] = 1;

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
          int scale = g_ascii_strtoll (scales_str_list[i], NULL, 10);
          if (scale == 1 || scale == 2)
            monitor_scales[i] = scale;
          else
            meta_warning ("Invalid dummy monitor scale");
        }
      g_strfreev (scales_str_list);
    }

  tiled_monitors_str = g_getenv ("MUTTER_DEBUG_TILED_DUMMY_MONITORS");
  tiled_monitors = g_strcmp0 (tiled_monitors_str, "1") == 0;

  modes = g_array_sized_new (FALSE, TRUE, sizeof (MetaCrtcMode), MAX_MODES);
  crtcs = g_array_sized_new (FALSE, TRUE, sizeof (MetaCrtc), MAX_CRTCS);
  outputs = g_array_sized_new (FALSE, TRUE, sizeof (MetaOutput), MAX_OUTPUTS);

  for (i = 0; i < num_monitors; i++)
    {
      if (tiled_monitors)
        append_tiled_monitor (modes, crtcs, outputs, monitor_scales[i]);
      else
        append_monitor (modes, crtcs, outputs, monitor_scales[i]);
    }

  manager->modes = (MetaCrtcMode *) modes->data;
  manager->n_modes = modes->len;
  manager->crtcs = (MetaCrtc *) crtcs->data;
  manager->n_crtcs = crtcs->len;
  manager->outputs = (MetaOutput *) outputs->data;
  manager->n_outputs = outputs->len;

  g_array_free (modes, FALSE);
  g_array_free (crtcs, FALSE);
  g_array_free (outputs, FALSE);
}

static void
meta_monitor_manager_dummy_ensure_initial_config (MetaMonitorManager *manager)
{
  MetaMonitorsConfig *config;

  config = meta_monitor_manager_ensure_configured (manager);

  if (meta_is_monitor_config_manager_enabled ())
    meta_monitor_manager_update_logical_state (manager, config);
  else
    meta_monitor_manager_update_logical_state_derived (manager);
}

static void
apply_crtc_assignments (MetaMonitorManager *manager,
                        MetaCrtcInfo      **crtcs,
                        unsigned int        n_crtcs,
                        MetaOutputInfo    **outputs,
                        unsigned int        n_outputs)
{
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
              output->crtc = crtc;
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
  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCrtc *crtc = &manager->crtcs[i];

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
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      output->crtc = NULL;
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
      manager->screen_width = 0;
      manager->screen_height = 0;

      meta_monitor_manager_rebuild (manager, NULL);
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

static void
legacy_calculate_screen_size (MetaMonitorManager *manager)
{
  unsigned int i;
  int width = 0, height = 0;

  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCrtc *crtc = &manager->crtcs[i];

      width = MAX (width, crtc->rect.x + crtc->rect.width);
      height = MAX (height, crtc->rect.y + crtc->rect.height);
    }

  manager->screen_width = width;
  manager->screen_height = height;
}

static void
meta_monitor_manager_dummy_apply_config (MetaMonitorManager *manager,
                                         MetaCrtcInfo      **crtcs,
                                         unsigned int        n_crtcs,
                                         MetaOutputInfo    **outputs,
                                         unsigned int        n_outputs)
{
  apply_crtc_assignments (manager, crtcs, n_crtcs, outputs, n_outputs);

  legacy_calculate_screen_size (manager);

  meta_monitor_manager_rebuild_derived (manager);
}

static gboolean
meta_monitor_manager_dummy_is_transform_handled (MetaMonitorManager  *manager,
                                                 MetaCrtc            *crtc,
                                                 MetaMonitorTransform transform)
{
  MetaMonitorManagerDummy *manager_dummy = META_MONITOR_MANAGER_DUMMY (manager);

  return manager_dummy->is_transform_handled;
}

static int
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

static void
meta_monitor_manager_dummy_get_supported_scales (MetaMonitorManager *manager,
                                                 float             **scales,
                                                 int                *n_scales)
{
  *scales = supported_scales_dummy;
  *n_scales = G_N_ELEMENTS (supported_scales_dummy);
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

  manager_class->read_current = meta_monitor_manager_dummy_read_current;
  manager_class->ensure_initial_config = meta_monitor_manager_dummy_ensure_initial_config;
  manager_class->apply_monitors_config = meta_monitor_manager_dummy_apply_monitors_config;
  manager_class->apply_configuration = meta_monitor_manager_dummy_apply_config;
  manager_class->is_transform_handled = meta_monitor_manager_dummy_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = meta_monitor_manager_dummy_calculate_monitor_mode_scale;
  manager_class->get_supported_scales = meta_monitor_manager_dummy_get_supported_scales;
  manager_class->get_capabilities = meta_monitor_manager_dummy_get_capabilities;
  manager_class->get_max_screen_size = meta_monitor_manager_dummy_get_max_screen_size;
  manager_class->get_default_layout_mode = meta_monitor_manager_dummy_get_default_layout_mode;
}

static void
meta_monitor_manager_dummy_init (MetaMonitorManagerDummy *manager)
{
  const char *nested_offscreen_transform;

  nested_offscreen_transform =
    g_getenv ("MUTTER_DEBUG_NESTED_OFFSCREEN_TRANSFORM");
  if (g_strcmp0 (nested_offscreen_transform, "1") == 0)
    manager->is_transform_handled = FALSE;
  else
    manager->is_transform_handled = TRUE;
}
