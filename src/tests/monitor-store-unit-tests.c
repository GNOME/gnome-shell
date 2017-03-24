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

#include "tests/monitor-store-unit-tests.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-monitor-config-store.h"
#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-monitor-manager-private.h"
#include "tests/monitor-test-utils.h"

#define MAX_N_MONITORS 10
#define MAX_N_LOGICAL_MONITORS 10
#define MAX_N_CONFIGURATIONS 10

typedef struct _MonitorTestCaseMonitorMode
{
  int width;
  int height;
  float refresh_rate;
  MetaCrtcModeFlag flags;
} MonitorTestCaseMonitorMode;

typedef struct _MonitorTestCaseMonitor
{
  const char *connector;
  const char *vendor;
  const char *product;
  const char *serial;
  MonitorTestCaseMonitorMode mode;
  gboolean is_underscanning;
} MonitorTestCaseMonitor;

typedef struct _MonitorTestCaseLogicalMonitor
{
  MetaRectangle layout;
  float scale;
  MetaMonitorTransform transform;
  gboolean is_primary;
  gboolean is_presentation;
  MonitorTestCaseMonitor monitors[MAX_N_MONITORS];
  int n_monitors;
} MonitorTestCaseLogicalMonitor;

typedef struct _MonitorStoreTestConfiguration
{
  MonitorTestCaseLogicalMonitor logical_monitors[MAX_N_LOGICAL_MONITORS];
  int n_logical_monitors;
} MonitorStoreTestConfiguration;

typedef struct _MonitorStoreTestExpect
{
  MonitorStoreTestConfiguration configurations[MAX_N_CONFIGURATIONS];
  int n_configurations;
} MonitorStoreTestExpect;

static MetaMonitorsConfigKey *
create_config_key_from_expect (MonitorStoreTestConfiguration *expect_config)
{
  MetaMonitorsConfigKey *config_key;
  GList *monitor_specs;
  int i;

  monitor_specs = NULL;
  for (i = 0; i < expect_config->n_logical_monitors; i++)
    {
      int j;

      for (j = 0; j < expect_config->logical_monitors[i].n_monitors; j++)
        {
          MetaMonitorSpec *monitor_spec;
          MonitorTestCaseMonitor *test_monitor =
            &expect_config->logical_monitors[i].monitors[j];

          monitor_spec = g_new0 (MetaMonitorSpec, 1);

          monitor_spec->connector = g_strdup (test_monitor->connector);
          monitor_spec->vendor = g_strdup (test_monitor->vendor);
          monitor_spec->product = g_strdup (test_monitor->product);
          monitor_spec->serial = g_strdup (test_monitor->serial);

          monitor_specs = g_list_prepend (monitor_specs, monitor_spec);
        }
    }

  g_assert_nonnull (monitor_specs);

  monitor_specs = g_list_sort (monitor_specs,
                               (GCompareFunc) meta_monitor_spec_compare);

  config_key = g_new0 (MetaMonitorsConfigKey, 1);
  *config_key = (MetaMonitorsConfigKey) {
    .monitor_specs = monitor_specs
  };

  return config_key;
}

