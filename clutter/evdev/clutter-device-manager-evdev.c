/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corp.
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

#include <linux/input.h>

#include <gudev/gudev.h>

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-device-manager-private.h"
#include "clutter-input-device-evdev.h"
#include "clutter-private.h"

#include "clutter-device-manager-evdev.h"

#define CLUTTER_DEVICE_MANAGER_EVDEV_GET_PRIVATE(obj)               \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                              \
                                CLUTTER_TYPE_DEVICE_MANAGER_EVDEV,  \
                                ClutterDeviceManagerEvdevPrivate))

G_DEFINE_TYPE (ClutterDeviceManagerEvdev,
               clutter_device_manager_evdev,
               CLUTTER_TYPE_DEVICE_MANAGER);

struct _ClutterDeviceManagerEvdevPrivate
{
  GUdevClient *udev_client;

  GSList *devices;

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;
};

static const gchar *subsystems[] = { "input", NULL };

static gboolean
is_evdev (const gchar *sysfs_path)
{
  GRegex *regex;
  gboolean match;

  regex = g_regex_new ("/input[0-9]+/event[0-9]+$", 0, 0, NULL);
  match = g_regex_match (regex, sysfs_path, 0, NULL);

  g_regex_unref (regex);
  return match;
}

static void
evdev_add_device (ClutterDeviceManagerEvdev *manager_evdev,
                  GUdevDevice               *udev_device)
{
  ClutterDeviceManager *manager = (ClutterDeviceManager *) manager_evdev;
  ClutterInputDeviceType type = CLUTTER_EXTENSION_DEVICE;
  ClutterInputDevice *device;
  const gchar *device_file, *sysfs_path;
  const gchar * const *keys;
  guint i;

  device_file = g_udev_device_get_device_file (udev_device);
  sysfs_path = g_udev_device_get_sysfs_path (udev_device);

  if (device_file == NULL || sysfs_path == NULL)
    return;

  if (g_udev_device_get_property (udev_device, "ID_INPUT") == NULL)
    return;

  /* Make sure to only add evdev devices, ie the device with a sysfs path that
   * finishes by input%d/event%d (We don't rely on the node name as this
   * policy is enforced by udev rules Vs API/ABI guarantees of sysfs) */
  if (!is_evdev (sysfs_path))
    return;

  keys = g_udev_device_get_property_keys (udev_device);
  for (i = 0; keys[i]; i++)
    {
      /* Clutter assumes that device types are exclusive in the
       * ClutterInputDevice API */
      if (strcmp (keys[i], "ID_INPUT_KEY") == 0)
        type = CLUTTER_KEYBOARD_DEVICE;
      else if (strcmp (keys[i], "ID_INPUT_MOUSE") == 0)
        type = CLUTTER_POINTER_DEVICE;
      else if (strcmp (keys[i], "ID_INPUT_JOYSTICK") == 0)
        type = CLUTTER_JOYSTICK_DEVICE;
      else if (strcmp (keys[i], "ID_INPUT_TABLET") == 0)
        type = CLUTTER_TABLET_DEVICE;
      else if (strcmp (keys[i], "ID_INPUT_TOUCHPAD") == 0)
        type = CLUTTER_TOUCHPAD_DEVICE;
      else if (strcmp (keys[i], "ID_INPUT_TOUCHSCREEN") == 0)
        type = CLUTTER_TOUCHSCREEN_DEVICE;
    }

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", 0,
                         "name", "Evdev device", /* FIXME */
                         "device-type", type,
                         "sysfs-path", sysfs_path,
                         "device-path", device_file,
                         NULL);
  _clutter_device_manager_add_device (manager, device);

  CLUTTER_NOTE (EVENT, "Added device %s, type %d, sysfs %s",
                device_file, type, sysfs_path);
}

static ClutterInputDeviceEvdev *
find_device_by_udev_device (ClutterDeviceManagerEvdev *manager_evdev,
                            GUdevDevice               *udev_device)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  GSList *l;
  const gchar *sysfs_path;

  sysfs_path = g_udev_device_get_sysfs_path (udev_device);
  if (sysfs_path == NULL)
    {
      g_message ("device file is NULL");
      return NULL;
    }

  for (l = priv->devices; l; l = g_slist_next (l))
    {
      ClutterInputDeviceEvdev *device = l->data;

      if (strcmp (sysfs_path,
                  _clutter_input_device_evdev_get_sysfs_path (device)) == 0)
        {
          return device;
        }
    }

  return NULL;
}

