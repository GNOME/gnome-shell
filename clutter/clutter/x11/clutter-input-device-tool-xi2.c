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

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-input-device-tool-xi2.h"

G_DEFINE_TYPE (ClutterInputDeviceToolXI2, clutter_input_device_tool_xi2,
               CLUTTER_TYPE_INPUT_DEVICE_TOOL)

static void
clutter_input_device_tool_xi2_class_init (ClutterInputDeviceToolXI2Class *klass)
{
}

static void
clutter_input_device_tool_xi2_init (ClutterInputDeviceToolXI2 *tool)
{
}

ClutterInputDeviceTool *
clutter_input_device_tool_xi2_new (guint                        serial,
                                   ClutterInputDeviceToolType   type)
{
  return g_object_new (CLUTTER_TYPE_INPUT_DEVICE_TOOL_XI2,
                       "type", type,
                       "serial", serial,
                       NULL);
}
