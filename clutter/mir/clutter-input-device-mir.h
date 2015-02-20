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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.

 * Authors:
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifndef __CLUTTER_INPUT_DEVICE_MIR_H__
#define __CLUTTER_INPUT_DEVICE_MIR_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>

#define CLUTTER_TYPE_INPUT_DEVICE_MIR       (_clutter_input_device_mir_get_type ())
#define CLUTTER_INPUT_DEVICE_MIR(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_MIR, ClutterInputDeviceMir))
#define CLUTTER_IS_INPUT_DEVICE_MIR(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_MIR))

typedef struct _ClutterInputDeviceMir ClutterInputDeviceMir;

struct _ClutterInputDeviceMir
{
  ClutterInputDevice parent_device;
};

GType _clutter_input_device_mir_get_type (void) G_GNUC_CONST;

#endif /* __CLUTTER_INPUT_DEVICE_MIR_H__ */
