/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2014 Canonical Ltd.
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
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-device-manager-private.h"
#include "clutter-input-device-mir.h"

typedef struct _ClutterInputDeviceClass ClutterInputDeviceMirClass;

#define clutter_input_device_mir_get_type _clutter_input_device_mir_get_type
G_DEFINE_TYPE (ClutterInputDeviceMir, clutter_input_device_mir, CLUTTER_TYPE_INPUT_DEVICE);

static gboolean
clutter_input_device_mir_keycode_to_evdev (ClutterInputDevice *device,
                                           guint hardware_keycode,
                                           guint *evdev_keycode)
{
  *evdev_keycode = hardware_keycode - 8;
  return TRUE;
}

static void
clutter_input_device_mir_class_init (ClutterInputDeviceMirClass *klass)
{
  klass->keycode_to_evdev = clutter_input_device_mir_keycode_to_evdev;
}

static void
clutter_input_device_mir_init (ClutterInputDeviceMir *self)
{
}
