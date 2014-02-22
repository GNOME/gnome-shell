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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <errno.h>

#include <test-fixtures/test-unit.h>

#include "cogl-gpu-info-private.h"
#include "cogl-context-private.h"
#include "cogl-version.h"

typedef struct
{
  const char *renderer_string;
  const char *version_string;
  const char *vendor_string;
} CoglGpuInfoStrings;

typedef struct CoglGpuInfoArchitectureDescription
{
  CoglGpuInfoArchitecture architecture;
  const char *name;
  CoglGpuInfoArchitectureFlag flags;
  CoglBool (* check_function) (const CoglGpuInfoStrings *strings);

} CoglGpuInfoArchitectureDescription;

typedef struct
{
  CoglGpuInfoVendor vendor;
  const char *name;
  CoglBool (* check_function) (const CoglGpuInfoStrings *strings);
  const CoglGpuInfoArchitectureDescription *architectures;

} CoglGpuInfoVendorDescription;

typedef struct
{
  CoglGpuInfoDriverPackage driver_package;
  const char *name;
  CoglBool (* check_function) (const CoglGpuInfoStrings *strings,
                               int *version_out);
} CoglGpuInfoDriverPackageDescription;

static CoglBool
_cogl_gpu_info_parse_version_string (const char *version_string,
                                     int n_components,
                                     const char **tail,
                                     int *version_ret)
{
  int version = 0;
  uint64_t part;
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

static CoglBool
match_phrase (const char *string, const char *phrase)
{
  const char *part = strstr (string, phrase);
  int len;

  if (part == NULL)
    return FALSE;

  /* The match must either be at the beginning of the string or
     preceded by a space. */
  if (part > string && part[-1] != ' ')
    return FALSE;

  /* Also match must either be at end of string or followed by a
   * space. */
  len = strlen (phrase);
  if (part[len] != '\0' && part[len] != ' ')
    return FALSE;

  return TRUE;
}

static CoglBool
check_intel_vendor (const CoglGpuInfoStrings *strings)
{
  return match_phrase (strings->renderer_string, "Intel(R)");
}

static CoglBool
check_imagination_technologies_vendor (const CoglGpuInfoStrings *strings)
{
  if (strcmp (strings->vendor_string, "Imagination Technologies") != 0)
    return FALSE;
  return TRUE;
}

static CoglBool
check_arm_vendor (const CoglGpuInfoStrings *strings)
{
  if (strcmp (strings->vendor_string, "ARM") != 0)
    return FALSE;
  return TRUE;
}

static CoglBool
check_qualcomm_vendor (const CoglGpuInfoStrings *strings)
{
  if (strcmp (strings->vendor_string, "Qualcomm") != 0)
    return FALSE;
  return TRUE;
}

static CoglBool
check_nvidia_vendor (const CoglGpuInfoStrings *strings)
{
  if (strcmp (strings->vendor_string, "NVIDIA") != 0)
    return FALSE;

  return TRUE;
}

static CoglBool
check_ati_vendor (const CoglGpuInfoStrings *strings)
{
  if (strcmp (strings->vendor_string, "ATI") != 0)
    return FALSE;

  return TRUE;
}

static CoglBool
check_mesa_vendor (const CoglGpuInfoStrings *strings)
{
  if (strcmp (strings->vendor_string, "Tungsten Graphics, Inc") == 0)
    return TRUE;
  else if (strcmp (strings->vendor_string, "VMware, Inc.") == 0)
    return TRUE;
  else if (strcmp (strings->vendor_string, "Mesa Project") == 0)
    return TRUE;

  return FALSE;
}

static CoglBool
check_true (const CoglGpuInfoStrings *strings)
{
  /* This is a last resort so it always matches */
  return TRUE;
}

static CoglBool
check_sandybridge_architecture (const CoglGpuInfoStrings *strings)
{
  return match_phrase (strings->renderer_string, "Sandybridge");
}

static CoglBool
check_llvmpipe_architecture (const CoglGpuInfoStrings *strings)
{
  return match_phrase (strings->renderer_string, "llvmpipe");
}

static CoglBool
check_softpipe_architecture (const CoglGpuInfoStrings *strings)
{
  return match_phrase (strings->renderer_string, "softpipe");
}

static CoglBool
check_swrast_architecture (const CoglGpuInfoStrings *strings)
{
  return match_phrase (strings->renderer_string, "software rasterizer") ||
    match_phrase (strings->renderer_string, "Software Rasterizer");
}

static CoglBool
check_sgx_architecture (const CoglGpuInfoStrings *strings)
{
  if (strncmp (strings->renderer_string, "PowerVR SGX", 12) != 0)
    return FALSE;

  return TRUE;
}

static CoglBool
check_mali_architecture (const CoglGpuInfoStrings *strings)
{
  if (strncmp (strings->renderer_string, "Mali-", 5) != 0)
    return FALSE;

  return TRUE;
}

static const CoglGpuInfoArchitectureDescription
intel_architectures[] =
  {
    {
      COGL_GPU_INFO_ARCHITECTURE_SANDYBRIDGE,
      "Sandybridge",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_sandybridge_architecture
    },
    {
      COGL_GPU_INFO_ARCHITECTURE_UNKNOWN,
      "Unknown",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_true
    }
  };

static const CoglGpuInfoArchitectureDescription
powervr_architectures[] =
  {
    {
      COGL_GPU_INFO_ARCHITECTURE_SGX,
      "SGX",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_DEFERRED,
      check_sgx_architecture
    },
    {
      COGL_GPU_INFO_ARCHITECTURE_UNKNOWN,
      "Unknown",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED,
      check_true
    }
  };

static const CoglGpuInfoArchitectureDescription
arm_architectures[] =
  {
    {
      COGL_GPU_INFO_ARCHITECTURE_MALI,
      "Mali",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_mali_architecture
    },
    {
      COGL_GPU_INFO_ARCHITECTURE_UNKNOWN,
      "Unknown",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_true
    }
  };

static const CoglGpuInfoArchitectureDescription
mesa_architectures[] =
  {
    {
      COGL_GPU_INFO_ARCHITECTURE_LLVMPIPE,
      "LLVM Pipe",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE,
      check_llvmpipe_architecture
    },
    {
      COGL_GPU_INFO_ARCHITECTURE_SOFTPIPE,
      "Softpipe",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE,
      check_softpipe_architecture
    },
    {
      COGL_GPU_INFO_ARCHITECTURE_SWRAST,
      "SWRast",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE,
      check_swrast_architecture
    },
    {
      COGL_GPU_INFO_ARCHITECTURE_UNKNOWN,
      "Unknown",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_true
    }
  };

static const CoglGpuInfoArchitectureDescription
unknown_architectures[] =
  {
    {
      COGL_GPU_INFO_ARCHITECTURE_UNKNOWN,
      "Unknown",
      COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
        COGL_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE,
      check_true
    }
  };

static const CoglGpuInfoVendorDescription
_cogl_gpu_info_vendors[] =
  {
    {
      COGL_GPU_INFO_VENDOR_INTEL,
      "Intel",
      check_intel_vendor,
      intel_architectures
    },
    {
      COGL_GPU_INFO_VENDOR_IMAGINATION_TECHNOLOGIES,
      "Imagination Technologies",
      check_imagination_technologies_vendor,
      powervr_architectures
    },
    {
      COGL_GPU_INFO_VENDOR_ARM,
      "ARM",
      check_arm_vendor,
      arm_architectures
    },
    {
      COGL_GPU_INFO_VENDOR_QUALCOMM,
      "Qualcomm",
      check_qualcomm_vendor,
      unknown_architectures
    },
    {
      COGL_GPU_INFO_VENDOR_NVIDIA,
      "Nvidia",
      check_nvidia_vendor,
      unknown_architectures
    },
    {
      COGL_GPU_INFO_VENDOR_ATI,
      "ATI",
      check_ati_vendor,
      unknown_architectures
    },
    /* Must be last */
    {
      COGL_GPU_INFO_VENDOR_MESA,
      "Mesa",
      check_mesa_vendor,
      mesa_architectures
    },
    {
      COGL_GPU_INFO_VENDOR_UNKNOWN,
      "Unknown",
      check_true,
      unknown_architectures
    }
  };

static CoglBool
check_mesa_driver_package (const CoglGpuInfoStrings *strings,
                           int *version_ret)
{
  uint64_t micro_part;
  const char *v;

  /* The version string should always begin a two-part GL version
     number */
  if (!_cogl_gpu_info_parse_version_string (strings->version_string,
                                            2, /* n_components */
                                            &v, /* tail */
                                            NULL /* version_ret */))
    return FALSE;

  /* In mesa this will be followed optionally by "(Core Profile)" and
   * then "Mesa" */
  v = strstr (v, " Mesa ");
  if (!v)
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

UNIT_TEST (check_mesa_driver_package_parser,
           0, /* no requirements */
           0 /* no failure cases */)
{
  /* renderer_string, version_string, vendor_string;*/
  const CoglGpuInfoStrings test_strings[2] = {
    { NULL, "3.1 Mesa 9.2-devel15436ad", NULL },
    { NULL, "3.1 (Core Profile) Mesa 9.2.0-devel (git-15436ad)", NULL }
  };
  int i;
  int version;

  for (i = 0; i < G_N_ELEMENTS (test_strings); i++)
    {
      g_assert (check_mesa_driver_package (&test_strings[i], &version));
      g_assert_cmpint (version, ==, COGL_VERSION_ENCODE (9, 2, 0));
    }
}

static CoglBool
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
  strings.version_string = _cogl_context_get_gl_version (ctx);
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
          int j;

          gpu->vendor = description->vendor;
          gpu->vendor_name = description->name;

          for (j = 0; ; j++)
            {
              const CoglGpuInfoArchitectureDescription *architecture =
                description->architectures + j;

              if (architecture->check_function (&strings))
                {
                  gpu->architecture = architecture->architecture;
                  gpu->architecture_name = architecture->name;
                  gpu->architecture_flags = architecture->flags;
                  goto probed;
                }
            }
        }
    }

probed:

  COGL_NOTE (WINSYS, "Driver package = %s, vendor = %s, architecture = %s\n",
             gpu->driver_package_name,
             gpu->vendor_name,
             gpu->architecture_name);

  /* Determine the driver bugs */

  /* In Mesa the glReadPixels implementation is really slow
     when using the Intel driver. The Intel
     driver has a fast blit path when reading into a PBO. Reading into
     a temporary PBO and then memcpying back out to the application's
     memory is faster than a regular glReadPixels in this case */
  if (gpu->vendor == COGL_GPU_INFO_VENDOR_INTEL &&
      gpu->driver_package == COGL_GPU_INFO_DRIVER_PACKAGE_MESA)
    gpu->driver_bugs |= COGL_GPU_INFO_DRIVER_BUG_MESA_46631_SLOW_READ_PIXELS;
}
