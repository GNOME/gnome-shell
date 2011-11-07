/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2011  Intel Corp.
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

#ifndef __CLUTTER_DEVICE_MANAGER_GDK_H__
#define __CLUTTER_DEVICE_MANAGER_GDK_H__

#include <clutter/clutter-device-manager.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEVICE_MANAGER_GDK            (_clutter_device_manager_gdk_get_type ())
#define CLUTTER_DEVICE_MANAGER_GDK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEVICE_MANAGER_GDK, ClutterDeviceManagerGdk))
#define CLUTTER_IS_DEVICE_MANAGER_GDK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEVICE_MANAGER_GDK))
#define CLUTTER_DEVICE_MANAGER_GDK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER_GDK, ClutterDeviceManagerGdkClass))
#define CLUTTER_IS_DEVICE_MANAGER_GDK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER_GDK))
#define CLUTTER_DEVICE_MANAGER_GDK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER_GDK, ClutterDeviceManagerGdkClass))

typedef struct _ClutterDeviceManagerGdk         ClutterDeviceManagerGdk;
typedef struct _ClutterDeviceManagerGdkClass    ClutterDeviceManagerGdkClass;

struct _ClutterDeviceManagerGdk
{
  ClutterDeviceManager parent_instance;

  GdkDeviceManager *device_manager;
  GSList           *device_cache;
  GHashTable       *device_by_id;
};

struct _ClutterDeviceManagerGdkClass
{
  ClutterDeviceManagerClass parent_class;
};

GType _clutter_device_manager_gdk_get_type (void) G_GNUC_CONST;

ClutterInputDevice * _clutter_device_manager_gdk_lookup_device (ClutterDeviceManager *manager,
								GdkDevice            *device);

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_GDK_H__ */
