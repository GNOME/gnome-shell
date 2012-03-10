/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2011  Intel Corp.
 *             2011  Giovanni Campagna <scampa.giovanni@gmail.com>
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "config.h"

#include "clutter-input-device-gdk.h"

#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "clutter-backend-gdk.h"
#include "clutter-stage-gdk.h"

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceGdkClass;

#define clutter_input_device_gdk_get_type       _clutter_input_device_gdk_get_type

G_DEFINE_TYPE (ClutterInputDeviceGdk,
               clutter_input_device_gdk,
               CLUTTER_TYPE_INPUT_DEVICE);

static int device_int_counter;

enum {
  PROP_0,
  PROP_GDK_DEVICE,
  PROP_LAST
};

static void
clutter_input_device_gdk_set_property (GObject      *gobject,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  ClutterInputDeviceGdk *self = CLUTTER_INPUT_DEVICE_GDK (gobject);

  switch (prop_id)
    {
    case PROP_GDK_DEVICE:
      self->gdk_device = GDK_DEVICE (g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_input_device_gdk_class_init (ClutterInputDeviceGdkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_input_device_gdk_set_property;

  g_object_class_install_property (gobject_class, PROP_GDK_DEVICE,
				   g_param_spec_object ("gdk-device",
							"GdkDevice",
							"The GDK device",
							GDK_TYPE_DEVICE,
							CLUTTER_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
clutter_input_device_gdk_init (ClutterInputDeviceGdk *self)
{
}

ClutterInputDevice*
_clutter_input_device_gdk_new (ClutterDeviceManager    *manager,
			       GdkDevice               *device)
{
  ClutterBackend *backend;
  ClutterInputDevice *clutter_device;
  ClutterInputMode input_mode = CLUTTER_INPUT_MODE_FLOATING;
  ClutterInputDeviceType device_type = CLUTTER_EXTENSION_DEVICE;
  gboolean has_cursor = FALSE;
  const gchar *name;
  gboolean is_enabled = FALSE;

  g_object_get (manager, "backend", &backend, NULL);

  /* yay for name consistency */
  switch (gdk_device_get_device_type (device))
    {
    case GDK_DEVICE_TYPE_MASTER:
      input_mode = CLUTTER_INPUT_MODE_MASTER;
      is_enabled = TRUE;
      break;
    case GDK_DEVICE_TYPE_SLAVE:
      input_mode = CLUTTER_INPUT_MODE_SLAVE;
      is_enabled = FALSE;
      break;
    case GDK_DEVICE_TYPE_FLOATING:
      input_mode = CLUTTER_INPUT_MODE_FLOATING;
      is_enabled = FALSE;
      break;
    }

  switch (gdk_device_get_source (device))
    {
    case GDK_SOURCE_MOUSE:
      device_type = CLUTTER_POINTER_DEVICE;
      break;
    case GDK_SOURCE_PEN:
      device_type = CLUTTER_PEN_DEVICE;
      break;
    case GDK_SOURCE_ERASER:
      device_type = CLUTTER_ERASER_DEVICE;
      break;
    case GDK_SOURCE_CURSOR:
      device_type = CLUTTER_CURSOR_DEVICE;
      break;
    case GDK_SOURCE_KEYBOARD:
      device_type = CLUTTER_KEYBOARD_DEVICE;
      break;
    case GDK_SOURCE_TOUCHSCREEN:
      device_type = CLUTTER_TOUCHSCREEN_DEVICE;
      break;
    case GDK_SOURCE_TOUCHPAD:
      device_type = CLUTTER_TOUCHPAD_DEVICE;
      break;
    }

  if (device_type != CLUTTER_KEYBOARD_DEVICE)
    has_cursor = gdk_device_get_has_cursor (device);

  name = gdk_device_get_name (device);

  clutter_device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_GDK,
				 "backend", backend,
				 "device-manager", manager,
				 "device-mode", input_mode,
				 "device-type", device_type,
				 "has-cursor", has_cursor,
				 "gdk-device", device,
				 "id", device_int_counter++,
				 "name", name,
				 "enabled", is_enabled,
				 NULL);
  return clutter_device;
}
