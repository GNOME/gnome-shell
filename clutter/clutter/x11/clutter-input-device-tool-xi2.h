/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2016 Red Hat
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

#ifndef __CLUTTER_INPUT_DEVICE_XI2_TOOL_H__
#define __CLUTTER_INPUT_DEVICE_XI2_TOOL_H__

#include <clutter/clutter-input-device-tool.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_TOOL_XI2 (clutter_input_device_tool_xi2_get_type ())

#define CLUTTER_INPUT_DEVICE_TOOL_XI2(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_XI2, ClutterInputDeviceToolXI2))

#define CLUTTER_IS_INPUT_DEVICE_TOOL_XI2(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_XI2))

#define CLUTTER_INPUT_DEVICE_TOOL_XI2_CLASS(c) \
  (G_TYPE_CHECK_CLASS_CAST ((c), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_XI2, ClutterInputDeviceToolXI2Class))

#define CLUTTER_IS_INPUT_DEVICE_TOOL_XI2_CLASS(c) \
  (G_TYPE_CHECK_CLASS_TYPE ((c), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_XI2))

#define CLUTTER_INPUT_DEVICE_TOOL_XI2_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), \
  CLUTTER_TYPE_INPUT_DEVICE_TOOL_XI2, ClutterInputDeviceToolXI2Class))

typedef struct _ClutterInputDeviceToolXI2 ClutterInputDeviceToolXI2;
typedef struct _ClutterInputDeviceToolXI2Class ClutterInputDeviceToolXI2Class;

struct _ClutterInputDeviceToolXI2
{
  ClutterInputDeviceTool parent_instance;
  struct libinput_tablet_tool *tool;
};

struct _ClutterInputDeviceToolXI2Class
{
  ClutterInputDeviceToolClass parent_class;
};

GType                    clutter_input_device_tool_xi2_get_type  (void) G_GNUC_CONST;

ClutterInputDeviceTool * clutter_input_device_tool_xi2_new       (guint                        serial,
                                                                  ClutterInputDeviceToolType   type);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_XI2_TOOL_H__ */
