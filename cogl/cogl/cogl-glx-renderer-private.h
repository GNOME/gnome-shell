/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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

  /* enumeration with relatioship between OML_sync_control
   * UST (unadjusted-system-time) and the system clock */
  enum {
    COGL_GLX_UST_IS_UNKNOWN,
    COGL_GLX_UST_IS_GETTIMEOFDAY,
    COGL_GLX_UST_IS_MONOTONIC_TIME,
    COGL_GLX_UST_IS_OTHER
  } ust_type;

  /* GModule pointing to libGL which we use to get glX functions out of */
  GModule *libgl_module;

  CoglClosure *flush_notifications_idle;

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
