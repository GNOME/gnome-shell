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
#include "config.h"
#endif

#include <math.h>
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

#include "clutter-device-manager-evdev.h"

#define AUTOREPEAT_VALUE 2

/* Try to keep the pointer inside the stage. Hopefully no one is using
 * this backend with stages smaller than this. */
#define INITIAL_POINTER_X 16
#define INITIAL_POINTER_Y 16

struct _ClutterSeatEvdev
{
  struct libinput_seat *libinput_seat;
  ClutterDeviceManagerEvdev *manager_evdev;

  GSList *devices;

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;

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

  ClutterPointerConstrainCallback constrain_callback;
  gpointer                        constrain_data;
  GDestroyNotify                  constrain_data_notify;

  ClutterStageManager *stage_manager;
  guint stage_added_handler;
  guint stage_removed_handler;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterDeviceManagerEvdev,
                            clutter_device_manager_evdev,
                            CLUTTER_TYPE_DEVICE_MANAGER)

static ClutterOpenDeviceCallback  device_open_callback;
static ClutterCloseDeviceCallback device_close_callback;
static gpointer                   device_callback_data;

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

/*
 * ClutterEventSource management
 *
 * The device manager is responsible for managing the GSource when devices
 * appear and disappear from the system.
 */

static const char *option_xkb_layout = "us";
static const char *option_xkb_variant = "";
static const char *option_xkb_options = "";

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
		   guint32             time_,
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
  if (!stage)
    {
      clear_repeat_timer (seat);
      return;
    }

  event = _clutter_key_event_new_from_evdev (input_device,
					     seat->core_keyboard,
					     stage,
					     seat->xkb,
					     seat->button_state,
					     time_, key, state);

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
  guint32 time;

  g_return_val_if_fail (seat->repeat_device != NULL, G_SOURCE_REMOVE);

  time = g_source_get_time (g_main_context_find_source_by_id (NULL, seat->repeat_timer)) / 1000;

  notify_key_device (seat->repeat_device, time, seat->repeat_key, AUTOREPEAT_VALUE, FALSE);

  return G_SOURCE_CONTINUE;
}

static void
notify_absolute_motion (ClutterInputDevice *input_device,
			guint32             time_,
			gfloat              x,
			gfloat              y)
{
  gfloat stage_width, stage_height;
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (input_device->device_manager);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
  stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

  event = clutter_event_new (CLUTTER_MOTION);

  if (manager_evdev->priv->constrain_callback)
    {
      manager_evdev->priv->constrain_callback (seat->core_pointer,
                                               time_, &x, &y,
					       manager_evdev->priv->constrain_data);
    }
  else
    {
      x = CLAMP (x, 0.f, stage_width - 1);
      y = CLAMP (y, 0.f, stage_height - 1);
    }

  event->motion.time = time_;
  event->motion.stage = stage;
  event->motion.device = seat->core_pointer;
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);
  event->motion.x = x;
  event->motion.y = y;
  clutter_event_set_source_device (event, input_device);

  queue_event (event);
}

static void
notify_relative_motion (ClutterInputDevice *input_device,
                        guint32             time_,
                        li_fixed_t          dx,
                        li_fixed_t          dy)
{
  gfloat new_x, new_y;
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterPoint point;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  if (!_clutter_input_device_get_stage (input_device))
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  /* Append previously discarded fraction. */
  dx += device_evdev->dx_frac;
  dy += device_evdev->dy_frac;

  clutter_input_device_get_coords (seat->core_pointer, NULL, &point);
  new_x = point.x + li_fixed_to_int (dx);
  new_y = point.y + li_fixed_to_int (dy);

  /* Save the discarded fraction part for next motion event. */
  device_evdev->dx_frac = (dx < 0 ? -1 : 1) * (0xff & dx);
  device_evdev->dy_frac = (dy < 0 ? -1 : 1) * (0xff & dy);

  notify_absolute_motion (input_device, time_, new_x, new_y);
}

static void
notify_scroll (ClutterInputDevice *input_device,
               guint32             time_,
               gdouble             dx,
               gdouble             dy)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  ClutterPoint point;
  const gdouble scroll_factor = 10.0f;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (CLUTTER_SCROLL);

  event->scroll.time = time_;
  event->scroll.stage = CLUTTER_STAGE (stage);
  event->scroll.device = seat->core_pointer;
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);

  event->scroll.direction = CLUTTER_SCROLL_SMOOTH;
  clutter_event_set_scroll_delta (event,
                                  scroll_factor * dx,
                                  scroll_factor * dy);

  clutter_input_device_get_coords (seat->core_pointer, NULL, &point);
  event->scroll.x = point.x;
  event->scroll.y = point.y;
  clutter_event_set_source_device (event, input_device);

  queue_event (event);
}

