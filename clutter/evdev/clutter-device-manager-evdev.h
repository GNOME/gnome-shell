/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corp.
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
 * Author: Damien Lespiau <damien.lespiau@intel.com>
 */

#ifndef __CLUTTER_DEVICE_MANAGER_EVDEV_H__
#define __CLUTTER_DEVICE_MANAGER_EVDEV_H__

#include <clutter/clutter-device-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEVICE_MANAGER_EVDEV            (clutter_device_manager_evdev_get_type ())
#define CLUTTER_DEVICE_MANAGER_EVDEV(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEVICE_MANAGER_EVDEV, ClutterDeviceManagerEvdev))
#define CLUTTER_IS_DEVICE_MANAGER_EVDEV(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEVICE_MANAGER_EVDEV))
#define CLUTTER_DEVICE_MANAGER_EVDEV_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER_EVDEV, ClutterDeviceManagerEvdevClass))
#define CLUTTER_IS_DEVICE_MANAGER_EVDEV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER_EVDEV))
#define CLUTTER_DEVICE_MANAGER_EVDEV_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER_EVDEV, ClutterDeviceManagerEvdevClass))

typedef struct _ClutterDeviceManagerEvdev         ClutterDeviceManagerEvdev;
typedef struct _ClutterDeviceManagerEvdevClass    ClutterDeviceManagerEvdevClass;
typedef struct _ClutterDeviceManagerEvdevPrivate  ClutterDeviceManagerEvdevPrivate;

struct _ClutterDeviceManagerEvdev
{
  ClutterDeviceManager parent_instance;

  ClutterDeviceManagerEvdevPrivate *priv;
};

struct _ClutterDeviceManagerEvdevClass
{
  ClutterDeviceManagerClass parent_class;
};

GType clutter_device_manager_evdev_get_type (void) G_GNUC_CONST;

void  _clutter_events_evdev_init            (ClutterBackend *backend);
void  _clutter_events_evdev_uninit          (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_EVDEV_H__ */
