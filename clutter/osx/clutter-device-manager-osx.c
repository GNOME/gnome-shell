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

#include "clutter-backend-osx.h"
#include "clutter-device-manager-osx.h"
#include "clutter-device-manager-osx.h"
#include "clutter-stage-osx.h"

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-private.h"

enum
{
  PROP_0
};

G_DEFINE_TYPE (ClutterDeviceManagerOSX,
               clutter_device_manager_osx,
               CLUTTER_TYPE_DEVICE_MANAGER);

static void
clutter_device_manager_osx_constructed (GObject *gobject)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (gobject);
  ClutterDeviceManagerOSX *manager_osx;
  ClutterInputDevice *device;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE,
                         "id", 0,
                         "name", "Core Pointer",
                         "device-type", CLUTTER_POINTER_DEVICE,
                         "device-mode", CLUTTER_INPUT_MODE_MASTER,
                         "has-cursor", TRUE,
                         "enabled", TRUE,
                         NULL);
  CLUTTER_NOTE (BACKEND, "Added core pointer device");
  _clutter_device_manager_add_device (manager, device);

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE,
                         "id", 1,
                         "name", "Core Keyboard",
                         "device-type", CLUTTER_KEYBOARD_DEVICE,
                         "device-mode", CLUTTER_INPUT_MODE_MASTER,
                         "enabled", TRUE,
                         NULL);
  CLUTTER_NOTE (BACKEND, "Added core keyboard device");
  _clutter_device_manager_add_device (manager, device);

  manager_osx = CLUTTER_DEVICE_MANAGER_OSX (manager);

  _clutter_input_device_set_associated_device (manager_osx->core_pointer,
                                               manager_osx->core_keyboard);
  _clutter_input_device_set_associated_device (manager_osx->core_keyboard,
                                               manager_osx->core_pointer);
}

static void
clutter_device_manager_osx_add_device (ClutterDeviceManager *manager,
                                       ClutterInputDevice   *device)
{
  ClutterDeviceManagerOSX *manager_osx = CLUTTER_DEVICE_MANAGER_OSX (manager);
  ClutterInputDeviceType device_type;
  gboolean is_pointer, is_keyboard;

  device_type = clutter_input_device_get_device_type (device);
  is_pointer  = (device_type == CLUTTER_POINTER_DEVICE)  ? TRUE : FALSE;
  is_keyboard = (device_type == CLUTTER_KEYBOARD_DEVICE) ? TRUE : FALSE;

  manager_osx->devices = g_slist_prepend (manager_osx->devices, device);

  if (is_pointer && manager_osx->core_pointer == NULL)
    manager_osx->core_pointer = device;

  if (is_keyboard && manager_osx->core_keyboard == NULL)
    manager_osx->core_keyboard = device;
}

static void
clutter_device_manager_osx_remove_device (ClutterDeviceManager *manager,
                                          ClutterInputDevice   *device)
{
  ClutterDeviceManagerOSX *manager_osx = CLUTTER_DEVICE_MANAGER_OSX (manager);

  manager_osx->devices = g_slist_remove (manager_osx->devices, device);
}

static const GSList *
clutter_device_manager_osx_get_devices (ClutterDeviceManager *manager)
{
  return CLUTTER_DEVICE_MANAGER_OSX (manager)->devices;
}

static ClutterInputDevice *
clutter_device_manager_osx_get_core_device (ClutterDeviceManager *manager,
                                            ClutterInputDeviceType type)
{
  ClutterDeviceManagerOSX *manager_osx;

  manager_osx = CLUTTER_DEVICE_MANAGER_OSX (manager);

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return manager_osx->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return manager_osx->core_keyboard;

    case CLUTTER_EXTENSION_DEVICE:
    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_osx_get_device (ClutterDeviceManager *manager,
                                       gint                  id_)
{
  ClutterDeviceManagerOSX *manager_osx = CLUTTER_DEVICE_MANAGER_OSX (manager);
  GSList *l;

  for (l = manager_osx->devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id_)
        return device;
    }

  return NULL;
}

static void
clutter_device_manager_osx_class_init (ClutterDeviceManagerOSXClass *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = clutter_device_manager_osx_constructed;
  
  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_osx_add_device;
  manager_class->remove_device = clutter_device_manager_osx_remove_device;
  manager_class->get_devices = clutter_device_manager_osx_get_devices;
  manager_class->get_core_device = clutter_device_manager_osx_get_core_device;
  manager_class->get_device = clutter_device_manager_osx_get_device;
}

static void
clutter_device_manager_osx_init (ClutterDeviceManagerOSX *self)
{
}
