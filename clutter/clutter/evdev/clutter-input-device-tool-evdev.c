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
#include "clutter-evdev.h"

G_DEFINE_TYPE (ClutterInputDeviceToolEvdev, clutter_input_device_tool_evdev,
               CLUTTER_TYPE_INPUT_DEVICE_TOOL)

static void
clutter_input_device_tool_evdev_finalize (GObject *object)
{
  ClutterInputDeviceToolEvdev *tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (object);

  g_hash_table_unref (tool->button_map);
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
  tool->button_map = g_hash_table_new (NULL, NULL);
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
                             "id", libinput_tablet_tool_get_tool_id (tool),
                             NULL);

  evdev_tool->tool = libinput_tablet_tool_ref (tool);

  return CLUTTER_INPUT_DEVICE_TOOL (evdev_tool);
}

void
clutter_evdev_input_device_tool_set_pressure_curve (ClutterInputDeviceTool *tool,
                                                    gdouble                 curve[4])
{
  ClutterInputDeviceToolEvdev *evdev_tool;

  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL_EVDEV (tool));
  g_return_if_fail (curve[0] >= 0 && curve[0] <= 1 &&
                    curve[1] >= 0 && curve[1] <= 1 &&
                    curve[2] >= 0 && curve[2] <= 1 &&
                    curve[3] >= 0 && curve[3] <= 1);

  evdev_tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (tool);
  evdev_tool->pressure_curve[0] = curve[0];
  evdev_tool->pressure_curve[1] = curve[1];
  evdev_tool->pressure_curve[2] = curve[2];
  evdev_tool->pressure_curve[3] = curve[3];
}

void
clutter_evdev_input_device_tool_set_button_code (ClutterInputDeviceTool *tool,
                                                 guint                   button,
                                                 guint                   evcode)
{
  ClutterInputDeviceToolEvdev *evdev_tool;

  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL_EVDEV (tool));

  evdev_tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (tool);

  if (evcode == 0)
    {
      g_hash_table_remove (evdev_tool->button_map, GUINT_TO_POINTER (button));
    }
  else
    {
      g_hash_table_insert (evdev_tool->button_map, GUINT_TO_POINTER (button),
                           GUINT_TO_POINTER (evcode));
    }
}

static gdouble
calculate_bezier_position (gdouble pos,
                           gdouble x1,
                           gdouble y1,
                           gdouble x2,
                           gdouble y2)
{
  gdouble int1_y, int2_y;

  pos = CLAMP (pos, 0, 1);

  /* Intersection between 0,0 and x1,y1 */
  int1_y = pos * y1;

  /* Intersection between x2,y2 and 1,1 */
  int2_y = (pos * (1 - y2)) + y2;

  /* Find the new position in the line traced by the previous points */
  return (pos * (int2_y - int1_y)) + int1_y;
}

gdouble
clutter_input_device_tool_evdev_translate_pressure (ClutterInputDeviceTool *tool,
                                                    gdouble                 pressure)
{
  ClutterInputDeviceToolEvdev *evdev_tool;

  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL (tool), pressure);

  evdev_tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (tool);

  return calculate_bezier_position (CLAMP (pressure, 0, 1),
                                    evdev_tool->pressure_curve[0],
                                    evdev_tool->pressure_curve[1],
                                    evdev_tool->pressure_curve[2],
                                    evdev_tool->pressure_curve[3]);
}

guint
clutter_input_device_tool_evdev_get_button_code (ClutterInputDeviceTool *tool,
                                                 guint                   button)
{
  ClutterInputDeviceToolEvdev *evdev_tool;

  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL (tool), 0);

  evdev_tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (tool);

  return GPOINTER_TO_UINT (g_hash_table_lookup (evdev_tool->button_map,
                                                GUINT_TO_POINTER (button)));
}
