/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corp.
 * Copyright (C) 2014  Jonas Ådahl
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

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include <math.h>
#include <float.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <libinput.h>

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-input-device-evdev.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-manager.h"
#include "clutter-xkb-utils.h"
#include "clutter-backend-private.h"
#include "clutter-evdev.h"
#include "clutter-stage-private.h"
#include "clutter-input-device-tool-evdev.h"

#include "clutter-device-manager-evdev.h"

#define DISCRETE_SCROLL_STEP 10.0

#define AUTOREPEAT_VALUE 2

/* Try to keep the pointer inside the stage. Hopefully no one is using
 * this backend with stages smaller than this. */
#define INITIAL_POINTER_X 16
#define INITIAL_POINTER_Y 16

/*
 * Clutter makes the assumption that two core devices have ID's 2 and 3 (core
 * pointer and core keyboard).
 *
 * Since the two first devices that will ever be created will be the virtual
 * pointer and virtual keyboard of the first seat, we fulfill the made
 * assumptions by having the first device having ID 2 and following 3.
 */
#define INITIAL_DEVICE_ID 2

typedef struct _ClutterTouchState ClutterTouchState;
typedef struct _ClutterEventFilter ClutterEventFilter;

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

struct _ClutterEventFilter
{
  ClutterEvdevFilterFunc func;
  gpointer data;
  GDestroyNotify destroy_notify;
};

typedef struct _ClutterEventSource  ClutterEventSource;

struct _ClutterDeviceManagerEvdevPrivate
{
  struct libinput *libinput;

  ClutterStage *stage;
  gboolean released;

  ClutterEventSource *event_source;

  GSList *devices;
  GSList *seats;

  ClutterSeatEvdev *main_seat;
  struct xkb_keymap *keymap;

  ClutterPointerConstrainCallback constrain_callback;
  gpointer                        constrain_data;
  GDestroyNotify                  constrain_data_notify;

  ClutterStageManager *stage_manager;
  guint stage_added_handler;
  guint stage_removed_handler;

  GSList *event_filters;

  gint device_id_next;
  GList *free_device_ids;
};

static void clutter_device_manager_evdev_event_extender_init (ClutterEventExtenderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterDeviceManagerEvdev,
                         clutter_device_manager_evdev,
                         CLUTTER_TYPE_DEVICE_MANAGER,
                         G_ADD_PRIVATE (ClutterDeviceManagerEvdev)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_EXTENDER,
                                                clutter_device_manager_evdev_event_extender_init))

static ClutterOpenDeviceCallback  device_open_callback;
static ClutterCloseDeviceCallback device_close_callback;
static gpointer                   device_callback_data;

#ifdef CLUTTER_ENABLE_DEBUG
static const char *device_type_str[] = {
  "pointer",            /* CLUTTER_POINTER_DEVICE */
  "keyboard",           /* CLUTTER_KEYBOARD_DEVICE */
  "extension",          /* CLUTTER_EXTENSION_DEVICE */
  "joystick",           /* CLUTTER_JOYSTICK_DEVICE */
  "tablet",             /* CLUTTER_TABLET_DEVICE */
  "touchpad",           /* CLUTTER_TOUCHPAD_DEVICE */
  "touchscreen",        /* CLUTTER_TOUCHSCREEN_DEVICE */
  "pen",                /* CLUTTER_PEN_DEVICE */
  "eraser",             /* CLUTTER_ERASER_DEVICE */
  "cursor",             /* CLUTTER_CURSOR_DEVICE */
};
#endif /* CLUTTER_ENABLE_DEBUG */

/*
 * ClutterEventSource management
 *
 * The device manager is responsible for managing the GSource when devices
 * appear and disappear from the system.
 */

static const char *option_xkb_layout = "us";
static const char *option_xkb_variant = "";
static const char *option_xkb_options = "";

static inline guint64
us (guint64 us)
{
  return us;
}

static inline guint64
ms2us (guint64 ms)
{
  return us (ms * 1000);
}

static inline guint32
us2ms (guint64 us)
{
  return (guint32) (us / 1000);
}

static void
clutter_device_manager_evdev_copy_event_data (ClutterEventExtender *event_extender,
                                              const ClutterEvent   *src,
                                              ClutterEvent         *dest)
{
  ClutterEventEvdev *event_evdev;

  event_evdev = _clutter_event_get_platform_data (src);
  if (event_evdev != NULL)
    _clutter_event_set_platform_data (dest, _clutter_event_evdev_copy (event_evdev));
}

static void
clutter_device_manager_evdev_free_event_data (ClutterEventExtender *event_extender,
                                              ClutterEvent         *event)
{
  ClutterEventEvdev *event_evdev;

  event_evdev = _clutter_event_get_platform_data (event);
  if (event_evdev != NULL)
    _clutter_event_evdev_free (event_evdev);
}

static void
clutter_device_manager_evdev_event_extender_init (ClutterEventExtenderInterface *iface)
{
  iface->copy_event_data = clutter_device_manager_evdev_copy_event_data;
  iface->free_event_data = clutter_device_manager_evdev_free_event_data;
}

/*
 * ClutterEventSource for reading input devices
 */

struct _ClutterEventSource
{
  GSource source;

  ClutterDeviceManagerEvdev *manager_evdev;
  GPollFD event_poll_fd;
};

static void
process_events (ClutterDeviceManagerEvdev *manager_evdev);

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  gboolean retval;

  _clutter_threads_acquire_lock ();

  *timeout = -1;
  retval = clutter_events_pending ();

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  retval = ((event_source->event_poll_fd.revents & G_IO_IN) ||
            clutter_events_pending ());

  _clutter_threads_release_lock ();

  return retval;
}

static void
queue_event (ClutterEvent *event)
{
  _clutter_event_push (event, FALSE);
}

static void
clear_repeat_timer (ClutterSeatEvdev *seat)
{
  if (seat->repeat_timer)
    {
      g_source_remove (seat->repeat_timer);
      seat->repeat_timer = 0;
      g_clear_object (&seat->repeat_device);
    }
}

static gboolean
keyboard_repeat (gpointer data);

static void
clutter_seat_evdev_sync_leds (ClutterSeatEvdev *seat);

