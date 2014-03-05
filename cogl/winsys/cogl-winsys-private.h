/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_WINSYS_PRIVATE_H
#define __COGL_WINSYS_PRIVATE_H

#include "cogl-renderer.h"
#include "cogl-onscreen.h"
#include "cogl-gles2.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include "cogl-texture-pixmap-x11-private.h"
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xutil.h>
#include "cogl-texture-pixmap-x11-private.h"
#endif

#ifdef COGL_HAS_EGL_SUPPORT
#include "cogl-egl-private.h"
#endif

#include "cogl-poll.h"

uint32_t
_cogl_winsys_error_quark (void);

#define COGL_WINSYS_ERROR (_cogl_winsys_error_quark ())

typedef enum { /*< prefix=COGL_WINSYS_ERROR >*/
  COGL_WINSYS_ERROR_INIT,
  COGL_WINSYS_ERROR_CREATE_CONTEXT,
  COGL_WINSYS_ERROR_CREATE_ONSCREEN,
  COGL_WINSYS_ERROR_MAKE_CURRENT,
  COGL_WINSYS_ERROR_CREATE_GLES2_CONTEXT,
} CoglWinsysError;

typedef enum
{
  COGL_WINSYS_RECTANGLE_STATE_UNKNOWN,
  COGL_WINSYS_RECTANGLE_STATE_DISABLE,
  COGL_WINSYS_RECTANGLE_STATE_ENABLE
} CoglWinsysRectangleState;

typedef struct _CoglWinsysVtable
{
  CoglWinsysID id;
  CoglRendererConstraint constraints;

  const char *name;

  /* Required functions */

  CoglFuncPtr
  (*renderer_get_proc_address) (CoglRenderer *renderer,
                                const char *name,
                                CoglBool in_core);

  CoglBool
  (*renderer_connect) (CoglRenderer *renderer, CoglError **error);

  void
  (*renderer_disconnect) (CoglRenderer *renderer);

  void
  (*renderer_outputs_changed) (CoglRenderer *renderer);

  CoglBool
  (*display_setup) (CoglDisplay *display, CoglError **error);

  void
  (*display_destroy) (CoglDisplay *display);

  CoglBool
  (*context_init) (CoglContext *context, CoglError **error);

  void
  (*context_deinit) (CoglContext *context);

  void *
  (*context_create_gles2_context) (CoglContext *ctx, CoglError **error);

  CoglBool
  (*onscreen_init) (CoglOnscreen *onscreen, CoglError **error);

  void
  (*onscreen_deinit) (CoglOnscreen *onscreen);

  void
  (*onscreen_bind) (CoglOnscreen *onscreen);

  void
  (*onscreen_swap_buffers_with_damage) (CoglOnscreen *onscreen,
                                        const int *rectangles,
                                        int n_rectangles);

  void
  (*onscreen_update_swap_throttled) (CoglOnscreen *onscreen);

  void
  (*onscreen_set_visibility) (CoglOnscreen *onscreen,
                              CoglBool visibility);

  /* Optional functions */

  int64_t
  (*context_get_clock_time) (CoglContext *context);

  void
  (*onscreen_swap_region) (CoglOnscreen *onscreen,
                           const int *rectangles,
                           int n_rectangles);

  void
  (*onscreen_set_resizable) (CoglOnscreen *onscreen, CoglBool resizable);

  int
  (*onscreen_get_buffer_age) (CoglOnscreen *onscreen);

#ifdef COGL_HAS_EGL_SUPPORT
  EGLDisplay
  (*context_egl_get_egl_display) (CoglContext *context);
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
  XVisualInfo *
  (*xlib_get_visual_info) (void);
#endif

  uint32_t
  (*onscreen_x11_get_window_xid) (CoglOnscreen *onscreen);

#ifdef COGL_HAS_WIN32_SUPPORT
  HWND
  (*onscreen_win32_get_window) (CoglOnscreen *onscreen);
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
  CoglBool
  (*texture_pixmap_x11_create) (CoglTexturePixmapX11 *tex_pixmap);
  void
  (*texture_pixmap_x11_free) (CoglTexturePixmapX11 *tex_pixmap);

  CoglBool
  (*texture_pixmap_x11_update) (CoglTexturePixmapX11 *tex_pixmap,
                                CoglBool needs_mipmap);

  void
  (*texture_pixmap_x11_damage_notify) (CoglTexturePixmapX11 *tex_pixmap);

  CoglTexture *
  (*texture_pixmap_x11_get_texture) (CoglTexturePixmapX11 *tex_pixmap);
#endif

  void
  (*save_context) (CoglContext *ctx);

  CoglBool
  (*set_gles2_context) (CoglGLES2Context *gles2_ctx, CoglError **error);

  void
  (*restore_context) (CoglContext *ctx);

  void
  (*destroy_gles2_context) (CoglGLES2Context *gles2_ctx);

  void *
  (*fence_add) (CoglContext *ctx);

  CoglBool
  (*fence_is_complete) (CoglContext *ctx, void *fence);

  void
  (*fence_destroy) (CoglContext *ctx, void *fence);

} CoglWinsysVtable;

CoglBool
_cogl_winsys_has_feature (CoglWinsysFeature feature);

#endif /* __COGL_WINSYS_PRIVATE_H */