static void
notify_button (ClutterInputDevice *input_device,
               guint32             time_,
               guint32             button,
               guint32             state)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  ClutterPoint point;
  gint button_nr;
  static gint maskmap[8] =
    {
      CLUTTER_BUTTON1_MASK, CLUTTER_BUTTON3_MASK, CLUTTER_BUTTON2_MASK,
      CLUTTER_BUTTON4_MASK, CLUTTER_BUTTON5_MASK, 0, 0, 0
    };

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  /* The evdev button numbers don't map sequentially to clutter button
   * numbers (the right and middle mouse buttons are in the opposite
   * order) so we'll map them directly with a switch statement */
  switch (button)
    {
    case BTN_LEFT:
      button_nr = CLUTTER_BUTTON_PRIMARY;
      break;

    case BTN_RIGHT:
      button_nr = CLUTTER_BUTTON_SECONDARY;
      break;

    case BTN_MIDDLE:
      button_nr = CLUTTER_BUTTON_MIDDLE;
      break;

    default:
      button_nr = button - BTN_MOUSE + 1;
      break;
    }

  if (G_UNLIKELY (button_nr < 1 || button_nr > 8))
    {
      g_warning ("Unhandled button event 0x%x", button);
      return;
    }

  if (state)
    event = clutter_event_new (CLUTTER_BUTTON_PRESS);
  else
    event = clutter_event_new (CLUTTER_BUTTON_RELEASE);

  /* Update the modifiers */
  if (state)
    seat->button_state |= maskmap[button - BTN_LEFT];
  else
    seat->button_state &= ~maskmap[button - BTN_LEFT];

  event->button.time = time_;
  event->button.stage = CLUTTER_STAGE (stage);
  event->button.device = seat->core_pointer;
  _clutter_xkb_translate_state (event, seat->xkb, seat->button_state);
  event->button.button = button_nr;
  clutter_input_device_get_coords (seat->core_pointer, NULL, &point);
  event->button.x = point.x;
  event->button.y = point.y;
  clutter_event_set_source_device (event, input_device);

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
      clutter_do_event (event);
      clutter_event_free (event);

      /* update the device states *after* the event */
      event_state = xkb_state_serialize_mods (seat->xkb, XKB_STATE_MODS_EFFECTIVE);
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

static ClutterSeatEvdev *
clutter_seat_evdev_new (ClutterDeviceManagerEvdev *manager_evdev,
                        struct libinput_seat *libinput_seat)
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

  libinput_seat_ref (libinput_seat);
  libinput_seat_set_user_data (libinput_seat, seat);
  seat->libinput_seat = libinput_seat;

  device = _clutter_input_device_evdev_new_virtual (
    manager, seat, CLUTTER_POINTER_DEVICE);
  _clutter_input_device_set_stage (device, priv->stage);
  _clutter_device_manager_add_device (manager, device);
  seat->core_pointer = device;

  /* Clutter has the notion of global "core" pointers and keyboard devices,
   * so we need to have a main seat to get them from. Make whatever seat comes
   * first the main seat. */
  if (priv->main_seat == NULL)
    priv->main_seat = seat;

  device = _clutter_input_device_evdev_new_virtual (
    manager, seat, CLUTTER_KEYBOARD_DEVICE);
  _clutter_input_device_set_stage (device, priv->stage);
  _clutter_device_manager_add_device (manager, device);
  seat->core_keyboard = device;

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

      xkb_keymap_unref (keymap);
    }

  seat->repeat = TRUE;
  seat->repeat_delay = 250;     /* ms */
  seat->repeat_interval = 33;   /* ms */

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

  xkb_state_unref (seat->xkb);

  clear_repeat_timer (seat);

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
      seat = clutter_seat_evdev_new (manager_evdev, libinput_seat);
      priv->seats = g_slist_append (priv->seats, seat);
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
        guint32 time, key, key_state;
        struct libinput_event_keyboard *key_event =
          libinput_event_get_keyboard_event (event);
        device = libinput_device_get_user_data (libinput_device);

        time = libinput_event_keyboard_get_time (key_event);
        key = libinput_event_keyboard_get_key (key_event);
        key_state = libinput_event_keyboard_get_key_state (key_event) ==
                    LIBINPUT_KEYBOARD_KEY_STATE_PRESSED;
        notify_key_device (device, time, key, key_state, TRUE);

        break;
      }

    case LIBINPUT_EVENT_POINTER_MOTION:
      {
        guint32 time;
        li_fixed_t dx, dy;
        struct libinput_event_pointer *motion_event =
          libinput_event_get_pointer_event (event);
        device = libinput_device_get_user_data (libinput_device);

        time = libinput_event_pointer_get_time (motion_event);
        dx = libinput_event_pointer_get_dx (motion_event);
        dy = libinput_event_pointer_get_dy (motion_event);
        notify_relative_motion (device, time, dx, dy);

        break;
      }

    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
      {
        guint32 time;
        li_fixed_t x, y;
        gfloat stage_width, stage_height;
        ClutterStage *stage;
        struct libinput_event_pointer *motion_event =
          libinput_event_get_pointer_event (event);
        device = libinput_device_get_user_data (libinput_device);

        stage = _clutter_input_device_get_stage (device);
        if (!stage)
          break;

        stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
        stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

        time = libinput_event_pointer_get_time (motion_event);
        x = libinput_event_pointer_get_absolute_x_transformed (motion_event,
                                                               stage_width);
        y = libinput_event_pointer_get_absolute_y_transformed (motion_event,
                                                               stage_height);
        notify_absolute_motion (device,
                                time,
                                li_fixed_to_double(x),
                                li_fixed_to_double(y));

        break;
      }

    case LIBINPUT_EVENT_POINTER_BUTTON:
      {
        guint32 time, button, button_state;
        struct libinput_event_pointer *button_event =
          libinput_event_get_pointer_event (event);
        device = libinput_device_get_user_data (libinput_device);

        time = libinput_event_pointer_get_time (button_event);
        button = libinput_event_pointer_get_button (button_event);
        button_state = libinput_event_pointer_get_button_state (button_event) ==
                       LIBINPUT_POINTER_BUTTON_STATE_PRESSED;
        notify_button (device, time, button, button_state);

        break;
      }

    case LIBINPUT_EVENT_POINTER_AXIS:
      {
        gdouble value, dx = 0.0, dy = 0.0;
        guint32 time;
        enum libinput_pointer_axis axis;
        struct libinput_event_pointer *axis_event =
          libinput_event_get_pointer_event (event);
        device = libinput_device_get_user_data (libinput_device);

        time = libinput_event_pointer_get_time (axis_event);
        value = li_fixed_to_double (
          libinput_event_pointer_get_axis_value (axis_event));
        axis = libinput_event_pointer_get_axis (axis_event);

        switch (axis)
          {
          case LIBINPUT_POINTER_AXIS_VERTICAL_SCROLL:
            dx = 0;
            dy = value;
            break;

          case LIBINPUT_POINTER_AXIS_HORIZONTAL_SCROLL:
            dx = value;
            dy = 0;
            break;

          }

        notify_scroll (device, time, dx, dy);
        break;

      }

    default:
      handled = FALSE;
    }

  return handled;
}

