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

#include <linux/input.h>
#include <math.h>

#include "clutter-event-private.h"
#include "clutter-input-device-evdev.h"
#include "clutter-input-device-tool-evdev.h"
#include "clutter-main.h"

/* Try to keep the pointer inside the stage. Hopefully no one is using
 * this backend with stages smaller than this. */
#define INITIAL_POINTER_X 16
#define INITIAL_POINTER_Y 16

#define AUTOREPEAT_VALUE 2

#define DISCRETE_SCROLL_STEP 10.0

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
      manager, seat, CLUTTER_POINTER_DEVICE,
      CLUTTER_INPUT_MODE_MASTER);
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
      manager, seat, CLUTTER_KEYBOARD_DEVICE,
      CLUTTER_INPUT_MODE_MASTER);
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

static gboolean
keyboard_repeat (gpointer data)
{
  ClutterSeatEvdev *seat = data;
  GSource *source;

  /* There might be events queued in libinput that could cancel the
     repeat timer. */
  _clutter_device_manager_evdev_dispatch (seat->manager_evdev);
  if (!seat->repeat_timer)
    return G_SOURCE_REMOVE;

  g_return_val_if_fail (seat->repeat_device != NULL, G_SOURCE_REMOVE);
  source = g_main_context_find_source_by_id (NULL, seat->repeat_timer);

  clutter_seat_evdev_notify_key (seat,
                                 seat->repeat_device,
                                 g_source_get_time (source),
                                 seat->repeat_key,
                                 AUTOREPEAT_VALUE,
                                 FALSE);

  return G_SOURCE_CONTINUE;
}

static void
queue_event (ClutterEvent *event)
{
  _clutter_event_push (event, FALSE);
}

static int
update_button_count (ClutterSeatEvdev *seat,
                     uint32_t          button,
                     uint32_t          state)
{
  if (state)
    {
      return ++seat->button_count[button];
    }
  else
    {
      /* Handle cases where we newer saw the initial pressed event. */
      if (seat->button_count[button] == 0)
        return 0;

      return --seat->button_count[button];
    }
}

void
clutter_seat_evdev_notify_key (ClutterSeatEvdev   *seat,
                               ClutterInputDevice *device,
                               uint64_t            time_us,
                               uint32_t            key,
                               uint32_t            state,
                               gboolean            update_keys)
{
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  enum xkb_state_component changed_state;

  if (state != AUTOREPEAT_VALUE)
    {
      /* Drop any repeated button press (for example from virtual devices. */
      int count = update_button_count (seat, key, state);
      if (state && count > 1)
        return;
      if (!state && count != 0)
        return;
    }

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (device);
  if (stage == NULL)
    {
      clutter_seat_evdev_clear_repeat_timer (seat);
      return;
    }

  event = _clutter_key_event_new_from_evdev (device,
					     seat->core_keyboard,
					     stage,
					     seat->xkb,
					     seat->button_state,
					     us2ms (time_us), key, state);
  _clutter_evdev_event_set_event_code (event, key);

  /* We must be careful and not pass multiple releases to xkb, otherwise it gets
     confused and locks the modifiers */
  if (state != AUTOREPEAT_VALUE)
    {
      changed_state = xkb_state_update_key (seat->xkb,
                                            event->key.hardware_keycode,
                                            state ? XKB_KEY_DOWN : XKB_KEY_UP);
    }
  else
    {
      changed_state = 0;
      clutter_event_set_flags (event, CLUTTER_EVENT_FLAG_SYNTHETIC);
    }

  queue_event (event);

  if (update_keys && (changed_state & XKB_STATE_LEDS))
    clutter_seat_evdev_sync_leds (seat);

  if (state == 0 ||             /* key release */
      !seat->repeat ||
      !xkb_keymap_key_repeats (xkb_state_get_keymap (seat->xkb),
                               event->key.hardware_keycode))
    {
      clutter_seat_evdev_clear_repeat_timer (seat);
      return;
    }

  if (state == 1)               /* key press */
    seat->repeat_count = 0;

  seat->repeat_count += 1;
  seat->repeat_key = key;

  switch (seat->repeat_count)
    {
    case 1:
    case 2:
      {
        guint32 interval;

        clutter_seat_evdev_clear_repeat_timer (seat);
        seat->repeat_device = g_object_ref (device);

        if (seat->repeat_count == 1)
          interval = seat->repeat_delay;
        else
          interval = seat->repeat_interval;

        seat->repeat_timer =
          clutter_threads_add_timeout_full (CLUTTER_PRIORITY_EVENTS,
                                            interval,
                                            keyboard_repeat,
                                            seat,
                                            NULL);
        return;
      }
    default:
      return;
    }
}

