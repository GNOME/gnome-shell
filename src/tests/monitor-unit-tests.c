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

void
init_monitor_tests (void)
{
  g_test_add_func ("/backends/monitor/linear-config",
                   meta_test_monitor_linear_config);
}
