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

#ifndef __CLUTTER_DEVICE_MANAGER_X11_H__
#define __CLUTTER_DEVICE_MANAGER_X11_H__

#include <clutter/clutter-device-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEVICE_MANAGER_X11            (clutter_device_manager_x11_get_type ())
#define CLUTTER_DEVICE_MANAGER_X11(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEVICE_MANAGER_X11, ClutterDeviceManagerX11))
#define CLUTTER_IS_DEVICE_MANAGER_X11(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEVICE_MANAGER_X11))
#define CLUTTER_DEVICE_MANAGER_X11_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER_X11, ClutterDeviceManagerX11Class))
#define CLUTTER_IS_DEVICE_MANAGER_X11_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER_X11))
#define CLUTTER_DEVICE_MANAGER_X11_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER_X11, ClutterDeviceManagerX11Class))

typedef struct _ClutterDeviceManagerX11         ClutterDeviceManagerX11;
typedef struct _ClutterDeviceManagerX11Class    ClutterDeviceManagerX11Class;

struct _ClutterDeviceManagerX11
{
  ClutterDeviceManager parent_instance;

  /* the list of transient devices */
  GSList *devices;

  /* the list of all devices, transient and core; this can be
   * NULL-ified when adding or removing devices
   */
  GSList *all_devices;

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;

  guint use_xinput_1 : 1;
};

struct _ClutterDeviceManagerX11Class
{
  ClutterDeviceManagerClass parent_class;
};

GType clutter_device_manager_x11_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_DEVICE_MANAGER_X11_H__ */