static void
notify_key_device (ClutterInputDevice *input_device,
		   guint64             time_us,
		   guint32             key,
		   guint32             state,
		   gboolean            update_keys)
{
  ClutterInputDeviceEvdev *device_evdev =
    CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  ClutterSeatEvdev *seat = _clutter_input_device_evdev_get_seat (device_evdev);
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  enum xkb_state_component changed_state;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    {
      clear_repeat_timer (seat);
      return;
    }

  event = _clutter_key_event_new_from_evdev (input_device,
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
      !xkb_keymap_key_repeats (xkb_state_get_keymap (seat->xkb), event->key.hardware_keycode))
    {
      clear_repeat_timer (seat);
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

        clear_repeat_timer (seat);
        seat->repeat_device = g_object_ref (input_device);

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

static gboolean
keyboard_repeat (gpointer data)
{
  ClutterSeatEvdev *seat = data;
  GSource *source;
  guint32 time_ms;

  g_return_val_if_fail (seat->repeat_device != NULL, G_SOURCE_REMOVE);
  source = g_main_context_find_source_by_id (NULL, seat->repeat_timer);
  time_ms = g_source_get_time (source) / 1000;

  notify_key_device (seat->repeat_device,
                     ms2us (time_ms),
                     seat->repeat_key,
                     AUTOREPEAT_VALUE,
                     FALSE);

  return G_SOURCE_CONTINUE;
}

static ClutterEvent *
new_absolute_motion_event (ClutterInputDevice *input_device,
                           guint64             time_us,
                           gfloat              x,
                           gfloat              y,
                           gdouble            *axes)
{
  gfloat stage_width, stage_height;
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;

  stage = _clutter_input_device_get_stage (input_device);
  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (input_device->device_manager);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
  stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

  event = clutter_event_new (CLUTTER_MOTION);

  if (manager_evdev->priv->constrain_callback &&
      clutter_input_device_get_device_type (input_device) != CLUTTER_TABLET_DEVICE)
    {
      manager_evdev->priv->constrain_callback (seat->core_pointer,
                                               us2ms (time_us),
                                               seat->pointer_x,
                                               seat->pointer_y,
                                               &x, &y,
					       manager_evdev->priv->constrain_data);
    }
  else
    {
      x = CLAMP (x, 0.f, stage_width - 1);
      y = CLAMP (y, 0.f, stage_height - 1);
    }

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->motion.time = us2ms (time_us);
  event->motion.stage = stage;
  event->motion.device = seat->core_pointer;
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.axes = axes;
  clutter_event_set_source_device (event, input_device);

  if (clutter_input_device_get_device_type (input_device) == CLUTTER_TABLET_DEVICE)
    {
      clutter_event_set_device_tool (event, device_evdev->last_tool);
      clutter_event_set_device (event, input_device);
    }
  else
    clutter_event_set_device (event, seat->core_pointer);

  _clutter_input_device_set_stage (seat->core_pointer, stage);

  if (clutter_input_device_get_device_type (input_device) != CLUTTER_TABLET_DEVICE)
    {
      seat->pointer_x = x;
      seat->pointer_y = y;
    }

  return event;
}

static void
notify_absolute_motion (ClutterInputDevice *input_device,
                        guint64             time_us,
                        gfloat              x,
                        gfloat              y,
                        gdouble            *axes)
{
  ClutterEvent *event;

  event = new_absolute_motion_event (input_device, time_us, x, y, axes);

  queue_event (event);
}

static void
notify_relative_motion (ClutterInputDevice *input_device,
                        struct libinput_event_pointer *pointer_event)
{
  guint64 time_us;
  double dx;
  double dy;
  double dx_unaccel;
  double dy_unaccel;
  gfloat new_x, new_y;
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterEvent *event;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  if (!_clutter_input_device_get_stage (input_device))
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  dx = libinput_event_pointer_get_dx (pointer_event);
  dy = libinput_event_pointer_get_dy (pointer_event);
  new_x = seat->pointer_x + dx;
  new_y = seat->pointer_y + dy;

  time_us = libinput_event_pointer_get_time_usec (pointer_event);
  event = new_absolute_motion_event (input_device, time_us, new_x, new_y, NULL);

  dx_unaccel = libinput_event_pointer_get_dx_unaccelerated (pointer_event);
  dy_unaccel = libinput_event_pointer_get_dy_unaccelerated (pointer_event);
  _clutter_evdev_event_set_relative_motion (event,
                                            dx, dy,
                                            dx_unaccel, dy_unaccel);

  queue_event (event);
}

static ClutterScrollDirection
discrete_to_direction (gdouble discrete_x,
                       gdouble discrete_y)
{
  if (discrete_x > 0)
    return CLUTTER_SCROLL_RIGHT;
  else if (discrete_x < 0)
    return CLUTTER_SCROLL_LEFT;
  else if (discrete_y > 0)
    return CLUTTER_SCROLL_DOWN;
  else if (discrete_y < 0)
    return CLUTTER_SCROLL_UP;
  else
    return CLUTTER_SCROLL_SMOOTH;
}

static void
notify_discrete_scroll (ClutterInputDevice     *input_device,
                        guint64                 time_us,
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
notify_scroll (ClutterInputDevice       *input_device,
               guint64                   time_us,
               gdouble                   dx,
               gdouble                   dy,
               ClutterScrollSource       source,
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
  event->scroll.scroll_source = source;
  event->scroll.finish_flags = flags;

  _clutter_event_set_pointer_emulated (event, emulated);

  queue_event (event);
}

static void
notify_button (ClutterInputDevice *input_device,
               guint64             time_us,
               guint32             button,
               guint32             state)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  gint button_nr;
  static gint maskmap[8] =
    {
      CLUTTER_BUTTON1_MASK, CLUTTER_BUTTON3_MASK, CLUTTER_BUTTON2_MASK,
      CLUTTER_BUTTON4_MASK, CLUTTER_BUTTON5_MASK, 0, 0, 0
    };

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

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

  clutter_event_set_source_device (event, input_device);

  _clutter_evdev_event_set_event_code (event, button);

  if (clutter_input_device_get_device_type (input_device) == CLUTTER_TABLET_DEVICE)
    {
      clutter_event_set_device_tool (event, device_evdev->last_tool);
      clutter_event_set_device (event, input_device);
    }
  else
    clutter_event_set_device (event, seat->core_pointer);

  _clutter_input_device_set_stage (seat->core_pointer, stage);

  queue_event (event);
}

static void
notify_touch_event (ClutterInputDevice *input_device,
		    ClutterEventType    evtype,
		    guint64             time_us,
		    gint32              slot,
		    gdouble             x,
		    gdouble             y)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (evtype);

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->touch.time = us2ms (time_us);
  event->touch.stage = CLUTTER_STAGE (stage);
  event->touch.device = seat->core_pointer;
  event->touch.x = x;
  event->touch.y = y;
  /* "NULL" sequences are special cased in clutter */
  event->touch.sequence = GINT_TO_POINTER (slot + 1);
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);

  if (evtype == CLUTTER_TOUCH_BEGIN ||
      evtype == CLUTTER_TOUCH_UPDATE)
    event->touch.modifier_state |= CLUTTER_BUTTON1_MASK;

  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);

  queue_event (event);
}

static void
notify_pinch_gesture_event (ClutterInputDevice          *input_device,
                            ClutterTouchpadGesturePhase  phase,
                            guint64                      time_us,
                            gdouble                      dx,
                            gdouble                      dy,
                            gdouble                      angle_delta,
                            gdouble                      scale)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  ClutterPoint pos;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (CLUTTER_TOUCHPAD_PINCH);

  clutter_input_device_get_coords (seat->core_pointer, NULL, &pos);

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->touchpad_pinch.phase = phase;
  event->touchpad_pinch.time = us2ms (time_us);
  event->touchpad_pinch.stage = CLUTTER_STAGE (stage);
  event->touchpad_pinch.x = pos.x;
  event->touchpad_pinch.y = pos.y;
  event->touchpad_pinch.dx = dx;
  event->touchpad_pinch.dy = dy;
  event->touchpad_pinch.angle_delta = angle_delta;
  event->touchpad_pinch.scale = scale;

  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);

  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);

  queue_event (event);
}

static void
notify_swipe_gesture_event (ClutterInputDevice          *input_device,
                            ClutterTouchpadGesturePhase  phase,
                            guint64                      time_us,
                            guint                        n_fingers,
                            gdouble                      dx,
                            gdouble                      dy)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  ClutterPoint pos;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (CLUTTER_TOUCHPAD_SWIPE);

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->touchpad_swipe.phase = phase;
  event->touchpad_swipe.time = us2ms (time_us);
  event->touchpad_swipe.stage = CLUTTER_STAGE (stage);

  clutter_input_device_get_coords (seat->core_pointer, NULL, &pos);
  event->touchpad_swipe.x = pos.x;
  event->touchpad_swipe.y = pos.y;
  event->touchpad_swipe.dx = dx;
  event->touchpad_swipe.dy = dy;
  event->touchpad_swipe.n_fingers = n_fingers;

  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);

  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);

  queue_event (event);
}

static void
notify_proximity (ClutterInputDevice *input_device,
                  guint64             time_us,
                  gboolean            in)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  if (in)
    event = clutter_event_new (CLUTTER_PROXIMITY_IN);
  else
    event = clutter_event_new (CLUTTER_PROXIMITY_OUT);

  _clutter_evdev_event_set_time_usec (event, time_us);

  event->proximity.time = us2ms (time_us);
  event->proximity.stage = CLUTTER_STAGE (stage);
  event->proximity.device = seat->core_pointer;
  clutter_event_set_device_tool (event, device_evdev->last_tool);
  clutter_event_set_device (event, seat->core_pointer);
  clutter_event_set_source_device (event, input_device);

  _clutter_input_device_set_stage (seat->core_pointer, stage);

  queue_event (event);
}

static void
dispatch_libinput (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;

  libinput_dispatch (priv->libinput);
  process_events (manager_evdev);
}

static gboolean
clutter_event_dispatch (GSource     *g_source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterEventSource *source = (ClutterEventSource *) g_source;
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterEvent *event;

  _clutter_threads_acquire_lock ();

  manager_evdev = source->manager_evdev;

  /* Don't queue more events if we haven't finished handling the previous batch
   */
  if (clutter_events_pending ())
    goto queue_event;

  dispatch_libinput (manager_evdev);

 queue_event:
  event = clutter_event_get ();

  if (event)
    {
      ClutterModifierType event_state;
      ClutterInputDevice *input_device =
        clutter_event_get_source_device (event);
      ClutterInputDeviceEvdev *device_evdev =
        CLUTTER_INPUT_DEVICE_EVDEV (input_device);
      ClutterSeatEvdev *seat =
        _clutter_input_device_evdev_get_seat (device_evdev);

      /* Drop events if we don't have any stage to forward them to */
      if (!_clutter_input_device_get_stage (input_device))
        goto out;

      /* forward the event into clutter for emission etc. */
      _clutter_stage_queue_event (event->any.stage, event, FALSE);

      /* update the device states *after* the event */
      event_state = seat->button_state |
        xkb_state_serialize_mods (seat->xkb, XKB_STATE_MODS_EFFECTIVE);
      _clutter_input_device_set_state (seat->core_pointer, event_state);
      _clutter_input_device_set_state (seat->core_keyboard, event_state);
    }

out:
  _clutter_threads_release_lock ();

  return TRUE;
}
static GSourceFuncs event_funcs = {
  clutter_event_prepare,
  clutter_event_check,
  clutter_event_dispatch,
  NULL
};

