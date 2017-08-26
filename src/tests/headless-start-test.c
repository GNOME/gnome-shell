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

#include "backends/meta-monitor-manager-private.h"
#include "backends/meta-crtc.h"
#include "backends/meta-output.h"
#include "compositor/meta-plugin-manager.h"
#include "core/main-private.h"
#include "meta/main.h"
#include "tests/meta-backend-test.h"
#include "tests/meta-monitor-manager-test.h"
#include "wayland/meta-wayland.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

static gboolean
run_tests (gpointer data)
{
  MetaBackend *backend = meta_get_backend ();
  MetaSettings *settings = meta_backend_get_settings (backend);
  gboolean ret;

  meta_settings_override_experimental_features (settings);

  meta_settings_enable_experimental_feature (
    settings,
    META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER);

  ret = g_test_run ();

  meta_quit (ret != 0);

  return FALSE;
}

static void
meta_test_headless_start (void)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  GList *gpus;
  MetaGpu *gpu;

  gpus = meta_monitor_manager_get_gpus (monitor_manager);
  g_assert_cmpint ((int) g_list_length (gpus), ==, 1);

  gpu = gpus->data;
  g_assert_null (meta_gpu_get_modes (gpu));
  g_assert_null (meta_gpu_get_outputs (gpu));
  g_assert_null (meta_gpu_get_crtcs (gpu));
  g_assert_null (monitor_manager->monitors);
  g_assert_null (monitor_manager->logical_monitors);

  g_assert_cmpint (monitor_manager->screen_width,
                   ==,
                   META_MONITOR_MANAGER_MIN_SCREEN_WIDTH);
  g_assert_cmpint (monitor_manager->screen_height,
                   ==,
                   META_MONITOR_MANAGER_MIN_SCREEN_HEIGHT);
}

static void
meta_test_headless_monitor_getters (void)
{
  MetaDisplay *display;
  int index;

  display = meta_get_display ();

  index = meta_display_get_monitor_index_for_rect (display,
                                                   &(MetaRectangle) { 0 });
  g_assert_cmpint (index, ==, -1);
}

static void
meta_test_headless_monitor_connect (void)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);
  MetaMonitorTestSetup *test_setup;
  MetaCrtcMode **modes;
  MetaCrtcMode *crtc_mode;
  MetaCrtc *crtc;
  MetaCrtc **possible_crtcs;
  MetaOutput *output;
  GList *logical_monitors;
  ClutterActor *stage;

  test_setup = g_new0 (MetaMonitorTestSetup, 1);

  crtc_mode = g_object_new (META_TYPE_CRTC_MODE, NULL);
  crtc_mode->mode_id = 1;
  crtc_mode->width = 1024;
  crtc_mode->height = 768;
  crtc_mode->refresh_rate = 60.0;
  test_setup->modes = g_list_append (NULL, crtc_mode);

  crtc = g_object_new (META_TYPE_CRTC, NULL);
  crtc->crtc_id = 1;
  crtc->all_transforms = ALL_TRANSFORMS;
  test_setup->crtcs = g_list_append (NULL, crtc);

  modes = g_new0 (MetaCrtcMode *, 1);
  modes[0] = crtc_mode;

  possible_crtcs = g_new0 (MetaCrtc *, 1);
  possible_crtcs[0] = g_list_first (test_setup->crtcs)->data;

  output = g_object_new (META_TYPE_OUTPUT, NULL);
  output->winsys_id = 1;
  output->name = g_strdup ("DP-1");
  output->vendor = g_strdup ("MetaProduct's Inc.");
  output->product = g_strdup ("MetaMonitor");
  output->serial = g_strdup ("0x987654");
  output->preferred_mode = modes[0];
  output->n_modes = 1;
  output->modes = modes;
  output->n_possible_crtcs = 1;
  output->possible_crtcs = possible_crtcs;
  output->connector_type = META_CONNECTOR_TYPE_DisplayPort;
  test_setup->outputs = g_list_append (NULL, output);

  meta_monitor_manager_test_emulate_hotplug (monitor_manager_test, test_setup);

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  g_assert_cmpint (g_list_length (logical_monitors), ==, 1);

  g_assert_cmpint (monitor_manager->screen_width, ==, 1024);
  g_assert_cmpint (monitor_manager->screen_height, ==, 768);

  stage = meta_backend_get_stage (backend);
  g_assert_cmpint (clutter_actor_get_width (stage), ==, 1024);
  g_assert_cmpint (clutter_actor_get_height (stage), ==, 768);
}

static MetaMonitorTestSetup *
create_headless_test_setup (void)
{
  return g_new0 (MetaMonitorTestSetup, 1);
}

static void
init_tests (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

  MetaMonitorTestSetup *initial_test_setup;

  initial_test_setup = create_headless_test_setup ();
  meta_monitor_manager_test_init_test_setup (initial_test_setup);

  g_test_add_func ("/headless-start/start", meta_test_headless_start);
  g_test_add_func ("/headless-start/monitor-getters",
                   meta_test_headless_monitor_getters);
  g_test_add_func ("/headless-start/connect",
                   meta_test_headless_monitor_connect);
}

int
main (int argc, char *argv[])
{
  init_tests (argc, argv);

  meta_plugin_manager_load ("default");

  meta_override_compositor_configuration (META_COMPOSITOR_TYPE_WAYLAND,
                                          META_TYPE_BACKEND_TEST);
  meta_wayland_override_display_name ("mutter-test-display");

  meta_init ();
  meta_register_with_session ();

  g_idle_add (run_tests, NULL);

  return meta_run ();
}
