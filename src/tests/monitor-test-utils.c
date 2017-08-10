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
  if (!meta_monitor_config_store_set_custom (config_store, path, NULL,
                                             &error))
    g_error ("Failed to set custom config: %s", error->message);
}

char *
read_file (const char *file_path)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInputStream) input_stream = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  goffset file_size;
  gsize bytes_read;
  g_autofree char *buffer = NULL;
  GError *error = NULL;

  file = g_file_new_for_path (file_path);
  input_stream = g_file_read (file, NULL, &error);
  if (!input_stream)
    g_error ("Failed to read migrated config file: %s", error->message);

  file_info = g_file_input_stream_query_info (input_stream,
                                              G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                              NULL, &error);
  if (!file_info)
    g_error ("Failed to read file info: %s", error->message);

  file_size = g_file_info_get_size (file_info);
  buffer = g_malloc0 (file_size + 1);

  if (!g_input_stream_read_all (G_INPUT_STREAM (input_stream),
                                buffer, file_size, &bytes_read, NULL, &error))
    g_error ("Failed to read file content: %s", error->message);
  g_assert_cmpint ((goffset) bytes_read, ==, file_size);

  return g_steal_pointer (&buffer);
}
