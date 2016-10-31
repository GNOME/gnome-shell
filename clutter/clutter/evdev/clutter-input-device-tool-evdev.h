/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2009, 2010, 2011  Intel Corp.
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __CLUTTER_INPUT_DEVICE_EVDEV_TOOL_H__
#define __CLUTTER_INPUT_DEVICE_EVDEV_TOOL_H__

#include <libinput.h>

#include <clutter/clutter-input-device-tool.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_TOOL_EVDEV (clutter_input_device_tool_evdev_get_type ())

#define CLUTTER_INPUT_DEVICE_TOOL_EVDEV(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_EVDEV, ClutterInputDeviceToolEvdev))

#define CLUTTER_IS_INPUT_DEVICE_TOOL_EVDEV(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_EVDEV))

#define CLUTTER_INPUT_DEVICE_TOOL_EVDEV_CLASS(c) \
  (G_TYPE_CHECK_CLASS_CAST ((c), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_EVDEV, ClutterInputDeviceToolEvdevClass))

#define CLUTTER_IS_INPUT_DEVICE_TOOL_EVDEV_CLASS(c) \
  (G_TYPE_CHECK_CLASS_TYPE ((c), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_EVDEV))

#define CLUTTER_INPUT_DEVICE_TOOL_EVDEV_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_EVDEV, ClutterInputDeviceToolEvdevClass))

typedef struct _ClutterInputDeviceToolEvdev ClutterInputDeviceToolEvdev;
typedef struct _ClutterInputDeviceToolEvdevClass ClutterInputDeviceToolEvdevClass;

struct _ClutterInputDeviceToolEvdev
{
  ClutterInputDeviceTool parent_instance;
  struct libinput_tablet_tool *tool;
  GHashTable *button_map;
  gdouble pressure_curve[4];
};

struct _ClutterInputDeviceToolEvdevClass
{
  ClutterInputDeviceToolClass parent_class;
};

GType                    clutter_input_device_tool_evdev_get_type (void) G_GNUC_CONST;

ClutterInputDeviceTool * clutter_input_device_tool_evdev_new      (struct libinput_tablet_tool *tool,
                                                                   guint64                      serial,
                                                                   ClutterInputDeviceToolType   type);

gdouble                  clutter_input_device_tool_evdev_translate_pressure (ClutterInputDeviceTool *tool,
                                                                             gdouble                 pressure);
guint                    clutter_input_device_tool_evdev_get_button_code    (ClutterInputDeviceTool *tool,
                                                                             guint                   button);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_EVDEV_TOOL_H__ */
