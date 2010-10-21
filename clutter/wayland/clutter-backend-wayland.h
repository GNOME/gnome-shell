/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "clutter-wayland.h"

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

  /* EGL Specific */
  EGLDisplay edpy;
  EGLContext egl_context;
  EGLConfig  egl_config;

  gint egl_version_major;
  gint egl_version_minor;

  struct wl_display *wayland_display;
  GSource *wayland_source;
  struct wl_compositor *wayland_compositor;
  struct wl_shell *wayland_shell;
  struct wl_drm *wayland_drm;
  char *device_name;
  int authenticated;
  struct wl_output *wayland_output;
  ClutterGeometry screen_allocation;
  int drm_fd;

  PFNEGLGETDRMDISPLAYMESA get_drm_display;
  PFNEGLCREATEDRMIMAGEMESA create_drm_image;
  PFNEGLDESTROYIMAGEKHRPROC destroy_image;
  PFNEGLEXPORTDRMIMAGEMESA export_drm_image;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
};

struct _ClutterBackendWaylandClass
{
  ClutterBackendClass parent_class;
};

GType _clutter_backend_wayland_get_type (void) G_GNUC_CONST;

GSource *_clutter_event_source_wayland_new(struct wl_display *display);
void _clutter_backend_add_input_device (ClutterBackendWayland *backend_wayland,
					uint32_t id);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_WAYLAND_H__ */
