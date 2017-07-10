/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_GPU_H
#define META_GPU_H

#include <glib-object.h>

#include "backends/meta-monitor-manager-private.h"

#define META_TYPE_GPU (meta_gpu_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaGpu, meta_gpu, META, GPU, GObject)

struct _MetaGpuClass
{
  GObjectClass parent_class;

  gboolean (* read_current) (MetaGpu  *gpu,
                             GError  **error);
};

int meta_gpu_get_kms_fd (MetaGpu *gpu);

const char * meta_gpu_get_kms_file_path (MetaGpu *gpu);

gboolean meta_gpu_read_current (MetaGpu  *gpu,
                                GError  **error);

gboolean meta_gpu_has_hotplug_mode_update (MetaGpu *gpu);

MetaMonitorManager * meta_gpu_get_monitor_manager (MetaGpu *gpu);

GList * meta_gpu_get_outputs (MetaGpu *gpu);

GList * meta_gpu_get_crtcs (MetaGpu *gpu);

GList * meta_gpu_get_modes (MetaGpu *gpu);

void meta_gpu_take_outputs (MetaGpu *gpu,
                            GList   *outputs);

void meta_gpu_take_crtcs (MetaGpu *gpu,
                          GList   *crtcs);

void meta_gpu_take_modes (MetaGpu *gpu,
                          GList   *modes);

#endif /* META_GPU_H */