static ClutterEventSource *
clutter_event_source_new (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  GSource *source;
  ClutterEventSource *event_source;
  gint fd;

  source = g_source_new (&event_funcs, sizeof (ClutterEventSource));
  event_source = (ClutterEventSource *) source;

  /* setup the source */
  event_source->manager_evdev = manager_evdev;

  fd = libinput_get_fd (priv->libinput);
  event_source->event_poll_fd.fd = fd;
  event_source->event_poll_fd.events = G_IO_IN;

  /* and finally configure and attach the GSource */
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  return event_source;
}

static void
clutter_event_source_free (ClutterEventSource *source)
{
  GSource *g_source = (GSource *) source;

  CLUTTER_NOTE (EVENT, "Removing GSource for evdev device manager");

  /* ignore the return value of close, it's not like we can do something
   * about it */
  close (source->event_poll_fd.fd);

  g_source_destroy (g_source);
  g_source_unref (g_source);
}

static void
clutter_touch_state_free (ClutterTouchState *touch_state)
{
  g_slice_free (ClutterTouchState, touch_state);
}

static void
clutter_seat_evdev_set_libinput_seat (ClutterSeatEvdev *seat,
                                      struct libinput_seat *libinput_seat)
{
  g_assert (seat->libinput_seat == NULL);

  libinput_seat_ref (libinput_seat);
  libinput_seat_set_user_data (libinput_seat, seat);
  seat->libinput_seat = libinput_seat;
}

static ClutterSeatEvdev *
clutter_seat_evdev_new (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (manager_evdev);
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  ClutterSeatEvdev *seat;
  ClutterInputDevice *device;
  struct xkb_context *ctx;
  struct xkb_rule_names names;
  struct xkb_keymap *keymap;

  seat = g_new0 (ClutterSeatEvdev, 1);
  if (!seat)
    return NULL;

  device = _clutter_input_device_evdev_new_virtual (
    manager, seat, CLUTTER_POINTER_DEVICE);
  _clutter_input_device_set_stage (device, priv->stage);
  seat->pointer_x = INITIAL_POINTER_X;
  seat->pointer_y = INITIAL_POINTER_Y;
  _clutter_input_device_set_coords (device, NULL,
                                    seat->pointer_x, seat->pointer_y,
                                    NULL);
  _clutter_device_manager_add_device (manager, device);
  seat->core_pointer = device;

  device = _clutter_input_device_evdev_new_virtual (
    manager, seat, CLUTTER_KEYBOARD_DEVICE);
  _clutter_input_device_set_stage (device, priv->stage);
  _clutter_device_manager_add_device (manager, device);
  seat->core_keyboard = device;

  seat->touches = g_hash_table_new_full (NULL, NULL, NULL,
                                         (GDestroyNotify) clutter_touch_state_free);

  ctx = xkb_context_new(0);
  g_assert (ctx);

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = option_xkb_layout;
  names.variant = option_xkb_variant;
  names.options = option_xkb_options;

  keymap = xkb_keymap_new_from_names (ctx, &names, 0);
  xkb_context_unref(ctx);
  if (keymap)
    {
      seat->xkb = xkb_state_new (keymap);

      seat->caps_lock_led =
        xkb_keymap_led_get_index (keymap, XKB_LED_NAME_CAPS);
      seat->num_lock_led =
        xkb_keymap_led_get_index (keymap, XKB_LED_NAME_NUM);
      seat->scroll_lock_led =
        xkb_keymap_led_get_index (keymap, XKB_LED_NAME_SCROLL);

      priv->keymap = keymap;
    }

  seat->repeat = TRUE;
  seat->repeat_delay = 250;     /* ms */
  seat->repeat_interval = 33;   /* ms */

  priv->seats = g_slist_append (priv->seats, seat);
  return seat;
}

static void
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

  clear_repeat_timer (seat);

  if (seat->libinput_seat)
    libinput_seat_unref (seat->libinput_seat);

  g_free (seat);
}

static void
clutter_seat_evdev_set_stage (ClutterSeatEvdev *seat, ClutterStage *stage)
{
  GSList *l;

  for (l = seat->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      _clutter_input_device_set_stage (device, stage);
    }
}

static void
evdev_add_device (ClutterDeviceManagerEvdev *manager_evdev,
                  struct libinput_device    *libinput_device)
{
  ClutterDeviceManager *manager = (ClutterDeviceManager *) manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  ClutterInputDeviceType type;
  struct libinput_seat *libinput_seat;
  ClutterSeatEvdev *seat;
  ClutterInputDevice *device;

  libinput_seat = libinput_device_get_seat (libinput_device);
  seat = libinput_seat_get_user_data (libinput_seat);
  if (seat == NULL)
    {
      /* Clutter has the notion of global "core" pointers and keyboard devices,
       * which are located on the main seat. Make whatever seat comes first the
       * main seat. */
      if (priv->main_seat->libinput_seat == NULL)
        seat = priv->main_seat;
      else
        seat = clutter_seat_evdev_new (manager_evdev);

      clutter_seat_evdev_set_libinput_seat (seat, libinput_seat);
    }

  device = _clutter_input_device_evdev_new (manager, seat, libinput_device);
  _clutter_input_device_set_stage (device, manager_evdev->priv->stage);

  _clutter_device_manager_add_device (manager, device);

  /* Clutter assumes that device types are exclusive in the
   * ClutterInputDevice API */
  type = _clutter_input_device_evdev_determine_type (libinput_device);

  if (type == CLUTTER_KEYBOARD_DEVICE)
    {
      _clutter_input_device_set_associated_device (device, seat->core_keyboard);
      _clutter_input_device_add_slave (seat->core_keyboard, device);
    }
  else if (type == CLUTTER_POINTER_DEVICE)
    {
      _clutter_input_device_set_associated_device (device, seat->core_pointer);
      _clutter_input_device_add_slave (seat->core_pointer, device);
    }

  CLUTTER_NOTE (EVENT, "Added physical device '%s', type %s",
                clutter_input_device_get_device_name (device),
                device_type_str[type]);
}

static void
evdev_remove_device (ClutterDeviceManagerEvdev *manager_evdev,
                     ClutterInputDeviceEvdev   *device_evdev)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (manager_evdev);
  ClutterInputDevice *input_device = CLUTTER_INPUT_DEVICE (device_evdev);

  _clutter_device_manager_remove_device (manager, input_device);
}

/*
 * ClutterDeviceManager implementation
 */

static void
clutter_device_manager_evdev_add_device (ClutterDeviceManager *manager,
                                         ClutterInputDevice   *device)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;
  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  seat->devices = g_slist_prepend (seat->devices, device);
  priv->devices = g_slist_prepend (priv->devices, device);
}

static void
clutter_device_manager_evdev_remove_device (ClutterDeviceManager *manager,
                                            ClutterInputDevice   *device)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);
  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  /* Remove the device */
  seat->devices = g_slist_remove (seat->devices, device);
  priv->devices = g_slist_remove (priv->devices, device);

  if (seat->repeat_timer && seat->repeat_device == device)
    clear_repeat_timer (seat);

  g_object_unref (device);
}

static const GSList *
clutter_device_manager_evdev_get_devices (ClutterDeviceManager *manager)
{
  return CLUTTER_DEVICE_MANAGER_EVDEV (manager)->priv->devices;
}

static ClutterInputDevice *
clutter_device_manager_evdev_get_core_device (ClutterDeviceManager   *manager,
                                              ClutterInputDeviceType  type)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return priv->main_seat->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return priv->main_seat->core_keyboard;

    case CLUTTER_EXTENSION_DEVICE:
    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_evdev_get_device (ClutterDeviceManager *manager,
                                         gint                  id)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *l;
  GSList *device_it;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  for (l = priv->seats; l; l = l->next)
    {
      ClutterSeatEvdev *seat = l->data;

      for (device_it = seat->devices; device_it; device_it = device_it->next)
        {
          ClutterInputDevice *device = device_it->data;

          if (clutter_input_device_get_device_id (device) == id)
            return device;
        }
    }

  return NULL;
}

static void
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
flush_event_queue (void)
{
  ClutterEvent *event;

  while ((event = clutter_event_get ()) != NULL)
    {
      _clutter_process_event (event);
      clutter_event_free (event);
    }
}