static ClutterEvent *
new_absolute_motion_event (ClutterSeatEvdev   *seat,
                           ClutterInputDevice *input_device,
                           guint64             time_us,
                           gfloat              x,
                           gfloat              y,
                           gdouble            *axes)
{
  ClutterStage *stage = _clutter_input_device_get_stage (input_device);
  ClutterEvent *event;

  event = clutter_event_new (CLUTTER_MOTION);

  if (clutter_input_device_get_device_type (input_device) != CLUTTER_TABLET_DEVICE)
    _clutter_device_manager_evdev_constrain_pointer (seat->manager_evdev,
                                                     seat->core_pointer,
                                                     time_us,
                                                     seat->pointer_x,
                                                     seat->pointer_y,
                                                     &x, &y);

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->motion.time = us2ms (time_us);
  event->motion.stage = stage;
  event->motion.device = seat->core_pointer;
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.axes = axes;
  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);

  if (clutter_input_device_get_device_type (input_device) == CLUTTER_TABLET_DEVICE)
    {
      ClutterInputDeviceEvdev *device_evdev =
        CLUTTER_INPUT_DEVICE_EVDEV (input_device);

      clutter_event_set_device_tool (event, device_evdev->last_tool);
      clutter_event_set_device (event, input_device);
    }
  else
    {
      clutter_event_set_device (event, seat->core_pointer);
    }

  _clutter_input_device_set_stage (seat->core_pointer, stage);

  if (clutter_input_device_get_device_type (input_device) != CLUTTER_TABLET_DEVICE)
    {
      seat->pointer_x = x;
      seat->pointer_y = y;
    }

  return event;
}

void
clutter_seat_evdev_notify_relative_motion (ClutterSeatEvdev   *seat,
                                           ClutterInputDevice *input_device,
                                           uint64_t            time_us,
                                           float               dx,
                                           float               dy,
                                           float               dx_unaccel,
                                           float               dy_unaccel)
{
  gfloat new_x, new_y;
  ClutterEvent *event;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  if (!_clutter_input_device_get_stage (input_device))
    return;

  _clutter_device_manager_evdev_filter_relative_motion (seat->manager_evdev,
                                                        input_device,
                                                        seat->pointer_x,
                                                        seat->pointer_y,
                                                        &dx,
                                                        &dy);

  new_x = seat->pointer_x + dx;
  new_y = seat->pointer_y + dy;
  event = new_absolute_motion_event (seat, input_device,
                                     time_us, new_x, new_y, NULL);

  _clutter_evdev_event_set_relative_motion (event,
                                            dx, dy,
                                            dx_unaccel, dy_unaccel);

  queue_event (event);
}

void clutter_seat_evdev_notify_absolute_motion (ClutterSeatEvdev   *seat,
                                                ClutterInputDevice *input_device,
                                                uint64_t            time_us,
                                                float               x,
                                                float               y,
                                                double             *axes)
{
  ClutterEvent *event;

  event = new_absolute_motion_event (seat, input_device, time_us, x, y, axes);

  queue_event (event);
}

