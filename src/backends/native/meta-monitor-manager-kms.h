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

typedef struct _MetaGpuKms MetaGpuKms;

#define META_TYPE_MONITOR_MANAGER_KMS (meta_monitor_manager_kms_get_type ())
G_DECLARE_FINAL_TYPE (MetaMonitorManagerKms, meta_monitor_manager_kms,
                      META, MONITOR_MANAGER_KMS,
                      MetaMonitorManager)

MetaGpuKms * meta_monitor_manager_kms_get_primary_gpu (MetaMonitorManagerKms *manager_kms);

void meta_monitor_manager_kms_pause (MetaMonitorManagerKms *manager_kms);

void meta_monitor_manager_kms_resume (MetaMonitorManagerKms *manager_kms);

#endif /* META_MONITOR_MANAGER_KMS_H */