static gboolean
process_base_event (ClutterDeviceManagerEvdev *manager_evdev,
                    struct libinput_event *event)
{
  ClutterInputDevice *device;
  struct libinput_device *libinput_device;
  gboolean handled = TRUE;

  switch (libinput_event_get_type (event))
    {
    case LIBINPUT_EVENT_DEVICE_ADDED:
      libinput_device = libinput_event_get_device (event);

      evdev_add_device (manager_evdev, libinput_device);
      break;

    case LIBINPUT_EVENT_DEVICE_REMOVED:
      /* Flush all queued events, there
       * might be some from this device.
       */
      flush_event_queue ();

      libinput_device = libinput_event_get_device (event);

      device = libinput_device_get_user_data (libinput_device);
      evdev_remove_device (manager_evdev,
                           CLUTTER_INPUT_DEVICE_EVDEV (device));
      break;

    default:
      handled = FALSE;
    }

  return handled;
}

static ClutterTouchState *
_device_seat_add_touch (ClutterInputDevice *input_device,
                        guint32             id)
{
  ClutterInputDeviceEvdev *device_evdev =
    CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  ClutterSeatEvdev *seat = _clutter_input_device_evdev_get_seat (device_evdev);
  ClutterTouchState *touch;

  touch = g_slice_new0 (ClutterTouchState);
  touch->id = id;

  g_hash_table_insert (seat->touches, GUINT_TO_POINTER (id), touch);

  return touch;
}

static void
_device_seat_remove_touch (ClutterInputDevice *input_device,
                           guint32             id)
{
  ClutterInputDeviceEvdev *device_evdev =
    CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  ClutterSeatEvdev *seat = _clutter_input_device_evdev_get_seat (device_evdev);

  g_hash_table_remove (seat->touches, GUINT_TO_POINTER (id));
}

static ClutterTouchState *
_device_seat_get_touch (ClutterInputDevice *input_device,
                        guint32             id)
{
  ClutterInputDeviceEvdev *device_evdev =
    CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  ClutterSeatEvdev *seat = _clutter_input_device_evdev_get_seat (device_evdev);

  return g_hash_table_lookup (seat->touches, GUINT_TO_POINTER (id));
}

