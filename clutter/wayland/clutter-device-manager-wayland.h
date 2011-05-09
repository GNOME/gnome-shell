/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corp.
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
 *  Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CLUTTER_DEVICE_MANAGER_WAYLAND_H__
#define __CLUTTER_DEVICE_MANAGER_WAYLAND_H__

#include <clutter/clutter-device-manager.h>
#include <wayland/clutter-backend-wayland.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEVICE_MANAGER_WAYLAND            (_clutter_device_manager_wayland_get_type ())
#define CLUTTER_DEVICE_MANAGER_WAYLAND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEVICE_MANAGER_WAYLAND, ClutterDeviceManagerWayland))
#define CLUTTER_IS_DEVICE_MANAGER_WAYLAND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEVICE_MANAGER_WAYLAND))
#define CLUTTER_DEVICE_MANAGER_WAYLAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER_WAYLAND, ClutterDeviceManagerWaylandClass))
#define CLUTTER_IS_DEVICE_MANAGER_WAYLAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER_WAYLAND))
#define CLUTTER_DEVICE_MANAGER_WAYLAND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER_WAYLAND, ClutterDeviceManagerWaylandClass))

typedef struct _ClutterDeviceManagerWayland         ClutterDeviceManagerWayland;
typedef struct _ClutterDeviceManagerWaylandClass    ClutterDeviceManagerWaylandClass;

struct _ClutterDeviceManagerWayland
{
  ClutterDeviceManager parent_instance;

  GSList *devices;

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;
};

struct _ClutterDeviceManagerWaylandClass
{
  ClutterDeviceManagerClass parent_class;
};

GType _clutter_device_manager_wayland_get_type (void) G_GNUC_CONST;

ClutterDeviceManager *
_clutter_device_manager_wayland_new (ClutterBackend *backend);

void
_clutter_device_manager_wayland_add_input_group (ClutterDeviceManager *manager,
                                                 uint32_t id);

void
_clutter_events_wayland_init (ClutterBackend *backend);

void
_clutter_events_wayland_uninit (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_WAYLAND_H__ */
