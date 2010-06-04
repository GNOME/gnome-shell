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

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

typedef enum
{
  COGL_WINSYS_FEATURE_STUB /* no features are defined yet */
} CoglWinsysFeatureFlags;

#ifdef COGL_HAS_XLIB_SUPPORT

typedef struct _CoglXlibTrapState CoglXlibTrapState;

struct _CoglXlibTrapState
{
  /* These values are intended to be internal to
     _cogl_xlib_{un,}trap_errors but they need to be in the header so
     that the struct can be allocated on the stack */
  int (* old_error_handler) (Display *, XErrorEvent *);
  int trapped_error_code;
  CoglXlibTrapState *old_state;
};

#endif

typedef struct
{
  /* These are specific to winsys backends supporting Xlib. This
     should probably eventually be moved into a separate file specific
     to Xlib when Cogl gains a more complete winsys abstraction */
#ifdef COGL_HAS_XLIB_SUPPORT
  GSList *event_filters;
  /* Current top of the XError trap state stack. The actual memory for
     these is expected to be allocated on the stack by the caller */
  CoglXlibTrapState *trap_state;
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
