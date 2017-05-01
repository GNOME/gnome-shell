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
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor.h"
#include "tests/meta-monitor-manager-test.h"
#include "tests/monitor-test-utils.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

#define MAX_N_MODES 10
#define MAX_N_OUTPUTS 10
#define MAX_N_CRTCS 10
#define MAX_N_MONITORS 10
#define MAX_N_LOGICAL_MONITORS 10

/*
 * The following structures are used to define test cases.
 *
 * Each test case consists of a test case setup and a test case expectaction.
 * and a expected result, consisting
 * of an array of monitors, logical monitors and a screen size.
 *
 * TEST CASE SETUP:
 *
 * A test case setup consists of an array of modes, an array of outputs and an
 * array of CRTCs.
 *
 * A mode has a width and height in pixels, and a refresh rate in updates per
 * second.
 *
 * An output has an array of available modes, and a preferred mode. Modes are
 * defined as indices into the modes array of the test case setup.
 *
 * It also has CRTc and an array of possible CRTCs. Crtcs are defined as indices
 * into the CRTC array. The CRTC value -1 means no CRTC.
 *
 * It also has various meta data, such as physical dimension, tile info and
 * scale.
 *
 * A CRTC only has a current mode. A mode is defined as an index into the modes
 * array.
 *
 *
 * TEST CASE EXPECTS:
 *
 * A test case expects consists of an array of monitors, an array of logical
 * monitors, a output and crtc count, and a screen width.
 *
 * A monitor represents a physical monitor (such as an external monitor, or a
 * laptop panel etc). A monitor consists of an array of outputs, defined by
 * indices into the setup output array, an array of monitor modes, and the
 * current mode, defined by an index into the monitor modes array, and the
 * physical dimensions.
 *
 * A logical monitor represents a region of the total screen area. It contains
 * the expected layout and a scale.
 */

typedef enum _MonitorTestFlag
{
  MONITOR_TEST_FLAG_NONE,
  MONITOR_TEST_FLAG_NO_STORED
} MonitorTestFlag;

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
  MetaTileInfo tile_info;
  int scale;
  gboolean is_laptop_panel;
  gboolean is_underscanning;
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

typedef struct _MonitorTestCaseMonitorCrtcMode
{
  int output;
  int crtc_mode;
} MetaTestCaseMonitorCrtcMode;

typedef struct _MonitorTestCaseMonitorMode
{
  int width;
  int height;
  MetaTestCaseMonitorCrtcMode crtc_modes[MAX_N_CRTCS];
} MetaMonitorTestCaseMonitorMode;

typedef struct _MonitorTestCaseMonitor
{
  long outputs[MAX_N_OUTPUTS];
  int n_outputs;
  MetaMonitorTestCaseMonitorMode modes[MAX_N_MODES];
  int n_modes;
  int current_mode;
  int width_mm;
  int height_mm;
  gboolean is_underscanning;
} MonitorTestCaseMonitor;

typedef struct _MonitorTestCaseLogicalMonitor
{
  MetaRectangle layout;
  int scale;
  int monitors[MAX_N_MONITORS];
  int n_monitors;
  MetaMonitorTransform transform;
} MonitorTestCaseLogicalMonitor;

typedef struct _MonitorTestCaseCrtcExpect
{
  MetaMonitorTransform transform;
  int current_mode;
  int x;
  int y;
} MonitorTestCaseCrtcExpect;

typedef struct _MonitorTestCaseExpect
{
  MonitorTestCaseMonitor monitors[MAX_N_MONITORS];
  int n_monitors;
  MonitorTestCaseLogicalMonitor logical_monitors[MAX_N_LOGICAL_MONITORS];
  int n_logical_monitors;
  int primary_logical_monitor;
  int n_outputs;
  MonitorTestCaseCrtcExpect crtcs[MAX_N_CRTCS];
  int n_crtcs;
  int n_tiled_monitors;
  int screen_width;
  int screen_height;
} MonitorTestCaseExpect;

