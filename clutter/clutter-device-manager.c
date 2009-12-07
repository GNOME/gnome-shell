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

/**
 * SECTION:clutter-device-manager:
 * @short_description: Maintains the list of input devices
 *
 * #ClutterDeviceManager is a singleton object, owned by Clutter, which
 * maintains the list of #ClutterInputDevice<!-- -->s.
 *
 * #ClutterDeviceManager is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#define CLUTTER_DEVICE_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManagerClass))
#define CLUTTER_IS_DEVICE_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER))
#define CLUTTER_DEVICE_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManagerClass))

typedef struct _ClutterDeviceManagerClass       ClutterDeviceManagerClass;

struct _ClutterDeviceManagerClass
{
  GObjectClass parent_instance;
};

enum
{
  DEVICE_ADDED,
  DEVICE_REMOVED,

  LAST_SIGNAL
};

static ClutterDeviceManager *default_manager = NULL;

static guint manager_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (ClutterDeviceManager, clutter_device_manager, G_TYPE_OBJECT);

static void
clutter_device_manager_class_init (ClutterDeviceManagerClass *klass)
{
  manager_signals[DEVICE_ADDED] =
    g_signal_new (I_("device-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  manager_signals[DEVICE_REMOVED] =
    g_signal_new (I_("device-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);
}

static void
clutter_device_manager_init (ClutterDeviceManager *self)
{
}

/**
 * clutter_device_manager_get_default:
 *
 * Retrieves the device manager singleton
 *
 * Return value: (transfer none): the #ClutterDeviceManager singleton.
 *   The returned instance is owned by Clutter and it should not be
 *   modified or freed
 *
 * Since: 1.2
 */
ClutterDeviceManager *
clutter_device_manager_get_default (void)
{
  if (G_UNLIKELY (default_manager == NULL))
    default_manager = g_object_new (CLUTTER_TYPE_DEVICE_MANAGER, NULL);

  return default_manager;
}

/**
 * clutter_device_manager_list_devices:
 * @device_manager: a #ClutterDeviceManager
 *
 * Lists all currently registered input devices
 *
 * Return value: (transfer container) (element-type ClutterInputDevice):
 *   a newly allocated list of #ClutterInputDevice objects. Use
 *   g_slist_free() to deallocate it when done
 *
 * Since: 1.2
 */
GSList *
clutter_device_manager_list_devices (ClutterDeviceManager *device_manager)
{
  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  return g_slist_copy (device_manager->devices);
}

/**
 * clutter_device_manager_peek_devices:
 * @device_manager: a #ClutterDeviceManager
 *
 * Lists all currently registered input devices
 *
 * Return value: (transfer none) (element-type ClutterInputDevice):
 *   a pointer to the internal list of #ClutterInputDevice objects. The
 *   returned list is owned by the #ClutterDeviceManager and should never
 *   be modified or freed
 *
 * Since: 1.2
 */
const GSList *
clutter_device_manager_peek_devices (ClutterDeviceManager *device_manager)
{
  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  return device_manager->devices;
}

/**
 * clutter_device_manager_get_device:
 * @device_manager: a #ClutterDeviceManager
 * @device_id: the integer id of a device
 *
 * Retrieves the #ClutterInputDevice with the given @device_id
 *
 * Return value: (transfer none): a #ClutterInputDevice or %NULL. The
 *   returned device is owned by the #ClutterDeviceManager and should
 *   never be modified or freed
 *
 * Since: 1.2
 */
ClutterInputDevice *
clutter_device_manager_get_device (ClutterDeviceManager *device_manager,
                                   gint                  device_id)
{
  GSList *l;

  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  for (l = device_manager->devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (device->id == device_id)
        return device;
    }

  return NULL;
}

static gint
input_device_cmp (gconstpointer a,
                  gconstpointer b)
{
  const ClutterInputDevice *device_a = a;
  const ClutterInputDevice *device_b = b;

  if (device_a->id < device_b->id)
    return -1;

  if (device_a->id > device_b->id)
    return 1;

  return 0;
}

/*
 * _clutter_device_manager_add_device:
 * @device_manager: a #ClutterDeviceManager
 * @device: a #ClutterInputDevice
 *
 * Adds @device to the list of #ClutterInputDevice<!-- -->s maintained
 * by @device_manager
 *
 * The reference count of @device is not increased
 *
 * The #ClutterDeviceManager::device-added signal is emitted after
 * adding @device to the list
 */
void
_clutter_device_manager_add_device (ClutterDeviceManager *device_manager,
                                    ClutterInputDevice   *device)
{
  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager));

  device_manager->devices = g_slist_insert_sorted (device_manager->devices,
                                                   device,
                                                   input_device_cmp);

  g_signal_emit (device_manager, manager_signals[DEVICE_ADDED], 0, device);
}

/*
 * _clutter_device_manager_remove_device:
 * @device_manager: a #ClutterDeviceManager
 * @device: a #ClutterInputDevice
 *
 * Removes @device from the list of #ClutterInputDevice<!-- -->s
 * maintained by @device_manager
 *
 * The reference count of @device is not decreased
 *
 * The #ClutterDeviceManager::device-removed signal is emitted after
 * removing @device from the list
 */
void
_clutter_device_manager_remove_device (ClutterDeviceManager *device_manager,
                                       ClutterInputDevice   *device)
{
  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager));

  if (g_slist_find (device_manager->devices, device) == NULL)
    return;

  device_manager->devices = g_slist_remove (device_manager->devices, device);

  g_signal_emit (device_manager, manager_signals[DEVICE_REMOVED], 0, device);
}

/*
 * _clutter_device_manager_update_devices:
 * @device_manager: a #ClutterDeviceManager
 *
 * Updates every #ClutterInputDevice handled by @device_manager
 * by performing a pick paint at the coordinates of each pointer
 * device
 */
void
_clutter_device_manager_update_devices (ClutterDeviceManager *device_manager)
{
  GSList *d;

  /* the user disabled motion events delivery on actors; we
   * don't perform any picking since the source of the events
   * will always be set to be the stage
   */
  if (!clutter_get_motion_events_enabled ())
    return;

  for (d = device_manager->devices; d != NULL; d = d->next)
    {
      ClutterInputDevice *device = d->data;
      ClutterInputDeviceType device_type;

      /* we only care about pointer devices */
      device_type = clutter_input_device_get_device_type (device);
      if (device_type != CLUTTER_POINTER_DEVICE)
        continue;

      /* out of stage */
      if (device->stage == NULL)
        continue;

      _clutter_input_device_update (device);
    }
}
