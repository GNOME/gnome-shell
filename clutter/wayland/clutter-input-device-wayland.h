/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 *  Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CLUTTER_INPUT_DEVICE_WAYLAND_H__
#define __CLUTTER_INPUT_DEVICE_WAYLAND_H__

#include <xkbcommon/xkbcommon.h>
#include <glib-object.h>
#include <clutter/clutter-event.h>

#include "clutter-device-manager-private.h"
#include "cogl/clutter-stage-cogl.h"

#define CLUTTER_TYPE_INPUT_DEVICE_WAYLAND       (_clutter_input_device_wayland_get_type ())
#define CLUTTER_INPUT_DEVICE_WAYLAND(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_WAYLAND, ClutterInputDeviceWayland))
#define CLUTTER_IS_INPUT_DEVICE_WAYLAND(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_WAYLAND))

typedef struct _ClutterInputDeviceWayland           ClutterInputDeviceWayland;

struct _ClutterInputDeviceWayland
{
  ClutterInputDevice      device;
  struct wl_seat         *input_device;
  ClutterStageCogl       *pointer_focus;
  ClutterStageCogl       *keyboard_focus;
  gdouble                 x, y;
  struct xkb_state       *xkb;
  gint                    has_pointer;
  gint                    has_keyboard;
  xkb_keycode_t           repeat_key;
  guint                   repeat_time;
  guint                   repeat_source;
  gboolean                is_initial_repeat;
  ClutterModifierType     button_modifier_state;
};

GType _clutter_input_device_wayland_get_type (void) G_GNUC_CONST;

extern const struct wl_seat_listener _clutter_seat_wayland_listener;

#endif /* __CLUTTER_INPUT_DEVICE_WAYLAND_H__ */
