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

#ifndef __COGL_DISPLAY_PRIVATE_H
#define __COGL_DISPLAY_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-display.h"
#include "cogl-renderer.h"
#include "cogl-onscreen-template.h"

struct _CoglDisplay
{
  CoglObject _parent;

  CoglBool setup;
  CoglRenderer *renderer;
  CoglOnscreenTemplate *onscreen_template;

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
  struct wl_display *wayland_compositor_display;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  gdl_plane_id_t gdl_plane;
#endif

  void *winsys;
};

#endif /* __COGL_DISPLAY_PRIVATE_H */
