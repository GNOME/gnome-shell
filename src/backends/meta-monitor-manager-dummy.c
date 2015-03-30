/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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

#include "meta-monitor-manager-dummy.h"

#include <stdlib.h>

#include <meta/util.h>

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

struct _MetaMonitorManagerDummy
{
  MetaMonitorManager parent_instance;
};

struct _MetaMonitorManagerDummyClass
{
  MetaMonitorManagerClass parent_class;
};

G_DEFINE_TYPE (MetaMonitorManagerDummy, meta_monitor_manager_dummy, META_TYPE_MONITOR_MANAGER);

static void
meta_monitor_manager_dummy_read_current (MetaMonitorManager *manager)
{
  unsigned int num_monitors = 1;
  int *monitor_scales = NULL;
  const char *num_monitors_str;
  const char *monitor_scales_str;
  unsigned int i;
  int current_x = 0;

  /* To control what monitor configuration is generated, there are two available
   * environmental variables that can be used:
   *
   * MUTTER_DEBUG_NUM_DUMMY_MONITORS
   *
   * Specifies the number of dummy monitors to include in the stage. Every
   * monitor is 1024x786 pixels and they are placed on a horizontal row.
   *
   * MUTTER_DEBUG_DUMMY_MONITOR_SCALES
   *
   * A comma separated list that specifies the scales of the dummy monitors.
   *
   * For example the following configuration results in two monitors, where the
   * first one has the monitor scale 1, and the other the monitor scale 2.
   *
   * MUTTER_DEBUG_NUM_DUMMY_MONITORS=2
   * MUTTER_DEBUG_DUMMY_MONITOR_SCALES=1,2
   */
  num_monitors_str = getenv ("MUTTER_DEBUG_NUM_DUMMY_MONITORS");
  if (num_monitors_str)
    {
      num_monitors = g_ascii_strtoll (num_monitors_str, NULL, 10);
      if (num_monitors <= 0)
        {
          meta_warning ("Invalid number of dummy monitors");
          num_monitors = 1;
        }
    }

  monitor_scales = g_newa (int, num_monitors);
  for (i = 0; i < num_monitors; i++)
    monitor_scales[i] = 1;

  monitor_scales_str = getenv ("MUTTER_DEBUG_DUMMY_MONITOR_SCALES");
  if (monitor_scales_str)
    {
      gchar **scales_str_list;

      scales_str_list = g_strsplit (monitor_scales_str, ",", -1);
      if (g_strv_length (scales_str_list) != num_monitors)
        meta_warning ("Number of specified monitor scales differ from number "
                      "of monitors (defaults to 1).\n");
      for (i = 0; i < num_monitors && scales_str_list[i]; i++)
        {
          int scale = g_ascii_strtoll (scales_str_list[i], NULL, 10);
          if (scale == 1 || scale == 2)
            monitor_scales[i] = scale;
          else
            meta_warning ("Invalid dummy monitor scale");
        }
      g_strfreev (scales_str_list);
    }

  manager->max_screen_width = 65535;
  manager->max_screen_height = 65535;
  manager->screen_width = 1024 * num_monitors;
  manager->screen_height = 768;

  manager->modes = g_new0 (MetaMonitorMode, 1);
  manager->n_modes = 1;

  manager->modes[0].mode_id = 0;
  manager->modes[0].width = 1024;
  manager->modes[0].height = 768;
  manager->modes[0].refresh_rate = 60.0;

  manager->crtcs = g_new0 (MetaCRTC, num_monitors);
  manager->n_crtcs = num_monitors;
  manager->outputs = g_new0 (MetaOutput, num_monitors);
  manager->n_outputs = num_monitors;

  for (i = 0; i < num_monitors; i++)
    {
      manager->crtcs[i].crtc_id = i + 1;
      manager->crtcs[i].rect.x = current_x;
      manager->crtcs[i].rect.y = 0;
      manager->crtcs[i].rect.width = manager->modes[0].width;
      manager->crtcs[i].rect.height = manager->modes[0].height;
      manager->crtcs[i].current_mode = &manager->modes[0];
      manager->crtcs[i].transform = META_MONITOR_TRANSFORM_NORMAL;
      manager->crtcs[i].all_transforms = ALL_TRANSFORMS;
      manager->crtcs[i].is_dirty = FALSE;
      manager->crtcs[i].logical_monitor = NULL;

      current_x += manager->crtcs[i].rect.width;

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
      manager->outputs[i].scale = monitor_scales[i];
    }
}

static void
meta_monitor_manager_dummy_apply_config (MetaMonitorManager *manager,
                                         MetaCRTCInfo       **crtcs,
                                         unsigned int         n_crtcs,
                                         MetaOutputInfo     **outputs,
                                         unsigned int         n_outputs)
{
    unsigned i;
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
          int i, n_outputs;
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

          n_outputs = crtc_info->outputs->len;
          for (i = 0; i < n_outputs; i++)
            {
              output = ((MetaOutput**)crtc_info->outputs->pdata)[i];

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
meta_monitor_manager_dummy_class_init (MetaMonitorManagerDummyClass *klass)
{
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_CLASS (klass);

  manager_class->read_current = meta_monitor_manager_dummy_read_current;
  manager_class->apply_configuration = meta_monitor_manager_dummy_apply_config;
}

static void
meta_monitor_manager_dummy_init (MetaMonitorManagerDummy *manager)
{
}
