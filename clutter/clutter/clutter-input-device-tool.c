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

#include "clutter-input-device-tool.h"
#include "clutter-private.h"

typedef struct _ClutterInputDeviceToolPrivate ClutterInputDeviceToolPrivate;

struct _ClutterInputDeviceToolPrivate
{
  ClutterInputDeviceToolType type;
  guint64 serial;
  guint64 id;
};

enum {
  PROP_0,
  PROP_TYPE,
  PROP_SERIAL,
  PROP_ID,
  PROP_LAST
};

static GParamSpec *props[PROP_LAST] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ClutterInputDeviceTool, clutter_input_device_tool, G_TYPE_OBJECT)

static void
clutter_input_device_tool_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ClutterInputDeviceTool *tool = CLUTTER_INPUT_DEVICE_TOOL (object);
  ClutterInputDeviceToolPrivate *priv;

  priv = clutter_input_device_tool_get_instance_private (tool);

  switch (prop_id)
    {
    case PROP_TYPE:
      priv->type = g_value_get_enum (value);
      break;
    case PROP_SERIAL:
      priv->serial = g_value_get_uint64 (value);
      break;
    case PROP_ID:
      priv->id = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clutter_input_device_tool_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ClutterInputDeviceTool *tool = CLUTTER_INPUT_DEVICE_TOOL (object);
  ClutterInputDeviceToolPrivate *priv;

  priv = clutter_input_device_tool_get_instance_private (tool);

  switch (prop_id)
    {
    case PROP_TYPE:
      g_value_set_enum (value, priv->type);
      break;
    case PROP_SERIAL:
      g_value_set_uint64 (value, priv->serial);
      break;
    case PROP_ID:
      g_value_set_uint64 (value, priv->id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clutter_input_device_tool_class_init (ClutterInputDeviceToolClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_input_device_tool_set_property;
  gobject_class->get_property = clutter_input_device_tool_get_property;

  props[PROP_TYPE] =
    g_param_spec_enum ("type",
                       P_("Tool type"),
                       P_("Tool type"),
                       CLUTTER_TYPE_INPUT_DEVICE_TOOL_TYPE,
                       CLUTTER_INPUT_DEVICE_TOOL_NONE,
                       CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  props[PROP_SERIAL] =
    g_param_spec_uint64 ("serial",
                         P_("Tool serial"),
                         P_("Tool serial"),
                         0, G_MAXUINT64, 0,
                         CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  props[PROP_ID] =
    g_param_spec_uint64 ("id",
                         P_("Tool ID"),
                         P_("Tool ID"),
                         0, G_MAXUINT64, 0,
                         CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, PROP_LAST, props);
}

static void
clutter_input_device_tool_init (ClutterInputDeviceTool *tool)
{
}

/**
 * clutter_input_device_tool_get_serial:
 * @tool: a #ClutterInputDeviceTool
 *
 * Gets the serial of this tool, this value can be used to identify a
 * physical tool (eg. a tablet pen) across program executions.
 *
 * Returns: The serial ID for this tool
 *
 * Since: 1.28
 **/
guint64
clutter_input_device_tool_get_serial (ClutterInputDeviceTool *tool)
{
  ClutterInputDeviceToolPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL (tool), 0);

  priv = clutter_input_device_tool_get_instance_private (tool);

  return priv->serial;
}


/**
 * clutter_input_device_tool_get_tool_type:
 * @tool: a #ClutterInputDeviceTool
 *
 * Gets the tool type of this tool.
 *
 * Returns: The tool type of this tool
 *
 * Since: 1.28
 **/
ClutterInputDeviceToolType
clutter_input_device_tool_get_tool_type (ClutterInputDeviceTool *tool)
{
  ClutterInputDeviceToolPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL (tool), 0);

  priv = clutter_input_device_tool_get_instance_private (tool);

  return priv->type;
}

/**
 * clutter_input_device_tool_get_id:
 * @tool: a #ClutterInputDeviceTool
 *
 * Gets the ID of this tool, this value can be used to identify a
 * physical tool (eg. a tablet pen) across program executions.
 *
 * Returns: The tool ID for this tool
 **/
guint64
clutter_input_device_tool_get_id (ClutterInputDeviceTool *tool)
{
  ClutterInputDeviceToolPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL (tool), 0);

  priv = clutter_input_device_tool_get_instance_private (tool);

  return priv->id;
}
