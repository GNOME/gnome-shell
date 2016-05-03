/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2016  Red Hat Inc.
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
 * Author: Jonas Ådahl <jadahl@gmail.com>
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include <glib-object.h>

#include "clutter-virtual-input-device.h"
#include "evdev/clutter-virtual-input-device-evdev.h"

struct _ClutterVirtualInputDeviceEvdev
{
  ClutterVirtualInputDevice parent;
};

G_DEFINE_TYPE (ClutterVirtualInputDeviceEvdev,
               clutter_virtual_input_device_evdev,
               CLUTTER_TYPE_VIRTUAL_INPUT_DEVICE)

static void
clutter_virtual_input_device_evdev_notify_relative_motion (ClutterVirtualInputDevice *virtual_device,
                                                           uint64_t                   time_us,
                                                           double                     dx,
                                                           double                     dy)
{
}

static void
clutter_virtual_input_device_evdev_notify_absolute_motion (ClutterVirtualInputDevice *virtual_device,
                                                           uint64_t                   time_us,
                                                           double                     x,
                                                           double                     y)
{
}

static void
clutter_virtual_input_device_evdev_notify_button (ClutterVirtualInputDevice *virtual_device,
                                                  uint64_t                   time_us,
                                                  uint32_t                   button,
                                                  ClutterButtonState         button_state)
{
}

static void
clutter_virtual_input_device_evdev_notify_key (ClutterVirtualInputDevice *virtual_device,
                                               uint64_t                   time_us,
                                               uint32_t                   key,
                                               ClutterKeyState            key_state)
{
}

static void
clutter_virtual_input_device_evdev_init (ClutterVirtualInputDeviceEvdev *virtual_device_evdev)
{
}

static void
clutter_virtual_input_device_evdev_class_init (ClutterVirtualInputDeviceEvdevClass *klass)
{
  ClutterVirtualInputDeviceClass *virtual_input_device_class =
    CLUTTER_VIRTUAL_INPUT_DEVICE_CLASS (klass);

  virtual_input_device_class->notify_relative_motion = clutter_virtual_input_device_evdev_notify_relative_motion;
  virtual_input_device_class->notify_absolute_motion = clutter_virtual_input_device_evdev_notify_absolute_motion;
  virtual_input_device_class->notify_button = clutter_virtual_input_device_evdev_notify_button;
  virtual_input_device_class->notify_key = clutter_virtual_input_device_evdev_notify_key;
}