void
clutter_seat_evdev_notify_button (ClutterSeatEvdev   *seat,
                                  ClutterInputDevice *input_device,
                                  uint64_t            time_us,
                                  uint32_t            button,
                                  uint32_t            state)
{
  ClutterInputDeviceEvdev *device_evdev = (ClutterInputDeviceEvdev *) input_device;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  gint button_nr;
  static gint maskmap[8] =
    {
      CLUTTER_BUTTON1_MASK, CLUTTER_BUTTON3_MASK, CLUTTER_BUTTON2_MASK,
      CLUTTER_BUTTON4_MASK, CLUTTER_BUTTON5_MASK, 0, 0, 0
    };
  int button_count;

  /* Drop any repeated button press (for example from virtual devices. */
  button_count = update_button_count (seat, button, state);
  if (state && button_count > 1)
    return;
  if (!state && button_count != 0)
    return;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  /* The evdev button numbers don't map sequentially to clutter button
   * numbers (the right and middle mouse buttons are in the opposite
   * order) so we'll map them directly with a switch statement */
  switch (button)
    {
    case BTN_LEFT:
    case BTN_TOUCH:
      button_nr = CLUTTER_BUTTON_PRIMARY;
      break;

    case BTN_RIGHT:
    case BTN_STYLUS:
      button_nr = CLUTTER_BUTTON_SECONDARY;
      break;

    case BTN_MIDDLE:
    case BTN_STYLUS2:
      button_nr = CLUTTER_BUTTON_MIDDLE;
      break;

    default:
      /* For compatibility reasons, all additional buttons go after the old 4-7 scroll ones */
      if (clutter_input_device_get_device_type (input_device) == CLUTTER_TABLET_DEVICE)
        button_nr = button - BTN_TOOL_PEN + 4;
      else
        button_nr = button - (BTN_LEFT - 1) + 4;
      break;
    }

  if (button_nr < 1 || button_nr > 12)
    {
      g_warning ("Unhandled button event 0x%x", button);
      return;
    }

  if (state)
    event = clutter_event_new (CLUTTER_BUTTON_PRESS);
  else
    event = clutter_event_new (CLUTTER_BUTTON_RELEASE);

  if (button_nr < G_N_ELEMENTS (maskmap))
    {
      /* Update the modifiers */
      if (state)
        seat->button_state |= maskmap[button_nr - 1];
      else
        seat->button_state &= ~maskmap[button_nr - 1];
    }

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->button.time = us2ms (time_us);
  event->button.stage = CLUTTER_STAGE (stage);
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);
  event->button.button = button_nr;

  if (clutter_input_device_get_device_type (input_device) == CLUTTER_TABLET_DEVICE)
    {
      ClutterPoint point;

      clutter_input_device_get_coords (input_device, NULL, &point);
      event->button.x = point.x;
      event->button.y = point.y;
    }
  else
    {
      event->button.x = seat->pointer_x;
      event->button.y = seat->pointer_y;
    }

  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);

  if (device_evdev->last_tool)
    {
      /* Apply the button event code as per the tool mapping */
      guint mapped_button;

      mapped_button = clutter_input_device_tool_evdev_get_button_code (device_evdev->last_tool,
                                                                       button_nr);
      if (mapped_button != 0)
        button = mapped_button;
    }

  _clutter_evdev_event_set_event_code (event, button);

  if (clutter_input_device_get_device_type (input_device) == CLUTTER_TABLET_DEVICE)
    {
      clutter_event_set_device_tool (event, device_evdev->last_tool);
      clutter_event_set_device (event, input_device);
    }
  else
    {
      clutter_event_set_device (event, seat->core_pointer);
    }

  _clutter_input_device_set_stage (seat->core_pointer, stage);

  queue_event (event);
}

static void
notify_scroll (ClutterInputDevice       *input_device,
               guint64                   time_us,
               gdouble                   dx,
               gdouble                   dy,
               ClutterScrollSource       scroll_source,
               ClutterScrollFinishFlags  flags,
               gboolean                  emulated)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  gdouble scroll_factor;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (CLUTTER_SCROLL);

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->scroll.time = us2ms (time_us);
  event->scroll.stage = CLUTTER_STAGE (stage);
  event->scroll.device = seat->core_pointer;
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);

  /* libinput pointer axis events are in pointer motion coordinate space.
   * To convert to Xi2 discrete step coordinate space, multiply the factor
   * 1/10. */
  event->scroll.direction = CLUTTER_SCROLL_SMOOTH;
  scroll_factor = 1.0 / DISCRETE_SCROLL_STEP;
  clutter_event_set_scroll_delta (event,
                                  scroll_factor * dx,
                                  scroll_factor * dy);

  event->scroll.x = seat->pointer_x;
  event->scroll.y = seat->pointer_y;
  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);
  event->scroll.scroll_source = scroll_source;
  event->scroll.finish_flags = flags;

  _clutter_event_set_pointer_emulated (event, emulated);

  queue_event (event);
}

