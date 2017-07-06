/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_MONITOR_MANAGER_KMS_H
#define META_MONITOR_MANAGER_KMS_H

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "meta-monitor-manager-private.h"

#define META_TYPE_MONITOR_MANAGER_KMS (meta_monitor_manager_kms_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorManagerKms, meta_monitor_manager_kms,
                      META, MONITOR_MANAGER_KMS,
                      MetaMonitorManager)

typedef struct _MetaKmsResources
{
  drmModeRes *resources;
  drmModeEncoder **encoders;
  unsigned int n_encoders;
} MetaKmsResources;

typedef void (*MetaKmsFlipCallback) (void *user_data);

int meta_monitor_manager_kms_get_fd (MetaMonitorManagerKms *manager_kms);

const char * meta_monitor_manager_kms_get_file_path (MetaMonitorManagerKms *manager_kms);

gboolean meta_drm_mode_equal (const drmModeModeInfo *one,
                              const drmModeModeInfo *two);

MetaCrtcMode * meta_monitor_manager_kms_get_mode_from_drm_mode (MetaMonitorManagerKms *manager_kms,
                                                                const drmModeModeInfo *drm_mode);

gboolean meta_monitor_manager_kms_apply_crtc_mode (MetaMonitorManagerKms *manager_kms,
                                                   MetaCrtc              *crtc,
                                                   int                    x,
                                                   int                    y,
                                                   uint32_t               fb_id);

gboolean meta_monitor_manager_kms_is_crtc_active (MetaMonitorManagerKms *manager_kms,
                                                  MetaCrtc              *crtc);

gboolean meta_monitor_manager_kms_flip_crtc (MetaMonitorManagerKms *manager_kms,
                                             MetaCrtc              *crtc,
                                             int                    x,
                                             int                    y,
                                             uint32_t               fb_id,
                                             GClosure              *flip_closure,
                                             gboolean              *fb_in_use);

void meta_monitor_manager_kms_wait_for_flip (MetaMonitorManagerKms *manager_kms);

void meta_monitor_manager_kms_pause (MetaMonitorManagerKms *manager_kms);

void meta_monitor_manager_kms_resume (MetaMonitorManagerKms *manager_kms);

float meta_calculate_drm_mode_refresh_rate (const drmModeModeInfo *mode);

#endif /* META_MONITOR_MANAGER_KMS_H */
