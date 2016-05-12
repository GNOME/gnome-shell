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

#ifndef __CLUTTER_INPUT_DEVICE_TOOL_H__
#define __CLUTTER_INPUT_DEVICE_TOOL_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#include <clutter/clutter-types.h>
#include "clutter-enum-types.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_TOOL            (clutter_input_device_tool_get_type ())
#define CLUTTER_INPUT_DEVICE_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_TOOL, ClutterInputDeviceTool))
#define CLUTTER_IS_INPUT_DEVICE_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_TOOL))
#define CLUTTER_INPUT_DEVICE_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_INPUT_DEVICE_TOOL, ClutterInputDeviceToolClass))
#define CLUTTER_IS_INPUT_DEVICE_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_INPUT_DEVICE_TOOL))
#define CLUTTER_INPUT_DEVICE_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_INPUT_DEVICE_TOOL, ClutterInputDeviceToolClass))

typedef struct _ClutterInputDeviceToolClass ClutterInputDeviceToolClass;

struct _ClutterInputDeviceTool
{
  GObject parent_instance;
};

struct _ClutterInputDeviceToolClass
{
  GObjectClass parent_class;
};

CLUTTER_AVAILABLE_IN_ALL
GType                      clutter_input_device_tool_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_ALL
guint64                    clutter_input_device_tool_get_serial    (ClutterInputDeviceTool *tool);

CLUTTER_AVAILABLE_IN_ALL
ClutterInputDeviceToolType clutter_input_device_tool_get_tool_type (ClutterInputDeviceTool *tool);

CLUTTER_AVAILABLE_IN_ALL
guint64                    clutter_input_device_tool_get_id        (ClutterInputDeviceTool *tool);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_TOOL_H__ */
