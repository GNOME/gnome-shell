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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __CLUTTER_DEVICE_MANAGER_WIN32_H__
#define __CLUTTER_DEVICE_MANAGER_WIN32_H__

#include <clutter/clutter-device-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEVICE_MANAGER_WIN32            (clutter_device_manager_win32_get_type ())
#define CLUTTER_DEVICE_MANAGER_WIN32(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEVICE_MANAGER_WIN32, ClutterDeviceManagerWin32))
#define CLUTTER_IS_DEVICE_MANAGER_WIN32(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEVICE_MANAGER_WIN32))
#define CLUTTER_DEVICE_MANAGER_WIN32_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER_WIN32, ClutterDeviceManagerWin32Class))
#define CLUTTER_IS_DEVICE_MANAGER_WIN32_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER_WIN32))
#define CLUTTER_DEVICE_MANAGER_WIN32_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER_WIN32, ClutterDeviceManagerWin32Class))

typedef struct _ClutterDeviceManagerWin32         ClutterDeviceManagerWin32;
typedef struct _ClutterDeviceManagerWin32Class    ClutterDeviceManagerWin32Class;

struct _ClutterDeviceManagerWin32
{
  ClutterDeviceManager parent_instance;

  GSList *devices;

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;
};

struct _ClutterDeviceManagerWin32Class
{
  ClutterDeviceManagerClass parent_class;
};

GType clutter_device_manager_win32_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_WIN32_H__ */