static void
notify_discrete_scroll (ClutterInputDevice     *input_device,
                        uint64_t                time_us,
                        ClutterScrollDirection  direction,
                        ClutterScrollSource     scroll_source,
                        gboolean                emulated)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;

  if (direction == CLUTTER_SCROLL_SMOOTH)
    return;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (CLUTTER_SCROLL);

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->scroll.time = us2ms (time_us);
  event->scroll.stage = CLUTTER_STAGE (stage);
  event->scroll.device = seat->core_pointer;
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);

  event->scroll.direction = direction;

  event->scroll.x = seat->pointer_x;
  event->scroll.y = seat->pointer_y;
  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);
  event->scroll.scroll_source = scroll_source;

  _clutter_event_set_pointer_emulated (event, emulated);

  queue_event (event);
}

static void
check_notify_discrete_scroll (ClutterSeatEvdev   *seat,
                              ClutterInputDevice *device,
                              uint64_t            time_us,
                              ClutterScrollSource scroll_source)
{
  int i, n_xscrolls, n_yscrolls;

  n_xscrolls = floor (fabs (seat->accum_scroll_dx) / DISCRETE_SCROLL_STEP);
  n_yscrolls = floor (fabs (seat->accum_scroll_dy) / DISCRETE_SCROLL_STEP);

  for (i = 0; i < n_xscrolls; i++)
    {
      notify_discrete_scroll (device, time_us,
                              seat->accum_scroll_dx > 0 ?
                              CLUTTER_SCROLL_RIGHT : CLUTTER_SCROLL_LEFT,
                              scroll_source, TRUE);
    }

  for (i = 0; i < n_yscrolls; i++)
    {
      notify_discrete_scroll (device, time_us,
                              seat->accum_scroll_dy > 0 ?
                              CLUTTER_SCROLL_DOWN : CLUTTER_SCROLL_UP,
                              scroll_source, TRUE);
    }

  seat->accum_scroll_dx = fmodf (seat->accum_scroll_dx, DISCRETE_SCROLL_STEP);
  seat->accum_scroll_dy = fmodf (seat->accum_scroll_dy, DISCRETE_SCROLL_STEP);
}

void
clutter_seat_evdev_notify_scroll_continuous (ClutterSeatEvdev         *seat,
                                             ClutterInputDevice       *input_device,
                                             uint64_t                  time_us,
                                             double                    dx,
                                             double                    dy,
                                             ClutterScrollSource       scroll_source,
                                             ClutterScrollFinishFlags  finish_flags)
{
  if (finish_flags & CLUTTER_SCROLL_FINISHED_HORIZONTAL)
    seat->accum_scroll_dx = 0;
  else
    seat->accum_scroll_dx += dx;

  if (finish_flags & CLUTTER_SCROLL_FINISHED_VERTICAL)
    seat->accum_scroll_dy = 0;
  else
    seat->accum_scroll_dy += dy;

  notify_scroll (input_device, time_us, dx, dy, scroll_source,
                 finish_flags, FALSE);
  check_notify_discrete_scroll (seat, input_device, time_us, scroll_source);
}

static ClutterScrollDirection
discrete_to_direction (double discrete_dx,
                       double discrete_dy)
{
  if (discrete_dx > 0)
    return CLUTTER_SCROLL_RIGHT;
  else if (discrete_dx < 0)
    return CLUTTER_SCROLL_LEFT;
  else if (discrete_dy > 0)
    return CLUTTER_SCROLL_DOWN;
  else if (discrete_dy < 0)
    return CLUTTER_SCROLL_UP;
  else
    g_assert_not_reached ();
}

void
clutter_seat_evdev_notify_discrete_scroll (ClutterSeatEvdev    *seat,
                                           ClutterInputDevice  *input_device,
                                           uint64_t             time_us,
                                           double               discrete_dx,
                                           double               discrete_dy,
                                           ClutterScrollSource  scroll_source)
{
  notify_scroll (input_device, time_us,
                 discrete_dx * DISCRETE_SCROLL_STEP,
                 discrete_dy * DISCRETE_SCROLL_STEP,
                 scroll_source, CLUTTER_SCROLL_FINISHED_NONE,
                 TRUE);
  notify_discrete_scroll (input_device, time_us,
                          discrete_to_direction (discrete_dx, discrete_dy),
                          scroll_source, FALSE);

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
