/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * cogl_wayland_renderer_set_event_dispatch_enabled:
 * @renderer: A #CoglRenderer
 * @enable: The new value
 *
 * Sets whether Cogl should handle calling wl_display_dispatch() and
 * wl_display_flush() as part of its main loop integration via
 * cogl_poll_renderer_get_info() and cogl_poll_renderer_dispatch().
 * The default value is %TRUE. When it is enabled the application can
 * register listeners for Wayland interfaces and the callbacks will be
 * invoked during cogl_poll_renderer_dispatch(). If the application
 * wants to integrate with its own code that is already handling
 * reading from the Wayland display socket, it should disable this to
 * avoid having competing code read from the socket.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_wayland_renderer_set_event_dispatch_enabled (CoglRenderer *renderer,
                                                  CoglBool enable);

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
