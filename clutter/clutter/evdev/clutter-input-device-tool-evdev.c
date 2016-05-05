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

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-input-device-tool-evdev.h"

G_DEFINE_TYPE (ClutterInputDeviceToolEvdev, clutter_input_device_tool_evdev,
               CLUTTER_TYPE_INPUT_DEVICE_TOOL)

static void
clutter_input_device_tool_evdev_finalize (GObject *object)
{
  ClutterInputDeviceToolEvdev *tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (object);

  libinput_tablet_tool_unref (tool->tool);

  G_OBJECT_CLASS (clutter_input_device_tool_evdev_parent_class)->finalize (object);
}

static void
clutter_input_device_tool_evdev_class_init (ClutterInputDeviceToolEvdevClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = clutter_input_device_tool_evdev_finalize;
}

static void
clutter_input_device_tool_evdev_init (ClutterInputDeviceToolEvdev *tool)
{
}

ClutterInputDeviceTool *
clutter_input_device_tool_evdev_new (struct libinput_tablet_tool *tool,
                                     guint64                      serial,
                                     ClutterInputDeviceToolType   type)
{
  ClutterInputDeviceToolEvdev *evdev_tool;

  evdev_tool = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_TOOL_EVDEV,
                             "type", type,
                             "serial", serial,
                             NULL);

  evdev_tool->tool = libinput_tablet_tool_ref (tool);

  return CLUTTER_INPUT_DEVICE_TOOL (evdev_tool);
}
