/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012 Intel Corporation
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

/**
 * SECTION:clutter-wayland
 * @short_description: Wayland specific API
 *
 * The Wayland backend for Clutter provides some specific API, allowing
 * integration with the Wayland client API for acessing the underlying data
 * structures
 *
 * The Clutter Wayland API is available since Clutter 1.10
 */

#ifndef __CLUTTER_WAYLAND_H__
#define __CLUTTER_WAYLAND_H__

#include <glib.h>
#include <wayland-client.h>
#include <clutter/clutter.h>
G_BEGIN_DECLS

struct wl_seat *clutter_wayland_input_device_get_wl_seat (ClutterInputDevice *device);

struct wl_shell_surface *clutter_wayland_stage_get_wl_shell_surface (ClutterStage *stage);

struct wl_surface *clutter_wayland_stage_get_wl_surface (ClutterStage *stage);

CLUTTER_AVAILABLE_IN_1_16
void clutter_wayland_stage_set_wl_surface (ClutterStage *stage, struct wl_surface *surface);

CLUTTER_AVAILABLE_IN_1_16
void clutter_wayland_set_display (struct wl_display *display);

CLUTTER_AVAILABLE_IN_1_16
void clutter_wayland_disable_event_retrieval (void);

G_END_DECLS
#endif /* __CLUTTER_WAYLAND_H__ */
