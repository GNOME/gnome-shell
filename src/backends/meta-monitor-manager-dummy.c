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

#define ALL_WL_TRANSFORMS ((1 << (WL_OUTPUT_TRANSFORM_FLIPPED_270 + 1)) - 1)

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

  manager->crtcs = g_new0 (MetaCRTC, 1);
  manager->n_crtcs = 1;

  manager->crtcs[0].crtc_id = 1;
  manager->crtcs[0].rect.x = 0;
  manager->crtcs[0].rect.y = 0;
  manager->crtcs[0].rect.width = manager->modes[0].width;
  manager->crtcs[0].rect.height = manager->modes[0].height;
  manager->crtcs[0].current_mode = &manager->modes[0];
  manager->crtcs[0].transform = WL_OUTPUT_TRANSFORM_NORMAL;
  manager->crtcs[0].all_transforms = ALL_WL_TRANSFORMS;
  manager->crtcs[0].is_dirty = FALSE;
  manager->crtcs[0].logical_monitor = NULL;

  manager->outputs = g_new0 (MetaOutput, 1);
  manager->n_outputs = 1;

  manager->outputs[0].crtc = &manager->crtcs[0];
  manager->outputs[0].output_id = 1;
  manager->outputs[0].name = g_strdup ("LVDS");
  manager->outputs[0].vendor = g_strdup ("MetaProducts Inc.");
  manager->outputs[0].product = g_strdup ("unknown");
  manager->outputs[0].serial = g_strdup ("0xC0FFEE");
  manager->outputs[0].width_mm = 222;
  manager->outputs[0].height_mm = 125;
  manager->outputs[0].subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
  manager->outputs[0].preferred_mode = &manager->modes[0];
  manager->outputs[0].n_modes = 1;
  manager->outputs[0].modes = g_new0 (MetaMonitorMode *, 1);
  manager->outputs[0].modes[0] = &manager->modes[0];
  manager->outputs[0].n_possible_crtcs = 1;
  manager->outputs[0].possible_crtcs = g_new0 (MetaCRTC *, 1);
  manager->outputs[0].possible_crtcs[0] = &manager->crtcs[0];
  manager->outputs[0].n_possible_clones = 0;
  manager->outputs[0].possible_clones = g_new0 (MetaOutput *, 0);
  manager->outputs[0].backlight = -1;
  manager->outputs[0].backlight_min = 0;
  manager->outputs[0].backlight_max = 0;
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