typedef struct _MonitorTestCase
{
  MonitorTestCaseSetup setup;
  MonitorTestCaseExpect expect;
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
        .width_mm = 220,
        .height_mm = 124
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
  },

  .expect = {
    .monitors = {
      {
        .outputs = { 0 },
        .n_outputs = 1,
        .modes = {
          {
            .width = 1024,
            .height = 768,
            .crtc_modes = {
              {
                .output = 0,
                .crtc_mode = 0
              }
            }
          }
        },
        .n_modes = 1,
        .current_mode = 0,
        .width_mm = 222,
        .height_mm = 125
      },
      {
        .outputs = { 1 },
        .n_outputs = 1,
        .modes = {
          {
            .width = 1024,
            .height = 768,
            .crtc_modes = {
              {
                .output = 1,
                .crtc_mode = 0
              }
            }
          }
        },
        .n_modes = 1,
        .current_mode = 0,
        .width_mm = 220,
        .height_mm = 124
      }
    },
    .n_monitors = 2,
    .logical_monitors = {
      {
        .monitors = { 0 },
        .n_monitors = 1,
        .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
      {
        .monitors = { 1 },
        .n_monitors = 1,
        .layout = { .x = 1024, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      }
    },
    .n_logical_monitors = 2,
    .primary_logical_monitor = 0,
    .n_outputs = 2,
    .crtcs = {
      {
        .current_mode = 0,
      },
      {
        .current_mode = 0,
      }
    },
    .n_crtcs = 2,
    .screen_width = 1024 * 2,
    .screen_height = 768
  }
};

static MetaOutput *
output_from_winsys_id (MetaMonitorManager *monitor_manager,
                       long                winsys_id)
{
  unsigned int i;

  for (i = 0; i < monitor_manager->n_outputs; i++)
    {
      MetaOutput *output = &monitor_manager->outputs[i];

      if (output->winsys_id == winsys_id)
        return output;
    }

  return NULL;
}

typedef struct _CheckMonitorModeData
{
  MetaMonitorManager *monitor_manager;
  MetaTestCaseMonitorCrtcMode *expect_crtc_mode_iter;
} CheckMonitorModeData;

static gboolean
check_monitor_mode (MetaMonitor         *monitor,
                    MetaMonitorMode     *mode,
                    MetaMonitorCrtcMode *monitor_crtc_mode,
                    gpointer             user_data,
                    GError             **error)
{
  CheckMonitorModeData *data = user_data;
  MetaMonitorManager *monitor_manager = data->monitor_manager;
  MetaOutput *output;
  MetaCrtcMode *crtc_mode;
  int expect_crtc_mode_index;

  output = output_from_winsys_id (monitor_manager,
                                  data->expect_crtc_mode_iter->output);
  expect_crtc_mode_index = data->expect_crtc_mode_iter->crtc_mode;
  if (expect_crtc_mode_index == -1)
    crtc_mode = NULL;
  else
    crtc_mode = &monitor_manager->modes[expect_crtc_mode_index];

  g_assert (monitor_crtc_mode->output == output);
  g_assert (monitor_crtc_mode->crtc_mode == crtc_mode);

  data->expect_crtc_mode_iter++;

  return TRUE;
}

static gboolean
check_current_monitor_mode (MetaMonitor         *monitor,
                            MetaMonitorMode     *mode,
                            MetaMonitorCrtcMode *monitor_crtc_mode,
                            gpointer             user_data,
                            GError             **error)
{
  CheckMonitorModeData *data = user_data;
  MetaMonitorManager *monitor_manager = data->monitor_manager;
  MetaOutput *output;

  output = output_from_winsys_id (monitor_manager,
                                  data->expect_crtc_mode_iter->output);

  if (data->expect_crtc_mode_iter->crtc_mode == -1)
    {
      g_assert_null (output->crtc);
    }
  else
    {
      MetaLogicalMonitor *logical_monitor;

      g_assert_nonnull (output->crtc);
      g_assert (monitor_crtc_mode->crtc_mode == output->crtc->current_mode);

      logical_monitor = output->crtc->logical_monitor;
      g_assert_nonnull (logical_monitor);
    }


  data->expect_crtc_mode_iter++;

  return TRUE;
}

static MetaLogicalMonitor *
logical_monitor_from_layout (MetaMonitorManager *monitor_manager,
                             MetaRectangle      *layout)
{
  GList *l;

  for (l = monitor_manager->logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      if (meta_rectangle_equal (layout, &logical_monitor->rect))
        return logical_monitor;
    }

  return NULL;
}

static void
check_logical_monitor (MonitorTestCase               *test_case,
                       MetaMonitorManager            *monitor_manager,
                       MonitorTestCaseLogicalMonitor *test_logical_monitor)
{
  MetaLogicalMonitor *logical_monitor;
  MetaOutput *primary_output;
  GList *monitors;
  GList *l;
  int i;

  logical_monitor = logical_monitor_from_layout (monitor_manager,
                                                 &test_logical_monitor->layout);
  g_assert_nonnull (logical_monitor);

  g_assert_cmpint (logical_monitor->rect.x,
                   ==,
                   test_logical_monitor->layout.x);
  g_assert_cmpint (logical_monitor->rect.y,
                   ==,
                   test_logical_monitor->layout.y);
  g_assert_cmpint (logical_monitor->rect.width,
                   ==,
                   test_logical_monitor->layout.width);
  g_assert_cmpint (logical_monitor->rect.height,
                   ==,
                   test_logical_monitor->layout.height);
  g_assert_cmpint (logical_monitor->scale,
                   ==,
                   test_logical_monitor->scale);
  g_assert_cmpuint (logical_monitor->transform,
                    ==,
                    test_logical_monitor->transform);

  if (logical_monitor == monitor_manager->primary_logical_monitor)
    g_assert (meta_logical_monitor_is_primary (logical_monitor));

  primary_output = NULL;
  monitors = meta_logical_monitor_get_monitors (logical_monitor);
  g_assert_cmpint ((int) g_list_length (monitors),
                   ==,
                   test_logical_monitor->n_monitors);

  for (i = 0; i < test_logical_monitor->n_monitors; i++)
    {
      MetaMonitor *monitor =
        g_list_nth (monitor_manager->monitors,
                    test_logical_monitor->monitors[i])->data;

      g_assert_nonnull (g_list_find (monitors, monitor));
    }

  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      GList *outputs;
      GList *l_output;

      outputs = meta_monitor_get_outputs (monitor);
      for (l_output = outputs; l_output; l_output = l_output->next)
        {
          MetaOutput *output = l_output->data;

          if (output->is_primary)
            {
              g_assert_null (primary_output);
              primary_output = output;
            }

          g_assert (!output->crtc ||
                    output->crtc->logical_monitor == logical_monitor);
          g_assert_cmpint (logical_monitor->is_presentation,
                           ==,
                           output->is_presentation);
        }
    }

  if (logical_monitor == monitor_manager->primary_logical_monitor)
    g_assert_nonnull (primary_output);
}

static void
get_compensated_crtc_position (MetaCrtc *crtc,
                               int      *x,
                               int      *y)
{
  MetaLogicalMonitor *logical_monitor;
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  GList *views;
  GList *l;

  logical_monitor = crtc->logical_monitor;
  g_assert_nonnull (logical_monitor);

  views = meta_renderer_get_views (renderer);
  for (l = views; l; l = l->next)
    {
      MetaRendererView *view = l->data;
      MetaRectangle view_layout;

      clutter_stage_view_get_layout (CLUTTER_STAGE_VIEW (view),
                                     &view_layout);

      if (meta_rectangle_equal (&view_layout,
                                &logical_monitor->rect))
        {
          *x = crtc->rect.x - view_layout.x;
          *y = crtc->rect.y - view_layout.y;
          return;
        }
    }

  *x = crtc->rect.x;
  *y = crtc->rect.y;
}

