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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_WAYLAND_RENDERER_H__
#define __COGL_WAYLAND_RENDERER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-renderer.h>

#include <wayland-client.h>

COGL_BEGIN_DECLS

/**
 * cogl_wayland_renderer_set_foreign_display:
 * @renderer: A #CoglRenderer
 * @display: A Wayland display
 *
 * Allows you to explicitly control what Wayland display you want Cogl
 * to work with instead of leaving Cogl to automatically connect to a
 * wayland compositor.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_wayland_renderer_set_foreign_display (CoglRenderer *renderer,
                                           struct wl_display *display);

/**
 * cogl_wayland_renderer_get_display:
 * @renderer: A #CoglRenderer
 *
 * Retrieves the Wayland display that Cogl is using. If a foreign
 * display has been specified using
 * cogl_wayland_renderer_set_foreign_display() then that display will
 * be returned. If no foreign display has been specified then the
 * display that Cogl creates internally will be returned unless the
 * renderer has not yet been connected (either implicitly or explicitly by
 * calling cogl_renderer_connect()) in which case %NULL is returned.
 *
 * Returns: The wayland display currently associated with @renderer,
 *          or %NULL if the renderer hasn't yet been connected and no
 *          foreign display has been specified.
 *
 * Since: 1.8
 * Stability: unstable
 */
struct wl_display *
cogl_wayland_renderer_get_display (CoglRenderer *renderer);

COGL_END_DECLS

#endif /* __COGL_WAYLAND_RENDERER_H__ */
