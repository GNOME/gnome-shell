/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2010, 2011  Intel Corp.
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "config.h"

#include "clutter-input-device-core-x11.h"

#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "clutter-backend-x11.h"
#include "clutter-stage-x11.h"

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceX11Class;

/* a specific X11 input device */
struct _ClutterInputDeviceX11
{
  ClutterInputDevice device;

  int min_keycode;
  int max_keycode;
};

#define clutter_input_device_x11_get_type       _clutter_input_device_x11_get_type

G_DEFINE_TYPE (ClutterInputDeviceX11,
               clutter_input_device_x11,
               CLUTTER_TYPE_INPUT_DEVICE);

static gboolean
clutter_input_device_x11_keycode_to_evdev (ClutterInputDevice *device,
                                           guint hardware_keycode,
                                           guint *evdev_keycode)
{
  /* When using evdev under X11 the hardware keycodes are the evdev
     keycodes plus 8. I haven't been able to find any documentation to
     know what the +8 is for. FIXME: This should probably verify that
     X server is using evdev. */
  *evdev_keycode = hardware_keycode - 8;

  return TRUE;
}

static void
clutter_input_device_x11_class_init (ClutterInputDeviceX11Class *klass)
{
  ClutterInputDeviceClass *device_class = CLUTTER_INPUT_DEVICE_CLASS (klass);

  device_class->keycode_to_evdev = clutter_input_device_x11_keycode_to_evdev;
}

static void
clutter_input_device_x11_init (ClutterInputDeviceX11 *self)
{
}
