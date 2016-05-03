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

G_DEFINE_TYPE (ClutterVirtualInputDevice,
               clutter_virtual_input_device,
               G_TYPE_OBJECT)

static void
clutter_virtual_input_device_init (ClutterVirtualInputDevice *virtual_device)
{
}

static void
clutter_virtual_input_device_class_init (ClutterVirtualInputDeviceClass *klass)
{
}

void
clutter_virtual_input_device_notify_relative_motion (ClutterVirtualInputDevice *virtual_device,
                                                     uint64_t                   time_us,
                                                     double                     dx,
                                                     double                     dy)
{
  ClutterVirtualInputDeviceClass *klass =
    CLUTTER_VIRTUAL_INPUT_DEVICE_GET_CLASS (virtual_device);

  klass->notify_relative_motion (virtual_device, time_us, dx, dy);
}

void
clutter_virtual_input_device_notify_absolute_motion (ClutterVirtualInputDevice *virtual_device,
                                                     uint64_t                   time_us,
                                                     double                     x,
                                                     double                     y)
{
  ClutterVirtualInputDeviceClass *klass =
    CLUTTER_VIRTUAL_INPUT_DEVICE_GET_CLASS (virtual_device);

  klass->notify_absolute_motion (virtual_device, time_us, x, y);
}

void
clutter_virtual_input_device_notify_button (ClutterVirtualInputDevice *virtual_device,
                                            uint64_t                   time_us,
                                            uint32_t                   button,
                                            ClutterButtonState         button_state)
{
  ClutterVirtualInputDeviceClass *klass =
    CLUTTER_VIRTUAL_INPUT_DEVICE_GET_CLASS (virtual_device);

  klass->notify_button (virtual_device, time_us, button, button_state);
}

void
clutter_virtual_input_device_notify_key (ClutterVirtualInputDevice *virtual_device,
                                         uint64_t                   time_us,
                                         uint32_t                   key,
                                         ClutterKeyState            key_state)
{
  ClutterVirtualInputDeviceClass *klass =
    CLUTTER_VIRTUAL_INPUT_DEVICE_GET_CLASS (virtual_device);

  klass->notify_key (virtual_device, time_us, key, key_state);
}
