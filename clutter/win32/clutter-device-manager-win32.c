/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corp.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-win32.h"
#include "clutter-device-manager-win32.h"
#include "clutter-device-manager-win32.h"
#include "clutter-stage-win32.h"

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-private.h"

enum
{
  PROP_0
};

G_DEFINE_TYPE (ClutterDeviceManagerWin32,
               clutter_device_manager_win32,
               CLUTTER_TYPE_DEVICE_MANAGER);

static void
clutter_device_manager_win32_constructed (GObject *gobject)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (gobject);
  ClutterInputDevice *device;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE,
                         "id", 0,
                         "name", "Core Pointer",
                         "device-type", CLUTTER_POINTER_DEVICE,
                         NULL);
  CLUTTER_NOTE (BACKEND, "Added core pointer device");
  _clutter_device_manager_add_device (manager, device);

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE,
                         "id", 1,
                         "name", "Core Keyboard",
                         "device-type", CLUTTER_KEYBOARD_DEVICE,
                         NULL);
  CLUTTER_NOTE (BACKEND, "Added core keyboard device");
  _clutter_device_manager_add_device (manager, device);
}

static void
clutter_device_manager_win32_add_device (ClutterDeviceManager *manager,
                                       ClutterInputDevice   *device)
{
  ClutterDeviceManagerWin32 *manager_win32 = CLUTTER_DEVICE_MANAGER_WIN32 (manager);
  ClutterInputDeviceType device_type;
  gboolean is_pointer, is_keyboard;

  device_type = clutter_input_device_get_device_type (device);
  is_pointer  = (device_type == CLUTTER_POINTER_DEVICE)  ? TRUE : FALSE;
  is_keyboard = (device_type == CLUTTER_KEYBOARD_DEVICE) ? TRUE : FALSE;

  manager_win32->devices = g_slist_prepend (manager_win32->devices, device);

  if (is_pointer && manager_win32->core_pointer == NULL)
    manager_win32->core_pointer = device;

  if (is_keyboard && manager_win32->core_keyboard == NULL)
    manager_win32->core_keyboard = device;
}

static void
clutter_device_manager_win32_remove_device (ClutterDeviceManager *manager,
                                          ClutterInputDevice   *device)
{
  ClutterDeviceManagerWin32 *manager_win32 = CLUTTER_DEVICE_MANAGER_WIN32 (manager);

  manager_win32->devices = g_slist_remove (manager_win32->devices, device);
}

static const GSList *
clutter_device_manager_win32_get_devices (ClutterDeviceManager *manager)
{
  return CLUTTER_DEVICE_MANAGER_WIN32 (manager)->devices;
}

static ClutterInputDevice *
clutter_device_manager_win32_get_core_device (ClutterDeviceManager *manager,
                                            ClutterInputDeviceType type)
{
  ClutterDeviceManagerWin32 *manager_win32;

  manager_win32 = CLUTTER_DEVICE_MANAGER_WIN32 (manager);

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return manager_win32->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return manager_win32->core_keyboard;

    case CLUTTER_EXTENSION_DEVICE:
    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_win32_get_device (ClutterDeviceManager *manager,
                                       gint                  id)
{
  ClutterDeviceManagerWin32 *manager_win32 = CLUTTER_DEVICE_MANAGER_WIN32 (manager);
  GSList *l;

  for (l = manager_win32->devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id)
        return device;
    }

  return NULL;
}

static void
clutter_device_manager_win32_class_init (ClutterDeviceManagerWin32Class *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = clutter_device_manager_win32_constructed;
  
  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_win32_add_device;
  manager_class->remove_device = clutter_device_manager_win32_remove_device;
  manager_class->get_devices = clutter_device_manager_win32_get_devices;
  manager_class->get_core_device = clutter_device_manager_win32_get_core_device;
  manager_class->get_device = clutter_device_manager_win32_get_device;
}

static void
clutter_device_manager_win32_init (ClutterDeviceManagerWin32 *self)
{
}