static void
check_monitor_configuration (MonitorTestCase *test_case)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);
  int tiled_monitor_count;
  GList *monitors;
  int n_logical_monitors;
  GList *l;
  int i;

  g_assert_cmpint (monitor_manager->screen_width,
                   ==,
                   test_case->expect.screen_width);
  g_assert_cmpint (monitor_manager->screen_height,
                   ==,
                   test_case->expect.screen_height);
  g_assert_cmpint ((int) monitor_manager->n_outputs,
                   ==,
                   test_case->expect.n_outputs);
  g_assert_cmpint ((int) monitor_manager->n_crtcs,
                   ==,
                   test_case->expect.n_crtcs);

  tiled_monitor_count =
    meta_monitor_manager_test_get_tiled_monitor_count (monitor_manager_test);
  g_assert_cmpint (tiled_monitor_count,
                   ==,
                   test_case->expect.n_tiled_monitors);

  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  g_assert_cmpint ((int) g_list_length (monitors),
                   ==,
                   test_case->expect.n_monitors);
  for (l = monitors, i = 0; l; l = l->next, i++)
    {
      MetaMonitor *monitor = l->data;
      GList *outputs;
      GList *l_output;
      int j;
      int width_mm, height_mm;
      GList *modes;
      GList *l_mode;
      MetaMonitorMode *current_mode;
      int expected_current_mode_index;
      MetaMonitorMode *expected_current_mode;

      outputs = meta_monitor_get_outputs (monitor);

      g_assert_cmpint ((int) g_list_length (outputs),
                       ==,
                       test_case->expect.monitors[i].n_outputs);

      for (l_output = outputs, j = 0; l_output; l_output = l_output->next, j++)
        {
          MetaOutput *output = l_output->data;
          long winsys_id = test_case->expect.monitors[i].outputs[j];

          g_assert (output == output_from_winsys_id (monitor_manager,
                                                     winsys_id));
          g_assert_cmpint (test_case->expect.monitors[i].is_underscanning,
                           ==,
                           output->is_underscanning);
        }

      meta_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);
      g_assert_cmpint (width_mm,
                       ==,
                       test_case->expect.monitors[i].width_mm);
      g_assert_cmpint (height_mm,
                       ==,
                       test_case->expect.monitors[i].height_mm);

      modes = meta_monitor_get_modes (monitor);
      for (l_mode = modes, j = 0; l_mode; l_mode = l_mode->next, j++)
        {
          MetaMonitorMode *mode = l_mode->data;
          int width;
          int height;
          CheckMonitorModeData data;

          meta_monitor_mode_get_resolution (mode, &width, &height);

          g_assert_cmpint (width,
                           ==,
                           test_case->expect.monitors[i].modes[j].width);
          g_assert_cmpint (height,
                           ==,
                           test_case->expect.monitors[i].modes[j].height);

          data = (CheckMonitorModeData) {
            .monitor_manager = monitor_manager,
            .expect_crtc_mode_iter =
              test_case->expect.monitors[i].modes[j].crtc_modes
          };
          meta_monitor_mode_foreach_crtc (monitor, mode,
                                          check_monitor_mode,
                                          &data,
                                          NULL);
        }

      current_mode = meta_monitor_get_current_mode (monitor);
      expected_current_mode_index = test_case->expect.monitors[i].current_mode;
      if (expected_current_mode_index == -1)
        expected_current_mode = NULL;
      else
        expected_current_mode = g_list_nth (modes,
                                            expected_current_mode_index)->data;

      g_assert (current_mode == expected_current_mode);
      if (current_mode)
        g_assert (meta_monitor_is_active (monitor));
      else
        g_assert (!meta_monitor_is_active (monitor));

      if (current_mode)
        {
          CheckMonitorModeData data;

          data = (CheckMonitorModeData) {
            .monitor_manager = monitor_manager,
            .expect_crtc_mode_iter =
              test_case->expect.monitors[i].modes[expected_current_mode_index].crtc_modes
          };
          meta_monitor_mode_foreach_crtc (monitor, expected_current_mode,
                                          check_current_monitor_mode,
                                          &data,
                                          NULL);
        }

      meta_monitor_derive_current_mode (monitor);
      g_assert (current_mode == meta_monitor_get_current_mode (monitor));
    }

  n_logical_monitors =
    meta_monitor_manager_get_num_logical_monitors (monitor_manager);
  g_assert_cmpint (n_logical_monitors,
                   ==,
                   test_case->expect.n_logical_monitors);

  /*
   * Check that we have a primary logical monitor (except for headless),
   * and that the main output of the first monitor is the only output
   * that is marked as primary (further below). Note: outputs being primary or
   * not only matters on X11.
   */
  if (test_case->expect.primary_logical_monitor == -1)
    {
      g_assert_null (monitor_manager->primary_logical_monitor);
      g_assert_null (monitor_manager->logical_monitors);
    }
  else
    {
      MonitorTestCaseLogicalMonitor *test_logical_monitor =
        &test_case->expect.logical_monitors[test_case->expect.primary_logical_monitor];
      MetaLogicalMonitor *logical_monitor;

      logical_monitor =
        logical_monitor_from_layout (monitor_manager,
                                     &test_logical_monitor->layout);
      g_assert (logical_monitor == monitor_manager->primary_logical_monitor);
    }

  for (i = 0; i < test_case->expect.n_logical_monitors; i++)
    {
      MonitorTestCaseLogicalMonitor *test_logical_monitor =
        &test_case->expect.logical_monitors[i];

      check_logical_monitor (test_case, monitor_manager, test_logical_monitor);
    }
  g_assert_cmpint (n_logical_monitors, ==, i);

  for (i = 0; i < test_case->expect.n_crtcs; i++)
    {
      if (test_case->expect.crtcs[i].current_mode == -1)
        {
          g_assert_null (monitor_manager->crtcs[i].current_mode);
        }
      else
        {
          MetaCrtc *crtc = &monitor_manager->crtcs[i];
          MetaLogicalMonitor *logical_monitor = crtc->logical_monitor;
          MetaCrtcMode *expected_current_mode =
            &monitor_manager->modes[test_case->expect.crtcs[i].current_mode];
          int crtc_x, crtc_y;

          g_assert (crtc->current_mode == expected_current_mode);

          g_assert_cmpuint (crtc->transform,
                            ==,
                            test_case->expect.crtcs[i].transform);

          if (meta_is_stage_views_enabled ())
            {
              get_compensated_crtc_position (crtc, &crtc_x, &crtc_y);

              g_assert_cmpint (crtc_x, ==, test_case->expect.crtcs[i].x);
              g_assert_cmpint (crtc_y, ==, test_case->expect.crtcs[i].y);
            }
          else
            {
              int expect_crtc_x;
              int expect_crtc_y;

              g_assert_cmpuint (logical_monitor->transform,
                                ==,
                                crtc->transform);

              expect_crtc_x = (test_case->expect.crtcs[i].x +
                               logical_monitor->rect.x);
              expect_crtc_y = (test_case->expect.crtcs[i].y +
                               logical_monitor->rect.y);

              g_assert_cmpint (crtc->rect.x, ==, expect_crtc_x);
              g_assert_cmpint (crtc->rect.y, ==, expect_crtc_y);
            }
        }
    }
}

