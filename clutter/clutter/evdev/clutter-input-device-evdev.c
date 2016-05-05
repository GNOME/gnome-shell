/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Intel Corp.
 * Copyright (C) 2014 Jonas Ådahl
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
 * Author: Jonas Ådahl <jadahl@gmail.com>
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter/clutter-device-manager-private.h"
#include "clutter-private.h"
#include "clutter-evdev.h"
#include "clutter-input-device-tool-evdev.h"

#include "clutter-input-device-evdev.h"
#include "clutter-device-manager-evdev.h"

typedef struct _ClutterInputDeviceClass        ClutterInputDeviceEvdevClass;

#define clutter_input_device_evdev_get_type _clutter_input_device_evdev_get_type

G_DEFINE_TYPE (ClutterInputDeviceEvdev,
               clutter_input_device_evdev,
               CLUTTER_TYPE_INPUT_DEVICE)

static void
clutter_input_device_evdev_finalize (GObject *object)
{
  ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (object);
  ClutterInputDeviceEvdev *device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (object);
  ClutterDeviceManagerEvdev *manager_evdev =
    CLUTTER_DEVICE_MANAGER_EVDEV (device->device_manager);

  if (device_evdev->libinput_device)
    libinput_device_unref (device_evdev->libinput_device);

  _clutter_device_manager_evdev_release_device_id (manager_evdev, device);

  G_OBJECT_CLASS (clutter_input_device_evdev_parent_class)->finalize (object);
}

static gboolean
clutter_input_device_evdev_keycode_to_evdev (ClutterInputDevice *device,
                                             guint hardware_keycode,
                                             guint *evdev_keycode)
{
  /* The hardware keycodes from the evdev backend are almost evdev
     keycodes: we use the evdev keycode file, but xkb rules have an
     offset by 8. See the comment in _clutter_key_event_new_from_evdev()
  */
  *evdev_keycode = hardware_keycode - 8;
  return TRUE;
}

static void
clutter_input_device_evdev_update_from_tool (ClutterInputDevice     *device,
                                             ClutterInputDeviceTool *tool)
{
  ClutterInputDeviceToolEvdev *evdev_tool;

  evdev_tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (tool);

  g_object_freeze_notify (G_OBJECT (device));

  _clutter_input_device_reset_axes (device);

  _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_X, 0, 0, 0);
  _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_Y, 0, 0, 0);

  if (libinput_tablet_tool_has_distance (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_DISTANCE, 0, 1, 0);

  if (libinput_tablet_tool_has_pressure (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_PRESSURE, 0, 1, 0);

  if (libinput_tablet_tool_has_tilt (evdev_tool->tool))
    {
      _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_XTILT, -90, 90, 0);
      _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_YTILT, -90, 90, 0);
    }

  if (libinput_tablet_tool_has_rotation (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_ROTATION, 0, 360, 0);

  if (libinput_tablet_tool_has_slider (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_SLIDER, -1, 1, 0);

  g_object_thaw_notify (G_OBJECT (device));
}

static void
clutter_input_device_evdev_class_init (ClutterInputDeviceEvdevClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = clutter_input_device_evdev_finalize;
  klass->keycode_to_evdev = clutter_input_device_evdev_keycode_to_evdev;
  klass->update_from_tool = clutter_input_device_evdev_update_from_tool;
}

static void
clutter_input_device_evdev_init (ClutterInputDeviceEvdev *self)
{
}

/*
 * _clutter_input_device_evdev_new:
 * @manager: the device manager
 * @seat: the seat the device will belong to
 * @libinput_device: the libinput device
 *
 * Create a new ClutterInputDevice given a libinput device and associate
 * it with the provided seat.
 */
ClutterInputDevice *
_clutter_input_device_evdev_new (ClutterDeviceManager *manager,
                                 ClutterSeatEvdev *seat,
                                 struct libinput_device *libinput_device)
{
  ClutterInputDeviceEvdev *device;
  ClutterInputDeviceType type;
  ClutterDeviceManagerEvdev *manager_evdev;
  gchar *vendor, *product;
  gint device_id;

  type = _clutter_input_device_evdev_determine_type (libinput_device);
  vendor = g_strdup_printf ("%.4x", libinput_device_get_id_vendor (libinput_device));
  product = g_strdup_printf ("%.4x", libinput_device_get_id_product (libinput_device));
  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  device_id = _clutter_device_manager_evdev_acquire_device_id (manager_evdev);
  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", device_id,
                         "name", libinput_device_get_name (libinput_device),
                         "device-manager", manager,
                         "device-type", type,
                         "device-mode", CLUTTER_INPUT_MODE_SLAVE,
                         "enabled", TRUE,
                         "vendor-id", vendor,
                         "product-id", product,
                         NULL);

  device->seat = seat;
  device->libinput_device = libinput_device;

  libinput_device_set_user_data (libinput_device, device);
  libinput_device_ref (libinput_device);
  g_free (vendor);
  g_free (product);

  return CLUTTER_INPUT_DEVICE (device);
}

