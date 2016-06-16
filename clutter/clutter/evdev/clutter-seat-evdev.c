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

#include "clutter-build-config.h"

#include "clutter-seat-evdev.h"

#include "clutter-input-device-evdev.h"

/* Try to keep the pointer inside the stage. Hopefully no one is using
 * this backend with stages smaller than this. */
#define INITIAL_POINTER_X 16
#define INITIAL_POINTER_Y 16

void
clutter_seat_evdev_set_libinput_seat (ClutterSeatEvdev     *seat,
                                      struct libinput_seat *libinput_seat)
{
  g_assert (seat->libinput_seat == NULL);

  libinput_seat_ref (libinput_seat);
  libinput_seat_set_user_data (libinput_seat, seat);
  seat->libinput_seat = libinput_seat;
}

void
clutter_seat_evdev_sync_leds (ClutterSeatEvdev *seat)
{
  GSList *iter;
  ClutterInputDeviceEvdev *device_evdev;
  int caps_lock, num_lock, scroll_lock;
  enum libinput_led leds = 0;

  caps_lock = xkb_state_led_index_is_active (seat->xkb, seat->caps_lock_led);
  num_lock = xkb_state_led_index_is_active (seat->xkb, seat->num_lock_led);
  scroll_lock = xkb_state_led_index_is_active (seat->xkb, seat->scroll_lock_led);

  if (caps_lock)
    leds |= LIBINPUT_LED_CAPS_LOCK;
  if (num_lock)
    leds |= LIBINPUT_LED_NUM_LOCK;
  if (scroll_lock)
    leds |= LIBINPUT_LED_SCROLL_LOCK;

  for (iter = seat->devices; iter; iter = iter->next)
    {
      device_evdev = iter->data;
      _clutter_input_device_evdev_update_leds (device_evdev, leds);
    }
}

static void
clutter_touch_state_free (ClutterTouchState *touch_state)
{
  g_slice_free (ClutterTouchState, touch_state);
}

ClutterTouchState *
clutter_seat_evdev_add_touch (ClutterSeatEvdev *seat,
                              guint32           id)
{
  ClutterTouchState *touch;

  touch = g_slice_new0 (ClutterTouchState);
  touch->id = id;

  g_hash_table_insert (seat->touches, GUINT_TO_POINTER (id), touch);

  return touch;
}

void
clutter_seat_evdev_remove_touch (ClutterSeatEvdev *seat,
                                 guint32           id)
{
  g_hash_table_remove (seat->touches, GUINT_TO_POINTER (id));
}

ClutterTouchState *
clutter_seat_evdev_get_touch (ClutterSeatEvdev *seat,
                              guint32           id)
{
  return g_hash_table_lookup (seat->touches, GUINT_TO_POINTER (id));
}

ClutterSeatEvdev *
clutter_seat_evdev_new (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (manager_evdev);
  ClutterSeatEvdev *seat;
  ClutterInputDevice *device;
  ClutterStage *stage;
  struct xkb_keymap *keymap;

  seat = g_new0 (ClutterSeatEvdev, 1);
  if (!seat)
    return NULL;

  seat->manager_evdev = manager_evdev;
  device = _clutter_input_device_evdev_new_virtual (
    manager, seat, CLUTTER_POINTER_DEVICE);
  stage = _clutter_device_manager_evdev_get_stage (manager_evdev);
  _clutter_input_device_set_stage (device, stage);
  seat->pointer_x = INITIAL_POINTER_X;
  seat->pointer_y = INITIAL_POINTER_Y;
  _clutter_input_device_set_coords (device, NULL,
                                    seat->pointer_x, seat->pointer_y,
                                    NULL);
  _clutter_device_manager_add_device (manager, device);
  seat->core_pointer = device;

  device = _clutter_input_device_evdev_new_virtual (
    manager, seat, CLUTTER_KEYBOARD_DEVICE);
  _clutter_input_device_set_stage (device, stage);
  _clutter_device_manager_add_device (manager, device);
  seat->core_keyboard = device;

  seat->touches = g_hash_table_new_full (NULL, NULL, NULL,
                                         (GDestroyNotify) clutter_touch_state_free);

  seat->repeat = TRUE;
  seat->repeat_delay = 250;     /* ms */
  seat->repeat_interval = 33;   /* ms */

  keymap = _clutter_device_manager_evdev_get_keymap (manager_evdev);
  if (keymap)
    {
      seat->xkb = xkb_state_new (keymap);

      seat->caps_lock_led =
        xkb_keymap_led_get_index (keymap, XKB_LED_NAME_CAPS);
      seat->num_lock_led =
        xkb_keymap_led_get_index (keymap, XKB_LED_NAME_NUM);
      seat->scroll_lock_led =
        xkb_keymap_led_get_index (keymap, XKB_LED_NAME_SCROLL);
    }

  return seat;
}

void
clutter_seat_evdev_clear_repeat_timer (ClutterSeatEvdev *seat)
{
  if (seat->repeat_timer)
    {
      g_source_remove (seat->repeat_timer);
      seat->repeat_timer = 0;
      g_clear_object (&seat->repeat_device);
    }
}

void
clutter_seat_evdev_free (ClutterSeatEvdev *seat)
{
  GSList *iter;

  for (iter = seat->devices; iter; iter = g_slist_next (iter))
    {
      ClutterInputDevice *device = iter->data;

      g_object_unref (device);
    }
  g_slist_free (seat->devices);
  g_hash_table_unref (seat->touches);

  xkb_state_unref (seat->xkb);

  clutter_seat_evdev_clear_repeat_timer (seat);

  if (seat->libinput_seat)
    libinput_seat_unref (seat->libinput_seat);

  g_free (seat);
}

void
clutter_seat_evdev_set_stage (ClutterSeatEvdev *seat,
                              ClutterStage     *stage)
{
  GSList *l;

  _clutter_input_device_set_stage (seat->core_pointer, stage);
  _clutter_input_device_set_stage (seat->core_keyboard, stage);

  for (l = seat->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      _clutter_input_device_set_stage (device, stage);
    }
}
