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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_WINSYS_EGL_PRIVATE_H
#define __COGL_WINSYS_EGL_PRIVATE_H

#include "cogl-defines.h"
#include "cogl-winsys-private.h"
#include "cogl-context.h"
#include "cogl-context-private.h"
#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
#include "cogl-winsys-kms.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
#include "cogl-xlib-renderer-private.h"
#include "cogl-xlib-display-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
#include <wayland-client.h>
#include <wayland-egl.h>
#endif

typedef enum _CoglEGLWinsysFeature
{
  COGL_EGL_WINSYS_FEATURE_SWAP_REGION                   =1L<<0,
  COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP     =1L<<1,
  COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_WAYLAND_BUFFER =1L<<2,
  COGL_EGL_WINSYS_FEATURE_SURFACELESS_OPENGL            =1L<<3,
  COGL_EGL_WINSYS_FEATURE_SURFACELESS_GLES1             =1L<<4,
  COGL_EGL_WINSYS_FEATURE_SURFACELESS_GLES2             =1L<<5
} CoglEGLWinsysFeature;

typedef struct _CoglRendererEGL
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglXlibRenderer _parent;
#endif

  CoglEGLWinsysFeature private_features;

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  struct wl_display *wayland_display;
  struct wl_compositor *wayland_compositor;
  struct wl_shell *wayland_shell;
#endif

  EGLDisplay edpy;

  EGLint egl_version_major;
  EGLint egl_version_minor;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  gboolean gdl_initialized;
#endif
#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
  CoglRendererKMS kms_renderer;
#endif

  /* Function pointers for GLX specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d)

#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * pf_ ## name) args;

#define COGL_WINSYS_FEATURE_END()

#include "cogl-winsys-egl-feature-functions.h"

#undef COGL_WINSYS_FEATURE_BEGIN
#undef COGL_WINSYS_FEATURE_FUNCTION
#undef COGL_WINSYS_FEATURE_END
} CoglRendererEGL;

typedef struct _CoglDisplayEGL
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglXlibDisplay _parent;
#endif

  EGLContext egl_context;
#if defined (COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT)
  EGLSurface dummy_surface;
#elif defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
  struct wl_surface *wayland_surface;
  struct wl_egl_window *wayland_egl_native_window;
  EGLSurface dummy_surface;
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT) || \
      defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT) || \
      defined (COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT) || \
      defined (COGL_HAS_EGL_PLATFORM_KMS_SUPPORT)
#ifndef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
  EGLSurface egl_surface;
#else
  CoglDisplayKMS kms_display;
#endif
  int egl_surface_width;
  int egl_surface_height;
  gboolean have_onscreen;
#else
#error "Unknown EGL platform"
#endif

  EGLConfig egl_config;
  gboolean found_egl_config;
  gboolean stencil_disabled;
} CoglDisplayEGL;

typedef struct _CoglContextEGL
{
  EGLSurface current_surface;
} CoglContextEGL;

const CoglWinsysVtable *
_cogl_winsys_egl_get_vtable (void);

#ifdef EGL_KHR_image_base
EGLImageKHR
_cogl_egl_create_image (CoglContext *ctx,
                        EGLenum target,
                        EGLClientBuffer buffer,
                        const EGLint *attribs);

void
_cogl_egl_destroy_image (CoglContext *ctx,
                         EGLImageKHR image);
#endif

#endif /* __COGL_WINSYS_EGL_PRIVATE_H */
