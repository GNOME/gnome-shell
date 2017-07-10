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

#ifndef META_CRTC_KMS_H
#define META_CRTC_KMS_H

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "backends/meta-crtc.h"
#include "backends/meta-gpu.h"
#include "backends/native/meta-gpu-kms.h"

gboolean meta_crtc_kms_is_transform_handled (MetaCrtc             *crtc,
                                             MetaMonitorTransform  transform);

void meta_crtc_kms_apply_transform (MetaCrtc *crtc);

void meta_crtc_kms_set_underscan (MetaCrtc *crtc,
                                  gboolean  is_underscanning);

MetaCrtc * meta_create_kms_crtc (MetaGpuKms   *gpu_kms,
                                 drmModeCrtc  *drm_crtc,
                                 unsigned int  crtc_index);

#endif /* META_CRTC_KMS_H */
