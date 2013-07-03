/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Intel Corp.
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
 * Author: Damien Lespiau <damien.lespiau@intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter/clutter-device-manager-private.h"
#include "clutter-private.h"

#include "clutter-input-device-evdev.h"

typedef struct _ClutterInputDeviceClass        ClutterInputDeviceEvdevClass;
typedef struct _ClutterInputDeviceEvdevPrivate ClutterInputDeviceEvdevPrivate;

enum
{
  PROP_0,

  PROP_SYSFS_PATH,
  PROP_DEVICE_PATH,

  PROP_LAST
};

struct _ClutterInputDeviceEvdevPrivate
{
  gchar *sysfs_path;
  gchar *device_path;
};

struct _ClutterInputDeviceEvdev
{
  ClutterInputDevice parent;

  ClutterInputDeviceEvdevPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterInputDeviceEvdev,
                            clutter_input_device_evdev,
                            CLUTTER_TYPE_INPUT_DEVICE)

static GParamSpec *obj_props[PROP_LAST];

static void
clutter_input_device_evdev_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ClutterInputDeviceEvdev  *input = CLUTTER_INPUT_DEVICE_EVDEV (object);
  ClutterInputDeviceEvdevPrivate *priv = input->priv;

  switch (property_id)
    {
    case PROP_SYSFS_PATH:
      g_value_set_string (value, priv->sysfs_path);
      break;
    case PROP_DEVICE_PATH:
      g_value_set_string (value, priv->device_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_input_device_evdev_set_property (GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ClutterInputDeviceEvdev  *input = CLUTTER_INPUT_DEVICE_EVDEV (object);
  ClutterInputDeviceEvdevPrivate *priv = input->priv;

  switch (property_id)
    {
    case PROP_SYSFS_PATH:
      priv->sysfs_path = g_value_dup_string (value);
      break;
    case PROP_DEVICE_PATH:
      priv->device_path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_input_device_evdev_finalize (GObject *object)
{
  ClutterInputDeviceEvdev  *input = CLUTTER_INPUT_DEVICE_EVDEV (object);
  ClutterInputDeviceEvdevPrivate *priv = input->priv;

  g_free (priv->sysfs_path);
  g_free (priv->device_path);

  G_OBJECT_CLASS (clutter_input_device_evdev_parent_class)->finalize (object);
}

static gboolean
clutter_input_device_evdev_keycode_to_evdev (ClutterInputDevice *device,
                                             guint hardware_keycode,
                                             guint *evdev_keycode)
{
  /* The hardware keycodes from the evdev backend are already evdev
     keycodes */
  *evdev_keycode = hardware_keycode;
  return TRUE;
}

static void
clutter_input_device_evdev_class_init (ClutterInputDeviceEvdevClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->get_property = clutter_input_device_evdev_get_property;
  object_class->set_property = clutter_input_device_evdev_set_property;
  object_class->finalize = clutter_input_device_evdev_finalize;
  klass->keycode_to_evdev = clutter_input_device_evdev_keycode_to_evdev;

  /*
   * ClutterInputDeviceEvdev:udev-device:
   *
   * The Sysfs path of the device
   *
   * Since: 1.6
   */
  pspec =
    g_param_spec_string ("sysfs-path",
                         P_("sysfs Path"),
                         P_("Path of the device in sysfs"),
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | CLUTTER_PARAM_READWRITE);
  obj_props[PROP_SYSFS_PATH] = pspec;
  g_object_class_install_property (object_class, PROP_SYSFS_PATH, pspec);

  /*
   * ClutterInputDeviceEvdev:device-path
   *
   * The path of the device file.
   *
   * Since: 1.6
   */
  pspec =
    g_param_spec_string ("device-path",
                         P_("Device Path"),
                         P_("Path of the device node"),
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | CLUTTER_PARAM_READWRITE);
  obj_props[PROP_DEVICE_PATH] = pspec;
  g_object_class_install_property (object_class, PROP_DEVICE_PATH, pspec);
}

static void
clutter_input_device_evdev_init (ClutterInputDeviceEvdev *self)
{
  self->priv = clutter_input_device_evdev_get_instance_private (self);
}

ClutterInputDeviceEvdev *
_clutter_input_device_evdev_new (void)
{
  return g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV, NULL);
}

const gchar *
_clutter_input_device_evdev_get_sysfs_path (ClutterInputDeviceEvdev *device)
{
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_EVDEV (device), NULL);

  return device->priv->sysfs_path;
}

const gchar *
_clutter_input_device_evdev_get_device_path (ClutterInputDeviceEvdev *device)
{
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_EVDEV (device), NULL);

  return device->priv->device_path;
}
