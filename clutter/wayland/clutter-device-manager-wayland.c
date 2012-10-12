/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corp.
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
 *  Emmanuele Bassi <ebassi@linux.intel.com>
 *  Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-input-device-wayland.h"
#include "clutter-device-manager-wayland.h"

#include "clutter-backend.h"
#include "wayland/clutter-backend-wayland.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-private.h"

#include <wayland-client.h>
#include <wayland-client-protocol.h>

enum
{
  PROP_0
};

G_DEFINE_TYPE (ClutterDeviceManagerWayland,
               _clutter_device_manager_wayland,
               CLUTTER_TYPE_DEVICE_MANAGER);

static void
clutter_device_manager_wayland_add_device (ClutterDeviceManager *manager,
                                           ClutterInputDevice   *device)
{
  ClutterDeviceManagerWayland *manager_wayland =
    CLUTTER_DEVICE_MANAGER_WAYLAND (manager);
  ClutterInputDeviceType device_type;
  gboolean is_pointer, is_keyboard;

  device_type = clutter_input_device_get_device_type (device);
  is_pointer  = (device_type == CLUTTER_POINTER_DEVICE)  ? TRUE : FALSE;
  is_keyboard = (device_type == CLUTTER_KEYBOARD_DEVICE) ? TRUE : FALSE;

  manager_wayland->devices = g_slist_prepend (manager_wayland->devices, device);

  if (is_pointer && manager_wayland->core_pointer == NULL)
    manager_wayland->core_pointer = device;

  if (is_keyboard && manager_wayland->core_keyboard == NULL)
    manager_wayland->core_keyboard = device;
}

static void
clutter_device_manager_wayland_remove_device (ClutterDeviceManager *manager,
                                              ClutterInputDevice   *device)
{
  ClutterDeviceManagerWayland *manager_wayland = CLUTTER_DEVICE_MANAGER_WAYLAND (manager);

  manager_wayland->devices = g_slist_remove (manager_wayland->devices, device);
}

static const GSList *
clutter_device_manager_wayland_get_devices (ClutterDeviceManager *manager)
{
  return CLUTTER_DEVICE_MANAGER_WAYLAND (manager)->devices;
}

static ClutterInputDevice *
clutter_device_manager_wayland_get_core_device (ClutterDeviceManager *manager,
                                                ClutterInputDeviceType type)
{
  ClutterDeviceManagerWayland *manager_wayland;

  manager_wayland = CLUTTER_DEVICE_MANAGER_WAYLAND (manager);

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return manager_wayland->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return manager_wayland->core_keyboard;

    case CLUTTER_EXTENSION_DEVICE:
    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_wayland_get_device (ClutterDeviceManager *manager,
                                           gint                  id)
{
  ClutterDeviceManagerWayland *manager_wayland =
    CLUTTER_DEVICE_MANAGER_WAYLAND (manager);
  GSList *l;

  for (l = manager_wayland->devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id)
        return device;
    }

  return NULL;
}

static void
_clutter_device_manager_wayland_class_init (ClutterDeviceManagerWaylandClass *klass)
{
  ClutterDeviceManagerClass *manager_class;

  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_wayland_add_device;
  manager_class->remove_device = clutter_device_manager_wayland_remove_device;
  manager_class->get_devices = clutter_device_manager_wayland_get_devices;
  manager_class->get_core_device = clutter_device_manager_wayland_get_core_device;
  manager_class->get_device = clutter_device_manager_wayland_get_device;
}

static void
_clutter_device_manager_wayland_init (ClutterDeviceManagerWayland *self)
{
}

void
_clutter_device_manager_wayland_add_input_group (ClutterDeviceManager *manager,
                                                 uint32_t id)
{
  ClutterBackend *backend = _clutter_device_manager_get_backend (manager);
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  ClutterInputDeviceWayland *device;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_WAYLAND,
                         "id", id,
                         "device-type", CLUTTER_POINTER_DEVICE,
                         "name", "wayland device",
                         "enabled", TRUE,
                         NULL);

  device->input_device =
    wl_registry_bind (backend_wayland->wayland_registry, id,
                      &wl_seat_interface, 1);
  wl_seat_add_listener (device->input_device,
                        &_clutter_seat_wayland_listener,
                        device);
  wl_seat_set_user_data (device->input_device, device);

  _clutter_device_manager_add_device (manager, CLUTTER_INPUT_DEVICE (device));
}

ClutterDeviceManager *
_clutter_device_manager_wayland_new (ClutterBackend *backend)
{
  return g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_WAYLAND,
                       "backend", backend,
                       NULL);
}

void
_clutter_events_wayland_init (ClutterBackend *backend)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);

  /* XXX: We actually create the wayland device manager in the backend
   * post_parse vfunc because that's the point where we connect to a compositor
   * and that's also the point where we will be notified of input devices so we
   * need the device-manager to exist early on.
   *
   * To be consistent with other clutter backends though we only associate the
   * device manager with the backend when _clutter_events_wayland_init() is
   * called in _clutter_backend_init_events(). This should still allow the
   * runtime selection of an alternative input backend if desired and in that
   * case the wayland device manager will be benign.
   *
   * FIXME: At some point we could perhaps collapse the
   * _clutter_backend_post_parse(), and _clutter_backend_init_events()
   * functions into one called something like _clutter_backend_init() which
   * would allow the real backend to manage the precise order of
   * initialization.
   */

  backend->device_manager = g_object_ref (backend_wayland->device_manager);
}

void
_clutter_events_wayland_uninit (ClutterBackend *backend)
{
  g_object_unref (backend->device_manager);
  backend->device_manager = NULL;
}
