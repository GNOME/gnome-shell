/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_WINSYS_PRIVATE_H
#define __COGL_WINSYS_PRIVATE_H

#include "cogl-renderer.h"

#include "cogl-framebuffer-private.h"
#ifdef COGL_HAS_XLIB_SUPPORT
#include "cogl-texture-pixmap-x11-private.h"
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xutil.h>
#include "cogl-texture-pixmap-x11-private.h"
#endif

GQuark
_cogl_winsys_error_quark (void);

#define COGL_WINSYS_ERROR (_cogl_winsys_error_quark ())

typedef enum { /*< prefix=COGL_WINSYS_ERROR >*/
  COGL_WINSYS_ERROR_INIT,
  COGL_WINSYS_ERROR_CREATE_CONTEXT,
  COGL_WINSYS_ERROR_CREATE_ONSCREEN,
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

  const char *name;

  /* Required functions */

  CoglFuncPtr
  (*get_proc_address) (const char *name);

  gboolean
  (*renderer_connect) (CoglRenderer *renderer, GError **error);

  void
  (*renderer_disconnect) (CoglRenderer *renderer);

  gboolean
  (*display_setup) (CoglDisplay *display, GError **error);

  void
  (*display_destroy) (CoglDisplay *display);

  gboolean
  (*context_init) (CoglContext *context, GError **error);

  void
  (*context_deinit) (CoglContext *context);

  gboolean
  (*onscreen_init) (CoglOnscreen *onscreen, GError **error);

  void
  (*onscreen_deinit) (CoglOnscreen *onscreen);

  void
  (*onscreen_bind) (CoglOnscreen *onscreen);

  void
  (*onscreen_swap_buffers) (CoglOnscreen *onscreen);

  void
  (*onscreen_update_swap_throttled) (CoglOnscreen *onscreen);

  void
  (*onscreen_set_visibility) (CoglOnscreen *onscreen,
                              gboolean visibility);

  /* Optional functions */

  void
  (*onscreen_swap_region) (CoglOnscreen *onscreen,
                           int *rectangles,
                           int n_rectangles);

#ifdef COGL_HAS_EGL_SUPPORT
  EGLDisplay
  (*context_egl_get_egl_display) (CoglContext *context);
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
  XVisualInfo *
  (*xlib_get_visual_info) (void);
#endif

  guint32
  (*onscreen_x11_get_window_xid) (CoglOnscreen *onscreen);

#ifdef COGL_HAS_WIN32_SUPPORT
  HWND
  (*onscreen_win32_get_window) (CoglOnscreen *onscreen);
#endif

  unsigned int
  (*onscreen_add_swap_buffers_callback) (CoglOnscreen *onscreen,
                                         CoglSwapBuffersNotify callback,
                                         void *user_data);

  void
  (*onscreen_remove_swap_buffers_callback) (CoglOnscreen *onscreen,
                                            unsigned int id);

#ifdef COGL_HAS_XLIB_SUPPORT
  gboolean
  (*texture_pixmap_x11_create) (CoglTexturePixmapX11 *tex_pixmap);
  void
  (*texture_pixmap_x11_free) (CoglTexturePixmapX11 *tex_pixmap);

  gboolean
  (*texture_pixmap_x11_update) (CoglTexturePixmapX11 *tex_pixmap,
                                gboolean needs_mipmap);

  void
  (*texture_pixmap_x11_damage_notify) (CoglTexturePixmapX11 *tex_pixmap);

  CoglHandle
  (*texture_pixmap_x11_get_texture) (CoglTexturePixmapX11 *tex_pixmap);
#endif

} CoglWinsysVtable;

gboolean
_cogl_winsys_has_feature (CoglWinsysFeature feature);

CoglFuncPtr
_cogl_get_proc_address (const CoglWinsysVtable *winsys,
                        const char *name);

#endif /* __COGL_WINSYS_PRIVATE_H */
