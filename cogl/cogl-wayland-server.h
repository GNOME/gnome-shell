/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifndef __COGL_WAYLAND_SERVER_H
#define __COGL_WAYLAND_SERVER_H

#include <glib.h>

#include <wayland-server.h>

#define __COGL_H_INSIDE__
#include <cogl/cogl-context.h>
#include <cogl/cogl-texture-2d.h>

G_BEGIN_DECLS

/**
 * cogl_wayland_display_set_compositor_display:
 * @display: a #CoglDisplay
 * @wayland_display: A compositor's Wayland display pointer
 *
 * Informs Cogl of a compositor's Wayland display pointer. This
 * enables Cogl to register private wayland extensions required to
 * pass buffers between the clients and compositor.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_wayland_display_set_compositor_display (CoglDisplay *display,
                                          struct wl_display *wayland_display);

/**
 * cogl_wayland_texture_2d_new_from_buffer:
 * @ctx: A #CoglContext
 * @buffer: A Wayland buffer
 * @error: A #GError for exceptions
 *
 * Uploads the given Wayland @buffer to a #CoglTexture2D.
 *
 * <note>The results are undefined for passing an invalid @buffer
 * pointer</note>
 * <note>It is undefined if future updates to @buffer outside the
 * control of Cogl will affect the allocated #CoglTexture2D. In some
 * cases the contents of the buffer are copied (such as shm buffers),
 * and in other cases the underlying storage is re-used directly (such
 * as drm buffers)</note>
 *
 * Returns: A newly allocated #CoglTexture2D, or if Cogl could not
 *          validate the @buffer in some way (perhaps because of
 *          an unsupported format) it will return %NULL and set
 *          @error.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglTexture2D *
cogl_wayland_texture_2d_new_from_buffer (CoglContext *ctx,
                                         struct wl_buffer *buffer,
                                         GError **error);

G_END_DECLS

#endif /* __COGL_WAYLAND_SERVER_H */