static void
meta_output_test_destroy_notify (MetaOutput *output)
{
  g_clear_pointer (&output->driver_private, g_free);
}

static MetaMonitorTestSetup *
create_monitor_test_setup (MonitorTestCase *test_case,
                           MonitorTestFlag  flags)
{
  MetaMonitorTestSetup *test_setup;
  int i;
  int n_laptop_panels = 0;
  int n_normal_panels = 0;
  gboolean hotplug_mode_update;

  if (flags & MONITOR_TEST_FLAG_NO_STORED)
    hotplug_mode_update = TRUE;
  else
    hotplug_mode_update = FALSE;

  test_setup = g_new0 (MetaMonitorTestSetup, 1);

  test_setup->n_modes = test_case->setup.n_modes;
  test_setup->modes = g_new0 (MetaCrtcMode, test_setup->n_modes);
  for (i = 0; i < test_setup->n_modes; i++)
    {
      test_setup->modes[i] = (MetaCrtcMode) {
        .mode_id = i,
        .width = test_case->setup.modes[i].width,
        .height = test_case->setup.modes[i].height,
        .refresh_rate = test_case->setup.modes[i].refresh_rate
      };
    }

  test_setup->n_crtcs = test_case->setup.n_crtcs;
  test_setup->crtcs = g_new0 (MetaCrtc, test_setup->n_crtcs);
  for (i = 0; i < test_setup->n_crtcs; i++)
    {
      int current_mode_index;
      MetaCrtcMode *current_mode;

      current_mode_index = test_case->setup.crtcs[i].current_mode;
      if (current_mode_index == -1)
        current_mode = NULL;
      else
        current_mode = &test_setup->modes[current_mode_index];

      test_setup->crtcs[i] = (MetaCrtc) {
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
      MetaOutputTest *output_test;
      int crtc_index;
      MetaCrtc *crtc;
      int preferred_mode_index;
      MetaCrtcMode *preferred_mode;
      MetaCrtcMode **modes;
      int n_modes;
      int j;
      MetaCrtc **possible_crtcs;
      int n_possible_crtcs;
      int scale;
      gboolean is_laptop_panel;

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
      modes = g_new0 (MetaCrtcMode *, n_modes);
      for (j = 0; j < n_modes; j++)
        {
          int mode_index;

          mode_index = test_case->setup.outputs[i].modes[j];
          modes[j] = &test_setup->modes[mode_index];
        }

      n_possible_crtcs = test_case->setup.outputs[i].n_possible_crtcs;
      possible_crtcs = g_new0 (MetaCrtc *, n_possible_crtcs);
      for (j = 0; j < n_possible_crtcs; j++)
        {
          int possible_crtc_index;

          possible_crtc_index = test_case->setup.outputs[i].possible_crtcs[j];
          possible_crtcs[j] = &test_setup->crtcs[possible_crtc_index];
        }

      output_test = g_new0 (MetaOutputTest, 1);

      scale = test_case->setup.outputs[i].scale;
      if (scale < 1)
        scale = 1;

      *output_test = (MetaOutputTest) {
        .scale = scale
      };

      is_laptop_panel = test_case->setup.outputs[i].is_laptop_panel;

      test_setup->outputs[i] = (MetaOutput) {
        .crtc = crtc,
        .winsys_id = i,
        .name = (is_laptop_panel ? g_strdup_printf ("eDP-%d",
                                                    ++n_laptop_panels)
                                 : g_strdup_printf ("DP-%d",
                                                    ++n_normal_panels)),
        .vendor = g_strdup ("MetaProduct's Inc."),
        .product = g_strdup ("MetaMonitor"),
        .serial = g_strdup ("0x123456"),
        .suggested_x = -1,
        .suggested_y = -1,
        .hotplug_mode_update = hotplug_mode_update,
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
        .connector_type = (is_laptop_panel ? META_CONNECTOR_TYPE_eDP
                                           : META_CONNECTOR_TYPE_DisplayPort),
        .tile_info = test_case->setup.outputs[i].tile_info,
        .is_underscanning = test_case->setup.outputs[i].is_underscanning,
        .driver_private = output_test,
        .driver_notify = (GDestroyNotify) meta_output_test_destroy_notify
      };
    }

  return test_setup;
}

static void
meta_test_monitor_initial_linear_config (void)
{
  check_monitor_configuration (&initial_test_case);
}

static void
emulate_hotplug (MetaMonitorTestSetup *test_setup)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);

  meta_monitor_manager_test_emulate_hotplug (monitor_manager_test, test_setup);
}

