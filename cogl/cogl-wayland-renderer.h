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
 * <note>If you use this API you must also explicitly set foreign
 * Wayland compositor and shell objects using the
 * cogl_wayland_renderer_set_foreign_compositor() and
 * cogl_wayland_renderer_set_foreign_shell() respectively. This ie
 * because Wayland doesn't currently provide a way to retrospectively
 * query these interfaces so the expectation is that if you have taken
 * ownership of the display then you will also have been notified of
 * the compositor and shell interfaces which Cogl needs to use.</note>
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
 * @display: A Wayland display
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

/**
 * cogl_wayland_renderer_set_foreign_compositor:
 * @renderer: A #CoglRenderer
 * @compositor: A Wayland compositor
 *
 * Allows you to explicitly notify Cogl of a Wayland compositor
 * interface to use. This API should be used in conjunction with
 * cogl_wayland_renderer_set_foreign_display() because if you are
 * connecting to a wayland compositor manually that will also mean you
 * will be notified on connection of the available interfaces that
 * can't be queried retrosectively with the current Wayland protocol.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_wayland_renderer_set_foreign_compositor (CoglRenderer *renderer,
                                              struct wl_compositor *compositor);

/**
 * cogl_wayland_renderer_get_compositor:
 * @renderer: A #CoglRenderer
 *
 * Retrieves the Wayland compositor interface that Cogl is using. If a
 * foreign compositor has been specified using
 * cogl_wayland_renderer_set_foreign_compositor() then that compositor
 * will be returned. If no foreign compositor has been specified then
 * the compositor that Cogl is notified of internally will be returned
 * unless the renderer has not yet been connected (either implicitly
 * or explicitly by calling cogl_renderer_connect()) in which case
 * %NULL is returned.
 *
 * Since: 1.8
 * Stability: unstable
 */
struct wl_compositor *
cogl_wayland_renderer_get_compositor (CoglRenderer *renderer);

/**
 * cogl_wayland_renderer_set_foreign_shell:
 * @renderer: A #CoglRenderer
 * @shell: A Wayland shell
 *
 * Allows you to explicitly notify Cogl of a Wayland shell interface
 * to use.  This API should be used in conjunction with
 * cogl_wayland_renderer_set_foreign_display() because if you are
 * connecting to a wayland compositor manually that will also mean you
 * will be notified on connection of the available interfaces that
 * can't be queried retrosectively with the current Wayland protocol.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_wayland_renderer_set_foreign_shell (CoglRenderer *renderer,
                                         struct wl_shell *shell);

/**
 * cogl_wayland_renderer_get_shell:
 * @renderer: A #CoglRenderer
 *
 * Retrieves the Wayland shell interface that Cogl is using. If a
 * foreign shell has been specified using
 * cogl_wayland_renderer_set_foreign_shell() then that shell
 * will be returned. If no foreign shell has been specified then
 * the shell that Cogl is notified of internally will be returned
 * unless the renderer has not yet been connected (either implicitly
 * or explicitly by calling cogl_renderer_connect()) in which case
 * %NULL is returned.
 *
 * Since: 1.10
 * Stability: unstable
 */
struct wl_shell *
cogl_wayland_renderer_get_shell (CoglRenderer *renderer);

COGL_END_DECLS

#endif /* __COGL_WAYLAND_RENDERER_H__ */