static void
check_notify_discrete_scroll (ClutterDeviceManagerEvdev *manager_evdev,
                              ClutterInputDevice        *device,
                              guint64                    time_us,
                              ClutterScrollSource        scroll_source)
{
  ClutterInputDeviceEvdev *device_evdev =
    CLUTTER_INPUT_DEVICE_EVDEV (device);
  ClutterSeatEvdev *seat = _clutter_input_device_evdev_get_seat (device_evdev);
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

static ClutterScrollSource
translate_scroll_source (enum libinput_pointer_axis_source source)
{
  switch (source)
    {
    case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL:
      return CLUTTER_SCROLL_SOURCE_WHEEL;
    case LIBINPUT_POINTER_AXIS_SOURCE_FINGER:
      return CLUTTER_SCROLL_SOURCE_FINGER;
    case LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS:
      return CLUTTER_SCROLL_SOURCE_CONTINUOUS;
    default:
      return CLUTTER_SCROLL_SOURCE_UNKNOWN;
    }
}

static ClutterInputDeviceToolType
translate_tool_type (struct libinput_tablet_tool *libinput_tool)
{
  enum libinput_tablet_tool_type tool;

  tool = libinput_tablet_tool_get_type (libinput_tool);

  switch (tool)
    {
    case LIBINPUT_TABLET_TOOL_TYPE_PEN:
      return CLUTTER_INPUT_DEVICE_TOOL_PEN;
    case LIBINPUT_TABLET_TOOL_TYPE_ERASER:
      return CLUTTER_INPUT_DEVICE_TOOL_ERASER;
    case LIBINPUT_TABLET_TOOL_TYPE_BRUSH:
      return CLUTTER_INPUT_DEVICE_TOOL_BRUSH;
    case LIBINPUT_TABLET_TOOL_TYPE_PENCIL:
      return CLUTTER_INPUT_DEVICE_TOOL_PENCIL;
    case LIBINPUT_TABLET_TOOL_TYPE_AIRBRUSH:
      return CLUTTER_INPUT_DEVICE_TOOL_AIRBRUSH;
    case LIBINPUT_TABLET_TOOL_TYPE_MOUSE:
      return CLUTTER_INPUT_DEVICE_TOOL_MOUSE;
    case LIBINPUT_TABLET_TOOL_TYPE_LENS:
      return CLUTTER_INPUT_DEVICE_TOOL_LENS;
    default:
      return CLUTTER_INPUT_DEVICE_TOOL_NONE;
    }
}

static void
input_device_update_tool (ClutterInputDevice          *input_device,
                          struct libinput_tablet_tool *libinput_tool)
{
  ClutterInputDeviceEvdev *evdev_device = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  ClutterInputDeviceTool *tool = NULL;
  ClutterInputDeviceToolType tool_type;
  guint64 tool_serial;

  if (libinput_tool)
    {
      tool_serial = libinput_tablet_tool_get_serial (libinput_tool);
      tool_type = translate_tool_type (libinput_tool);
      tool = clutter_input_device_lookup_tool (input_device,
                                               tool_serial, tool_type);

      if (!tool)
        {
          tool = clutter_input_device_tool_evdev_new (libinput_tool,
                                                      tool_serial, tool_type);
          clutter_input_device_add_tool (input_device, tool);
        }
    }

  evdev_device->last_tool = tool;
}

static gdouble *
translate_tablet_axes (struct libinput_event_tablet_tool *tablet_event)
{
  GArray *axes = g_array_new (FALSE, FALSE, sizeof (gdouble));
  struct libinput_tablet_tool *libinput_tool;
  gdouble value;

  libinput_tool = libinput_event_tablet_tool_get_tool (tablet_event);

  value = libinput_event_tablet_tool_get_x (tablet_event);
  g_array_append_val (axes, value);
  value = libinput_event_tablet_tool_get_y (tablet_event);
  g_array_append_val (axes, value);

  if (libinput_tablet_tool_has_distance (libinput_tool))
    {
      value = libinput_event_tablet_tool_get_distance (tablet_event);
      g_array_append_val (axes, value);
    }

  if (libinput_tablet_tool_has_pressure (libinput_tool))
    {
      value = libinput_event_tablet_tool_get_pressure (tablet_event);
      g_array_append_val (axes, value);
    }

  if (libinput_tablet_tool_has_tilt (libinput_tool))
    {
      value = libinput_event_tablet_tool_get_tilt_x (tablet_event);
      g_array_append_val (axes, value);
      value = libinput_event_tablet_tool_get_tilt_y (tablet_event);
      g_array_append_val (axes, value);
    }

  if (libinput_tablet_tool_has_rotation (libinput_tool))
    {
      value = libinput_event_tablet_tool_get_rotation (tablet_event);
      g_array_append_val (axes, value);
    }

  if (libinput_tablet_tool_has_slider (libinput_tool))
    {
      value = libinput_event_tablet_tool_get_slider_position (tablet_event);
      g_array_append_val (axes, value);
    }

  if (libinput_tablet_tool_has_wheel (libinput_tool))
    {
      value = libinput_event_tablet_tool_get_wheel_delta (tablet_event);
      g_array_append_val (axes, value);
    }

  if (axes->len == 0)
    {
      g_array_free (axes, TRUE);
      return NULL;
    }
  else
    return (gdouble *) g_array_free (axes, FALSE);
}

static gboolean
process_device_event (ClutterDeviceManagerEvdev *manager_evdev,
                      struct libinput_event *event)
{
  gboolean handled = TRUE;
  struct libinput_device *libinput_device = libinput_event_get_device(event);
  ClutterInputDevice *device;

  switch (libinput_event_get_type (event))
    {
    case LIBINPUT_EVENT_KEYBOARD_KEY:
      {
        guint32 key, key_state, seat_key_count;
        guint64 time_us;
        struct libinput_event_keyboard *key_event =
          libinput_event_get_keyboard_event (event);
        device = libinput_device_get_user_data (libinput_device);

        time_us = libinput_event_keyboard_get_time_usec (key_event);
        key = libinput_event_keyboard_get_key (key_event);
        key_state = libinput_event_keyboard_get_key_state (key_event) ==
                    LIBINPUT_KEY_STATE_PRESSED;
        seat_key_count =
          libinput_event_keyboard_get_seat_key_count (key_event);

	/* Ignore key events that are not seat wide state changes. */
	if ((key_state == LIBINPUT_KEY_STATE_PRESSED &&
	     seat_key_count != 1) ||
	    (key_state == LIBINPUT_KEY_STATE_RELEASED &&
	     seat_key_count != 0))
          break;

        notify_key_device (device, time_us, key, key_state, TRUE);

        break;
      }

    case LIBINPUT_EVENT_POINTER_MOTION:
      {
        struct libinput_event_pointer *motion_event =
          libinput_event_get_pointer_event (event);
        device = libinput_device_get_user_data (libinput_device);

        notify_relative_motion (device, motion_event);

        break;
      }

    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
      {
        guint64 time_us;
        double x, y;
        gfloat stage_width, stage_height;
        ClutterStage *stage;
        struct libinput_event_pointer *motion_event =
          libinput_event_get_pointer_event (event);
        device = libinput_device_get_user_data (libinput_device);

        stage = _clutter_input_device_get_stage (device);
        if (stage == NULL)
          break;

        stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
        stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

        time_us = libinput_event_pointer_get_time_usec (motion_event);
        x = libinput_event_pointer_get_absolute_x_transformed (motion_event,
                                                               stage_width);
        y = libinput_event_pointer_get_absolute_y_transformed (motion_event,
                                                               stage_height);
        notify_absolute_motion (device, time_us, x, y, NULL);

        break;
      }

    case LIBINPUT_EVENT_POINTER_BUTTON:
      {
        guint32 button, button_state, seat_button_count;
        guint64 time_us;
        struct libinput_event_pointer *button_event =
          libinput_event_get_pointer_event (event);
        device = libinput_device_get_user_data (libinput_device);

        time_us = libinput_event_pointer_get_time_usec (button_event);
        button = libinput_event_pointer_get_button (button_event);
        button_state = libinput_event_pointer_get_button_state (button_event) ==
                       LIBINPUT_BUTTON_STATE_PRESSED;
        seat_button_count =
          libinput_event_pointer_get_seat_button_count (button_event);

        /* Ignore button events that are not seat wide state changes. */
        if ((button_state == LIBINPUT_BUTTON_STATE_PRESSED &&
             seat_button_count != 1) ||
            (button_state == LIBINPUT_BUTTON_STATE_RELEASED &&
             seat_button_count != 0))
          break;

        notify_button (device, time_us, button, button_state);

        break;
      }

    case LIBINPUT_EVENT_POINTER_AXIS:
      {
        gdouble dx = 0.0, dy = 0.0;
        gdouble discrete_x = 0.0, discrete_y = 0.0;
        guint64 time_us;
        gboolean wheel = FALSE;
        enum libinput_pointer_axis axis;
        enum libinput_pointer_axis_source source;
        struct libinput_event_pointer *axis_event =
          libinput_event_get_pointer_event (event);
        ClutterSeatEvdev *seat;
        ClutterScrollSource scroll_source;
        ClutterScrollFinishFlags finish_flags = 0;

        device = libinput_device_get_user_data (libinput_device);
        seat = _clutter_input_device_evdev_get_seat (CLUTTER_INPUT_DEVICE_EVDEV (device));

        time_us = libinput_event_pointer_get_time_usec (axis_event);
        source = libinput_event_pointer_get_axis_source (axis_event);
        scroll_source = translate_scroll_source (source);

        /* libinput < 0.8 sent wheel click events with value 10. Since 0.8
           the value is the angle of the click in degrees. To keep
           backwards-compat with existing clients, we just send multiples of
           the click count. */

        if (source == LIBINPUT_POINTER_AXIS_SOURCE_WHEEL)
            wheel = TRUE;

        axis = LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
        if (libinput_event_pointer_has_axis (axis_event, axis))
          {
            discrete_y = libinput_event_pointer_get_axis_value_discrete (axis_event, axis);
            dy = libinput_event_pointer_get_axis_value (axis_event, axis);

            if (wheel)
              seat->accum_scroll_dy = 0;
            else if (fabs (dy) < DBL_EPSILON)
              {
                finish_flags |= CLUTTER_SCROLL_FINISHED_VERTICAL;
                seat->accum_scroll_dy = 0;
              }
            else
              seat->accum_scroll_dy += dy;
          }

        axis = LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL;
        if (libinput_event_pointer_has_axis (axis_event, axis))
          {
            discrete_x = libinput_event_pointer_get_axis_value_discrete (axis_event, axis);
            dx = libinput_event_pointer_get_axis_value (axis_event, axis);

            if (wheel)
              seat->accum_scroll_dx = 0;
            else if (fabs (dx) < DBL_EPSILON)
              {
                finish_flags |= CLUTTER_SCROLL_FINISHED_HORIZONTAL;
                seat->accum_scroll_dx = 0;
              }
            else
              seat->accum_scroll_dx += dx;
          }

        if (wheel)
          {
            notify_scroll (device, time_us,
                           discrete_x * DISCRETE_SCROLL_STEP,
                           discrete_y * DISCRETE_SCROLL_STEP,
                           scroll_source, finish_flags, TRUE);
            notify_discrete_scroll (device, time_us,
                                    discrete_to_direction (discrete_x, discrete_y),
                                    scroll_source, FALSE);
          }
        else
          {
            notify_scroll (device, time_us, dx, dy,
                           scroll_source, finish_flags, FALSE);
            check_notify_discrete_scroll (manager_evdev, device,
                                          time_us, scroll_source);
          }

        break;

      }

    case LIBINPUT_EVENT_TOUCH_DOWN:
      {
        gint32 slot;
        guint64 time_us;
        double x, y;
        gfloat stage_width, stage_height;
        ClutterStage *stage;
        ClutterTouchState *touch_state;
        struct libinput_event_touch *touch_event =
          libinput_event_get_touch_event (event);
        device = libinput_device_get_user_data (libinput_device);

        stage = _clutter_input_device_get_stage (device);
        if (stage == NULL)
          break;

        stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
        stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

        slot = libinput_event_touch_get_slot (touch_event);
        time_us = libinput_event_touch_get_time_usec (touch_event);
        x = libinput_event_touch_get_x_transformed (touch_event,
                                                    stage_width);
        y = libinput_event_touch_get_y_transformed (touch_event,
                                                    stage_height);

        touch_state = _device_seat_add_touch (device, slot);
        touch_state->coords.x = x;
        touch_state->coords.y = y;

        notify_touch_event (device, CLUTTER_TOUCH_BEGIN, time_us, slot,
                             touch_state->coords.x, touch_state->coords.y);
        break;
      }

    case LIBINPUT_EVENT_TOUCH_UP:
      {
        gint32 slot;
        guint64 time_us;
        ClutterTouchState *touch_state;
        struct libinput_event_touch *touch_event =
          libinput_event_get_touch_event (event);
        device = libinput_device_get_user_data (libinput_device);

        slot = libinput_event_touch_get_slot (touch_event);
        time_us = libinput_event_touch_get_time_usec (touch_event);
        touch_state = _device_seat_get_touch (device, slot);

        notify_touch_event (device, CLUTTER_TOUCH_END, time_us, slot,
			    touch_state->coords.x, touch_state->coords.y);
        _device_seat_remove_touch (device, slot);

        break;
      }

    case LIBINPUT_EVENT_TOUCH_MOTION:
      {
        gint32 slot;
        guint64 time_us;
        double x, y;
        gfloat stage_width, stage_height;
        ClutterStage *stage;
        ClutterTouchState *touch_state;
        struct libinput_event_touch *touch_event =
          libinput_event_get_touch_event (event);
        device = libinput_device_get_user_data (libinput_device);

        stage = _clutter_input_device_get_stage (device);
        if (stage == NULL)
          break;

        stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
        stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

        slot = libinput_event_touch_get_slot (touch_event);
        time_us = libinput_event_touch_get_time_usec (touch_event);
        x = libinput_event_touch_get_x_transformed (touch_event,
                                                    stage_width);
        y = libinput_event_touch_get_y_transformed (touch_event,
                                                    stage_height);

        touch_state = _device_seat_get_touch (device, slot);
        touch_state->coords.x = x;
        touch_state->coords.y = y;

        notify_touch_event (device, CLUTTER_TOUCH_UPDATE, time_us, slot,
			    touch_state->coords.x, touch_state->coords.y);
        break;
      }
    case LIBINPUT_EVENT_TOUCH_CANCEL:
      {
        ClutterTouchState *touch_state;
        GHashTableIter iter;
        guint64 time_us;
        struct libinput_event_touch *touch_event =
          libinput_event_get_touch_event (event);
        ClutterSeatEvdev *seat;

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_touch_get_time_usec (touch_event);
        seat = _clutter_input_device_evdev_get_seat (CLUTTER_INPUT_DEVICE_EVDEV (device));
        g_hash_table_iter_init (&iter, seat->touches);

        while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &touch_state))
          {
            notify_touch_event (device, CLUTTER_TOUCH_CANCEL,
                                time_us, touch_state->id,
                                touch_state->coords.x, touch_state->coords.y);
            g_hash_table_iter_remove (&iter);
          }

        break;
      }
    case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
    case LIBINPUT_EVENT_GESTURE_PINCH_END:
      {
        struct libinput_event_gesture *gesture_event =
          libinput_event_get_gesture_event (event);
        ClutterTouchpadGesturePhase phase;
        guint64 time_us;

        if (libinput_event_get_type (event) == LIBINPUT_EVENT_GESTURE_PINCH_BEGIN)
          phase = CLUTTER_TOUCHPAD_GESTURE_PHASE_BEGIN;
        else
          phase = libinput_event_gesture_get_cancelled (gesture_event) ?
            CLUTTER_TOUCHPAD_GESTURE_PHASE_CANCEL : CLUTTER_TOUCHPAD_GESTURE_PHASE_END;

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_gesture_get_time_usec (gesture_event);
        notify_pinch_gesture_event (device, phase, time_us, 0, 0, 0, 0);
        break;
      }
    case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
      {
        struct libinput_event_gesture *gesture_event =
          libinput_event_get_gesture_event (event);
        gdouble angle_delta, scale, dx, dy;
        guint64 time_us;

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_gesture_get_time_usec (gesture_event);
        angle_delta = libinput_event_gesture_get_angle_delta (gesture_event);
        scale = libinput_event_gesture_get_scale (gesture_event);
        dx = libinput_event_gesture_get_dx (gesture_event);
        dy = libinput_event_gesture_get_dx (gesture_event);

        notify_pinch_gesture_event (device,
                                    CLUTTER_TOUCHPAD_GESTURE_PHASE_UPDATE,
                                    time_us, dx, dy, angle_delta, scale);
        break;
      }
    case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
    case LIBINPUT_EVENT_GESTURE_SWIPE_END:
      {
        struct libinput_event_gesture *gesture_event =
          libinput_event_get_gesture_event (event);
        ClutterTouchpadGesturePhase phase;
        guint32 n_fingers;
        guint64 time_us;

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_gesture_get_time_usec (gesture_event);
        n_fingers = libinput_event_gesture_get_finger_count (gesture_event);

        if (libinput_event_get_type (event) == LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN)
          phase = CLUTTER_TOUCHPAD_GESTURE_PHASE_BEGIN;
        else
          phase = libinput_event_gesture_get_cancelled (gesture_event) ?
            CLUTTER_TOUCHPAD_GESTURE_PHASE_CANCEL : CLUTTER_TOUCHPAD_GESTURE_PHASE_END;

        notify_swipe_gesture_event (device, phase, time_us, n_fingers, 0, 0);
        break;
      }
    case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
      {
        struct libinput_event_gesture *gesture_event =
          libinput_event_get_gesture_event (event);
        guint32 n_fingers;
        guint64 time_us;
        gdouble dx, dy;

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_gesture_get_time_usec (gesture_event);
        n_fingers = libinput_event_gesture_get_finger_count (gesture_event);
        dx = libinput_event_gesture_get_dx (gesture_event);
        dy = libinput_event_gesture_get_dy (gesture_event);

        notify_swipe_gesture_event (device,
                                    CLUTTER_TOUCHPAD_GESTURE_PHASE_UPDATE,
                                    time_us, n_fingers, dx, dy);
        break;
      }
    case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
      {
        guint64 time;
        double x, y, *axes;
        gfloat stage_width, stage_height;
        ClutterStage *stage;
        struct libinput_event_tablet_tool *tablet_event =
          libinput_event_get_tablet_tool_event (event);
        device = libinput_device_get_user_data (libinput_device);

        stage = _clutter_input_device_get_stage (device);
        if (!stage)
          break;

        axes = translate_tablet_axes (tablet_event);
        if (!axes)
          break;

        stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
        stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

        time = libinput_event_tablet_tool_get_time_usec (tablet_event);
        x = libinput_event_tablet_tool_get_x_transformed (tablet_event, stage_width);
        y = libinput_event_tablet_tool_get_y_transformed (tablet_event, stage_height);

        notify_absolute_motion (device, time, x, y, axes);
        break;
      }
    case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
      {
        guint64 time;
        struct libinput_event_tablet_tool *tablet_event =
          libinput_event_get_tablet_tool_event (event);
        struct libinput_tablet_tool *libinput_tool = NULL;
        enum libinput_tablet_tool_proximity_state state;

        state = libinput_event_tablet_tool_get_proximity_state (tablet_event);
        time = libinput_event_tablet_tool_get_time_usec (tablet_event);
        device = libinput_device_get_user_data (libinput_device);

        libinput_tool = libinput_event_tablet_tool_get_tool (tablet_event);

        input_device_update_tool (device, libinput_tool);
        notify_proximity (device, time, state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN);
        break;
      }
    case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
      {
        guint64 time;
        guint32 button_state;
        struct libinput_event_tablet_tool *tablet_event =
          libinput_event_get_tablet_tool_event (event);
        guint tablet_button;

        device = libinput_device_get_user_data (libinput_device);
        time = libinput_event_tablet_tool_get_time_usec (tablet_event);
        tablet_button = libinput_event_tablet_tool_get_button (tablet_event);

        button_state = libinput_event_tablet_tool_get_button_state (tablet_event) ==
                       LIBINPUT_BUTTON_STATE_PRESSED;
        notify_button (device, time, tablet_button, button_state);
        break;
      }
    case LIBINPUT_EVENT_TABLET_TOOL_TIP:
      {
        guint64 time;
        guint32 button_state;
        struct libinput_event_tablet_tool *tablet_event =
          libinput_event_get_tablet_tool_event (event);

        device = libinput_device_get_user_data (libinput_device);
        time = libinput_event_tablet_tool_get_time_usec (tablet_event);

        button_state = libinput_event_tablet_tool_get_tip_state (tablet_event) ==
                       LIBINPUT_TABLET_TOOL_TIP_DOWN;
        notify_button (device, time, BTN_TOUCH, button_state);
        break;
      }
    default:
      handled = FALSE;
    }

  return handled;
}

