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
#include "clutter-seat-evdev.h"
#include "clutter-virtual-input-device-evdev.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-manager.h"
#include "clutter-xkb-utils.h"
#include "clutter-backend-private.h"
#include "clutter-evdev.h"
#include "clutter-stage-private.h"
#include "clutter-input-device-tool-evdev.h"

#include "clutter-device-manager-evdev.h"

/*
 * Clutter makes the assumption that two core devices have ID's 2 and 3 (core
 * pointer and core keyboard).
 *
 * Since the two first devices that will ever be created will be the virtual
 * pointer and virtual keyboard of the first seat, we fulfill the made
 * assumptions by having the first device having ID 2 and following 3.
 */
#define INITIAL_DEVICE_ID 2

typedef struct _ClutterEventFilter ClutterEventFilter;

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

  ClutterRelativeMotionFilter relative_motion_filter;
  gpointer relative_motion_filter_user_data;

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
static gchar *                    evdev_seat_id;

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
  "pad",                /* CLUTTER_PAD_DEVICE */
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

void
_clutter_device_manager_evdev_constrain_pointer (ClutterDeviceManagerEvdev *manager_evdev,
                                                 ClutterInputDevice        *core_pointer,
                                                 uint64_t                   time_us,
                                                 float                      x,
                                                 float                      y,
                                                 float                     *new_x,
                                                 float                     *new_y)
{
  if (manager_evdev->priv->constrain_callback)
    {
      manager_evdev->priv->constrain_callback (core_pointer,
                                               us2ms (time_us),
                                               x, y,
                                               new_x, new_y,
					       manager_evdev->priv->constrain_data);
    }
  else
    {
      ClutterActor *stage = CLUTTER_ACTOR (manager_evdev->priv->stage);
      float stage_width = clutter_actor_get_width (stage);
      float stage_height = clutter_actor_get_height (stage);

      x = CLAMP (x, 0.f, stage_width - 1);
      y = CLAMP (y, 0.f, stage_height - 1);
    }
}

void
_clutter_device_manager_evdev_filter_relative_motion (ClutterDeviceManagerEvdev *manager_evdev,
                                                      ClutterInputDevice        *device,
                                                      float                      x,
                                                      float                      y,
                                                      float                     *dx,
                                                      float                     *dy)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;

  if (!priv->relative_motion_filter)
    return;

  priv->relative_motion_filter (device, x, y, dx, dy,
                                priv->relative_motion_filter_user_data);
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
  clutter_input_device_evdev_translate_coordinates (input_device, stage,
                                                    &event->motion.x,
                                                    &event->motion.y);
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
notify_relative_tool_motion (ClutterInputDevice *input_device,
                             guint64             time_us,
                             gfloat              dx,
                             gfloat              dy,
                             gdouble            *axes)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterEvent *event;
  ClutterSeatEvdev *seat;
  gfloat x, y;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);
  x = input_device->current_x + dx;
  y = input_device->current_y + dy;

  _clutter_device_manager_evdev_filter_relative_motion (seat->manager_evdev,
                                                        input_device,
                                                        seat->pointer_x,
                                                        seat->pointer_y,
                                                        &dx,
                                                        &dy);

  event = new_absolute_motion_event (input_device, time_us, x, y, axes);
  _clutter_evdev_event_set_relative_motion (event, dx, dy, 0, 0);

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
  clutter_input_device_evdev_translate_coordinates (input_device, stage,
                                                    &event->touch.x,
                                                    &event->touch.y);

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
                            gdouble                      scale,
                            guint                        n_fingers)
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
  event->touchpad_pinch.n_fingers = n_fingers;

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
notify_pad_button (ClutterInputDevice *input_device,
                   guint64             time_us,
                   guint32             button,
                   guint32             mode_group,
                   guint32             mode,
                   guint32             pressed)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  if (pressed)
    event = clutter_event_new (CLUTTER_PAD_BUTTON_PRESS);
  else
    event = clutter_event_new (CLUTTER_PAD_BUTTON_RELEASE);

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  _clutter_evdev_event_set_time_usec (event, time_us);
  event->pad_button.stage = stage;
  event->pad_button.button = button;
  event->pad_button.group = mode_group;
  event->pad_button.mode = mode;
  clutter_event_set_device (event, input_device);
  clutter_event_set_source_device (event, input_device);
  clutter_event_set_time (event, us2ms (time_us));

  _clutter_input_device_set_stage (seat->core_pointer, stage);

  queue_event (event);
}