static void
evdev_remove_device (ClutterDeviceManagerEvdev *manager_evdev,
                     GUdevDevice               *device)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (manager_evdev);
  ClutterInputDeviceEvdev *device_evdev;
  ClutterInputDevice *input_device;

  device_evdev = find_device_by_udev_device (manager_evdev, device);
  if (device_evdev == NULL)
      return;

  input_device = CLUTTER_INPUT_DEVICE (device_evdev);
  _clutter_device_manager_remove_device (manager, input_device);
}

static void
on_uevent (GUdevClient *client,
           gchar       *action,
           GUdevDevice *device,
           gpointer     data)
{
  ClutterDeviceManagerEvdev *manager = CLUTTER_DEVICE_MANAGER_EVDEV (data);

  if (g_strcmp0 (action, "add") == 0)
    evdev_add_device (manager, device);
  else if (g_strcmp0 (action, "remove") == 0)
    evdev_remove_device (manager, device);
}

/*
 * ClutterDeviceManager implementation
 */

static void
clutter_device_manager_evdev_add_device (ClutterDeviceManager *manager,
                                         ClutterInputDevice   *device)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterInputDeviceType device_type;
  gboolean is_pointer, is_keyboard;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  device_type = clutter_input_device_get_device_type (device);
  is_pointer  = device_type == CLUTTER_POINTER_DEVICE;
  is_keyboard = device_type == CLUTTER_KEYBOARD_DEVICE;

  priv->devices = g_slist_prepend (priv->devices, device);

  if (is_pointer && priv->core_pointer == NULL)
    priv->core_pointer = device;

  if (is_keyboard && priv->core_keyboard == NULL)
    priv->core_keyboard = device;
}

static void
clutter_device_manager_evdev_remove_device (ClutterDeviceManager *manager,
                                            ClutterInputDevice   *device)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  priv->devices = g_slist_remove (priv->devices, device);
}

static const GSList *
clutter_device_manager_evdev_get_devices (ClutterDeviceManager *manager)
{
  return CLUTTER_DEVICE_MANAGER_EVDEV (manager)->priv->devices;
}

static ClutterInputDevice *
clutter_device_manager_evdev_get_core_device (ClutterDeviceManager   *manager,
                                              ClutterInputDeviceType  type)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return priv->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return priv->core_keyboard;

    case CLUTTER_EXTENSION_DEVICE:
    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_evdev_get_device (ClutterDeviceManager *manager,
                                         gint                  id)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *l;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  for (l = priv->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id)
        return device;
    }

  return NULL;
}

/*
 * GObject implementation
 */

static void
clutter_device_manager_evdev_constructed (GObject *gobject)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  GList *devices, *l;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (gobject);
  priv = manager_evdev->priv;

  priv->udev_client = g_udev_client_new (subsystems);

  devices = g_udev_client_query_by_subsystem (priv->udev_client, subsystems[0]);
  for (l = devices; l; l = g_list_next (l))
    {
      GUdevDevice *device = l->data;

      evdev_add_device (manager_evdev, device);
      g_object_unref (device);
    }
  g_list_free (devices);

  /* subcribe for events on input devices */
  g_signal_connect (priv->udev_client, "uevent",
                    G_CALLBACK (on_uevent), manager_evdev);
}

static void
clutter_device_manager_evdev_finalize (GObject *object)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (object);
  priv = manager_evdev->priv;

  g_object_unref (priv->udev_client);

  G_OBJECT_CLASS (clutter_device_manager_evdev_parent_class)->finalize (object);
}

static void
clutter_device_manager_evdev_class_init (ClutterDeviceManagerEvdevClass *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (ClutterDeviceManagerEvdevPrivate));

  gobject_class->constructed = clutter_device_manager_evdev_constructed;
  gobject_class->finalize = clutter_device_manager_evdev_finalize;

  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_evdev_add_device;
  manager_class->remove_device = clutter_device_manager_evdev_remove_device;
  manager_class->get_devices = clutter_device_manager_evdev_get_devices;
  manager_class->get_core_device = clutter_device_manager_evdev_get_core_device;
  manager_class->get_device = clutter_device_manager_evdev_get_device;
}

static void
clutter_device_manager_evdev_init (ClutterDeviceManagerEvdev *self)
{
  self->priv = CLUTTER_DEVICE_MANAGER_EVDEV_GET_PRIVATE (self);
}
