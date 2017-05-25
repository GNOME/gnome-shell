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

#ifndef META_MONITOR_MANAGER_TEST_H
#define META_MONITOR_MANAGER_TEST_H

#include "backends/meta-monitor-manager-private.h"

typedef struct _MetaMonitorTestSetup
{
  MetaCrtcMode *modes;
  int n_modes;
  MetaOutput *outputs;
  int n_outputs;
  MetaCrtc *crtcs;
  int n_crtcs;
} MetaMonitorTestSetup;

typedef struct _MetaOutputTest
{
  float scale;
} MetaOutputTest;

#define META_TYPE_MONITOR_MANAGER_TEST (meta_monitor_manager_test_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorManagerTest, meta_monitor_manager_test,
                      META, MONITOR_MANAGER_TEST, MetaMonitorManager)

void meta_monitor_manager_test_init_test_setup (MetaMonitorTestSetup *test_setup);

void meta_monitor_manager_test_emulate_hotplug (MetaMonitorManagerTest *manager_test,
                                                MetaMonitorTestSetup   *test_setup);

void meta_monitor_manager_test_set_is_lid_closed (MetaMonitorManagerTest *manager_test,
                                                  gboolean                is_lid_closed);

void meta_monitor_manager_test_set_handles_transforms (MetaMonitorManagerTest *manager_test,
                                                       gboolean                handles_transforms);

int meta_monitor_manager_test_get_tiled_monitor_count (MetaMonitorManagerTest *manager_test);

#endif /* META_MONITOR_MANAGER_TEST_H */
