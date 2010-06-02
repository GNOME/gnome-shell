/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __COGL_CONTEXT_WINSYS_H
#define __COGL_CONTEXT_WINSYS_H

typedef enum
{
  COGL_WINSYS_FEATURE_STUB /* no features are defined yet */
} CoglWinsysFeatureFlags;

typedef struct
{
  /* These are specific to winsys backends supporting Xlib. This
     should probably eventually be moved into a separate file specific
     to Xlib when Cogl gains a more complete winsys abstraction */
#ifdef COGL_HAS_XLIB_SUPPORT
  GSList *event_filters;
#endif

  /* Function pointers for winsys specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e)

#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * pf_ ## name) args;

#define COGL_WINSYS_FEATURE_END()

#include "cogl-winsys-feature-functions.h"

#undef COGL_WINSYS_FEATURE_BEGIN
#undef COGL_WINSYS_FEATURE_FUNCTION
#undef COGL_WINSYS_FEATURE_END

  CoglWinsysFeatureFlags feature_flags;
} CoglContextWinsys;

#endif /* __COGL_CONTEXT_WINSYS_H */
