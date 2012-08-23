/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010, 2011  Intel Corporation.
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

 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 *  Kristian Høgsberg
 */

#ifndef __CLUTTER_BACKEND_WAYLAND_H__
#define __CLUTTER_BACKEND_WAYLAND_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <clutter/clutter-device-manager.h>

#include "clutter-backend-private.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_WAYLAND                (_clutter_backend_wayland_get_type ())
#define CLUTTER_BACKEND_WAYLAND(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_WAYLAND, ClutterBackendWayland))
#define CLUTTER_IS_BACKEND_WAYLAND(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_WAYLAND))
#define CLUTTER_BACKEND_WAYLAND_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_WAYLAND, ClutterBackendWaylandClass))
#define CLUTTER_IS_BACKEND_WAYLAND_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_WAYLAND))
#define CLUTTER_BACKEND_WAYLAND_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_WAYLAND, ClutterBackendWaylandClass))

typedef struct _ClutterBackendWayland       ClutterBackendWayland;
typedef struct _ClutterBackendWaylandClass  ClutterBackendWaylandClass;

struct _ClutterBackendWayland
{
  ClutterBackend parent_instance;

  ClutterDeviceManager *device_manager;

  struct wl_display *wayland_display;
  struct wl_compositor *wayland_compositor;
  struct wl_shell *wayland_shell;
  struct wl_shm *wayland_shm;
  struct wl_surface *cursor_surface;
  struct wl_buffer *cursor_buffer;
  struct wl_output *wayland_output;

  gint cursor_x, cursor_y;
  gint output_width, output_height;

  GSource *wayland_source;

  /* event timer */
  GTimer *event_timer;
};

struct _ClutterBackendWaylandClass
{
  ClutterBackendClass parent_class;
};

GType _clutter_backend_wayland_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_BACKEND_WAYLAND_H__ */
