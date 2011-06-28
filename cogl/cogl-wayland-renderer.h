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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_WAYLAND_RENDERER_H__
#define __COGL_WAYLAND_RENDERER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-renderer.h>

#include <wayland-client.h>

G_BEGIN_DECLS

#define cogl_wayland_renderer_set_foreign_display \
  cogl_wayland_renderer_set_foreign_display_EXP
void
cogl_wayland_renderer_set_foreign_display (CoglRenderer *renderer,
                                           struct wl_display *display);

#define cogl_wayland_renderer_get_display \
  cogl_wayland_renderer_get_display_EXP
struct wl_display *
cogl_wayland_renderer_get_display (CoglRenderer *renderer);

#define cogl_wayland_renderer_set_foreign_compositor \
  cogl_wayland_renderer_set_foreign_compositor_EXP
void
cogl_wayland_renderer_set_foreign_compositor (CoglRenderer *renderer,
                                              struct wl_compositor *compositor);

#define cogl_wayland_renderer_get_compositor \
  cogl_wayland_renderer_get_compositor_EXP
struct wl_compositor *
cogl_wayland_renderer_get_compositor (CoglRenderer *renderer);

G_END_DECLS

#endif /* __COGL_WAYLAND_RENDERER_H__ */
