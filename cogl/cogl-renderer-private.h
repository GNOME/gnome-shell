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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_RENDERER_PRIVATE_H
#define __COGL_RENDERER_PRIVATE_H

#include <gmodule.h>

#include "cogl-object-private.h"
#include "cogl-winsys-private.h"
#include "cogl-internal.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

#if COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
#include <wayland-client.h>
#endif

struct _CoglRenderer
{
  CoglObject _parent;
  gboolean connected;
  const CoglWinsysVtable *winsys_vtable;
  CoglWinsysID winsys_id_override;
#ifdef COGL_HAS_XLIB_SUPPORT
  Display *foreign_xdpy;
#endif

  CoglDriver driver;
#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY
  GModule *libgl_module;
#endif

#if COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  struct wl_display *foreign_wayland_display;
  struct wl_compositor *foreign_wayland_compositor;
#endif
  /* List of callback functions that will be given every native event */
  GSList *event_filters;
  void *winsys;
};

typedef CoglFilterReturn (* CoglNativeFilterFunc) (void *native_event,
                                                   void *data);

CoglFilterReturn
_cogl_renderer_handle_native_event (CoglRenderer *renderer,
                                    void *event);

void
_cogl_renderer_add_native_filter (CoglRenderer *renderer,
                                  CoglNativeFilterFunc func,
                                  void *data);

void
_cogl_renderer_remove_native_filter (CoglRenderer *renderer,
                                     CoglNativeFilterFunc func,
                                     void *data);

#endif /* __COGL_RENDERER_PRIVATE_H */