static gboolean
filter_event (ClutterDeviceManagerEvdev *manager_evdev,
              struct libinput_event     *event)
{
  gboolean retval = CLUTTER_EVENT_PROPAGATE;
  ClutterEventFilter *filter;
  GSList *tmp_list;

  tmp_list = manager_evdev->priv->event_filters;

  while (tmp_list)
    {
      filter = tmp_list->data;
      retval = filter->func (event, filter->data);
      tmp_list = tmp_list->next;

      if (retval != CLUTTER_EVENT_PROPAGATE)
        break;
    }

  return retval;
}

static void
process_event (ClutterDeviceManagerEvdev *manager_evdev,
               struct libinput_event *event)
{
  gboolean retval;

  retval = filter_event (manager_evdev, event);

  if (retval != CLUTTER_EVENT_PROPAGATE)
    return;

  if (process_base_event (manager_evdev, event))
    return;
  if (process_device_event (manager_evdev, event))
    return;
}

static void
process_events (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  struct libinput_event *event;

  while ((event = libinput_get_event (priv->libinput)))
    {
      process_event(manager_evdev, event);
      libinput_event_destroy(event);
    }
}

static int
open_restricted (const char *path,
                 int flags,
                 void *user_data)
{
  gint fd;

  if (device_open_callback)
    {
      GError *error = NULL;

      fd = device_open_callback (path, flags, device_callback_data, &error);

      if (fd < 0)
        {
          g_warning ("Could not open device %s: %s", path, error->message);
          g_error_free (error);
        }
    }
  else
    {
      fd = open (path, O_RDWR | O_NONBLOCK);
      if (fd < 0)
        {
          g_warning ("Could not open device %s: %s", path, strerror (errno));
        }
    }

  return fd;
}

static void
close_restricted (int fd,
                  void *user_data)
{
  if (device_close_callback)
    device_close_callback (fd, device_callback_data);
  else
    close (fd);
}

static const struct libinput_interface libinput_interface = {
  open_restricted,
  close_restricted
};

/*
 * GObject implementation
 */

static void
clutter_device_manager_evdev_constructed (GObject *gobject)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterEventSource *source;
  struct udev *udev;

  udev = udev_new ();
  if (G_UNLIKELY (udev == NULL))
    {
      g_warning ("Failed to create udev object");
      return;
    }

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (gobject);
  priv = manager_evdev->priv;

  priv->libinput = libinput_udev_create_context (&libinput_interface,
                                                 manager_evdev,
                                                 udev);
  if (priv->libinput == NULL)
    {
      g_critical ("Failed to create the libinput object.");
      return;
    }

  if (libinput_udev_assign_seat (priv->libinput, "seat0") == -1)
    {
      g_critical ("Failed to assign a seat to the libinput object.");
      libinput_unref (priv->libinput);
      priv->libinput = NULL;
      return;
    }

  udev_unref (udev);

  priv->main_seat = clutter_seat_evdev_new (manager_evdev);

  dispatch_libinput (manager_evdev);

  source = clutter_event_source_new (manager_evdev);
  priv->event_source = source;
}

