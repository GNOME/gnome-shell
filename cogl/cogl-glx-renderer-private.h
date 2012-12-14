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

#include <gmodule.h>
#include "cogl-object-private.h"
#include "cogl-xlib-renderer-private.h"

typedef struct _CoglGLXRenderer
{
  int glx_major;
  int glx_minor;

  int glx_error_base;
  int glx_event_base;

  CoglBool is_direct;

  /* Vblank stuff */
  int dri_fd;

  /* GModule pointing to libGL which we use to get glX functions out of */
  GModule *libgl_module;

  /* Copy of the winsys features that are based purely on the
   * information we can get without using a GL context. We want to
   * determine this before we have a context so that we can use the
   * function pointers from the extensions earlier. This is necessary
   * to use the glXCreateContextAttribs function. */
  unsigned long base_winsys_features
    [COGL_FLAGS_N_LONGS_FOR_SIZE (COGL_WINSYS_FEATURE_N_FEATURES)];

  CoglFeatureFlags legacy_feature_flags;

  /* Function pointers for core GLX functionality. We can't just link
     against these directly because we need to conditionally load
     libGL when we are using GLX so that it won't conflict with a GLES
     library if we are using EGL + GLES. These are just the functions
     that we want to use before calling glXGetProcAddress */
  Bool
  (* glXQueryExtension) (Display *dpy, int *errorb, int *event);
  const char *
  (* glXQueryExtensionsString) (Display *dpy, int screen);
  Bool
  (* glXQueryVersion) (Display *dpy, int *maj, int *min);
  void *
  (* glXGetProcAddress) (const GLubyte *procName);

  int
  (* glXQueryDrawable) (Display *dpy, GLXDrawable drawable,
                        int attribute, unsigned int *value);

  /* Function pointers for GLX specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e, f, g)

#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * name) args;

#define COGL_WINSYS_FEATURE_END()

#include "cogl-winsys-glx-feature-functions.h"

#undef COGL_WINSYS_FEATURE_BEGIN
#undef COGL_WINSYS_FEATURE_FUNCTION
#undef COGL_WINSYS_FEATURE_END
} CoglGLXRenderer;

#endif /* __COGL_RENDERER_GLX_PRIVATE_H */
