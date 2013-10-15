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
#include "cogl-framebuffer-private.h"

typedef struct _CoglWinsysEGLVtable
{
  CoglBool
  (* display_setup) (CoglDisplay *display,
                     CoglError **error);
  void
  (* display_destroy) (CoglDisplay *display);

  CoglBool
  (* context_created) (CoglDisplay *display,
                       CoglError **error);

  void
  (* cleanup_context) (CoglDisplay *display);

  CoglBool
  (* context_init) (CoglContext *context, CoglError **error);

  void
  (* context_deinit) (CoglContext *context);

  CoglBool
  (* onscreen_init) (CoglOnscreen *onscreen,
                     EGLConfig config,
                     CoglError **error);
  void
  (* onscreen_deinit) (CoglOnscreen *onscreen);

  int
  (* add_config_attributes) (CoglDisplay *display,
                             CoglFramebufferConfig *config,
                             EGLint *attributes);
} CoglWinsysEGLVtable;

typedef enum _CoglEGLWinsysFeature
{
  COGL_EGL_WINSYS_FEATURE_SWAP_REGION                   =1L<<0,
  COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP     =1L<<1,
  COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_WAYLAND_BUFFER =1L<<2,
  COGL_EGL_WINSYS_FEATURE_CREATE_CONTEXT                =1L<<3,
  COGL_EGL_WINSYS_FEATURE_BUFFER_AGE                    =1L<<4,
  COGL_EGL_WINSYS_FEATURE_FENCE_SYNC                    =1L<<5
} CoglEGLWinsysFeature;

typedef struct _CoglRendererEGL
{
  CoglEGLWinsysFeature private_features;

  EGLDisplay edpy;

  EGLint egl_version_major;
  EGLint egl_version_minor;

  CoglClosure *resize_notify_idle;

  /* Data specific to the EGL platform */
  void *platform;
  /* vtable for platform specific parts */
  const CoglWinsysEGLVtable *platform_vtable;

  /* Function pointers for EGL specific extensions */
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
  EGLContext egl_context;
  EGLSurface dummy_surface;
  EGLSurface egl_surface;

  EGLConfig egl_config;
  CoglBool found_egl_config;

  EGLSurface current_read_surface;
  EGLSurface current_draw_surface;
  EGLContext current_context;

  /* Platform specific display data */
  void *platform;
} CoglDisplayEGL;

typedef struct _CoglContextEGL
{
  EGLSurface saved_draw_surface;
  EGLSurface saved_read_surface;
} CoglContextEGL;

typedef struct _CoglOnscreenEGL
{
  EGLSurface egl_surface;

  CoglBool pending_resize_notify;

  /* Platform specific data */
  void *platform;
} CoglOnscreenEGL;

const CoglWinsysVtable *
_cogl_winsys_egl_get_vtable (void);

EGLBoolean
_cogl_winsys_egl_make_current (CoglDisplay *display,
                               EGLSurface draw,
                               EGLSurface read,
                               EGLContext context);

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

#ifdef EGL_WL_bind_wayland_display
CoglBool
_cogl_egl_query_wayland_buffer (CoglContext *ctx,
                                struct wl_resource *buffer,
                                int attribute,
                                int *value);
#endif

CoglBool
_cogl_winsys_egl_renderer_connect_common (CoglRenderer *renderer,
                                          CoglError **error);

#endif /* __COGL_WINSYS_EGL_PRIVATE_H */
