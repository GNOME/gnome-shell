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

#ifndef META_GPU_XRANDR_H
#define META_GPU_XRANDR_H

#include <glib-object.h>
#include <X11/extensions/Xrandr.h>

#include "backends/meta-gpu.h"
#include "backends/x11/meta-monitor-manager-xrandr.h"

#define META_TYPE_GPU_XRANDR (meta_gpu_xrandr_get_type ())
G_DECLARE_FINAL_TYPE (MetaGpuXrandr, meta_gpu_xrandr, META, GPU_XRANDR, MetaGpu)

XRRScreenResources * meta_gpu_xrandr_get_resources (MetaGpuXrandr *gpu_xrandr);

void meta_gpu_xrandr_get_max_screen_size (MetaGpuXrandr *gpu_xrandr,
                                          int           *max_width,
                                          int           *max_height);

MetaGpuXrandr * meta_gpu_xrandr_new (MetaMonitorManagerXrandr *monitor_manager_xrandr);

#endif /* META_GPU_XRANDR_H */
