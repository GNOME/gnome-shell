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
#include "cogl-driver.h"
#include "cogl-texture-driver.h"
#include "cogl-context.h"
#include "cogl-closure-list-private.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

#if defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
#include <wayland-client.h>
#endif

struct _CoglRenderer
{
  CoglObject _parent;
  CoglBool connected;
  CoglDriver driver_override;
  const CoglDriverVtable *driver_vtable;
  const CoglTextureDriver *texture_driver;
  const CoglWinsysVtable *winsys_vtable;
  CoglWinsysID winsys_id_override;
  GList *constraints;

  GArray *poll_fds;
  int poll_fds_age;
  GList *poll_sources;

  CoglList idle_closures;

  GList *outputs;

#ifdef COGL_HAS_XLIB_SUPPORT
  Display *foreign_xdpy;
  CoglBool xlib_enable_event_retrieval;
#endif

#ifdef COGL_HAS_WIN32_SUPPORT
  CoglBool win32_enable_event_retrieval;
#endif

  CoglDriver driver;
  CoglPrivateFeatureFlags private_feature_flags;
#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY
  GModule *libgl_module;
#endif

#if defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
  struct wl_display *foreign_wayland_display;
  CoglBool wayland_enable_event_dispatch;
#endif

#ifdef COGL_HAS_SDL_SUPPORT
  CoglBool sdl_event_type_set;
  uint32_t sdl_event_type;
#endif

  /* List of callback functions that will be given every native event */
  GSList *event_filters;
  void *winsys;
};

/* Mask of constraints that effect driver selection. All of the other
 * constraints effect only the winsys selection */
#define COGL_RENDERER_DRIVER_CONSTRAINTS \
  COGL_RENDERER_CONSTRAINT_SUPPORTS_COGL_GLES2

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

void *
_cogl_renderer_get_proc_address (CoglRenderer *renderer,
                                 const char *name,
                                 CoglBool in_core);

#endif /* __COGL_RENDERER_PRIVATE_H */