static void
clutter_device_manager_evdev_dispose (GObject *object)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (object);
  priv = manager_evdev->priv;

  if (priv->stage_added_handler)
    {
      g_signal_handler_disconnect (priv->stage_manager,
                                   priv->stage_added_handler);
      priv->stage_added_handler = 0;
    }

  if (priv->stage_removed_handler)
    {
      g_signal_handler_disconnect (priv->stage_manager,
                                   priv->stage_removed_handler);
      priv->stage_removed_handler = 0;
    }

  if (priv->stage_manager)
    {
      g_object_unref (priv->stage_manager);
      priv->stage_manager = NULL;
    }

  G_OBJECT_CLASS (clutter_device_manager_evdev_parent_class)->dispose (object);
}

static void
clutter_device_manager_evdev_finalize (GObject *object)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (object);
  priv = manager_evdev->priv;

  g_slist_free_full (priv->seats, (GDestroyNotify) clutter_seat_evdev_free);
  g_slist_free (priv->devices);

  if (priv->keymap)
    xkb_keymap_unref (priv->keymap);

  if (priv->event_source != NULL)
    clutter_event_source_free (priv->event_source);

  if (priv->constrain_data_notify != NULL)
    priv->constrain_data_notify (priv->constrain_data);

  if (priv->libinput != NULL)
    libinput_unref (priv->libinput);

  g_list_free (priv->free_device_ids);

  G_OBJECT_CLASS (clutter_device_manager_evdev_parent_class)->finalize (object);
}

static void
clutter_device_manager_evdev_class_init (ClutterDeviceManagerEvdevClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterDeviceManagerClass *manager_class;

  gobject_class->constructed = clutter_device_manager_evdev_constructed;
  gobject_class->finalize = clutter_device_manager_evdev_finalize;
  gobject_class->dispose = clutter_device_manager_evdev_dispose;

  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_evdev_add_device;
  manager_class->remove_device = clutter_device_manager_evdev_remove_device;
  manager_class->get_devices = clutter_device_manager_evdev_get_devices;
  manager_class->get_core_device = clutter_device_manager_evdev_get_core_device;
  manager_class->get_device = clutter_device_manager_evdev_get_device;
}

static void
clutter_device_manager_evdev_stage_added_cb (ClutterStageManager *manager,
                                             ClutterStage *stage,
                                             ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv = self->priv;
  GSList *l;

  /* NB: Currently we can only associate a single stage with all evdev
   * devices.
   *
   * We save a pointer to the stage so if we release/reclaim input
   * devices due to switching virtual terminals then we know what
   * stage to re associate the devices with.
   */
  priv->stage = stage;

  /* Set the stage of any devices that don't already have a stage */
  for (l = priv->seats; l; l = l->next)
    {
      ClutterSeatEvdev *seat = l->data;

      clutter_seat_evdev_set_stage (seat, stage);
    }

  /* We only want to do this once so we can catch the default
     stage. If the application has multiple stages then it will need
     to manage the stage of the input devices itself */
  g_signal_handler_disconnect (priv->stage_manager,
                               priv->stage_added_handler);
  priv->stage_added_handler = 0;
}

static void
clutter_device_manager_evdev_stage_removed_cb (ClutterStageManager *manager,
                                               ClutterStage *stage,
                                               ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv = self->priv;
  GSList *l;

  /* Remove the stage of any input devices that were pointing to this
     stage so we don't send events to invalid stages */
  for (l = priv->seats; l; l = l->next)
    {
      ClutterSeatEvdev *seat = l->data;

      clutter_seat_evdev_set_stage (seat, NULL);
    }
}

static void
clutter_device_manager_evdev_init (ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv;

  priv = self->priv = clutter_device_manager_evdev_get_instance_private (self);

  priv->stage_manager = clutter_stage_manager_get_default ();
  g_object_ref (priv->stage_manager);

  /* evdev doesn't have any way to link an event to a particular stage
     so we'll have to leave it up to applications to set the
     corresponding stage for an input device. However to make it
     easier for applications that are only using one fullscreen stage
     (which is probably the most frequent use-case for the evdev
     backend) we'll associate any input devices that don't have a
     stage with the first stage created. */
  priv->stage_added_handler =
    g_signal_connect (priv->stage_manager,
                      "stage-added",
                      G_CALLBACK (clutter_device_manager_evdev_stage_added_cb),
                      self);
  priv->stage_removed_handler =
    g_signal_connect (priv->stage_manager,
                      "stage-removed",
                      G_CALLBACK (clutter_device_manager_evdev_stage_removed_cb),
                      self);

  priv->device_id_next = INITIAL_DEVICE_ID;
}

void
_clutter_events_evdev_init (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "Initializing evdev backend");

  backend->device_manager = g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_EVDEV,
                                          "backend", backend,
                                          NULL);
}

void
_clutter_events_evdev_uninit (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "Uninitializing evdev backend");
}

gint
_clutter_device_manager_evdev_acquire_device_id (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  GList *first;
  gint next_id;

  if (priv->free_device_ids == NULL)
    {
      gint i;

      /* We ran out of free ID's, so append 10 new ones. */
      for (i = 0; i < 10; i++)
        priv->free_device_ids =
          g_list_append (priv->free_device_ids,
                         GINT_TO_POINTER (priv->device_id_next++));
    }

  first = g_list_first (priv->free_device_ids);
  next_id = GPOINTER_TO_INT (first->data);
  priv->free_device_ids = g_list_remove_link (priv->free_device_ids, first);

  return next_id;
}

static int
compare_ids (gconstpointer a,
             gconstpointer b)
{
  return GPOINTER_TO_INT (a) - GPOINTER_TO_INT (b);
}

void
_clutter_device_manager_evdev_release_device_id (ClutterDeviceManagerEvdev *manager_evdev,
                                                 ClutterInputDevice        *device)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  gint device_id;

  device_id = clutter_input_device_get_device_id (device);
  priv->free_device_ids = g_list_insert_sorted (priv->free_device_ids,
                                                GINT_TO_POINTER (device_id),
                                                compare_ids);
}

/**
 * clutter_evdev_release_devices:
 *
 * Releases all the evdev devices that Clutter is currently managing. This api
 * is typically used when switching away from the Clutter application when
 * switching tty. The devices can be reclaimed later with a call to
 * clutter_evdev_reclaim_devices().
 *
 * This function should only be called after clutter has been initialized.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
clutter_evdev_release_devices (void)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  if (!manager)
    {
      g_warning ("clutter_evdev_release_devices shouldn't be called "
                 "before clutter_init()");
      return;
    }

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (manager));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  if (priv->released)
    {
      g_warning ("clutter_evdev_release_devices() shouldn't be called "
                 "multiple times without a corresponding call to "
                 "clutter_evdev_reclaim_devices() first");
      return;
    }

  libinput_suspend (priv->libinput);
  process_events (manager_evdev);

  priv->released = TRUE;
}

static void
clutter_evdev_update_xkb_state (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *iter;
  ClutterSeatEvdev *seat;
  xkb_mod_mask_t latched_mods;
  xkb_mod_mask_t locked_mods;

  priv = manager_evdev->priv;

  for (iter = priv->seats; iter; iter = iter->next)
    {
      seat = iter->data;

      latched_mods = xkb_state_serialize_mods (seat->xkb,
                                               XKB_STATE_MODS_LATCHED);
      locked_mods = xkb_state_serialize_mods (seat->xkb,
                                              XKB_STATE_MODS_LOCKED);
      xkb_state_unref (seat->xkb);
      seat->xkb = xkb_state_new (priv->keymap);

      xkb_state_update_mask (seat->xkb,
                             0, /* depressed */
                             latched_mods,
                             locked_mods,
                             0, 0, 0);

      seat->caps_lock_led = xkb_keymap_led_get_index (priv->keymap, XKB_LED_NAME_CAPS);
      seat->num_lock_led = xkb_keymap_led_get_index (priv->keymap, XKB_LED_NAME_NUM);
      seat->scroll_lock_led = xkb_keymap_led_get_index (priv->keymap, XKB_LED_NAME_SCROLL);

      clutter_seat_evdev_sync_leds (seat);
    }
}

