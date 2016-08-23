/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corp.
 * Copyright (C) 2014  Jonas Ådahl
 * Copyright (C) 2016  Red Hat Inc.
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
 * Author: Damien Lespiau <damien.lespiau@intel.com>
 * Author: Jonas Ådahl <jadahl@gmail.com>
 */

#ifndef __CLUTTER_SEAT_EVDEV_H__
#define __CLUTTER_SEAT_EVDEV_H__

#include <libinput.h>
#include <linux/input.h>

#include "clutter-input-device.h"
#include "clutter-device-manager-evdev.h"
#include "clutter-xkb-utils.h"

typedef struct _ClutterTouchState ClutterTouchState;

struct _ClutterTouchState
{
  guint32 id;
  ClutterPoint coords;
};

struct _ClutterSeatEvdev
{
  struct libinput_seat *libinput_seat;
  ClutterDeviceManagerEvdev *manager_evdev;

  GSList *devices;

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;

  GHashTable *touches;

  struct xkb_state *xkb;
  xkb_led_index_t caps_lock_led;
  xkb_led_index_t num_lock_led;
  xkb_led_index_t scroll_lock_led;
  uint32_t button_state;
  int button_count[KEY_CNT];

  /* keyboard repeat */
  gboolean repeat;
  guint32 repeat_delay;
  guint32 repeat_interval;
  guint32 repeat_key;
  guint32 repeat_count;
  guint32 repeat_timer;
  ClutterInputDevice *repeat_device;

  gfloat pointer_x;
  gfloat pointer_y;

  /* Emulation of discrete scroll events out of smooth ones */
  gfloat accum_scroll_dx;
  gfloat accum_scroll_dy;
};

void clutter_seat_evdev_notify_key (ClutterSeatEvdev   *seat,
                                    ClutterInputDevice *device,
                                    uint64_t            time_us,
                                    uint32_t            key,
                                    uint32_t            state,
                                    gboolean            update_keys);

void clutter_seat_evdev_notify_relative_motion (ClutterSeatEvdev   *seat_evdev,
                                                ClutterInputDevice *input_device,
                                                uint64_t            time_us,
                                                float               dx,
                                                float               dy,
                                                float               dx_unaccel,
                                                float               dy_unaccel);

void clutter_seat_evdev_notify_absolute_motion (ClutterSeatEvdev   *seat_evdev,
                                                ClutterInputDevice *input_device,
                                                uint64_t            time_us,
                                                float               x,
                                                float               y,
                                                double             *axes);

void clutter_seat_evdev_notify_button (ClutterSeatEvdev   *seat,
                                       ClutterInputDevice *input_device,
                                       uint64_t            time_us,
                                       uint32_t            button,
                                       uint32_t            state);

void clutter_seat_evdev_notify_scroll_continuous (ClutterSeatEvdev         *seat,
                                                  ClutterInputDevice       *input_device,
                                                  uint64_t                  time_us,
                                                  double                    dx,
                                                  double                    dy,
                                                  ClutterScrollSource       source,
                                                  ClutterScrollFinishFlags  flags);

void clutter_seat_evdev_notify_discrete_scroll (ClutterSeatEvdev    *seat,
                                                ClutterInputDevice  *input_device,
                                                uint64_t             time_us,
                                                double               discrete_dx,
                                                double               discrete_dy,
                                                ClutterScrollSource  source);

void clutter_seat_evdev_set_libinput_seat (ClutterSeatEvdev     *seat,
                                           struct libinput_seat *libinput_seat);

void clutter_seat_evdev_sync_leds (ClutterSeatEvdev *seat);

ClutterTouchState * clutter_seat_evdev_add_touch (ClutterSeatEvdev *seat,
                                                  guint32           id);

void clutter_seat_evdev_remove_touch (ClutterSeatEvdev *seat,
                                      guint32           id);

ClutterTouchState * clutter_seat_evdev_get_touch (ClutterSeatEvdev *seat,
                                                  guint32           id);

void clutter_seat_evdev_set_stage (ClutterSeatEvdev *seat,
                                   ClutterStage     *stage);

void clutter_seat_evdev_clear_repeat_timer (ClutterSeatEvdev *seat);

ClutterSeatEvdev * clutter_seat_evdev_new (ClutterDeviceManagerEvdev *manager_evdev);

void clutter_seat_evdev_free (ClutterSeatEvdev *seat);

#endif /* __CLUTTER_SEAT_EVDEV_H__ */