static void
notify_pad_strip (ClutterInputDevice *input_device,
                  guint64             time_us,
                  guint32             strip_number,
                  guint32             strip_source,
                  guint32             mode_group,
                  guint32             mode,
                  gdouble             value)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterInputDevicePadSource source;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  if (strip_source == LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER)
    source = CLUTTER_INPUT_DEVICE_PAD_SOURCE_FINGER;
  else
    source = CLUTTER_INPUT_DEVICE_PAD_SOURCE_UNKNOWN;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (CLUTTER_PAD_STRIP);
  _clutter_evdev_event_set_time_usec (event, time_us);
  event->pad_strip.strip_source = source;
  event->pad_strip.stage = stage;
  event->pad_strip.strip_number = strip_number;
  event->pad_strip.value = value;
  event->pad_strip.group = mode_group;
  event->pad_strip.mode = mode;
  clutter_event_set_device (event, input_device);
  clutter_event_set_source_device (event, input_device);
  clutter_event_set_time (event, us2ms (time_us));

  _clutter_input_device_set_stage (seat->core_pointer, stage);

  queue_event (event);
}

static void
notify_pad_ring (ClutterInputDevice *input_device,
                 guint64             time_us,
                 guint32             ring_number,
                 guint32             ring_source,
                 guint32             mode_group,
                 guint32             mode,
                 gdouble             angle)
{
  ClutterInputDeviceEvdev *device_evdev;
  ClutterInputDevicePadSource source;
  ClutterSeatEvdev *seat;
  ClutterStage *stage;
  ClutterEvent *event;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (stage == NULL)
    return;

  if (ring_source == LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER)
    source = CLUTTER_INPUT_DEVICE_PAD_SOURCE_FINGER;
  else
    source = CLUTTER_INPUT_DEVICE_PAD_SOURCE_UNKNOWN;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (input_device);
  seat = _clutter_input_device_evdev_get_seat (device_evdev);

  event = clutter_event_new (CLUTTER_PAD_RING);
  _clutter_evdev_event_set_time_usec (event, time_us);
  event->pad_ring.ring_source = source;
  event->pad_ring.stage = stage;
  event->pad_ring.ring_number = ring_number;
  event->pad_ring.angle = angle;
  event->pad_ring.group = mode_group;
  event->pad_ring.mode = mode;
  clutter_event_set_device (event, input_device);
  clutter_event_set_source_device (event, input_device);
  clutter_event_set_time (event, us2ms (time_us));

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
    clutter_seat_evdev_clear_repeat_timer (seat);

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

  if (evdev_device->last_tool != tool)
    {
      evdev_device->last_tool = tool;
      g_signal_emit_by_name (clutter_device_manager_get_default (),
                             "tool-changed", input_device, tool);
    }
}

static gdouble *
translate_tablet_axes (struct libinput_event_tablet_tool *tablet_event,
                       ClutterInputDeviceTool            *tool)
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
      value = clutter_input_device_tool_evdev_translate_pressure (tool, value);
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

static ClutterSeatEvdev *
seat_from_device (ClutterInputDevice *device)
{
  ClutterInputDeviceEvdev *device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);

  return _clutter_input_device_evdev_get_seat (device_evdev);
}

static void
notify_continuous_axis (ClutterSeatEvdev              *seat,
                        ClutterInputDevice            *device,
                        uint64_t                       time_us,
                        ClutterScrollSource            scroll_source,
                        struct libinput_event_pointer *axis_event)
{
  gdouble dx = 0.0, dy = 0.0;
  ClutterScrollFinishFlags finish_flags = CLUTTER_SCROLL_FINISHED_NONE;

