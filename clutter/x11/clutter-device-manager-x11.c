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

#include "clutter-backend-x11.h"
#include "clutter-device-manager-x11.h"
#include "clutter-input-device-x11.h"
#include "clutter-stage-x11.h"

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-private.h"

#ifdef HAVE_XINPUT
#include <X11/extensions/XInput.h>
#endif

enum
{
  PROP_0,

  PROP_USE_XINPUT_1
};

G_DEFINE_TYPE (ClutterDeviceManagerX11,
               clutter_device_manager_x11,
               CLUTTER_TYPE_DEVICE_MANAGER);

static void
clutter_device_manager_x11_constructed (GObject *gobject)
{
  ClutterDeviceManagerX11 *manager_x11;
  ClutterBackendX11 *backend_x11;
  ClutterDeviceManager *manager;
  ClutterInputDevice *device;
#ifdef HAVE_XINPUT
  XDeviceInfo *x_devices = NULL;
  int res, opcode, event, error;
  int i, n_devices;
#endif /* HAVE_XINPUT */

  manager = CLUTTER_DEVICE_MANAGER (gobject);
  manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (gobject);
  if (!manager_x11->use_xinput_1)
    {
      CLUTTER_NOTE (BACKEND, "XInput support not enabled");
      goto default_device;
    }

  g_object_get (gobject, "backend", &backend_x11, NULL);
  g_assert (backend_x11 != NULL);

#ifdef HAVE_XINPUT
  res = XQueryExtension (backend_x11->xdpy, "XInputExtension",
                         &opcode,
                         &event,
                         &error);
  if (!res)
    {
      CLUTTER_NOTE (BACKEND, "No XInput extension available");
      goto default_device;
    }

  backend_x11->xi_event_base = event;

  x_devices = XListInputDevices (backend_x11->xdpy, &n_devices);
  if (n_devices == 0)
    {
      CLUTTER_NOTE (BACKEND, "No XInput devices found");
      goto default_device;
    }

  for (i = 0; i < n_devices; i++)
    {
      XDeviceInfo *info = x_devices + i;

      CLUTTER_NOTE (BACKEND,
                    "Considering device %li with type %d, %d of %d",
                    info->id,
                    info->use,
                    i, n_devices);

      /* we only want 'raw' devices, not virtual ones */
      if (info->use == IsXExtensionPointer ||
       /* info->use == IsXExtensionKeyboard || XInput1 is broken */
          info->use == IsXExtensionDevice)
        {
          ClutterInputDeviceType device_type;
          gint n_events = 0;

          switch (info->use)
            {
            case IsXExtensionPointer:
              device_type = CLUTTER_POINTER_DEVICE;
              break;

            /* XInput1 is broken for keyboards */
            case IsXExtensionKeyboard:
              device_type = CLUTTER_KEYBOARD_DEVICE;
              break;

            case IsXExtensionDevice:
            default:
              device_type = CLUTTER_EXTENSION_DEVICE;
              break;
            }

          device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_X11,
                                 "id", info->id,
                                 "device-type", device_type,
                                 "name", info->name,
                                 NULL);
          n_events = _clutter_input_device_x11_construct (device, backend_x11);

          _clutter_device_manager_add_device (manager, device);

          if (info->use == IsXExtensionPointer && n_events > 0)
            backend_x11->have_xinput = TRUE;
        }
    }

  XFree (x_devices);
#endif /* HAVE_XINPUT */

default_device:
  /* fallback code in case:
   *
   *  - we do not have XInput support compiled in
   *  - we do not have XInput support enabled
   *  - we do not have the XInput extension
   *
   * we register two default devices, one for the pointer
   * and one for the keyboard. this block must also be
   * executed for the XInput support because XI does not
   * cover core devices
   */
  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_X11,
                         "id", 0,
                         "name", "Core Pointer",
                         "device-type", CLUTTER_POINTER_DEVICE,
                         "is-core", TRUE,
                         NULL);
  CLUTTER_NOTE (BACKEND, "Added core pointer device");
  manager_x11->core_pointer = device;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_X11,
                         "id", 1,
                         "name", "Core Keyboard",
                         "device-type", CLUTTER_KEYBOARD_DEVICE,
                         "is-core", TRUE,
                         NULL);
  CLUTTER_NOTE (BACKEND, "Added core keyboard device");
  manager_x11->core_keyboard = device;

  if (G_OBJECT_CLASS (clutter_device_manager_x11_parent_class)->constructed)
    G_OBJECT_CLASS (clutter_device_manager_x11_parent_class)->constructed (gobject);
}

