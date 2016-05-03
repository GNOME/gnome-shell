/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2016  Red Hat inc.
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

#ifndef __CLUTTER_VIRTUAL_INPUT_DEVICE_H__
#define __CLUTTER_VIRTUAL_INPUT_DEVICE_H__

#include <glib-object.h>
#include <stdint.h>

#define CLUTTER_TYPE_VIRTUAL_INPUT_DEVICE (clutter_virtual_input_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (ClutterVirtualInputDevice,
                          clutter_virtual_input_device,
                          CLUTTER, VIRTUAL_INPUT_DEVICE,
                          GObject)

typedef enum _ClutterButtonState
{
  CLUTTER_BUTTON_STATE_RELEASED,
  CLUTTER_BUTTON_STATE_PRESSED
} ClutterButtonState;

typedef enum _ClutterKeyState
{
  CLUTTER_KEY_STATE_RELEASED,
  CLUTTER_KEY_STATE_PRESSED
} ClutterKeyState;

struct _ClutterVirtualInputDeviceClass
{
  GObjectClass parent_class;

  void (*notify_relative_motion) (ClutterVirtualInputDevice *virtual_device,
                                  uint64_t                   time_us,
                                  double                     dx,
                                  double                     dy);

  void (*notify_absolute_motion) (ClutterVirtualInputDevice *virtual_device,
                                  uint64_t                   time_us,
                                  double                     x,
                                  double                     y);

  void (*notify_button) (ClutterVirtualInputDevice *virtual_device,
                         uint64_t                   time_us,
                         uint32_t                   button,
                         ClutterButtonState         button_state);

  void (*notify_key) (ClutterVirtualInputDevice *virtual_device,
                      uint64_t                   time_us,
                      uint32_t                   key,
                      ClutterKeyState            key_state);
};

void clutter_virtual_input_device_notify_relative_motion (ClutterVirtualInputDevice *virtual_device,
                                                          uint64_t                   time_us,
                                                          double                     dx,
                                                          double                     dy);

void clutter_virtual_input_device_notify_absolute_motion (ClutterVirtualInputDevice *virtual_device,
                                                          uint64_t                   time_us,
                                                          double                     x,
                                                          double                     y);

void clutter_virtual_input_device_notify_button (ClutterVirtualInputDevice *virtual_device,
                                                 uint64_t                   time_us,
                                                 uint32_t                   button,
                                                 ClutterButtonState         button_state);

void clutter_virtual_input_device_notify_key (ClutterVirtualInputDevice *virtual_device,
                                              uint64_t                   time_us,
                                              uint32_t                   key,
                                              ClutterKeyState            key_state);

#endif /* __CLUTTER_VIRTUAL_INPUT_DEVICE_H__ */
