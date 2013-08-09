/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Intel Corp.
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

#ifndef __CLUTTER_INPUT_DEVICE_EVDEV_H__
#define __CLUTTER_INPUT_DEVICE_EVDEV_H__

#include <glib-object.h>
#include <gudev/gudev.h>

#include <clutter/clutter-input-device.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_EVDEV clutter_input_device_evdev_get_type()

#define CLUTTER_INPUT_DEVICE_EVDEV(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV, ClutterInputDeviceEvdev))

#define CLUTTER_INPUT_DEVICE_EVDEV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV, ClutterInputDeviceEvdevClass))

#define CLUTTER_IS_INPUT_DEVICE_EVDEV(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV))

#define CLUTTER_IS_INPUT_DEVICE_EVDEV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV))

#define CLUTTER_INPUT_DEVICE_EVDEV_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV, ClutterInputDeviceEvdevClass))

typedef struct _ClutterInputDeviceEvdev ClutterInputDeviceEvdev;

GType                     clutter_input_device_evdev_get_type         (void) G_GNUC_CONST;

const gchar *             _clutter_input_device_evdev_get_sysfs_path  (ClutterInputDeviceEvdev *device);
const gchar *             _clutter_input_device_evdev_get_device_path (ClutterInputDeviceEvdev *device);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_EVDEV_H__ */
