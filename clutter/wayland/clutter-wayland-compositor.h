/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011 Intel Corporation
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

/**
 * SECTION:clutter-wayland-compositor
 * @short_description: Wayland compositor specific APIs
 *
 * Clutter provides some Wayland specific APIs to aid in writing
 * Clutter based compositors.
 *
 * The Clutter Wayland compositor API is available since Clutter 1.8
 */

#ifndef __CLUTTER_WAYLAND_COMPOSITOR_H__
#define __CLUTTER_WAYLAND_COMPOSITOR_H__

G_BEGIN_DECLS

CLUTTER_AVAILABLE_IN_1_10
void    clutter_wayland_set_compositor_display  (void *display);

G_END_DECLS

#endif /* __CLUTTER_WAYLAND_COMPOSITOR_H__ */
