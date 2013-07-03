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
 * SECTION:clutter-device-manager
 * @short_description: Maintains the list of input devices
 *
 * #ClutterDeviceManager is a singleton object, owned by Clutter, which
 * maintains the list of #ClutterInputDevice<!-- -->s.
 *
 * Depending on the backend used by Clutter it is possible to use the
 * #ClutterDeviceManager::device-added and
 * #ClutterDeviceManager::device-removed to monitor addition and removal
 * of devices.
 *
 * #ClutterDeviceManager is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

struct _ClutterDeviceManagerPrivate
{
  /* back-pointer to the backend */
  ClutterBackend *backend;
};

enum
{
  PROP_0,

  PROP_BACKEND,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

enum
{
  DEVICE_ADDED,
  DEVICE_REMOVED,

  LAST_SIGNAL
};

static guint manager_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ClutterDeviceManager,
                                     clutter_device_manager,
                                     G_TYPE_OBJECT)

static void
clutter_device_manager_set_property (GObject      *gobject,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ClutterDeviceManagerPrivate *priv = CLUTTER_DEVICE_MANAGER (gobject)->priv;

  switch (prop_id)
    {
    case PROP_BACKEND:
      priv->backend = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_device_manager_get_property (GObject    *gobject,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ClutterDeviceManagerPrivate *priv = CLUTTER_DEVICE_MANAGER (gobject)->priv;

  switch (prop_id)
    {
    case PROP_BACKEND:
      g_value_set_object (value, priv->backend);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_device_manager_class_init (ClutterDeviceManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  obj_props[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         P_("Backend"),
                         P_("The ClutterBackend of the device manager"),
                         CLUTTER_TYPE_BACKEND,
                         CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  gobject_class->set_property = clutter_device_manager_set_property;
  gobject_class->get_property = clutter_device_manager_get_property;
  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     obj_props);

  /**
   * ClutterDeviceManager::device-added:
   * @manager: the #ClutterDeviceManager that emitted the signal
   * @device: the newly added #ClutterInputDevice
   *
   * The ::device-added signal is emitted each time a device has been
   * added to the #ClutterDeviceManager
   *
   * Since: 1.2
   */
  manager_signals[DEVICE_ADDED] =
    g_signal_new (I_("device-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_INPUT_DEVICE);

  /**
   * ClutterDeviceManager::device-removed:
   * @manager: the #ClutterDeviceManager that emitted the signal
   * @device: the removed #ClutterInputDevice
   *
   * The ::device-removed signal is emitted each time a device has been
   * removed from the #ClutterDeviceManager
   *
   * Since: 1.2
   */
  manager_signals[DEVICE_REMOVED] =
    g_signal_new (I_("device-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_INPUT_DEVICE);
}

static void
clutter_device_manager_init (ClutterDeviceManager *self)
{
  self->priv = clutter_device_manager_get_instance_private (self);
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
  ClutterBackend *backend = clutter_get_default_backend ();

  return backend->device_manager;
}

/**
 * clutter_device_manager_list_devices:
 * @device_manager: a #ClutterDeviceManager
 *
 * Lists all currently registered input devices
 *
 * Return value: (transfer container) (element-type Clutter.InputDevice):
 *   a newly allocated list of #ClutterInputDevice objects. Use
 *   g_slist_free() to deallocate it when done
 *
 * Since: 1.2
 */
GSList *
clutter_device_manager_list_devices (ClutterDeviceManager *device_manager)
{
  const GSList *devices;

  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  devices = clutter_device_manager_peek_devices (device_manager);

  return g_slist_copy ((GSList *) devices);
}

/**
 * clutter_device_manager_peek_devices:
 * @device_manager: a #ClutterDeviceManager
 *
 * Lists all currently registered input devices
 *
 * Return value: (transfer none) (element-type Clutter.InputDevice):
 *   a pointer to the internal list of #ClutterInputDevice objects. The
 *   returned list is owned by the #ClutterDeviceManager and should never
 *   be modified or freed
 *
 * Since: 1.2
 */
const GSList *
clutter_device_manager_peek_devices (ClutterDeviceManager *device_manager)
{
  ClutterDeviceManagerClass *manager_class;

  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  manager_class = CLUTTER_DEVICE_MANAGER_GET_CLASS (device_manager);
  return manager_class->get_devices (device_manager);
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
  ClutterDeviceManagerClass *manager_class;

  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  manager_class = CLUTTER_DEVICE_MANAGER_GET_CLASS (device_manager);
  return manager_class->get_device (device_manager, device_id);
}

/**
 * clutter_device_manager_get_core_device:
 * @device_manager: a #ClutterDeviceManager
 * @device_type: the type of the core device
 *
 * Retrieves the core #ClutterInputDevice of type @device_type
 *
 * Core devices are devices created automatically by the default
 * Clutter backend
 *
 * Return value: (transfer none): a #ClutterInputDevice or %NULL. The
 *   returned device is owned by the #ClutterDeviceManager and should
 *   not be modified or freed
 *
 * Since: 1.2
 */
ClutterInputDevice *
clutter_device_manager_get_core_device (ClutterDeviceManager   *device_manager,
                                        ClutterInputDeviceType  device_type)
{
  ClutterDeviceManagerClass *manager_class;

  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  manager_class = CLUTTER_DEVICE_MANAGER_GET_CLASS (device_manager);
  return manager_class->get_core_device (device_manager, device_type);
}

void
_clutter_device_manager_select_stage_events (ClutterDeviceManager *device_manager,
                                             ClutterStage         *stage,
                                             gint                  event_flags)
{
  ClutterDeviceManagerClass *manager_class;
  const GSList *devices, *d;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager));

  manager_class = CLUTTER_DEVICE_MANAGER_GET_CLASS (device_manager);
  devices = manager_class->get_devices (device_manager);

  for (d = devices; d != NULL; d = d->next)
    {
      ClutterInputDevice *device = d->data;

      if (device->is_enabled)
        _clutter_input_device_select_stage_events (device, stage, event_flags);
    }
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
  ClutterDeviceManagerClass *manager_class;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager));

  manager_class = CLUTTER_DEVICE_MANAGER_GET_CLASS (device_manager);
  g_assert (manager_class->add_device != NULL);

  manager_class->add_device (device_manager, device);

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
  ClutterDeviceManagerClass *manager_class;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager));

  manager_class = CLUTTER_DEVICE_MANAGER_GET_CLASS (device_manager);
  g_assert (manager_class->remove_device != NULL);

  manager_class->remove_device (device_manager, device);

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
  const GSList *d;

  for (d = clutter_device_manager_peek_devices (device_manager);
       d != NULL;
       d = d->next)
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

      /* the user disabled motion events delivery on actors for
       * the stage the device is on; we don't perform any picking
       * since the source of the events will always be set to be
       * the stage
       */
      if (!clutter_stage_get_motion_events_enabled (device->stage))
        continue;

      _clutter_input_device_update (device, NULL, TRUE);
    }
}

ClutterBackend *
_clutter_device_manager_get_backend (ClutterDeviceManager *manager)
{
  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (manager), NULL);

  return manager->priv->backend;
}
