/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2014 Canonical Ltd.
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
 * Authors:
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-device-manager-private.h"
#include "clutter-input-device-mir.h"
#include "clutter-device-manager-mir.h"

static guint device_counter;

G_DEFINE_TYPE (ClutterDeviceManagerMir, _clutter_device_manager_mir, CLUTTER_TYPE_DEVICE_MANAGER);

static void
clutter_device_manager_mir_add_device (ClutterDeviceManager *manager,
                                       ClutterInputDevice   *device)
{
  ClutterDeviceManagerMir *manager_mir = CLUTTER_DEVICE_MANAGER_MIR (manager);
  manager_mir->devices = g_slist_prepend (manager_mir->devices, device);
}

static void
clutter_device_manager_mir_remove_device (ClutterDeviceManager *manager,
                                          ClutterInputDevice   *device)
{
  ClutterDeviceManagerMir *manager_mir = CLUTTER_DEVICE_MANAGER_MIR (manager);
  manager_mir->devices = g_slist_remove (manager_mir->devices, device);
}

static const GSList *
clutter_device_manager_mir_get_devices (ClutterDeviceManager *manager)
{
  return CLUTTER_DEVICE_MANAGER_MIR (manager)->devices;
}

static ClutterInputDevice *
clutter_device_manager_mir_get_core_device (ClutterDeviceManager *manager,
                                            ClutterInputDeviceType type)
{
  ClutterDeviceManagerMir *manager_mir;

  manager_mir = CLUTTER_DEVICE_MANAGER_MIR (manager);

  switch (type)
    {
      case CLUTTER_POINTER_DEVICE:
        return manager_mir->core_pointer;

      case CLUTTER_KEYBOARD_DEVICE:
        return manager_mir->core_keyboard;

      case CLUTTER_EXTENSION_DEVICE:
      default:
        return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_mir_get_device (ClutterDeviceManager *manager,
                                       gint                  id)
{
  ClutterDeviceManagerMir *manager_mir =
    CLUTTER_DEVICE_MANAGER_MIR (manager);
  GSList *l;

  for (l = manager_mir->devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id)
        return device;
    }

  return NULL;
}

static void
clutter_device_manager_mir_constructed (GObject *gobject)
{
  ClutterBackend *backend;
  ClutterDeviceManager *manager;
  ClutterDeviceManagerMir *manager_mir;
  ClutterInputDevice *device;

  manager = CLUTTER_DEVICE_MANAGER (gobject);
  manager_mir = CLUTTER_DEVICE_MANAGER_MIR (manager);

  g_object_get (manager, "backend", &backend, NULL);

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_MIR,
                         "id", device_counter++,
                         "backend", backend,
                         "device-manager", manager,
                         "device-type", CLUTTER_POINTER_DEVICE,
                         "device-mode", CLUTTER_INPUT_MODE_MASTER,
                         "name", "Mir pointer",
                         "enabled", TRUE,
                         "has-cursor", TRUE,
                         NULL);

  manager_mir->core_pointer = device;
  _clutter_device_manager_add_device (manager, CLUTTER_INPUT_DEVICE (device));

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_MIR,
                         "id", device_counter++,
                         "backend", backend,
                         "device-manager", manager,
                         "device-type", CLUTTER_KEYBOARD_DEVICE,
                         "device-mode", CLUTTER_INPUT_MODE_MASTER,
                         "name", "Mir keyboard",
                         "enabled", TRUE,
                         "has-cursor", FALSE,
                         NULL);

  manager_mir->core_keyboard = device;
  _clutter_device_manager_add_device (manager, CLUTTER_INPUT_DEVICE (device));

  _clutter_input_device_set_associated_device (manager_mir->core_pointer,
                                               manager_mir->core_keyboard);
  _clutter_input_device_set_associated_device (manager_mir->core_keyboard,
                                               manager_mir->core_pointer);

  if (G_OBJECT_CLASS (_clutter_device_manager_mir_parent_class)->constructed)
    G_OBJECT_CLASS (_clutter_device_manager_mir_parent_class)->constructed (gobject);
}

static void
clutter_device_manager_mir_finalize (GObject *gobject)
{
  ClutterDeviceManagerMir *manager_mir;

  manager_mir = CLUTTER_DEVICE_MANAGER_MIR (gobject);
  g_slist_free_full (manager_mir->devices, g_object_unref);

  G_OBJECT_CLASS (_clutter_device_manager_mir_parent_class)->finalize (gobject);
}

static void
_clutter_device_manager_mir_class_init (ClutterDeviceManagerMirClass *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = clutter_device_manager_mir_constructed;
  gobject_class->finalize = clutter_device_manager_mir_finalize;

  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_mir_add_device;
  manager_class->remove_device = clutter_device_manager_mir_remove_device;
  manager_class->get_devices = clutter_device_manager_mir_get_devices;
  manager_class->get_core_device = clutter_device_manager_mir_get_core_device;
  manager_class->get_device = clutter_device_manager_mir_get_device;
}

static void
_clutter_device_manager_mir_init (ClutterDeviceManagerMir *self)
{
}

ClutterDeviceManager *
_clutter_device_manager_mir_new (ClutterBackend *backend)
{
  return g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_MIR,
                       "backend", backend,
                       NULL);
}