static void
meta_test_monitor_one_disconnected_linear_config (void)
{
  MonitorTestCase test_case = initial_test_case;
  MetaMonitorTestSetup *test_setup;

  test_case.setup.n_outputs = 1;

  test_case.expect = (MonitorTestCaseExpect) {
    .monitors = {
      {
        .outputs = { 0 },
        .n_outputs = 1,
        .modes = {
          {
            .width = 1024,
            .height = 768,
            .crtc_modes = {
              {
                .output = 0,
                .crtc_mode = 0
              }
            }
          }
        },
        .n_modes = 1,
        .current_mode = 0,
        .width_mm = 222,
        .height_mm = 125
      }
    },
    .n_monitors = 1,
    .logical_monitors = {
      {
        .monitors = { 0 },
        .n_monitors = 1,
        .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
    },
    .n_logical_monitors = 1,
    .primary_logical_monitor = 0,
    .n_outputs = 1,
    .crtcs = {
      {
        .current_mode = 0,
      },
      {
        .current_mode = -1,
      }
    },
    .n_crtcs = 2,
    .screen_width = 1024,
    .screen_height = 768
  };

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_one_off_linear_config (void)
{
  MonitorTestCase test_case;
  MetaMonitorTestSetup *test_setup;
  MonitorTestCaseOutput outputs[] = {
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
      .crtc = -1,
      .modes = { 0 },
      .n_modes = 1,
      .preferred_mode = 0,
      .possible_crtcs = { 1 },
      .n_possible_crtcs = 1,
      .width_mm = 224,
      .height_mm = 126
    }
  };

  test_case = initial_test_case;

  memcpy (&test_case.setup.outputs, &outputs, sizeof (outputs));
  test_case.setup.n_outputs = G_N_ELEMENTS (outputs);

  test_case.setup.crtcs[1].current_mode = -1;

  test_case.expect = (MonitorTestCaseExpect) {
    .monitors = {
      {
        .outputs = { 0 },
        .n_outputs = 1,
        .modes = {
          {
            .width = 1024,
            .height = 768,
            .crtc_modes = {
              {
                .output = 0,
                .crtc_mode = 0
              }
            }
          }
        },
        .n_modes = 1,
        .current_mode = 0,
        .width_mm = 222,
        .height_mm = 125
      },
      {
        .outputs = { 1 },
        .n_outputs = 1,
        .modes = {
          {
            .width = 1024,
            .height = 768,
            .crtc_modes = {
              {
                .output = 1,
                .crtc_mode = 0
              }
            }
          }
        },
        .n_modes = 1,
        .current_mode = 0,
        .width_mm = 224,
        .height_mm = 126
      }
    },
    .n_monitors = 2,
    .logical_monitors = {
      {
        .monitors = { 0 },
        .n_monitors = 1,
        .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
      {
        .monitors = { 1 },
        .n_monitors = 1,
        .layout = { .x = 1024, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
    },
    .n_logical_monitors = 2,
    .primary_logical_monitor = 0,
    .n_outputs = 2,
    .crtcs = {
      {
        .current_mode = 0,
      },
      {
        .current_mode = 0,
      }
    },
    .n_crtcs = 2,
    .screen_width = 1024 * 2,
    .screen_height = 768
  };

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_preferred_linear_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 800,
          .height = 600,
          .refresh_rate = 60.0
        },
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.0
        },
        {
          .width = 1280,
          .height = 720,
          .refresh_rate = 60.0
        }
      },
      .n_modes = 3,
      .outputs = {
        {
          .crtc = -1,
          .modes = { 0, 1, 2 },
          .n_modes = 3,
          .preferred_mode = 1,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125
        }
      },
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 1
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            },
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 1
                }
              }
            },
            {
              .width = 1280,
              .height = 720,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 2
                }
              }
            }
          },
          .n_modes = 3,
          .current_mode = 1,
          .width_mm = 222,
          .height_mm = 125
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        },
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 1,
        }
      },
      .n_crtcs = 1,
      .screen_width = 1024,
      .screen_height = 768,
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_tiled_linear_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 400,
          .height = 600,
          .refresh_rate = 60.0
        },
      },
      .n_modes = 1,
      .outputs = {
        {
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 0,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        },
        {
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 1,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0, 1 },
          .n_outputs = 2,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                },
                {
                  .output = 1,
                  .crtc_mode = 0,
                }
              }
            },
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 800, .height = 600 },
          .scale = 1
        },
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 0,
          .x = 400,
          .y = 0
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 1,
      .screen_width = 800,
      .screen_height = 600,
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_hidpi_linear_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1280,
          .height = 720,
          .refresh_rate = 60.0
        },
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.0
        }
      },
      .n_modes = 2,
      .outputs = {
        {
          .crtc = 0,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          /* These will result in DPI of about 216" */
          .width_mm = 150,
          .height_mm = 85,
          .scale = 2,
        },
        {
          .crtc = 1,
          .modes = { 1 },
          .n_modes = 1,
          .preferred_mode = 1,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
          .scale = 1,
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1280,
              .height = 720,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            },
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 150,
          .height_mm = 85
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 1
                }
              }
            },
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 640, .height = 360 },
          .scale = 2
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 640, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 1,
        }
      },
      .n_crtcs = 2,
      .screen_width = 640 + 1024,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_suggested_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
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
      },
      .n_modes = 2,
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
          .modes = { 1 },
          .n_modes = 1,
          .preferred_mode = 1,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 1
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_monitors = 2,
      /*
       * Logical monitors expectations altered to correspond to the
       * "suggested_x/y" changed further below.
       */
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 1024, .y = 758, .width = 800, .height = 600 },
          .scale = 1
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 1,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 1,
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 0,
      .screen_width = 1024 + 800,
      .screen_height = 1358
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);

  test_setup->outputs[0].suggested_x = 1024;
  test_setup->outputs[0].suggested_y = 758;
  test_setup->outputs[1].suggested_x = 0;
  test_setup->outputs[1].suggested_y = 0;

  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_limited_crtcs (void)
{
  MonitorTestCase test_case = {
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
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0
        }
      },
      .n_crtcs = 1
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
                {
                  .width = 1024,
                  .height = 768,
                  .crtc_modes = {
                    {
                      .output = 1,
                      .crtc_mode = 0
                    }
                  }
                }
          },
          .n_modes = 1,
          .current_mode = -1,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        },
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 1,
      .n_tiled_monitors = 0,
      .screen_width = 1024,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);

  /*
   * With the config manager, we'll get a g_warning.
   * With the old it's just a meta_warning().
   */
  if (is_using_monitor_config_manager ())
    {
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                             "Failed to use linear *");
    }
  emulate_hotplug (test_setup);
  g_test_assert_expected_messages ();

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_lid_switch_config (void)
{
  MonitorTestCase test_case = {
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
          .height_mm = 125,
          .is_laptop_panel = TRUE
        },
        {
          .crtc = 1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 220,
          .height_mm = 124
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
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 1024, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 0,
      .screen_width = 1024 * 2,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);

  meta_monitor_manager_test_set_is_lid_closed (monitor_manager_test, TRUE);
  meta_monitor_manager_lid_is_closed_changed (monitor_manager);

  test_case.expect.logical_monitors[0] = (MonitorTestCaseLogicalMonitor) {
    .monitors = { 1 },
    .n_monitors = 1,
    .layout = {.x = 0, .y = 0, .width = 1024, .height = 768 },
    .scale = 1
  };
  test_case.expect.n_logical_monitors = 1;
  test_case.expect.screen_width = 1024;
  test_case.expect.monitors[0].current_mode = -1;
  test_case.expect.crtcs[0].current_mode = -1;

  check_monitor_configuration (&test_case);

  meta_monitor_manager_test_set_is_lid_closed (monitor_manager_test, FALSE);
  meta_monitor_manager_lid_is_closed_changed (monitor_manager);

  test_case.expect.n_logical_monitors = 2;
  test_case.expect.screen_width = 1024 * 2;
  test_case.expect.monitors[0].current_mode = 0;

  test_case.expect.crtcs[0].current_mode = 0;
  test_case.expect.crtcs[1].current_mode = 0;

  if (!is_using_monitor_config_manager ())
    {
      test_case.expect.logical_monitors[0] = (MonitorTestCaseLogicalMonitor) {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = {.x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
      };
      test_case.expect.logical_monitors[1] = (MonitorTestCaseLogicalMonitor) {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = {.x = 1024, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
      };
      test_case.expect.n_logical_monitors = 2;
      test_case.expect.primary_logical_monitor = 0;
    }
  else
    {
      /*
       * FIXME: The above expectation is correct, but MetaMonitorConfigManager
       * doesn't support restoring previous configurations yet, so it'll
       * pick keep the external monitor as primary and put it first.
       */
      test_case.expect.logical_monitors[0] = (MonitorTestCaseLogicalMonitor) {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = {.x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
      };
      test_case.expect.logical_monitors[1] = (MonitorTestCaseLogicalMonitor) {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = {.x = 1024, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
      };
      test_case.expect.n_logical_monitors = 2;
      test_case.expect.primary_logical_monitor = 0;
    }

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_lid_opened_config (void)
{
  MonitorTestCase test_case = {
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
          .height_mm = 125,
          .is_laptop_panel = TRUE
        },
        {
          .crtc = 1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 220,
          .height_mm = 124
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
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = -1,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        },
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 1024, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 1, /* Second one checked after lid opened. */
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = -1,
        },
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 0,
      .screen_width = 1024,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Only the new monitor config manager handles this case.");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  meta_monitor_manager_test_set_is_lid_closed (monitor_manager_test, TRUE);

  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);

  meta_monitor_manager_test_set_is_lid_closed (monitor_manager_test, FALSE);
  meta_monitor_manager_lid_is_closed_changed (monitor_manager);

  test_case.expect.n_logical_monitors = 2;
  test_case.expect.screen_width = 1024 * 2;
  test_case.expect.monitors[0].current_mode = 0;
  test_case.expect.crtcs[0].current_mode = 0;
  test_case.expect.crtcs[1].current_mode = 0;

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_lid_closed_no_external (void)
{
  MonitorTestCase test_case = {
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
          .height_mm = 125,
          .is_laptop_panel = TRUE
        }
      },
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0
        }
      },
      .n_crtcs = 1
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0,
        },
      },
      .n_crtcs = 1,
      .n_tiled_monitors = 0,
      .screen_width = 1024,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  meta_monitor_manager_test_set_is_lid_closed (monitor_manager_test, TRUE);

  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_no_outputs (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .n_modes = 0,
      .n_outputs = 0,
      .n_crtcs = 0
    },

    .expect = {
      .n_monitors = 0,
      .n_logical_monitors = 0,
      .primary_logical_monitor = -1,
      .n_outputs = 0,
      .n_crtcs = 0,
      .n_tiled_monitors = 0,
      /* The screen is made 1x1, as clutter stage used cannot be empty. */
      .screen_width = 1,
      .screen_height = 1
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Only the new monitor config manager handles this case.");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);

  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_underscanning_config (void)
{
  MonitorTestCase test_case = {
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
          .height_mm = 125,
          .is_underscanning = TRUE,
        }
      },
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0
        }
      },
      .n_crtcs = 1
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
          .is_underscanning = TRUE,
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 1,
      .screen_width = 1024,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NO_STORED);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_vertical_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.000495910644531
        },
        {
          .width = 800,
          .height = 600,
          .refresh_rate = 60.000495910644531
        }
      },
      .n_modes = 2,
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
          .modes = { 1 },
          .n_modes = 1,
          .preferred_mode = 1,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 220,
          .height_mm = 124
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
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 1
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 768, .width = 800, .height = 600 },
          .scale = 1
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 1,
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 0,
      .screen_width = 1024,
      .screen_height = 768 + 600
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("vertical.xml");
  emulate_hotplug (test_setup);

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_primary_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.000495910644531
        },
        {
          .width = 800,
          .height = 600,
          .refresh_rate = 60.000495910644531
        }
      },
      .n_modes = 2,
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
          .modes = { 1 },
          .n_modes = 1,
          .preferred_mode = 1,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 220,
          .height_mm = 124
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
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 1
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 1024, .y = 0, .width = 800, .height = 600 },
          .scale = 1
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 1,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 1,
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 0,
      .screen_width = 1024 + 800,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("primary.xml");
  emulate_hotplug (test_setup);

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_underscanning_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.000495910644531
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
      },
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0
        },
      },
      .n_crtcs = 1
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
          .is_underscanning = TRUE,
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 1,
      .n_tiled_monitors = 0,
      .screen_width = 1024,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("underscanning.xml");
  emulate_hotplug (test_setup);

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_scale_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1920,
          .height = 1080,
          .refresh_rate = 60.000495910644531
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
      },
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0
        },
      },
      .n_crtcs = 1
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1920,
              .height = 1080,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 960, .height = 540 },
          .scale = 2
        }
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 1,
      .n_tiled_monitors = 0,
      .screen_width = 960,
      .screen_height = 540
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("scale.xml");
  emulate_hotplug (test_setup);

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_tiled_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 400,
          .height = 600,
          .refresh_rate = 60.000495910644531
        }
      },
      .n_modes = 1,
      .outputs = {
        {
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0, 1 },
          .n_possible_crtcs = 2,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 0,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        },
        {
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0, 1 },
          .n_possible_crtcs = 2,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 1,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0, 1 },
          .n_outputs = 2,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0,
                },
                {
                  .output = 1,
                  .crtc_mode = 0,
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 400, .height = 300 },
          .scale = 2
        }
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 0,
          .x = 400,
          .y = 0
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 1,
      .screen_width = 400,
      .screen_height = 300
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("tiled.xml");
  emulate_hotplug (test_setup);

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_tiled_custom_resolution_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 400,
          .height = 600,
          .refresh_rate = 60.000495910644531
        },
        {
          .width = 640,
          .height = 480,
          .refresh_rate = 60.000495910644531
        }
      },
      .n_modes = 2,
      .outputs = {
        {
          .crtc = -1,
          .modes = { 0, 1 },
          .n_modes = 2,
          .preferred_mode = 0,
          .possible_crtcs = { 0, 1 },
          .n_possible_crtcs = 2,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 0,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        },
        {
          .crtc = -1,
          .modes = { 0, 1 },
          .n_modes = 2,
          .preferred_mode = 0,
          .possible_crtcs = { 0, 1 },
          .n_possible_crtcs = 2,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 1,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0, 1 },
          .n_outputs = 2,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0,
                },
                {
                  .output = 1,
                  .crtc_mode = 0,
                }
              }
            },
            {
              .width = 640,
              .height = 480,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 1,
                },
                {
                  .output = 1,
                  .crtc_mode = -1,
                }
              }
            }
          },
          .n_modes = 2,
          .current_mode = 1,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 320, .height = 240 },
          .scale = 2
        }
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 1,
        },
        {
          .current_mode = -1,
          .x = 400,
          .y = 0,
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 1,
      .screen_width = 320,
      .screen_height = 240
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("tiled-custom-resolution.xml");
  emulate_hotplug (test_setup);

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_mirrored_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 800,
          .height = 600,
          .refresh_rate = 60.000495910644531
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
          .width_mm = 220,
          .height_mm = 124
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
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 220,
          .height_mm = 124
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0, 1 },
          .n_monitors = 2,
          .layout = { .x = 0, .y = 0, .width = 800, .height = 600 },
          .scale = 1
        }
      },
      .n_logical_monitors = 1,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 2,
      .n_tiled_monitors = 0,
      .screen_width = 800,
      .screen_height = 600
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("mirrored.xml");
  emulate_hotplug (test_setup);

  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_first_rotated_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.000495910644531
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
          .height_mm = 125,
        },
        {
          .crtc = 1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 0
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 0, .width = 768, .height = 1024 },
          .scale = 1,
          .transform = META_MONITOR_TRANSFORM_270
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 768, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
          .transform = META_MONITOR_TRANSFORM_270
        },
        {
          .current_mode = 0,
        }
      },
      .n_crtcs = 2,
      .screen_width = 768 + 1024,
      .screen_height = 1024
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("first-rotated.xml");
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_second_rotated_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.000495910644531
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
          .height_mm = 125,
        },
        {
          .crtc = 1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
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
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 256, .width = 1024, .height = 768 },
          .scale = 1
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 1024, .y = 0, .width = 768, .height = 1024 },
          .scale = 1,
          .transform = META_MONITOR_TRANSFORM_90
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 0,
          .transform = META_MONITOR_TRANSFORM_90
        }
      },
      .n_crtcs = 2,
      .screen_width = 768 + 1024,
      .screen_height = 1024
    }
  };
  MetaMonitorTestSetup *test_setup;

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("second-rotated.xml");
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_second_rotated_tiled_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.000495910644531
        },
        {
          .width = 400,
          .height = 600,
          .refresh_rate = 60.000495910644531
        }
      },
      .n_modes = 2,
      .outputs = {
        {
          .crtc = 0,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
        },
        {
          .crtc = -1,
          .modes = { 1 },
          .n_modes = 1,
          .preferred_mode = 1,
          .possible_crtcs = { 1, 2 },
          .n_possible_crtcs = 2,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 0,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        },
        {
          .crtc = -1,
          .modes = { 1 },
          .n_modes = 1,
          .preferred_mode = 1,
          .possible_crtcs = { 1, 2 },
          .n_possible_crtcs = 2,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 1,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        }
      },
      .n_outputs = 3,
      .crtcs = {
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 3
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        },
        {
          .outputs = { 1, 2 },
          .n_outputs = 2,
          .modes = {
            {
              .width = 800,
              .height = 600,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 1,
                },
                {
                  .output = 2,
                  .crtc_mode = 1,
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 256, .width = 1024, .height = 768 },
          .scale = 1
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 1024, .y = 0, .width = 600, .height = 800 },
          .scale = 1,
          .transform = META_MONITOR_TRANSFORM_90
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 0,
      .n_outputs = 3,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 1,
          .transform = META_MONITOR_TRANSFORM_90,
          .x = 0,
          .y = 400,
        },
        {
          .current_mode = 1,
          .transform = META_MONITOR_TRANSFORM_90
        }
      },
      .n_crtcs = 3,
      .n_tiled_monitors = 1,
      .screen_width = 1024 + 600,
      .screen_height = 1024
    }
  };
  MetaMonitorTestSetup *test_setup;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  meta_monitor_manager_test_set_handles_transforms (monitor_manager_test,
                                                    TRUE);

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("second-rotated-tiled.xml");
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_custom_second_rotated_nonnative_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.000495910644531
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
          .height_mm = 125,
        },
        {
          .crtc = 1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
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
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 0,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .modes = {
            {
              .width = 1024,
              .height = 768,
              .crtc_modes = {
                {
                  .output = 1,
                  .crtc_mode = 0
                }
              }
            }
          },
          .n_modes = 1,
          .current_mode = 0,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .monitors = { 0 },
          .n_monitors = 1,
          .layout = { .x = 0, .y = 256, .width = 1024, .height = 768 },
          .scale = 1
        },
        {
          .monitors = { 1 },
          .n_monitors = 1,
          .layout = { .x = 1024, .y = 0, .width = 768, .height = 1024 },
          .scale = 1,
          .transform = META_MONITOR_TRANSFORM_90
        }
      },
      .n_logical_monitors = 2,
      .primary_logical_monitor = 0,
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = 0,
        },
        {
          .current_mode = 0,
          .transform = META_MONITOR_TRANSFORM_NORMAL
        }
      },
      .n_crtcs = 2,
      .screen_width = 768 + 1024,
      .screen_height = 1024
    }
  };
  MetaMonitorTestSetup *test_setup;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);

  if (!is_using_monitor_config_manager ())
    {
      g_test_skip ("Not using MetaMonitorConfigManager");
      return;
    }

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  meta_monitor_manager_test_set_handles_transforms (monitor_manager_test,
                                                    FALSE);

  test_setup = create_monitor_test_setup (&test_case,
                                          MONITOR_TEST_FLAG_NONE);
  set_custom_monitor_config ("second-rotated.xml");
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

