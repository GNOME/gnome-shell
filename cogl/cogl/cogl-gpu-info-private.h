/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifndef __COGL_GPU_INFO_PRIVATE_H
#define __COGL_GPU_INFO_PRIVATE_H

#include "cogl-context.h"

typedef enum _CoglGpuInfoArchitectureFlag
{
  COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE,
  COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED,
  COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE,
  COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
  COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_DEFERRED,
  COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE
} CoglGpuInfoArchitectureFlag;

typedef enum _CoglGpuInfoArchitecture
{
  COGL_GPU_INFO_ARCHITECTURE_UNKNOWN,
  COGL_GPU_INFO_ARCHITECTURE_SANDYBRIDGE,
  COGL_GPU_INFO_ARCHITECTURE_SGX,
  COGL_GPU_INFO_ARCHITECTURE_MALI,
  COGL_GPU_INFO_ARCHITECTURE_LLVMPIPE,
  COGL_GPU_INFO_ARCHITECTURE_SOFTPIPE,
  COGL_GPU_INFO_ARCHITECTURE_SWRAST
} CoglGpuInfoArchitecture;

typedef enum
{
  COGL_GPU_INFO_VENDOR_UNKNOWN,
  COGL_GPU_INFO_VENDOR_INTEL,
  COGL_GPU_INFO_VENDOR_IMAGINATION_TECHNOLOGIES,
  COGL_GPU_INFO_VENDOR_ARM,
  COGL_GPU_INFO_VENDOR_QUALCOMM,
  COGL_GPU_INFO_VENDOR_NVIDIA,
  COGL_GPU_INFO_VENDOR_ATI,
  COGL_GPU_INFO_VENDOR_MESA
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

  CoglGpuInfoArchitecture architecture;
  const char *architecture_name;
  CoglGpuInfoArchitectureFlag architecture_flags;

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
