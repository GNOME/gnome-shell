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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_DEVICE_MANAGER_H__
#define __CLUTTER_DEVICE_MANAGER_H__

#include <clutter/clutter-input-device.h>
#include <clutter/clutter-stage.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEVICE_MANAGER             (clutter_device_manager_get_type ())
#define CLUTTER_DEVICE_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManager))
#define CLUTTER_IS_DEVICE_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEVICE_MANAGER))
#define CLUTTER_DEVICE_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManagerClass))
#define CLUTTER_IS_DEVICE_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER))
#define CLUTTER_DEVICE_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManagerClass))

typedef struct _ClutterDeviceManager            ClutterDeviceManager;
typedef struct _ClutterDeviceManagerPrivate     ClutterDeviceManagerPrivate;
typedef struct _ClutterDeviceManagerClass       ClutterDeviceManagerClass;

/**
 * ClutterDeviceManager:
 *
 * The #ClutterDeviceManager structure contains only private data
 *
 * Since: 1.2
 */
struct _ClutterDeviceManager
{
  /*< private >*/
  GObject parent_instance;

  ClutterDeviceManagerPrivate *priv;
};

/**
 * ClutterDeviceManagerClass:
 *
 * The #ClutterDeviceManagerClass structure contains only private data
 *
 * Since: 1.2
 */
struct _ClutterDeviceManagerClass
{
  /*< private >*/
  GObjectClass parent_class;

  const GSList *      (* get_devices)     (ClutterDeviceManager   *device_manager);
  ClutterInputDevice *(* get_core_device) (ClutterDeviceManager   *device_manager,
                                           ClutterInputDeviceType  device_type);
  ClutterInputDevice *(* get_device)      (ClutterDeviceManager   *device_manager,
                                           gint                    device_id);

  void                (* add_device)      (ClutterDeviceManager   *manager,
                                           ClutterInputDevice     *device);
  void                (* remove_device)   (ClutterDeviceManager   *manager,
                                           ClutterInputDevice     *device);
  void                (* select_stage_events) (ClutterDeviceManager *manager,
                                               ClutterStage       *stage);

  /* padding */
  gpointer _padding[7];
};

CLUTTER_AVAILABLE_IN_1_2
GType clutter_device_manager_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_2
ClutterDeviceManager *clutter_device_manager_get_default     (void);
CLUTTER_AVAILABLE_IN_1_2
GSList *              clutter_device_manager_list_devices    (ClutterDeviceManager   *device_manager);
CLUTTER_AVAILABLE_IN_1_2
const GSList *        clutter_device_manager_peek_devices    (ClutterDeviceManager   *device_manager);

CLUTTER_AVAILABLE_IN_1_2
ClutterInputDevice *  clutter_device_manager_get_device      (ClutterDeviceManager   *device_manager,
                                                              gint                    device_id);
CLUTTER_AVAILABLE_IN_1_2
ClutterInputDevice *  clutter_device_manager_get_core_device (ClutterDeviceManager   *device_manager,
                                                              ClutterInputDeviceType  device_type);

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_H__ */