static void
clutter_device_manager_x11_add_device (ClutterDeviceManager *manager,
                                       ClutterInputDevice   *device)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  manager_x11->devices = g_slist_prepend (manager_x11->devices, device);

  /* blow the cache */
  g_slist_free (manager_x11->all_devices);
  manager_x11->all_devices = NULL;
}

static void
clutter_device_manager_x11_remove_device (ClutterDeviceManager *manager,
                                          ClutterInputDevice   *device)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  manager_x11->devices = g_slist_remove (manager_x11->devices, device);

  /* blow the cache */
  g_slist_free (manager_x11->all_devices);
  manager_x11->all_devices = NULL;
}

static const GSList *
clutter_device_manager_x11_get_devices (ClutterDeviceManager *manager)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  /* cache the devices list so that we can keep the core pointer
   * and keyboard outside of the ManagerX11:devices list
   */
  if (manager_x11->all_devices == NULL)
    {
      GSList *all_devices = NULL;

      all_devices = g_slist_prepend (all_devices, manager_x11->core_keyboard);
      all_devices = g_slist_prepend (all_devices, manager_x11->core_pointer);
      all_devices->next = manager_x11->devices;

      manager_x11->all_devices = all_devices;
    }
    
  return CLUTTER_DEVICE_MANAGER_X11 (manager)->all_devices;
}

static ClutterInputDevice *
clutter_device_manager_x11_get_core_device (ClutterDeviceManager *manager,
                                            ClutterInputDeviceType type)
{
  ClutterDeviceManagerX11 *manager_x11;

  manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return manager_x11->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return manager_x11->core_keyboard;

    case CLUTTER_EXTENSION_DEVICE:
    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_x11_get_device (ClutterDeviceManager *manager,
                                       gint                  id)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);
  GSList *l;

  for (l = manager_x11->devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id)
        return device;
    }

  return NULL;
}

static void
clutter_device_manager_x11_set_property (GObject      *gobject,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ClutterDeviceManagerX11 *manager_x11;

  manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (gobject);

  switch (prop_id)
    {
    case PROP_USE_XINPUT_1:
      manager_x11->use_xinput_1 = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_device_manager_x11_get_property (GObject    *gobject,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ClutterDeviceManagerX11 *manager_x11;

  manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (gobject);

  switch (prop_id)
    {
    case PROP_USE_XINPUT_1:
      g_value_set_boolean (value, manager_x11->use_xinput_1);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_device_manager_x11_class_init (ClutterDeviceManagerX11Class *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = clutter_device_manager_x11_set_property;
  gobject_class->get_property = clutter_device_manager_x11_get_property;
  gobject_class->constructed = clutter_device_manager_x11_constructed;
  
  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_x11_add_device;
  manager_class->remove_device = clutter_device_manager_x11_remove_device;
  manager_class->get_devices = clutter_device_manager_x11_get_devices;
  manager_class->get_core_device = clutter_device_manager_x11_get_core_device;
  manager_class->get_device = clutter_device_manager_x11_get_device;

  pspec = g_param_spec_boolean ("use-xinput-1",
                                "Use XInput 1",
                                "Use the XInput 1.0 extension",
                                FALSE,
                                CLUTTER_PARAM_READWRITE |
                                G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_USE_XINPUT_1, pspec);
}

static void
clutter_device_manager_x11_init (ClutterDeviceManagerX11 *self)
{
  self->use_xinput_1 = FALSE;
}
