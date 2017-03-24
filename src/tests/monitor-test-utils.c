/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat, Inc.
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

#include "tests/monitor-test-utils.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-monitor-config-store.h"

void
set_custom_monitor_config (const char *filename)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorConfigManager *config_manager = monitor_manager->config_manager;
  MetaMonitorConfigStore *config_store;
  GError *error = NULL;
  const char *path;

  g_assert_nonnull (config_manager);

  config_store = meta_monitor_config_manager_get_store (config_manager);

  path = g_test_get_filename (G_TEST_DIST, "tests", "monitor-configs",
                              filename, NULL);
  if (!meta_monitor_config_store_set_custom (config_store, path, &error))
    g_error ("Failed to set custom config: %s", error->message);
}