static void
check_monitor_configuration (MetaMonitorConfigStore        *config_store,
                             MonitorStoreTestConfiguration *config_expect)
{
  MetaMonitorsConfigKey *config_key;
  MetaMonitorsConfig *config;
  GList *l;
  int i;

  config_key = create_config_key_from_expect (config_expect);
  config = meta_monitor_config_store_lookup (config_store, config_key);
  g_assert_nonnull (config);

  g_assert (meta_monitors_config_key_equal (config->key, config_key));
  meta_monitors_config_key_free (config_key);

  g_assert_cmpuint (g_list_length (config->logical_monitor_configs),
                    ==,
                    config_expect->n_logical_monitors);

  for (l = config->logical_monitor_configs, i = 0; l; l = l->next, i++)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      GList *k;
      int j;

      g_assert (meta_rectangle_equal (&logical_monitor_config->layout,
                                      &config_expect->logical_monitors[i].layout));
      g_assert_cmpfloat (logical_monitor_config->scale,
                         ==,
                         config_expect->logical_monitors[i].scale);
      g_assert_cmpint (logical_monitor_config->transform,
                       ==,
                       config_expect->logical_monitors[i].transform);
      g_assert_cmpint (logical_monitor_config->is_primary,
                       ==,
                       config_expect->logical_monitors[i].is_primary);
      g_assert_cmpint (logical_monitor_config->is_presentation,
                       ==,
                       config_expect->logical_monitors[i].is_presentation);

      g_assert_cmpint ((int) g_list_length (logical_monitor_config->monitor_configs),
                       ==,
                       config_expect->logical_monitors[i].n_monitors);

      for (k = logical_monitor_config->monitor_configs, j = 0;
           k;
           k = k->next, j++)
        {
          MetaMonitorConfig *monitor_config = k->data;
          MonitorTestCaseMonitor *test_monitor =
            &config_expect->logical_monitors[i].monitors[j];

          g_assert_cmpstr (monitor_config->monitor_spec->connector,
                           ==,
                           test_monitor->connector);
          g_assert_cmpstr (monitor_config->monitor_spec->vendor,
                           ==,
                           test_monitor->vendor);
          g_assert_cmpstr (monitor_config->monitor_spec->product,
                           ==,
                           test_monitor->product);
          g_assert_cmpstr (monitor_config->monitor_spec->serial,
                           ==,
                           test_monitor->serial);

          g_assert_cmpint (monitor_config->mode_spec->width,
                           ==,
                           test_monitor->mode.width);
          g_assert_cmpint (monitor_config->mode_spec->height,
                           ==,
                           test_monitor->mode.height);
          g_assert_cmpfloat (monitor_config->mode_spec->refresh_rate,
                             ==,
                             test_monitor->mode.refresh_rate);
          g_assert_cmpint (monitor_config->mode_spec->flags,
                           ==,
                           test_monitor->mode.flags);
          g_assert_cmpint (monitor_config->enable_underscanning,
                           ==,
                           test_monitor->is_underscanning);
        }
    }
}

static void
check_monitor_configurations (MonitorStoreTestExpect *expect)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorConfigManager *config_manager = monitor_manager->config_manager;
  MetaMonitorConfigStore *config_store =
    meta_monitor_config_manager_get_store (config_manager);
  int i;

  g_assert_cmpint (meta_monitor_config_store_get_config_count (config_store),
                   ==,
                   expect->n_configurations);

  for (i = 0; i < expect->n_configurations; i++)
    check_monitor_configuration (config_store, &expect->configurations[i]);
}

