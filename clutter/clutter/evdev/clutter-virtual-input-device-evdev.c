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

#include "clutter-private.h"
#include "clutter-virtual-input-device.h"
#include "evdev/clutter-input-device-evdev.h"
#include "evdev/clutter-seat-evdev.h"
#include "evdev/clutter-virtual-input-device-evdev.h"

enum
{
  PROP_0,

  PROP_SEAT,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

struct _ClutterVirtualInputDeviceEvdev
{
  ClutterVirtualInputDevice parent;

  ClutterInputDevice *device;
  ClutterSeatEvdev *seat;
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
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);

  clutter_seat_evdev_notify_relative_motion (virtual_evdev->seat,
                                             virtual_evdev->device,
                                             time_us,
                                             dx, dy,
                                             dx, dy);
}

static void
clutter_virtual_input_device_evdev_notify_absolute_motion (ClutterVirtualInputDevice *virtual_device,
                                                           uint64_t                   time_us,
                                                           double                     x,
                                                           double                     y)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);

  clutter_seat_evdev_notify_absolute_motion (virtual_evdev->seat,
                                             virtual_evdev->device,
                                             time_us,
                                             x, y,
                                             NULL);
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
clutter_virtual_input_device_evdev_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);

  switch (prop_id)
    {
    case PROP_SEAT:
      g_value_set_pointer (value, virtual_evdev->seat);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_virtual_input_device_evdev_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);

  switch (prop_id)
    {
    case PROP_SEAT:
      virtual_evdev->seat = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_virtual_input_device_evdev_constructed (GObject *object)
{
  ClutterVirtualInputDevice *virtual_device =
    CLUTTER_VIRTUAL_INPUT_DEVICE (object);
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);
  ClutterDeviceManager *manager;
  ClutterInputDeviceType device_type;
  ClutterStage *stage;

  manager = clutter_virtual_input_device_get_manager (virtual_device);
  device_type = clutter_virtual_input_device_get_device_type (virtual_device);

  virtual_evdev->device =
    _clutter_input_device_evdev_new_virtual (manager,
                                             virtual_evdev->seat,
                                             device_type);

  stage = _clutter_device_manager_evdev_get_stage (CLUTTER_DEVICE_MANAGER_EVDEV (manager));
  _clutter_input_device_set_stage (virtual_evdev->device, stage);
}

static void
clutter_virtual_input_device_evdev_finalize (GObject *object)
{
  ClutterVirtualInputDevice *virtual_device =
    CLUTTER_VIRTUAL_INPUT_DEVICE (object);
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);
  GObjectClass *object_class;

  g_clear_object (&virtual_evdev->device);

  object_class =
    G_OBJECT_CLASS (clutter_virtual_input_device_evdev_parent_class);
  object_class->finalize (object);
}

static void
clutter_virtual_input_device_evdev_init (ClutterVirtualInputDeviceEvdev *virtual_device_evdev)
{
}

static void
clutter_virtual_input_device_evdev_class_init (ClutterVirtualInputDeviceEvdevClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterVirtualInputDeviceClass *virtual_input_device_class =
    CLUTTER_VIRTUAL_INPUT_DEVICE_CLASS (klass);

  object_class->get_property = clutter_virtual_input_device_evdev_get_property;
  object_class->set_property = clutter_virtual_input_device_evdev_set_property;
  object_class->constructed = clutter_virtual_input_device_evdev_constructed;
  object_class->finalize = clutter_virtual_input_device_evdev_finalize;

  virtual_input_device_class->notify_relative_motion = clutter_virtual_input_device_evdev_notify_relative_motion;
  virtual_input_device_class->notify_absolute_motion = clutter_virtual_input_device_evdev_notify_absolute_motion;
  virtual_input_device_class->notify_button = clutter_virtual_input_device_evdev_notify_button;
  virtual_input_device_class->notify_key = clutter_virtual_input_device_evdev_notify_key;

  obj_props[PROP_SEAT] = g_param_spec_pointer ("seat",
                                               P_("ClutterSeatEvdev"),
                                               P_("ClutterSeatEvdev"),
                                               CLUTTER_PARAM_READWRITE |
                                               G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}
