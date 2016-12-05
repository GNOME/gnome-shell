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

static MetaMonitorTestSetup *current_test_setup = NULL;

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

static void
setup_initial_monitor_test_setup (void)
{
  MetaMonitorTestSetup *test_setup;
  int n_monitors = 2;
  int i;

  test_setup = g_new0 (MetaMonitorTestSetup, 1);

  test_setup->n_modes = 1;
  test_setup->modes = g_new0 (MetaMonitorMode, 1);
  test_setup->modes[0].mode_id = 0;
  test_setup->modes[0].width = 1024;
  test_setup->modes[0].height = 768;
  test_setup->modes[0].refresh_rate = 60.0;

  test_setup->n_crtcs = n_monitors;
  test_setup->crtcs = g_new0 (MetaCRTC, n_monitors);

  test_setup->n_outputs = n_monitors;
  test_setup->outputs = g_new0 (MetaOutput, n_monitors);

  for (i = 0; i < n_monitors; i++)
    {
      test_setup->crtcs[i].crtc_id = i + 1;
      test_setup->crtcs[i].current_mode = &test_setup->modes[0];
      test_setup->crtcs[i].transform = META_MONITOR_TRANSFORM_NORMAL;
      test_setup->crtcs[i].all_transforms = ALL_TRANSFORMS;

      test_setup->outputs[i].crtc = &test_setup->crtcs[i];
      test_setup->outputs[i].winsys_id = i + 1;
      test_setup->outputs[i].name = g_strdup_printf ("LVDS%d", i + 1);
      test_setup->outputs[i].vendor = g_strdup ("MetaProducts Inc.");
      test_setup->outputs[i].product = g_strdup ("unknown");
      test_setup->outputs[i].serial = g_strdup ("0xC0FFEE");
      test_setup->outputs[i].suggested_x = -1;
      test_setup->outputs[i].suggested_y = -1;
      test_setup->outputs[i].width_mm = 222;
      test_setup->outputs[i].height_mm = 125;
      test_setup->outputs[i].subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
      test_setup->outputs[i].preferred_mode = &test_setup->modes[0];
      test_setup->outputs[i].n_modes = 1;
      test_setup->outputs[i].modes = g_new0 (MetaMonitorMode *, 1);
      test_setup->outputs[i].modes[0] = &test_setup->modes[0];
      test_setup->outputs[i].n_possible_crtcs = 1;
      test_setup->outputs[i].possible_crtcs = g_new0 (MetaCRTC *, 1);
      test_setup->outputs[i].possible_crtcs[0] = &test_setup->crtcs[i];
      test_setup->outputs[i].n_possible_clones = 0;
      test_setup->outputs[i].possible_clones = NULL;
      test_setup->outputs[i].backlight = -1;
      test_setup->outputs[i].connector_type = META_CONNECTOR_TYPE_LVDS;
      test_setup->outputs[i].scale = 1;
    }

  meta_monitor_manager_test_init_test_setup (test_setup);
  current_test_setup = test_setup;
}

void
init_monitor_tests (void)
{
  setup_initial_monitor_test_setup ();

  g_test_add_func ("/backends/monitor/linear-config",
                   meta_test_monitor_linear_config);
}
