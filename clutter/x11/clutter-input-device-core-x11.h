/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2010, 2011  Intel Corp.
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

#ifndef __CLUTTER_INPUT_DEVICE_X11_H__
#define __CLUTTER_INPUT_DEVICE_X11_H__

#include <clutter/clutter-input-device.h>
#include <X11/Xlib.h>

#include "clutter-stage-x11.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_X11           (_clutter_input_device_x11_get_type ())
#define CLUTTER_INPUT_DEVICE_X11(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_X11, ClutterInputDeviceX11))
#define CLUTTER_IS_INPUT_DEVICE_X11(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_X11))

typedef struct _ClutterInputDeviceX11           ClutterInputDeviceX11;

GType _clutter_input_device_x11_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_X11_H__ */
