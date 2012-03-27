/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_GPU_INFO_PRIVATE_H
#define __COGL_GPU_INFO_PRIVATE_H

#include "cogl-context.h"

typedef enum
{
  COGL_GPU_INFO_VENDOR_UNKNOWN,
  COGL_GPU_INFO_VENDOR_INTEL
} CoglGpuInfoVendor;

typedef enum
{
  COGL_GPU_INFO_DRIVER_PACKAGE_UNKNOWN,
  COGL_GPU_INFO_DRIVER_PACKAGE_MESA
} CoglGpuInfoDriverPackage;

typedef enum
{
  /* If this bug is present then it is faster to read pixels into a
   * PBO and then memcpy out of the PBO into system memory rather than
   * directly read into system memory.
   * https://bugs.freedesktop.org/show_bug.cgi?id=46631
   */
  COGL_GPU_INFO_DRIVER_BUG_MESA_46631_SLOW_READ_PIXELS = 1 << 0
} CoglGpuInfoDriverBug;

typedef struct _CoglGpuInfoVersion CoglGpuInfoVersion;

typedef struct _CoglGpuInfo CoglGpuInfo;

struct _CoglGpuInfo
{
  CoglGpuInfoVendor vendor;
  const char *vendor_name;

  CoglGpuInfoDriverPackage driver_package;
  const char *driver_package_name;
  int driver_package_version;

  CoglGpuInfoDriverBug driver_bugs;
};

/*
 * _cogl_gpu_info_init:
 * @ctx: A #CoglContext
 * @gpu: A return location for the GPU information
 *
 * Determines information about the GPU and driver from the given
 * context.
 */
void
_cogl_gpu_info_init (CoglContext *ctx,
                     CoglGpuInfo *gpu);

#endif /* __COGL_GPU_INFO_PRIVATE_H */
