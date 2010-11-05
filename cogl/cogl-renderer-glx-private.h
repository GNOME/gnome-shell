/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_RENDERER_GLX_PRIVATE_H
#define __COGL_RENDERER_GLX_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-renderer-xlib-private.h"

typedef struct _CoglRendererGLX
{
  CoglRendererXlib _parent;

  int glx_major;
  int glx_minor;

  int glx_error_base;
  int glx_event_base;

  gboolean is_direct;

  /* Vblank stuff */
  int dri_fd;

  /* Function pointers for GLX specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e, f)

#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * pf_ ## name) args;

#define COGL_WINSYS_FEATURE_END()

#include "cogl-winsys-glx-feature-functions.h"

#undef COGL_WINSYS_FEATURE_BEGIN
#undef COGL_WINSYS_FEATURE_FUNCTION
#undef COGL_WINSYS_FEATURE_END
} CoglRendererGLX;

#endif /* __COGL_RENDERER_GLX_PRIVATE_H */
