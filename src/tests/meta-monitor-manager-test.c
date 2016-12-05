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

#include "tests/meta-monitor-manager-test.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

struct _MetaMonitorManagerTest
{
  MetaMonitorManager parent;
};

G_DEFINE_TYPE (MetaMonitorManagerTest, meta_monitor_manager_test,
               META_TYPE_MONITOR_MANAGER)

static void
meta_monitor_manager_test_read_current (MetaMonitorManager *manager)
{
  int n_monitors = 2;
  int i;

  manager->max_screen_width = 65535;
  manager->max_screen_height = 65535;
  manager->screen_width = 1024;
  manager->screen_height = 768;

  manager->modes = g_new0 (MetaMonitorMode, 1);
  manager->n_modes = 1;

  manager->modes[0].mode_id = 0;
  manager->modes[0].width = 1024;
  manager->modes[0].height = 768;
  manager->modes[0].refresh_rate = 60.0;

  manager->crtcs = g_new0 (MetaCRTC, n_monitors);
  manager->n_crtcs = n_monitors;
  manager->outputs = g_new0 (MetaOutput, n_monitors);
  manager->n_outputs = n_monitors;

  for (i = 0; i < n_monitors; i++)
    {
      manager->crtcs[i].crtc_id = i + 1;
      manager->crtcs[i].current_mode = &manager->modes[0];
      manager->crtcs[i].transform = META_MONITOR_TRANSFORM_NORMAL;
      manager->crtcs[i].all_transforms = ALL_TRANSFORMS;


      manager->outputs[i].crtc = &manager->crtcs[i];
      manager->outputs[i].winsys_id = i + 1;
      manager->outputs[i].name = g_strdup_printf ("LVDS%d", i + 1);
      manager->outputs[i].vendor = g_strdup ("MetaProducts Inc.");
      manager->outputs[i].product = g_strdup ("unknown");
      manager->outputs[i].serial = g_strdup ("0xC0FFEE");
      manager->outputs[i].suggested_x = -1;
      manager->outputs[i].suggested_y = -1;
      manager->outputs[i].width_mm = 222;
      manager->outputs[i].height_mm = 125;
      manager->outputs[i].subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
      manager->outputs[i].preferred_mode = &manager->modes[0];
      manager->outputs[i].n_modes = 1;
      manager->outputs[i].modes = g_new0 (MetaMonitorMode *, 1);
      manager->outputs[i].modes[0] = &manager->modes[0];
      manager->outputs[i].n_possible_crtcs = 1;
      manager->outputs[i].possible_crtcs = g_new0 (MetaCRTC *, 1);
      manager->outputs[i].possible_crtcs[0] = &manager->crtcs[i];
      manager->outputs[i].n_possible_clones = 0;
      manager->outputs[i].possible_clones = g_new0 (MetaOutput *, 0);
      manager->outputs[i].backlight = -1;
      manager->outputs[i].backlight_min = 0;
      manager->outputs[i].backlight_max = 0;
      manager->outputs[i].connector_type = META_CONNECTOR_TYPE_LVDS;
      manager->outputs[i].scale = 1;
    }
}

static void
meta_monitor_manager_test_apply_configuration (MetaMonitorManager *manager,
                                               MetaCRTCInfo      **crtcs,
                                               unsigned int        n_crtcs,
                                               MetaOutputInfo    **outputs,
                                               unsigned int        n_outputs)
{
  unsigned int i;
  int screen_width = 0, screen_height = 0;

  for (i = 0; i < n_crtcs; i++)
    {
      MetaCRTCInfo *crtc_info = crtcs[i];
      MetaCRTC *crtc = crtc_info->crtc;
      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        {
          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
      else
        {
          MetaMonitorMode *mode;
          MetaOutput *output;
          unsigned int j;
          int width, height;

          mode = crtc_info->mode;

          if (meta_monitor_transform_is_rotated (crtc_info->transform))
            {
              width = mode->height;
              height = mode->width;
            }
          else
            {
              width = mode->width;
              height = mode->height;
            }

          crtc->rect.x = crtc_info->x;
          crtc->rect.y = crtc_info->y;
          crtc->rect.width = width;
          crtc->rect.height = height;
          crtc->current_mode = mode;
          crtc->transform = crtc_info->transform;

          screen_width = MAX (screen_width, crtc_info->x + width);
          screen_height = MAX (screen_height, crtc_info->y + height);

          for (j = 0; j < crtc_info->outputs->len; j++)
            {
              output = ((MetaOutput**)crtc_info->outputs->pdata)[j];

              output->is_dirty = TRUE;
              output->crtc = crtc;
            }
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutputInfo *output_info = outputs[i];
      MetaOutput *output = output_info->output;

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
    }

  /* Disable CRTCs not mentioned in the list */
  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCRTC *crtc = &manager->crtcs[i];

      crtc->logical_monitor = NULL;

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }

      crtc->rect.x = 0;
      crtc->rect.y = 0;
      crtc->rect.width = 0;
      crtc->rect.height = 0;
      crtc->current_mode = NULL;
    }

  /* Disable outputs not mentioned in the list */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      output->crtc = NULL;
      output->is_primary = FALSE;
    }

  manager->screen_width = screen_width;
  manager->screen_height = screen_height;

  meta_monitor_manager_rebuild_derived (manager);
}

static void
meta_monitor_manager_test_init (MetaMonitorManagerTest *manager_test)
{
}

static void
meta_monitor_manager_test_class_init (MetaMonitorManagerTestClass *klass)
{
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_CLASS (klass);

  manager_class->read_current = meta_monitor_manager_test_read_current;
  manager_class->apply_configuration = meta_monitor_manager_test_apply_configuration;
}