void
init_monitor_tests (void)
{
  MetaMonitorTestSetup *initial_test_setup;

  initial_test_setup = create_monitor_test_setup (&initial_test_case,
                                                  MONITOR_TEST_FLAG_NO_STORED);
  meta_monitor_manager_test_init_test_setup (initial_test_setup);

  g_test_add_func ("/backends/monitor/initial-linear-config",
                   meta_test_monitor_initial_linear_config);
  g_test_add_func ("/backends/monitor/one-disconnected-linear-config",
                   meta_test_monitor_one_disconnected_linear_config);
  g_test_add_func ("/backends/monitor/one-off-linear-config",
                   meta_test_monitor_one_off_linear_config);
  g_test_add_func ("/backends/monitor/preferred-linear-config",
                   meta_test_monitor_preferred_linear_config);
  g_test_add_func ("/backends/monitor/tiled-linear-config",
                   meta_test_monitor_tiled_linear_config);
  g_test_add_func ("/backends/monitor/hidpi-linear-config",
                   meta_test_monitor_hidpi_linear_config);
  g_test_add_func ("/backends/monitor/suggested-config",
                   meta_test_monitor_suggested_config);
  g_test_add_func ("/backends/monitor/limited-crtcs",
                   meta_test_monitor_limited_crtcs);
  g_test_add_func ("/backends/monitor/lid-switch-config",
                   meta_test_monitor_lid_switch_config);
  g_test_add_func ("/backends/monitor/lid-opened-config",
                   meta_test_monitor_lid_opened_config);
  g_test_add_func ("/backends/monitor/lid-closed-no-external",
                   meta_test_monitor_lid_closed_no_external);
  g_test_add_func ("/backends/monitor/no-outputs",
                   meta_test_monitor_no_outputs);
  g_test_add_func ("/backends/monitor/underscanning-config",
                   meta_test_monitor_underscanning_config);

  g_test_add_func ("/backends/monitor/custom/vertical-config",
                   meta_test_monitor_custom_vertical_config);
  g_test_add_func ("/backends/monitor/custom/primary-config",
                   meta_test_monitor_custom_primary_config);
  g_test_add_func ("/backends/monitor/custom/underscanning-config",
                   meta_test_monitor_custom_underscanning_config);
  g_test_add_func ("/backends/monitor/custom/scale-config",
                   meta_test_monitor_custom_scale_config);
  g_test_add_func ("/backends/monitor/custom/tiled-config",
                   meta_test_monitor_custom_tiled_config);
  g_test_add_func ("/backends/monitor/custom/tiled-custom-resolution-config",
                   meta_test_monitor_custom_tiled_custom_resolution_config);
  g_test_add_func ("/backends/monitor/custom/mirrored-config",
                   meta_test_monitor_custom_mirrored_config);
  g_test_add_func ("/backends/monitor/custom/first-rotated-config",
                   meta_test_monitor_custom_first_rotated_config);
  g_test_add_func ("/backends/monitor/custom/second-rotated-config",
                   meta_test_monitor_custom_second_rotated_config);
  g_test_add_func ("/backends/monitor/custom/second-rotated-tiled-config",
                   meta_test_monitor_custom_second_rotated_tiled_config);
  g_test_add_func ("/backends/monitor/custom/second-rotated-nonnative-config",
                   meta_test_monitor_custom_second_rotated_nonnative_config);
}