/**
 * clutter_evdev_reclaim_devices:
 *
 * This causes Clutter to re-probe for evdev devices. This is must only be
 * called after a corresponding call to clutter_evdev_release_devices()
 * was previously used to release all evdev devices. This API is typically
 * used when a clutter application using evdev has regained focus due to
 * switching ttys.
 *
 * This function should only be called after clutter has been initialized.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
clutter_evdev_reclaim_devices (void)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  ClutterDeviceManagerEvdev *manager_evdev =
    CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;

  if (!priv->released)
    {
      g_warning ("Spurious call to clutter_evdev_reclaim_devices() without "
                 "previous call to clutter_evdev_release_devices");
      return;
    }

  libinput_resume (priv->libinput);
  clutter_evdev_update_xkb_state (manager_evdev);
  process_events (manager_evdev);

  priv->released = FALSE;
}

/**
 * clutter_evdev_set_device_callbacks: (skip)
 * @open_callback: the user replacement for open()
 * @close_callback: the user replacement for close()
 * @user_data: user data for @callback
 *
 * Through this function, the application can set a custom callback
 * to invoked when Clutter is about to open an evdev device. It can do
 * so if special handling is needed, for example to circumvent permission
 * problems.
 *
 * Setting @callback to %NULL will reset the default behavior.
 *
 * For reliable effects, this function must be called before clutter_init().
 *
 * Since: 1.16
 * Stability: unstable
 */
void
clutter_evdev_set_device_callbacks (ClutterOpenDeviceCallback  open_callback,
                                    ClutterCloseDeviceCallback close_callback,
                                    gpointer                   user_data)
{
  device_open_callback = open_callback;
  device_close_callback = close_callback;
  device_callback_data = user_data;
}

/**
 * clutter_evdev_set_keyboard_map: (skip)
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 * @keymap: the new keymap
 *
 * Instructs @evdev to use the speficied keyboard map. This will cause
 * the backend to drop the state and create a new one with the new
 * map. To avoid state being lost, callers should ensure that no key
 * is pressed when calling this function.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
clutter_evdev_set_keyboard_map (ClutterDeviceManager *evdev,
				struct xkb_keymap    *keymap)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  priv = manager_evdev->priv;

  if (priv->keymap)
    xkb_keymap_unref (priv->keymap);

  priv->keymap = xkb_keymap_ref (keymap);
  clutter_evdev_update_xkb_state (manager_evdev);
}

/**
 * clutter_evdev_get_keyboard_map: (skip)
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 *
 * Retrieves the #xkb_keymap in use by the evdev backend.
 *
 * Return value: the #xkb_keymap.
 *
 * Since: 1.18
 * Stability: unstable
 */
struct xkb_keymap *
clutter_evdev_get_keyboard_map (ClutterDeviceManager *evdev)
{
  ClutterDeviceManagerEvdev *manager_evdev;

  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev), NULL);

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);

  return xkb_state_get_keymap (manager_evdev->priv->main_seat->xkb);
}

/**
 * clutter_evdev_set_keyboard_layout_index: (skip)
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 * @idx: the xkb layout index to set
 *
 * Sets the xkb layout index on the backend's #xkb_state .
 *
 * Since: 1.20
 * Stability: unstable
 */
void
clutter_evdev_set_keyboard_layout_index (ClutterDeviceManager *evdev,
                                         xkb_layout_index_t    idx)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  xkb_mod_mask_t depressed_mods;
  xkb_mod_mask_t latched_mods;
  xkb_mod_mask_t locked_mods;
  struct xkb_state *state;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  state = manager_evdev->priv->main_seat->xkb;

  depressed_mods = xkb_state_serialize_mods (state, XKB_STATE_MODS_DEPRESSED);
  latched_mods = xkb_state_serialize_mods (state, XKB_STATE_MODS_LATCHED);
  locked_mods = xkb_state_serialize_mods (state, XKB_STATE_MODS_LOCKED);

  xkb_state_update_mask (state, depressed_mods, latched_mods, locked_mods, 0, 0, idx);
}

/**
 * clutter_evdev_set_pointer_constrain_callback:
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 * @callback: the callback
 * @user_data: data to pass to the callback
 * @user_data_notify: function to be called when removing the callback
 *
 * Sets a callback to be invoked for every pointer motion. The callback
 * can then modify the new pointer coordinates to constrain movement within
 * a specific region.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
clutter_evdev_set_pointer_constrain_callback (ClutterDeviceManager            *evdev,
					      ClutterPointerConstrainCallback  callback,
					      gpointer                         user_data,
					      GDestroyNotify                   user_data_notify)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  priv = manager_evdev->priv;

  if (priv->constrain_data_notify)
    priv->constrain_data_notify (priv->constrain_data);

  priv->constrain_callback = callback;
  priv->constrain_data = user_data;
  priv->constrain_data_notify = user_data_notify;
}

/**
 * clutter_evdev_set_keyboard_repeat:
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 * @repeat: whether to enable or disable keyboard repeat events
 * @delay: the delay in ms between the hardware key press event and
 * the first synthetic event
 * @interval: the period in ms between consecutive synthetic key
 * press events
 *
 * Enables or disables sythetic key press events, allowing for initial
 * delay and interval period to be specified.
 *
 * Since: 1.18
 * Stability: unstable
 */
void
clutter_evdev_set_keyboard_repeat (ClutterDeviceManager *evdev,
                                   gboolean              repeat,
                                   guint32               delay,
                                   guint32               interval)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterSeatEvdev *seat;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  seat = manager_evdev->priv->main_seat;

  seat->repeat = repeat;
  seat->repeat_delay = delay;
  seat->repeat_interval = interval;
}

/**
 * clutter_evdev_add_filter: (skip)
 * @func: (closure data): a filter function
 * @data: (allow-none): user data to be passed to the filter function, or %NULL
 * @destroy_notify: (allow-none): function to call on @data when the filter is removed, or %NULL
 *
 * Adds an event filter function.
 *
 * Since: 1.20
 * Stability: unstable
 */
void
clutter_evdev_add_filter (ClutterEvdevFilterFunc func,
                          gpointer               data,
                          GDestroyNotify         destroy_notify)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManager *manager;
  ClutterEventFilter *filter;

  g_return_if_fail (func != NULL);

  manager = clutter_device_manager_get_default ();

  if (!CLUTTER_IS_DEVICE_MANAGER_EVDEV (manager))
    {
      g_critical ("The Clutter input backend is not a evdev backend");
      return;
    }

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);

  filter = g_new0 (ClutterEventFilter, 1);
  filter->func = func;
  filter->data = data;
  filter->destroy_notify = destroy_notify;

  manager_evdev->priv->event_filters =
    g_slist_append (manager_evdev->priv->event_filters, filter);
}

/**
 * clutter_evdev_remove_filter: (skip)
 * @func: a filter function
 * @data: (allow-none): user data to be passed to the filter function, or %NULL
 *
 * Removes the given filter function.
 *
 * Since: 1.20
 * Stability: unstable
 */
void
clutter_evdev_remove_filter (ClutterEvdevFilterFunc func,
                             gpointer               data)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManager *manager;
  ClutterEventFilter *filter;
  GSList *tmp_list;

  g_return_if_fail (func != NULL);

  manager = clutter_device_manager_get_default ();

  if (!CLUTTER_IS_DEVICE_MANAGER_EVDEV (manager))
    {
      g_critical ("The Clutter input backend is not a evdev backend");
      return;
    }

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  tmp_list = manager_evdev->priv->event_filters;

  while (tmp_list)
    {
      filter = tmp_list->data;

      if (filter->func == func && filter->data == data)
        {
          if (filter->destroy_notify)
            filter->destroy_notify (filter->data);
          g_free (filter);
          manager_evdev->priv->event_filters =
            g_slist_delete_link (manager_evdev->priv->event_filters, tmp_list);
          return;
        }

      tmp_list = tmp_list->next;
    }
}

/**
 * clutter_evdev_warp_pointer:
 * @pointer_device: the pointer device to warp
 * @time: the timestamp for the warp event
 * @x: the new X position of the pointer
 * @y: the new Y position of the pointer
 *
 * Warps the pointer to a new location. Technically, this is
 * processed the same way as an absolute motion event from
 * libinput: it simply generates an absolute motion event that
 * will be processed on the next iteration of the mainloop.
 *
 * The intended use for this is for display servers that need
 * to warp cursor the cursor to a new location.
 *
 * Since: 1.20
 * Stability: unstable
 */
void
clutter_evdev_warp_pointer (ClutterInputDevice   *pointer_device,
                            guint32               time_,
                            int                   x,
                            int                   y)
{
  notify_absolute_motion (pointer_device, ms2us(time_), x, y, NULL);
}
