/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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

#ifndef __COGL_FEATURE_PRIVATE_H
#define __COGL_FEATURE_PRIVATE_H

#include <glib.h>


#define COGL_CHECK_GL_VERSION(driver_major, driver_minor, \
                              target_major, target_minor) \
  ((driver_major) > (target_major) || \
   ((driver_major) == (target_major) && (driver_minor) >= (target_minor)))

typedef enum
{
  COGL_EXT_IN_GLES = (1 << 0),
  COGL_EXT_IN_GLES2 = (1 << 1)
} CoglExtGlesAvailability;

typedef struct _CoglFeatureFunction CoglFeatureFunction;

struct _CoglFeatureFunction
{
  /* The name of the function without the "EXT" or "ARB" suffix */
  const char *name;
  /* The offset in the context of where to store the function pointer */
  unsigned int pointer_offset;
};

typedef struct _CoglFeatureData CoglFeatureData;

struct _CoglFeatureData
{
  /* A minimum GL version which the functions should be defined in
     without needing an extension. Set to 255,255 if it's only
     provided in an extension */
  int min_gl_major, min_gl_minor;
  /* Flags specifying which versions of GLES the feature is available
     in core in */
  CoglExtGlesAvailability gles_availability;
  /* \0 separated list of namespaces to try. Eg "EXT\0ARB\0" */
  const char *namespaces;
  /* \0 separated list of required extension names without the GL_EXT
     or GL_ARB prefix. Any of the extensions must be available for the
     feature to be considered available. If the suffix for an
     extension is different from the namespace, you can specify it
     with a ':' after the namespace */
  const char *extension_names;
  /* A set of feature flags to enable if the extension is available */
  CoglFeatureFlags feature_flags;
  /* A set of private feature flags to enable if the extension is
   * available */
  int feature_flags_private;
  /* An optional corresponding winsys feature. */
  CoglWinsysFeature winsys_feature;
  /* A list of functions required for this feature. Terminated with a
     NULL name */
  const CoglFeatureFunction *functions;
};

CoglBool
_cogl_feature_check (CoglRenderer *renderer,
                     const char *driver_prefix,
                     const CoglFeatureData *data,
                     int gl_major,
                     int gl_minor,
                     CoglDriver driver,
                     char * const *extensions,
                     void *function_table);

void
_cogl_feature_check_ext_functions (CoglContext *context,
                                   int gl_major,
                                   int gl_minor,
                                   char * const *gl_extensions);

#endif /* __COGL_FEATURE_PRIVATE_H */
