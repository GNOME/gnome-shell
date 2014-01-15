/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corp.
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
 *
 */

#ifndef __CLUTTER_EVDEV_H__
#define __CLUTTER_EVDEV_H__

#include <glib.h>
#include <glib-object.h>
#include <xkbcommon/xkbcommon.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#if !defined(CLUTTER_ENABLE_COMPOSITOR_API) && !defined(CLUTTER_COMPILATION)
#error "You need to define CLUTTER_ENABLE_COMPOSITOR_API before including clutter-evdev.h"
#endif

/**
 * ClutterOpenDeviceCallback:
 * @path: the device path
 * @flags: flags to be passed to open
 *
 * This callback will be called when Clutter needs to access an input
 * device. It should return an open file descriptor for the file at @path,
 * or -1 if opening failed.
 */
typedef int (*ClutterOpenDeviceCallback) (const char  *path,
					  int          flags,
					  gpointer     user_data,
					  GError     **error);

void  clutter_evdev_set_open_callback (ClutterOpenDeviceCallback callback,
				       gpointer                  user_data);

void  clutter_evdev_release_devices (void);
void  clutter_evdev_reclaim_devices (void);

/**
 * ClutterPointerConstrainCallback:
 * @device: the core pointer device
 * @time: the event time in milliseconds
 * @x: (inout): the new X coordinate
 * @y: (inout): the new Y coordinate
 * @user_data:
 *
 * This callback will be called for all pointer motion events, and should
 * update (@x, @y) to constrain the pointer position appropriately.
 * The subsequent motion event will use the updated values as the new coordinates.
 * Note that the coordinates are not clamped to the stage size, and the callback
 * must make sure that this happens before it returns.
 * Also note that the event will be emitted even if the pointer is constrained
 * to be in the same position.
 */
typedef void (*ClutterPointerConstrainCallback) (ClutterInputDevice *device,
						 guint32             time,
						 float              *x,
						 float              *y,
						 gpointer            user_data);

void  clutter_evdev_set_pointer_constrain_callback (ClutterDeviceManager            *evdev,
						    ClutterPointerConstrainCallback  callback,
						    gpointer                         user_data,
						    GDestroyNotify                   user_data_notify);

void               clutter_evdev_set_keyboard_map   (ClutterDeviceManager *evdev,
						     struct xkb_keymap    *keymap);

struct xkb_keymap * clutter_evdev_get_keyboard_map (ClutterDeviceManager *evdev);

void clutter_evdev_set_keyboard_repeat (ClutterDeviceManager *evdev,
                                        gboolean              repeat,
                                        guint32               delay,
                                        guint32               interval);

G_END_DECLS

#endif /* __CLUTTER_EVDEV_H__ */
