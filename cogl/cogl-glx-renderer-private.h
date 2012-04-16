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

  /* Function pointers for core GLX functionality. We can't just link
     against these directly because we need to conditionally load
     libGL when we are using GLX so that it won't conflict with a GLES
     library if we are using EGL + GLES */
  void
  (* glXDestroyContext) (Display *dpy, GLXContext ctx);
  void
  (* glXSwapBuffers) (Display *dpy, GLXDrawable drawable);
  Bool
  (* glXQueryExtension) (Display *dpy, int *errorb, int *event);
  const char *
  (* glXQueryExtensionsString) (Display *dpy, int screen);
  Bool
  (* glXQueryVersion) (Display *dpy, int *maj, int *min);
  Bool
  (* glXIsDirect) (Display *dpy, GLXContext ctx);
  int
  (* glXGetFBConfigAttrib) (Display *dpy, GLXFBConfig config,
                            int attribute, int *value);
  GLXWindow
  (* glXCreateWindow) (Display *dpy, GLXFBConfig config,
                       Window win, const int *attribList);
  void
  (* glXDestroyWindow) (Display *dpy, GLXWindow window);
  GLXPixmap
  (* glXCreatePixmap) (Display *dpy, GLXFBConfig config,
                       Pixmap pixmap, const int *attribList);
  void
  (* glXDestroyPixmap) (Display *dpy, GLXPixmap pixmap);
  GLXContext
  (* glXCreateNewContext) (Display *dpy, GLXFBConfig config,
                           int renderType, GLXContext shareList,
                           Bool direct);
  Bool
  (* glXMakeContextCurrent) (Display *dpy, GLXDrawable draw,
                             GLXDrawable read, GLXContext ctx);
  void
  (* glXSelectEvent) (Display *dpy, GLXDrawable drawable,
                      unsigned long mask);
  GLXFBConfig *
  (* glXGetFBConfigs) (Display *dpy, int screen, int *nelements);
  GLXFBConfig *
  (* glXChooseFBConfig) (Display *dpy, int screen,
                         const int *attrib_list, int *nelements);
  XVisualInfo *
  (* glXGetVisualFromFBConfig) (Display *dpy, GLXFBConfig config);

  void *
  (* glXGetProcAddress) (const GLubyte *procName);

  /* Function pointers for GLX specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e, f)

#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * pf_ ## name) args;

#define COGL_WINSYS_FEATURE_END()

#include "cogl-winsys-glx-feature-functions.h"

#undef COGL_WINSYS_FEATURE_BEGIN
#undef COGL_WINSYS_FEATURE_FUNCTION
#undef COGL_WINSYS_FEATURE_END
} CoglGLXRenderer;

#endif /* __COGL_RENDERER_GLX_PRIVATE_H */
