/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat, Inc.
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

#include "tests/monitor-unit-tests.h"

#include "backends/meta-backend-private.h"
#include "tests/meta-monitor-manager-test.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

#define MAX_N_MODES 10
#define MAX_N_OUTPUTS 10
#define MAX_N_CRTCS 10
#define MAX_N_LOGICAL_MONITORS 10

typedef struct _MonitorTestCaseMode
{
  int width;
  int height;
  float refresh_rate;
} MonitorTestCaseMode;

typedef struct _MonitorTestCaseOutput
{
  int crtc;
  int modes[MAX_N_MODES];
  int n_modes;
  int preferred_mode;
  int possible_crtcs[MAX_N_CRTCS];
  int n_possible_crtcs;
  int width_mm;
  int height_mm;
} MonitorTestCaseOutput;

typedef struct _MonitorTestCaseCrtc
{
  int current_mode;
} MonitorTestCaseCrtc;

typedef struct _MonitorTestCaseSetup
{
  MonitorTestCaseMode modes[MAX_N_MODES];
  int n_modes;

  MonitorTestCaseOutput outputs[MAX_N_OUTPUTS];
  int n_outputs;

  MonitorTestCaseCrtc crtcs[MAX_N_CRTCS];
  int n_crtcs;
} MonitorTestCaseSetup;

typedef struct _MonitorTestCase
{
  MonitorTestCaseSetup setup;
} MonitorTestCase;

static MonitorTestCase initial_test_case = {
  .setup = {
    .modes = {
      {
        .width = 1024,
        .height = 768,
        .refresh_rate = 60.0
      }
    },
    .n_modes = 1,
    .outputs = {
       {
        .crtc = 0,
        .modes = { 0 },
        .n_modes = 1,
        .preferred_mode = 0,
        .possible_crtcs = { 0 },
        .n_possible_crtcs = 1,
        .width_mm = 222,
        .height_mm = 125
      },
      {
        .crtc = 1,
        .modes = { 0 },
        .n_modes = 1,
        .preferred_mode = 0,
        .possible_crtcs = { 1 },
        .n_possible_crtcs = 1,
        .width_mm = 222,
        .height_mm = 125
      }
    },
    .n_outputs = 2,
    .crtcs = {
      {
        .current_mode = 0
      },
      {
        .current_mode = 0
      }
    },
    .n_crtcs = 2
  }
};

static void
meta_test_monitor_linear_config (void)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  GList *logical_monitors, *l;
  int n_logical_monitors, i;
  MetaRectangle expected_rects[] = {
    { .x = 0, .y = 0, .width = 1024, .height = 768 },
    { .x = 1024, .y = 0, .width = 1024, .height = 768 },
  };

  g_assert (monitor_manager->screen_width == 1024 * 2);
  g_assert (monitor_manager->screen_height == 768);
  g_assert (monitor_manager->n_outputs == 2);
  g_assert (monitor_manager->n_crtcs == 2);

  n_logical_monitors =
    meta_monitor_manager_get_num_logical_monitors (monitor_manager);
  g_assert (n_logical_monitors == 2);

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  i = 0;
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      g_assert (logical_monitor->rect.x == expected_rects[i].x);
      g_assert (logical_monitor->rect.y == expected_rects[i].y);
      g_assert (logical_monitor->rect.width == expected_rects[i].width);
      g_assert (logical_monitor->rect.height == expected_rects[i].height);
      i++;
    }
}