/*
 * _clutter_input_device_evdev_new_virtual:
 * @manager: the device manager
 * @seat: the seat the device will belong to
 * @type: the input device type
 *
 * Create a new virtual ClutterInputDevice of the given type.
 */
ClutterInputDevice *
_clutter_input_device_evdev_new_virtual (ClutterDeviceManager *manager,
                                         ClutterSeatEvdev *seat,
                                         ClutterInputDeviceType type)
{
  ClutterInputDeviceEvdev *device;
  ClutterDeviceManagerEvdev *manager_evdev;
  const char *name;
  gint device_id;

  switch (type)
    {
    case CLUTTER_KEYBOARD_DEVICE:
      name = "Virtual keyboard device for seat";
      break;
    case CLUTTER_POINTER_DEVICE:
      name = "Virtual pointer device for seat";
      break;
    default:
      name = "Virtual device for seat";
      break;
    };

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  device_id = _clutter_device_manager_evdev_acquire_device_id (manager_evdev);
  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", device_id,
                         "name", name,
                         "device-manager", manager,
                         "device-type", type,
                         "device-mode", CLUTTER_INPUT_MODE_MASTER,
                         "enabled", TRUE,
                         NULL);

  device->seat = seat;

  return CLUTTER_INPUT_DEVICE (device);
}

ClutterSeatEvdev *
_clutter_input_device_evdev_get_seat (ClutterInputDeviceEvdev *device)
{
  return device->seat;
}

void
_clutter_input_device_evdev_update_leds (ClutterInputDeviceEvdev *device,
                                         enum libinput_led leds)
{
  if (!device->libinput_device)
    return;

  libinput_device_led_update (device->libinput_device, leds);
}

ClutterInputDeviceType
_clutter_input_device_evdev_determine_type (struct libinput_device *ldev)
{
  /* This setting is specific to touchpads and alike, only in these
   * devices there is this additional layer of touch event interpretation.
   */
  if (libinput_device_config_tap_get_finger_count (ldev) > 0)
    return CLUTTER_TOUCHPAD_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
    return CLUTTER_TABLET_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_POINTER))
    return CLUTTER_POINTER_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_TOUCH))
    return CLUTTER_TOUCHSCREEN_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_KEYBOARD))
    return CLUTTER_KEYBOARD_DEVICE;
  else
    return CLUTTER_EXTENSION_DEVICE;
}

/**
 * clutter_evdev_input_device_get_libinput_device:
 * @device: a #ClutterInputDevice
 *
 * Retrieves the libinput_device struct held in @device.
 *
 * Returns: The libinput_device struct
 *
 * Since: 1.20
 * Stability: unstable
 **/
struct libinput_device *
clutter_evdev_input_device_get_libinput_device (ClutterInputDevice *device)
{
  ClutterInputDeviceEvdev *device_evdev;

  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_EVDEV (device), NULL);

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);

  return device_evdev->libinput_device;
}

/**
 * clutter_evdev_event_sequence_get_slot:
 * @sequence: a #ClutterEventSequence
 *
 * Retrieves the touch slot triggered by this @sequence
 *
 * Returns: the libinput touch slot.
 *
 * Since: 1.20
 * Stability: unstable
 **/
gint32
clutter_evdev_event_sequence_get_slot (const ClutterEventSequence *sequence)
{
  if (!sequence)
    return -1;

  return GPOINTER_TO_INT (sequence) - 1;
}