static void
process_event (ClutterDeviceManagerEvdev *manager_evdev,
               struct libinput_event *event)
{
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
  if (!udev)
    {
      g_warning ("Failed to create udev object");
      return;
    }

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (gobject);
  priv = manager_evdev->priv;

  priv->libinput = libinput_udev_create_for_seat (&libinput_interface,
                                                  manager_evdev,
                                                  udev,
                                                  "seat0");
  udev_unref (udev);

  if (!priv->libinput)
    {
      g_warning ("Failed to create libinput object");
      return;
    }

  dispatch_libinput (manager_evdev);

  g_assert (priv->main_seat != NULL);
  g_assert (priv->main_seat->core_pointer != NULL);
  _clutter_input_device_set_coords (priv->main_seat->core_pointer,
                                    NULL,
                                    INITIAL_POINTER_X, INITIAL_POINTER_Y,
                                    NULL);

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
  GSList *l;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (object);
  priv = manager_evdev->priv;

  for (l = priv->seats; l; l = g_slist_next (l))
    {
      ClutterSeatEvdev *seat = l->data;

      clutter_seat_evdev_free (seat);
    }
  g_slist_free (priv->seats);
  g_slist_free (priv->devices);

  clutter_event_source_free (priv->event_source);

  if (priv->constrain_data_notify)
    priv->constrain_data_notify (priv->constrain_data);

  libinput_destroy (priv->libinput);

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
  GSList *iter;
  ClutterSeatEvdev *seat;
  xkb_mod_mask_t latched_mods;
  xkb_mod_mask_t locked_mods;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  priv = manager_evdev->priv;

  for (iter = priv->seats; iter; iter = iter->next)
    {
      seat = iter->data;

      latched_mods = xkb_state_serialize_mods (seat->xkb,
                                               XKB_STATE_MODS_LATCHED);
      locked_mods = xkb_state_serialize_mods (seat->xkb,
                                              XKB_STATE_MODS_LOCKED);
      xkb_state_unref (seat->xkb);
      seat->xkb = xkb_state_new (keymap);

      xkb_state_update_mask (seat->xkb,
                             0, /* depressed */
                             latched_mods,
                             locked_mods,
                             0, 0, 0);

      seat->caps_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_CAPS);
      seat->num_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_NUM);
      seat->scroll_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_SCROLL);

      clutter_seat_evdev_sync_leds (seat);
    }
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
 * clutter_evdev_set_pointer_constrain_callback:
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 * @callback: the callback
 * @user_data:
 * @user_data_notify:
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