static MetaMonitorTestSetup *
create_monitor_test_setup (MonitorTestCase *test_case)
{
  MetaMonitorTestSetup *test_setup;
  int i;

  test_setup = g_new0 (MetaMonitorTestSetup, 1);

  test_setup->n_modes = test_case->setup.n_modes;
  test_setup->modes = g_new0 (MetaMonitorMode, test_setup->n_modes);
  for (i = 0; i < test_setup->n_modes; i++)
    {
      test_setup->modes[i] = (MetaMonitorMode) {
        .mode_id = i,
        .width = test_case->setup.modes[i].width,
        .height = test_case->setup.modes[i].height,
        .refresh_rate = test_case->setup.modes[i].refresh_rate
      };
    }

  test_setup->n_crtcs = test_case->setup.n_crtcs;
  test_setup->crtcs = g_new0 (MetaCRTC, test_setup->n_crtcs);
  for (i = 0; i < test_setup->n_crtcs; i++)
    {
      int current_mode_index;
      MetaMonitorMode *current_mode;

      current_mode_index = test_case->setup.crtcs[i].current_mode;
      if (current_mode_index == -1)
        current_mode = NULL;
      else
        current_mode = &test_setup->modes[current_mode_index];

      test_setup->crtcs[i] = (MetaCRTC) {
        .crtc_id = i + 1,
        .current_mode = current_mode,
        .transform = META_MONITOR_TRANSFORM_NORMAL,
        .all_transforms = ALL_TRANSFORMS
      };
    }

  test_setup->n_outputs = test_case->setup.n_outputs;
  test_setup->outputs = g_new0 (MetaOutput, test_setup->n_outputs);
  for (i = 0; i < test_setup->n_outputs; i++)
    {
      int crtc_index;
      MetaCRTC *crtc;
      int preferred_mode_index;
      MetaMonitorMode *preferred_mode;
      MetaMonitorMode **modes;
      int n_modes;
      int j;
      MetaCRTC **possible_crtcs;
      int n_possible_crtcs;

      crtc_index = test_case->setup.outputs[i].crtc;
      if (crtc_index == -1)
        crtc = NULL;
      else
        crtc = &test_setup->crtcs[crtc_index];

      preferred_mode_index = test_case->setup.outputs[i].preferred_mode;
      if (preferred_mode_index == -1)
        preferred_mode = NULL;
      else
        preferred_mode = &test_setup->modes[preferred_mode_index];

      n_modes = test_case->setup.outputs[i].n_modes;
      modes = g_new0 (MetaMonitorMode *, n_modes);
      for (j = 0; j < n_modes; j++)
        {
          int mode_index;

          mode_index = test_case->setup.outputs[i].modes[j];
          modes[j] = &test_setup->modes[mode_index];
        }

      n_possible_crtcs = test_case->setup.outputs[i].n_possible_crtcs;
      possible_crtcs = g_new0 (MetaCRTC *, n_possible_crtcs);
      for (j = 0; j < n_possible_crtcs; j++)
        {
          int possible_crtc_index;

          possible_crtc_index = test_case->setup.outputs[i].possible_crtcs[j];
          possible_crtcs[j] = &test_setup->crtcs[possible_crtc_index];
        }

      test_setup->outputs[i] = (MetaOutput) {
        .crtc = crtc,
        .winsys_id = i + 1,
        .name = g_strdup_printf ("LVDS%d", i + 1),
        .vendor = g_strdup ("MetaProducts Inc."),
        .product = g_strdup ("unknown"),
        .serial = g_strdup ("0xC0FFEE"),
        .suggested_x = -1,
        .suggested_y = -1,
        .hotplug_mode_update = TRUE, /* Results in config being ignored */
        .width_mm = test_case->setup.outputs[i].width_mm,
        .height_mm = test_case->setup.outputs[i].height_mm,
        .subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN,
        .preferred_mode = preferred_mode,
        .n_modes = n_modes,
        .modes = modes,
        .n_possible_crtcs = n_possible_crtcs,
        .possible_crtcs = possible_crtcs,
        .n_possible_clones = 0,
        .possible_clones = NULL,
        .backlight = -1,
        .connector_type = META_CONNECTOR_TYPE_LVDS,
        .scale = 1
      };
    }

  return test_setup;
}

void
init_monitor_tests (void)
{
  MetaMonitorTestSetup *initial_test_setup;

  initial_test_setup = create_monitor_test_setup (&initial_test_case);
  meta_monitor_manager_test_init_test_setup (initial_test_setup);

  g_test_add_func ("/backends/monitor/linear-config",
                   meta_test_monitor_linear_config);
}