static void
meta_test_monitor_store_single (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 1920,
              .height = 1080
            },
            .scale = 1,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1920,
                  .height = 1080,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 1
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("single.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_vertical (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 1024,
              .height = 768
            },
            .scale = 1,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          },
          {
            .layout = {
              .x = 0,
              .y = 768,
              .width = 800,
              .height = 600
            },
            .scale = 1,
            .is_primary = FALSE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-2",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 800,
                  .height = 600,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 2
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("vertical.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_primary (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 1024,
              .height = 768
            },
            .scale = 1,
            .is_primary = FALSE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          },
          {
            .layout = {
              .x = 1024,
              .y = 0,
              .width = 800,
              .height = 600
            },
            .scale = 1,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-2",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 800,
                  .height = 600,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 2
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("primary.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_underscanning (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 1024,
              .height = 768
            },
            .scale = 1,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .is_underscanning = TRUE,
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          },
        },
        .n_logical_monitors = 1
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("underscanning.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_scale (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 960,
              .height = 540
            },
            .scale = 2,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1920,
                  .height = 1080,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 1
      }
    },
    .n_configurations = 1
  };

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  set_custom_monitor_config ("scale.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_fractional_scale (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 800,
              .height = 600
            },
            .scale = 1.5,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1200,
                  .height = 900,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 1
      }
    },
    .n_configurations = 1
  };

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  set_custom_monitor_config ("fractional-scale.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_high_precision_fractional_scale (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 744,
              .height = 558
            },
            .scale = 1.3763440847396851,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 1
      }
    },
    .n_configurations = 1
  };

  if (!meta_is_stage_views_enabled ())
    {
      g_test_skip ("Not using stage views");
      return;
    }

  set_custom_monitor_config ("high-precision-fractional-scale.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_mirrored (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 800,
              .height = 600
            },
            .scale = 1,
            .is_primary = TRUE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 800,
                  .height = 600,
                  .refresh_rate = 60.000495910644531
                }
              },
              {
                .connector = "DP-2",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 800,
                  .height = 600,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 2,
          }
        },
        .n_logical_monitors = 1
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("mirrored.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_first_rotated (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 768,
              .height = 1024
            },
            .scale = 1,
            .transform = META_MONITOR_TRANSFORM_270,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          },
          {
            .layout = {
              .x = 768,
              .y = 0,
              .width = 1024,
              .height = 768
            },
            .scale = 1,
            .transform = META_MONITOR_TRANSFORM_NORMAL,
            .is_primary = FALSE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-2",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 2
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("first-rotated.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_second_rotated (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 256,
              .width = 1024,
              .height = 768
            },
            .scale = 1,
            .transform = META_MONITOR_TRANSFORM_NORMAL,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          },
          {
            .layout = {
              .x = 1024,
              .y = 0,
              .width = 768,
              .height = 1024
            },
            .scale = 1,
            .transform = META_MONITOR_TRANSFORM_90,
            .is_primary = FALSE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-2",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531
                }
              }
            },
            .n_monitors = 1,
          }
        },
        .n_logical_monitors = 2
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("second-rotated.xml");

  check_monitor_configurations (&expect);
}

static void
meta_test_monitor_store_interlaced (void)
{
  MonitorStoreTestExpect expect = {
    .configurations = {
      {
        .logical_monitors = {
          {
            .layout = {
              .x = 0,
              .y = 0,
              .width = 1024,
              .height = 768
            },
            .scale = 1,
            .is_primary = TRUE,
            .is_presentation = FALSE,
            .monitors = {
              {
                .connector = "DP-1",
                .vendor = "MetaProduct's Inc.",
                .product = "MetaMonitor",
                .serial = "0x123456",
                .mode = {
                  .width = 1024,
                  .height = 768,
                  .refresh_rate = 60.000495910644531,
                  .flags = META_CRTC_MODE_FLAG_INTERLACE,
                }
              }
            },
            .n_monitors = 1,
          },
        },
        .n_logical_monitors = 1
      }
    },
    .n_configurations = 1
  };

  set_custom_monitor_config ("interlaced.xml");

  check_monitor_configurations (&expect);
}

void
init_monitor_store_tests (void)
{
  g_test_add_func ("/backends/monitor-store/single",
                   meta_test_monitor_store_single);
  g_test_add_func ("/backends/monitor-store/vertical",
                   meta_test_monitor_store_vertical);
  g_test_add_func ("/backends/monitor-store/primary",
                   meta_test_monitor_store_primary);
  g_test_add_func ("/backends/monitor-store/underscanning",
                   meta_test_monitor_store_underscanning);
  g_test_add_func ("/backends/monitor-store/scale",
                   meta_test_monitor_store_scale);
  g_test_add_func ("/backends/monitor-store/fractional-scale",
                   meta_test_monitor_store_fractional_scale);
  g_test_add_func ("/backends/monitor-store/high-precision-fractional-scale",
                   meta_test_monitor_store_high_precision_fractional_scale);
  g_test_add_func ("/backends/monitor-store/mirrored",
                   meta_test_monitor_store_mirrored);
  g_test_add_func ("/backends/monitor-store/first-rotated",
                   meta_test_monitor_store_first_rotated);
  g_test_add_func ("/backends/monitor-store/second-rotated",
                   meta_test_monitor_store_second_rotated);
  g_test_add_func ("/backends/monitor-store/interlaced",
                   meta_test_monitor_store_interlaced);
}
