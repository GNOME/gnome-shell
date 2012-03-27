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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <errno.h>

#include "cogl-gpu-info-private.h"
#include "cogl-context-private.h"
#include "cogl-util.h"

typedef struct
{
  const char *renderer_string;
  const char *version_string;
  const char *vendor_string;
} CoglGpuInfoStrings;

typedef struct
{
  CoglGpuInfoVendor vendor;
  const char *name;
  gboolean (* check_function) (const CoglGpuInfoStrings *strings);
} CoglGpuInfoVendorDescription;

typedef struct
{
  CoglGpuInfoDriverPackage driver_package;
  const char *name;
  gboolean (* check_function) (const CoglGpuInfoStrings *strings,
                               int *version_out);
} CoglGpuInfoDriverPackageDescription;

static gboolean
_cogl_gpu_info_parse_version_string (const char *version_string,
                                     int n_components,
                                     const char **tail,
                                     int *version_ret)
{
  int version = 0;
  guint64 part;
  int i;

  for (i = 0; ; i++)
    {
      errno = 0;
      part = g_ascii_strtoull (version_string,
                               (char **) &version_string,
                               10);

      if (errno || part > COGL_VERSION_MAX_COMPONENT_VALUE)
        return FALSE;

      version |= part << ((2 - i) * COGL_VERSION_COMPONENT_BITS);

      if (i + 1 >= n_components)
        break;

      if (*version_string != '.')
        return FALSE;

      version_string++;
    }

  if (version_ret)
    *version_ret = version;
  if (tail)
    *tail = version_string;

  return TRUE;
}

static gboolean
check_intel_vendor (const CoglGpuInfoStrings *strings)
{
  const char *intel_part = strstr (strings->renderer_string, "Intel(R)");

  if (intel_part == NULL)
    return FALSE;

  /* The match must either be at the beginning of the string or
     preceded by a space. Just in case there's a company called
     IAmNotIntel (R) or something */
  if (intel_part > strings->renderer_string && intel_part[-1] != ' ')
    return FALSE;

  return TRUE;
}

static gboolean
check_unknown_vendor (const CoglGpuInfoStrings *strings)
{
  /* This is a last resort so it always matches */
  return TRUE;
}

static const CoglGpuInfoVendorDescription
_cogl_gpu_info_vendors[] =
  {
    {
      COGL_GPU_INFO_VENDOR_INTEL,
      "Intel",
      check_intel_vendor
    },
    /* Must be last */
    {
      COGL_GPU_INFO_VENDOR_UNKNOWN,
      "Unknown",
      check_unknown_vendor
    }
  };

static gboolean
check_mesa_driver_package (const CoglGpuInfoStrings *strings,
                           int *version_ret)
{
  guint64 micro_part;
  const char *v;

  /* The version string should always begin a two-part GL version
     number */
  if (!_cogl_gpu_info_parse_version_string (strings->version_string,
                                            2, /* n_components */
                                            &v, /* tail */
                                            NULL /* version_ret */))
    return FALSE;

  /* In mesa this will be followed by a space and the name "Mesa" */
  if (!g_str_has_prefix (v, " Mesa "))
    return FALSE;

  v += 6;

  /* Next there will be a version string that is at least two
     components. On a git devel build the version will be something
     like "-devel<git hash>" instead */
  if (!_cogl_gpu_info_parse_version_string (v,
                                            2, /* n_components */
                                            &v, /* tail */
                                            version_ret))
    return FALSE;

  /* If it is a development build then we'll just leave the micro
     number as 0 */
  if (g_str_has_prefix (v, "-devel"))
    return TRUE;

  /* Otherwise there should be a micro version number */
  if (*v != '.')
    return FALSE;

  errno = 0;
  micro_part = g_ascii_strtoull (v + 1, NULL /* endptr */, 10 /* base */);
  if (errno || micro_part > COGL_VERSION_MAX_COMPONENT_VALUE)
    return FALSE;

  *version_ret = COGL_VERSION_ENCODE (COGL_VERSION_GET_MAJOR (*version_ret),
                                      COGL_VERSION_GET_MINOR (*version_ret),
                                      micro_part);

  return TRUE;
}

static gboolean
check_unknown_driver_package (const CoglGpuInfoStrings *strings,
                              int *version_out)
{
  *version_out = 0;

  /* This is a last resort so it always matches */
  return TRUE;
}

static const CoglGpuInfoDriverPackageDescription
_cogl_gpu_info_driver_packages[] =
  {
    {
      COGL_GPU_INFO_DRIVER_PACKAGE_MESA,
      "Mesa",
      check_mesa_driver_package
    },
    /* Must be last */
    {
      COGL_GPU_INFO_DRIVER_PACKAGE_UNKNOWN,
      "Unknown",
      check_unknown_driver_package
    }
  };

void
_cogl_gpu_info_init (CoglContext *ctx,
                     CoglGpuInfo *gpu)
{
  CoglGpuInfoStrings strings;
  int i;

  strings.renderer_string = (const char *) ctx->glGetString (GL_RENDERER);
  strings.version_string = (const char *) ctx->glGetString (GL_VERSION);
  strings.vendor_string = (const char *) ctx->glGetString (GL_VENDOR);

  /* Determine the driver package */
  for (i = 0; ; i++)
    {
      const CoglGpuInfoDriverPackageDescription *description =
        _cogl_gpu_info_driver_packages + i;

      if (description->check_function (&strings, &gpu->driver_package_version))
        {
          gpu->driver_package = description->driver_package;
          gpu->driver_package_name = description->name;
          break;
        }
    }

  /* Determine the GPU vendor */
  for (i = 0; ; i++)
    {
      const CoglGpuInfoVendorDescription *description =
        _cogl_gpu_info_vendors + i;

      if (description->check_function (&strings))
        {
          gpu->vendor = description->vendor;
          gpu->vendor_name = description->name;
          break;
        }
    }

  /* Determine the driver bugs */

  /* In Mesa < 8.0.2 the glReadPixels implementation is really slow
     because it converts each pixel to a floating point representation
     and back even if the data could just be memcpy'd. The Intel
     driver has a fast blit path when reading into a PBO. Reading into
     a temporary PBO and then memcpying back out to the application's
     memory is faster than a regular glReadPixels in this case */
  if (gpu->vendor == COGL_GPU_INFO_VENDOR_INTEL &&
      gpu->driver_package == COGL_GPU_INFO_DRIVER_PACKAGE_MESA &&
      gpu->driver_package_version < COGL_VERSION_ENCODE (8, 0, 2))
    gpu->driver_bugs |= COGL_GPU_INFO_DRIVER_BUG_MESA_46631_SLOW_READ_PIXELS;
}
