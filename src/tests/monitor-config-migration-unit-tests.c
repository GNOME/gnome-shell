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

#include "tests/monitor-config-migration-unit-tests.h"

#include <glib.h>
#include <gio/gio.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-monitor-config-store.h"
#include "backends/meta-monitor-manager-private.h"
#include "backends/meta-monitor-config-migration.h"
#include "tests/monitor-test-utils.h"

static void
test_migration (const char *old_config,
                const char *new_config)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorConfigManager *config_manager = monitor_manager->config_manager;
  MetaMonitorConfigStore *config_store =
    meta_monitor_config_manager_get_store (config_manager);
  GError *error = NULL;
  const char *old_config_path;
  g_autoptr (GFile) old_config_file = NULL;
  g_autofree char *migrated_path = NULL;
  const char *expected_path;
  g_autofree char *migrated_data = NULL;
  g_autofree char *expected_data = NULL;
  g_autoptr (GFile) migrated_file = NULL;

  migrated_path = g_build_filename (g_get_tmp_dir (),
                                    "test-migrated-monitors.xml",
                                    NULL);
  if (!meta_monitor_config_store_set_custom (config_store, "/dev/null",
                                             migrated_path,
                                             &error))
    g_error ("Failed to set custom config store: %s", error->message);

  old_config_path = g_test_get_filename (G_TEST_DIST, "tests", "migration",
                                         old_config, NULL);
  old_config_file = g_file_new_for_path (old_config_path);
  if (!meta_migrate_old_monitors_config (config_store,
                                         old_config_file,
                                         &error))
    g_error ("Failed to migrate config: %s", error->message);

  expected_path = g_test_get_filename (G_TEST_DIST, "tests", "migration",
                                       new_config, NULL);

  expected_data = read_file (expected_path);
  migrated_data = read_file (migrated_path);

  g_assert_nonnull (expected_data);
  g_assert_nonnull (migrated_data);

  g_assert (strcmp (expected_data, migrated_data) == 0);

  migrated_file = g_file_new_for_path (migrated_path);
  if (!g_file_delete (migrated_file, NULL, &error))
    g_error ("Failed to remove test data output file: %s", error->message);
}

static void
meta_test_monitor_config_migration_basic (void)
{
  test_migration ("basic-old.xml", "basic-new.xml");
}

static void
meta_test_monitor_config_migration_rotated (void)
{
  test_migration ("rotated-old.xml", "rotated-new.xml");
}

static void
meta_test_monitor_config_migration_tiled (void)
{
  test_migration ("tiled-old.xml", "tiled-new.xml");
}

static void
meta_test_monitor_config_migration_first_rotated (void)
{
  test_migration ("first-rotated-old.xml", "first-rotated-new.xml");
}

static void
meta_test_monitor_config_migration_oneoff (void)
{
  test_migration ("oneoff-old.xml", "oneoff-new.xml");
}

static void
meta_test_monitor_config_migration_wiggle (void)
{
  test_migration ("wiggle-old.xml", "wiggle-new.xml");
}

void
init_monitor_config_migration_tests (void)
{
  g_test_add_func ("/backends/monitor-config-migration/basic",
                   meta_test_monitor_config_migration_basic);
  g_test_add_func ("/backends/monitor-config-migration/rotated",
                   meta_test_monitor_config_migration_rotated);
  g_test_add_func ("/backends/monitor-config-migration/tiled",
                   meta_test_monitor_config_migration_tiled);
  g_test_add_func ("/backends/monitor-config-migration/first-rotated",
                   meta_test_monitor_config_migration_first_rotated);
  g_test_add_func ("/backends/monitor-config-migration/oneoff",
                   meta_test_monitor_config_migration_oneoff);
  g_test_add_func ("/backends/monitor-config-migration/wiggle",
                   meta_test_monitor_config_migration_wiggle);
}