  if (libinput_event_pointer_has_axis (axis_event,
                                       LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL))
    {
      dx = libinput_event_pointer_get_axis_value (
          axis_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);

      if (fabs (dx) < DBL_EPSILON)
        finish_flags |= CLUTTER_SCROLL_FINISHED_HORIZONTAL;
    }
  if (libinput_event_pointer_has_axis (axis_event,
                                       LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
    {
      dy = libinput_event_pointer_get_axis_value (
          axis_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);

      if (fabs (dy) < DBL_EPSILON)
        finish_flags |= CLUTTER_SCROLL_FINISHED_VERTICAL;
    }

  clutter_seat_evdev_notify_scroll_continuous (seat, device, time_us,
                                               dx, dy,
                                               scroll_source, finish_flags);
}

static void
notify_discrete_axis (ClutterSeatEvdev              *seat,
                      ClutterInputDevice            *device,
                      uint64_t                       time_us,
                      ClutterScrollSource            scroll_source,
                      struct libinput_event_pointer *axis_event)
{
  gdouble discrete_dx = 0.0, discrete_dy = 0.0;

  if (libinput_event_pointer_has_axis (axis_event,
                                       LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL))
    {
      discrete_dx = libinput_event_pointer_get_axis_value_discrete (
          axis_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
    }
  if (libinput_event_pointer_has_axis (axis_event,
                                       LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
    {
      discrete_dy = libinput_event_pointer_get_axis_value_discrete (
          axis_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
    }

  clutter_seat_evdev_notify_discrete_scroll (seat, device,
                                             time_us,
                                             discrete_dx, discrete_dy,
                                             scroll_source);
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

        clutter_seat_evdev_notify_key (seat_from_device (device),
                                       device,
                                       time_us, key, key_state, TRUE);

        break;
      }

    case LIBINPUT_EVENT_POINTER_MOTION:
      {
        struct libinput_event_pointer *pointer_event =
          libinput_event_get_pointer_event (event);
        uint64_t time_us;
        double dx;
        double dy;
        double dx_unaccel;
        double dy_unaccel;

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_pointer_get_time_usec (pointer_event);
        dx = libinput_event_pointer_get_dx (pointer_event);
        dy = libinput_event_pointer_get_dy (pointer_event);
        dx_unaccel = libinput_event_pointer_get_dx_unaccelerated (pointer_event);
        dy_unaccel = libinput_event_pointer_get_dy_unaccelerated (pointer_event);

        clutter_seat_evdev_notify_relative_motion (seat_from_device (device),
                                                   device,
                                                   time_us,
                                                   dx, dy,
                                                   dx_unaccel, dy_unaccel);

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

        clutter_seat_evdev_notify_absolute_motion (seat_from_device (device),
                                                   device,
                                                   time_us,
                                                   x, y,
                                                   NULL);

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

        clutter_seat_evdev_notify_button (seat_from_device (device), device,
                                          time_us, button, button_state);
        break;
      }

    case LIBINPUT_EVENT_POINTER_AXIS:
      {
        guint64 time_us;
        enum libinput_pointer_axis_source source;
        struct libinput_event_pointer *axis_event =
          libinput_event_get_pointer_event (event);
        ClutterSeatEvdev *seat;
        ClutterScrollSource scroll_source;

        device = libinput_device_get_user_data (libinput_device);
        seat = _clutter_input_device_evdev_get_seat (CLUTTER_INPUT_DEVICE_EVDEV (device));

        time_us = libinput_event_pointer_get_time_usec (axis_event);
        source = libinput_event_pointer_get_axis_source (axis_event);
        scroll_source = translate_scroll_source (source);

        /* libinput < 0.8 sent wheel click events with value 10. Since 0.8
           the value is the angle of the click in degrees. To keep
           backwards-compat with existing clients, we just send multiples of
           the click count. */

        switch (scroll_source)
          {
          case CLUTTER_SCROLL_SOURCE_WHEEL:
            notify_discrete_axis (seat, device, time_us, scroll_source,
                                  axis_event);
            break;
          case CLUTTER_SCROLL_SOURCE_FINGER:
          case CLUTTER_SCROLL_SOURCE_CONTINUOUS:
          case CLUTTER_SCROLL_SOURCE_UNKNOWN:
            notify_continuous_axis (seat, device, time_us, scroll_source,
                                    axis_event);
            break;
          }
        break;
      }

    case LIBINPUT_EVENT_TOUCH_DOWN:
      {
        gint32 slot;
        guint64 time_us;
        double x, y;
        gfloat stage_width, stage_height;
        ClutterSeatEvdev *seat;
        ClutterStage *stage;
        ClutterTouchState *touch_state;
        struct libinput_event_touch *touch_event =
          libinput_event_get_touch_event (event);

        device = libinput_device_get_user_data (libinput_device);
        seat = _clutter_input_device_evdev_get_seat (CLUTTER_INPUT_DEVICE_EVDEV (device));

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

        touch_state = clutter_seat_evdev_add_touch (seat, slot);
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
        ClutterSeatEvdev *seat;
        ClutterTouchState *touch_state;
        struct libinput_event_touch *touch_event =
          libinput_event_get_touch_event (event);

        device = libinput_device_get_user_data (libinput_device);
        seat = _clutter_input_device_evdev_get_seat (CLUTTER_INPUT_DEVICE_EVDEV (device));

        slot = libinput_event_touch_get_slot (touch_event);
        time_us = libinput_event_touch_get_time_usec (touch_event);
        touch_state = clutter_seat_evdev_get_touch (seat, slot);

        notify_touch_event (device, CLUTTER_TOUCH_END, time_us, slot,
			    touch_state->coords.x, touch_state->coords.y);
        clutter_seat_evdev_remove_touch (seat, slot);

        break;
      }

    case LIBINPUT_EVENT_TOUCH_MOTION:
      {
        gint32 slot;
        guint64 time_us;
        double x, y;
        gfloat stage_width, stage_height;
        ClutterSeatEvdev *seat;
        ClutterStage *stage;
        ClutterTouchState *touch_state;
        struct libinput_event_touch *touch_event =
          libinput_event_get_touch_event (event);

        device = libinput_device_get_user_data (libinput_device);
        seat = _clutter_input_device_evdev_get_seat (CLUTTER_INPUT_DEVICE_EVDEV (device));

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

        touch_state = clutter_seat_evdev_get_touch (seat, slot);
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
        guint n_fingers;
        guint64 time_us;

        if (libinput_event_get_type (event) == LIBINPUT_EVENT_GESTURE_PINCH_BEGIN)
          phase = CLUTTER_TOUCHPAD_GESTURE_PHASE_BEGIN;
        else
          phase = libinput_event_gesture_get_cancelled (gesture_event) ?
            CLUTTER_TOUCHPAD_GESTURE_PHASE_CANCEL : CLUTTER_TOUCHPAD_GESTURE_PHASE_END;

        n_fingers = libinput_event_gesture_get_finger_count (gesture_event);
        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_gesture_get_time_usec (gesture_event);
        notify_pinch_gesture_event (device, phase, time_us, 0, 0, 0, 0, n_fingers);
        break;
      }
    case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
      {
        struct libinput_event_gesture *gesture_event =
          libinput_event_get_gesture_event (event);
        gdouble angle_delta, scale, dx, dy;
        guint n_fingers;
        guint64 time_us;

        n_fingers = libinput_event_gesture_get_finger_count (gesture_event);
        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_gesture_get_time_usec (gesture_event);
        angle_delta = libinput_event_gesture_get_angle_delta (gesture_event);
        scale = libinput_event_gesture_get_scale (gesture_event);
        dx = libinput_event_gesture_get_dx (gesture_event);
        dy = libinput_event_gesture_get_dx (gesture_event);

        notify_pinch_gesture_event (device,
                                    CLUTTER_TOUCHPAD_GESTURE_PHASE_UPDATE,
                                    time_us, dx, dy, angle_delta, scale, n_fingers);
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
        double x, y, dx, dy, *axes;
        gfloat stage_width, stage_height;
        ClutterStage *stage;
        struct libinput_event_tablet_tool *tablet_event =
          libinput_event_get_tablet_tool_event (event);
        ClutterInputDeviceEvdev *evdev_device;

        device = libinput_device_get_user_data (libinput_device);
        evdev_device = CLUTTER_INPUT_DEVICE_EVDEV (device);

        stage = _clutter_input_device_get_stage (device);
        if (!stage)
          break;

        axes = translate_tablet_axes (tablet_event,
                                      evdev_device->last_tool);
        if (!axes)
          break;

        stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
        stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

        time = libinput_event_tablet_tool_get_time_usec (tablet_event);

        if (clutter_input_device_get_mapping_mode (device) == CLUTTER_INPUT_DEVICE_MAPPING_RELATIVE ||
            clutter_input_device_tool_get_tool_type (evdev_device->last_tool) == CLUTTER_INPUT_DEVICE_TOOL_MOUSE ||
            clutter_input_device_tool_get_tool_type (evdev_device->last_tool) == CLUTTER_INPUT_DEVICE_TOOL_LENS)
          {
            dx = libinput_event_tablet_tool_get_dx (tablet_event);
            dy = libinput_event_tablet_tool_get_dy (tablet_event);
            notify_relative_tool_motion (device, time, dx, dy, axes);
          }
        else
          {
            x = libinput_event_tablet_tool_get_x_transformed (tablet_event, stage_width);
            y = libinput_event_tablet_tool_get_y_transformed (tablet_event, stage_height);
            notify_absolute_motion (device, time, x, y, axes);
          }

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

        if (state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN)
          input_device_update_tool (device, libinput_tool);
        notify_proximity (device, time, state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN);
        if (state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_OUT)
          input_device_update_tool (device, NULL);
        break;
      }
    case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
      {
        guint64 time_us;
        guint32 button_state;
        struct libinput_event_tablet_tool *tablet_event =
          libinput_event_get_tablet_tool_event (event);
        guint tablet_button;

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_tablet_tool_get_time_usec (tablet_event);
        tablet_button = libinput_event_tablet_tool_get_button (tablet_event);

        button_state = libinput_event_tablet_tool_get_button_state (tablet_event) ==
                       LIBINPUT_BUTTON_STATE_PRESSED;

        clutter_seat_evdev_notify_button (seat_from_device (device), device,
                                          time_us, tablet_button, button_state);
        break;
      }
    case LIBINPUT_EVENT_TABLET_TOOL_TIP:
      {
        guint64 time_us;
        guint32 button_state;
        struct libinput_event_tablet_tool *tablet_event =
          libinput_event_get_tablet_tool_event (event);

        device = libinput_device_get_user_data (libinput_device);
        time_us = libinput_event_tablet_tool_get_time_usec (tablet_event);

        button_state = libinput_event_tablet_tool_get_tip_state (tablet_event) ==
                       LIBINPUT_TABLET_TOOL_TIP_DOWN;

        clutter_seat_evdev_notify_button (seat_from_device (device), device,
                                          time_us, BTN_TOUCH, button_state);
        break;
      }
    case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
      {
        guint64 time;
        guint32 button_state, button, group, mode;
        struct libinput_tablet_pad_mode_group *mode_group;
        struct libinput_event_tablet_pad *pad_event =
          libinput_event_get_tablet_pad_event (event);

        device = libinput_device_get_user_data (libinput_device);
        time = libinput_event_tablet_pad_get_time_usec (pad_event);

        mode_group = libinput_event_tablet_pad_get_mode_group (pad_event);
        group = libinput_tablet_pad_mode_group_get_index (mode_group);
        mode = libinput_event_tablet_pad_get_mode (pad_event);

        button = libinput_event_tablet_pad_get_button_number (pad_event);
        button_state = libinput_event_tablet_pad_get_button_state (pad_event) ==
                       LIBINPUT_BUTTON_STATE_PRESSED;
        notify_pad_button (device, time, button, group, mode, button_state);
        break;
      }
    case LIBINPUT_EVENT_TABLET_PAD_STRIP:
      {
        guint64 time;
        guint32 number, source, group, mode;
        struct libinput_tablet_pad_mode_group *mode_group;
        struct libinput_event_tablet_pad *pad_event =
          libinput_event_get_tablet_pad_event (event);
        gdouble value;

        device = libinput_device_get_user_data (libinput_device);
        time = libinput_event_tablet_pad_get_time_usec (pad_event);
        number = libinput_event_tablet_pad_get_strip_number (pad_event);
        value = libinput_event_tablet_pad_get_strip_position (pad_event);
        source = libinput_event_tablet_pad_get_strip_source (pad_event);

        mode_group = libinput_event_tablet_pad_get_mode_group (pad_event);
        group = libinput_tablet_pad_mode_group_get_index (mode_group);
        mode = libinput_event_tablet_pad_get_mode (pad_event);

        notify_pad_strip (device, time, number, source, group, mode, value);
        break;
      }
    case LIBINPUT_EVENT_TABLET_PAD_RING:
      {
        guint64 time;
        guint32 number, source, group, mode;
        struct libinput_tablet_pad_mode_group *mode_group;
        struct libinput_event_tablet_pad *pad_event =
          libinput_event_get_tablet_pad_event (event);
        gdouble angle;

        device = libinput_device_get_user_data (libinput_device);
        time = libinput_event_tablet_pad_get_time_usec (pad_event);
        number = libinput_event_tablet_pad_get_ring_number (pad_event);
        angle = libinput_event_tablet_pad_get_ring_position (pad_event);
        source = libinput_event_tablet_pad_get_ring_source (pad_event);

        mode_group = libinput_event_tablet_pad_get_mode_group (pad_event);
        group = libinput_tablet_pad_mode_group_get_index (mode_group);
        mode = libinput_event_tablet_pad_get_mode (pad_event);

        notify_pad_ring (device, time, number, source, group, mode, angle);
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

static ClutterVirtualInputDevice *
clutter_device_manager_evdev_create_virtual_device (ClutterDeviceManager  *manager,
                                                    ClutterInputDeviceType device_type)
{
  ClutterDeviceManagerEvdev *manager_evdev =
    CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;

  return g_object_new (CLUTTER_TYPE_VIRTUAL_INPUT_DEVICE_EVDEV,
                       "device-manager", manager,
                       "seat", priv->main_seat,
                       "device-type", device_type,
                       NULL);
}

static void
clutter_device_manager_evdev_compress_motion (ClutterDeviceManager *device_manger,
                                              ClutterEvent         *event,
                                              const ClutterEvent   *to_discard)
{
  double dx, dy;
  double dx_unaccel, dy_unaccel;
  double dst_dx = 0.0, dst_dy = 0.0;
  double dst_dx_unaccel = 0.0, dst_dy_unaccel = 0.0;

  if (!clutter_evdev_event_get_relative_motion (to_discard,
                                                &dx, &dy,
                                                &dx_unaccel, &dy_unaccel))
    return;

  clutter_evdev_event_get_relative_motion (event,
                                           &dst_dx, &dst_dy,
                                           &dst_dx_unaccel, &dst_dy_unaccel);
  _clutter_evdev_event_set_relative_motion (event,
                                            dx + dst_dx,
                                            dy + dst_dy,
                                            dx_unaccel + dst_dx_unaccel,
                                            dy_unaccel + dst_dy_unaccel);
}

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
  struct xkb_context *ctx;
  struct xkb_rule_names names;

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

  if (libinput_udev_assign_seat (priv->libinput,
                                 evdev_seat_id ? evdev_seat_id : "seat0") == -1)
    {
      g_critical ("Failed to assign a seat to the libinput object.");
      libinput_unref (priv->libinput);
      priv->libinput = NULL;
      return;
    }

  udev_unref (udev);

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = option_xkb_layout;
  names.variant = option_xkb_variant;
  names.options = option_xkb_options;

  ctx = xkb_context_new (0);
  g_assert (ctx);
  priv->keymap = xkb_keymap_new_from_names (ctx, &names, 0);
  xkb_context_unref (ctx);

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
  manager_class->create_virtual_device = clutter_device_manager_evdev_create_virtual_device;
  manager_class->compress_motion = clutter_device_manager_evdev_compress_motion;
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

void
_clutter_device_manager_evdev_dispatch (ClutterDeviceManagerEvdev *manager_evdev)
{
  dispatch_libinput (manager_evdev);
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

struct xkb_keymap *
_clutter_device_manager_evdev_get_keymap (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;

  return priv->keymap;
}

ClutterStage *
_clutter_device_manager_evdev_get_stage (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;

  return priv->stage;
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
 * clutter_evdev_get_keyboard_layout_index: (skip)
 */
xkb_layout_index_t
clutter_evdev_get_keyboard_layout_index (ClutterDeviceManager *evdev)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  struct xkb_state *state;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  state = manager_evdev->priv->main_seat->xkb;

  return xkb_state_serialize_layout (state, XKB_STATE_LAYOUT_LOCKED);
}

/**
 * clutter_evdev_set_keyboard_numlock: (skip)
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 * @numlock_set: TRUE to set NumLock ON, FALSE otherwise.
 *
 * Sets the NumLock state on the backend's #xkb_state .
 *
 * Stability: unstable
 */
void
clutter_evdev_set_keyboard_numlock (ClutterDeviceManager *evdev,
                                    gboolean              numlock_state)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *iter;
  xkb_mod_mask_t numlock;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  priv = manager_evdev->priv;
  numlock = (1 << xkb_keymap_mod_get_index(priv->keymap, "Mod2"));

  for (iter = priv->seats; iter; iter = iter->next)
    {
      ClutterSeatEvdev *seat = iter->data;
      xkb_mod_mask_t depressed_mods;
      xkb_mod_mask_t latched_mods;
      xkb_mod_mask_t locked_mods;
      xkb_mod_mask_t group_mods;

      depressed_mods = xkb_state_serialize_mods (seat->xkb, XKB_STATE_MODS_DEPRESSED);
      latched_mods = xkb_state_serialize_mods (seat->xkb, XKB_STATE_MODS_LATCHED);
      locked_mods = xkb_state_serialize_mods (seat->xkb, XKB_STATE_MODS_LOCKED);
      group_mods = xkb_state_serialize_layout (seat->xkb, XKB_STATE_LAYOUT_EFFECTIVE);

      if (numlock_state)
        locked_mods |= numlock;
      else
        locked_mods &= ~numlock;

      xkb_state_update_mask (seat->xkb,
                             depressed_mods,
                             latched_mods,
                             locked_mods,
                             0, 0,
                             group_mods);

      clutter_seat_evdev_sync_leds (seat);
    }
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

void
clutter_evdev_set_relative_motion_filter (ClutterDeviceManager       *evdev,
                                          ClutterRelativeMotionFilter filter,
                                          gpointer                    user_data)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  priv = manager_evdev->priv;

  priv->relative_motion_filter = filter;
  priv->relative_motion_filter_user_data = user_data;
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

/**
 * clutter_evdev_set_seat_id:
 * @seat_id: The seat ID
 *
 * Sets the seat to assign to the libinput context.
 *
 * For reliable effects, this function must be called before clutter_init().
 */
void
clutter_evdev_set_seat_id (const gchar *seat_id)
{
  g_free (evdev_seat_id);
  evdev_seat_id = g_strdup (seat_id);
}
