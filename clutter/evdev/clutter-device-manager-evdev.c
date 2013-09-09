/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corp.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gudev/gudev.h>
#include <libevdev/libevdev.h>

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

/* These are the same as the XInput2 IDs */
#define CORE_POINTER_ID 2
#define CORE_KEYBOARD_ID 3

#define FIRST_SLAVE_ID 4
#define INVALID_SLAVE_ID 255

#define AUTOREPEAT_VALUE 2

struct _ClutterDeviceManagerEvdevPrivate
{
  GUdevClient *udev_client;

  ClutterStage *stage;
  gboolean released;

  GSList *devices;          /* list of ClutterInputDeviceEvdevs */
  GSList *event_sources;    /* list of the event sources */

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;

  ClutterPointerConstrainCallback constrain_callback;
  gpointer                        constrain_data;
  GDestroyNotify                  constrain_data_notify;

  ClutterStageManager *stage_manager;
  guint stage_added_handler;
  guint stage_removed_handler;

  GArray *keys;
  struct xkb_state *xkb;              /* XKB state object */
  xkb_led_index_t caps_lock_led;
  xkb_led_index_t num_lock_led;
  xkb_led_index_t scroll_lock_led;
  uint32_t button_state;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterDeviceManagerEvdev,
                            clutter_device_manager_evdev,
                            CLUTTER_TYPE_DEVICE_MANAGER)

static ClutterOpenDeviceCallback open_callback;
static gpointer                  open_callback_data;

static const gchar *subsystems[] = { "input", NULL };

/*
 * ClutterEventSource management
 *
 * The device manager is responsible for managing the GSource when devices
 * appear and disappear from the system.
 *
 * FIXME: For now, we associate a GSource with every single device. Starting
 * from glib 2.28 we can use g_source_add_child_source() to have a single
 * GSource for the device manager, each device becoming a child source. Revisit
 * this once we depend on glib >= 2.28.
 */


static const char *option_xkb_layout = "us";
static const char *option_xkb_variant = "";
static const char *option_xkb_options = "";

/*
 * ClutterEventSource for reading input devices
 */

typedef struct _ClutterEventSource  ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterInputDeviceEvdev *device;    /* back pointer to the slave evdev device */
  GPollFD event_poll_fd;              /* file descriptor of the /dev node */
  struct libevdev *dev;

  int dx, dy;
};

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
add_key (GArray  *keys,
	 guint32  key)
{
  g_array_append_val (keys, key);
}

static void
remove_key (GArray  *keys,
	    guint32  key)
{
  unsigned int i;

  for (i = 0; i < keys->len; i++)
    {
      if (g_array_index (keys, guint32, i) == key)
	{
	  g_array_remove_index_fast (keys, i);
	  return;
	}
    }
}

static void
sync_leds (ClutterDeviceManagerEvdev *manager_evdev)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  GSList *iter;
  int caps_lock, num_lock, scroll_lock;

  caps_lock = xkb_state_led_index_is_active (priv->xkb, priv->caps_lock_led);
  num_lock = xkb_state_led_index_is_active (priv->xkb, priv->num_lock_led);
  scroll_lock = xkb_state_led_index_is_active (priv->xkb, priv->scroll_lock_led);

  for (iter = priv->event_sources; iter; iter = iter->next)
    {
      ClutterEventSource *source = iter->data;

      if (libevdev_has_event_type (source->dev, EV_LED))
	libevdev_kernel_set_led_values (source->dev,
					LED_CAPSL, caps_lock == 1 ? LIBEVDEV_LED_ON : LIBEVDEV_LED_OFF,
					LED_NUML, num_lock == 1 ? LIBEVDEV_LED_ON : LIBEVDEV_LED_OFF,
					LED_SCROLLL, scroll_lock == 1 ? LIBEVDEV_LED_ON : LIBEVDEV_LED_OFF,
					-1);
    }
}

static void
notify_key_device (ClutterInputDevice *input_device,
		   guint32             time_,
		   guint32             key,
		   guint32             state,
		   gboolean            update_keys)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  enum xkb_state_component changed_state;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (input_device->device_manager);

  event = _clutter_key_event_new_from_evdev (input_device,
					     manager_evdev->priv->core_keyboard,
					     stage,
					     manager_evdev->priv->xkb,
					     manager_evdev->priv->button_state,
					     time_, key, state);

  /* We must be careful and not pass multiple releases to xkb, otherwise it gets
     confused and locks the modifiers */
  if (state != AUTOREPEAT_VALUE)
    {
      changed_state = xkb_state_update_key (manager_evdev->priv->xkb, event->key.hardware_keycode, state ? XKB_KEY_DOWN : XKB_KEY_UP);

      if (update_keys)
	{
	  if (state)
	    add_key (manager_evdev->priv->keys, event->key.hardware_keycode);
	  else
	    remove_key (manager_evdev->priv->keys, event->key.hardware_keycode);
	}
    }
  else
    changed_state = 0;

  queue_event (event);

  if (update_keys && (changed_state & XKB_STATE_LEDS))
    sync_leds (manager_evdev);
}

static void
notify_key (ClutterEventSource *source,
            guint32             time_,
            guint32             key,
            guint32             state)
{
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;

  notify_key_device (input_device, time_, key, state, TRUE);
}

static void
notify_relative_motion (ClutterEventSource *source,
			guint32             time_,
			gint                dx,
			gint                dy)
{
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  gfloat stage_width, stage_height, new_x, new_y;
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  ClutterPoint point;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (input_device->device_manager);

  stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
  stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

  event = clutter_event_new (CLUTTER_MOTION);

  clutter_input_device_get_coords (manager_evdev->priv->core_pointer, NULL, &point);
  new_x = point.x + dx;
  new_y = point.y + dy;

  if (manager_evdev->priv->constrain_callback)
    {
      manager_evdev->priv->constrain_callback (manager_evdev->priv->core_pointer, time_, &new_x, &new_y,
					       manager_evdev->priv->constrain_data);
    }
  else
    {
      new_x = CLAMP (new_x, 0.f, stage_width - 1);
      new_y = CLAMP (new_y, 0.f, stage_height - 1);
    }

  event->motion.time = time_;
  event->motion.stage = stage;
  event->motion.device = manager_evdev->priv->core_pointer;
  _clutter_xkb_translate_state (event, manager_evdev->priv->xkb, manager_evdev->priv->button_state);
  event->motion.x = new_x;
  event->motion.y = new_y;
  clutter_event_set_source_device (event, input_device);

  queue_event (event);
}

static void
notify_scroll (ClutterEventSource  *source,
	       guint32              time_,
	       gint32               value,
	       ClutterOrientation   orientation)
{
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterStage *stage;
  ClutterEvent *event = NULL;
  ClutterPoint point;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (input_device->device_manager);

  event = clutter_event_new (CLUTTER_SCROLL);

  event->scroll.time = time_;
  event->scroll.stage = CLUTTER_STAGE (stage);
  event->scroll.device = manager_evdev->priv->core_pointer;
  _clutter_xkb_translate_state (event, manager_evdev->priv->xkb, manager_evdev->priv->button_state);
  if (orientation == CLUTTER_ORIENTATION_VERTICAL)
    event->scroll.direction = value < 0 ? CLUTTER_SCROLL_DOWN : CLUTTER_SCROLL_UP;
  else
    event->scroll.direction = value < 0 ? CLUTTER_SCROLL_LEFT : CLUTTER_SCROLL_RIGHT;
  clutter_input_device_get_coords (manager_evdev->priv->core_pointer, NULL, &point);
  event->scroll.x = point.x;
  event->scroll.y = point.y;
  clutter_event_set_source_device (event, (ClutterInputDevice*) source->device);

  queue_event (event);
}

static void
notify_button (ClutterEventSource *source,
               guint32             time_,
               guint32             button,
               guint32             state)
{
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  ClutterDeviceManagerEvdev *manager_evdev;
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

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (input_device->device_manager);

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
    manager_evdev->priv->button_state |= maskmap[button - BTN_LEFT];
  else
    manager_evdev->priv->button_state &= ~maskmap[button - BTN_LEFT];

  event->button.time = time_;
  event->button.stage = CLUTTER_STAGE (stage);
  event->button.device = manager_evdev->priv->core_pointer;
  _clutter_xkb_translate_state (event, manager_evdev->priv->xkb, manager_evdev->priv->button_state);
  event->button.button = button_nr;
  clutter_input_device_get_coords (manager_evdev->priv->core_pointer, NULL, &point);
  event->button.x = point.x;
  event->button.y = point.y;
  clutter_event_set_source_device (event, (ClutterInputDevice*) source->device);

  queue_event (event);
}

static void
dispatch_one_event (ClutterEventSource *source,
		    struct input_event *e)
{
  guint32 _time;

  _time = e->time.tv_sec * 1000 + e->time.tv_usec / 1000;

  switch (e->type)
    {
    case EV_KEY:
      if (e->code < BTN_MISC || (e->code >= KEY_OK && e->code < BTN_TRIGGER_HAPPY))
	notify_key (source, _time, e->code, e->value);
      else if (e->code >= BTN_MOUSE && e->code < BTN_JOYSTICK)
	{
	  /* don't repeat mouse buttons */
	  if (e->value != AUTOREPEAT_VALUE)
	    notify_button (source, _time, e->code, e->value);
	}
      else
	{
	  /* We don't know about this code, ignore */
	}
      break;

    case EV_SYN:
      if (e->code == SYN_REPORT)
	{
	  /* Flush accumulated motion deltas */
	  if (source->dx != 0 || source->dy != 0)
	    {
	      notify_relative_motion (source, _time, source->dx, source->dy);
	      source->dx = 0;
	      source->dy = 0;
	    }
	}
      break;

    case EV_MSC:
      /* Nothing to do here */
      break;

    case EV_REL:
      /* compress the EV_REL events in dx/dy */
      switch (e->code)
	{
	case REL_X:
	  source->dx += e->value;
	  break;
	case REL_Y:
	  source->dy += e->value;
	  break;

	case REL_WHEEL:
	  notify_scroll (source, _time, e->value, CLUTTER_ORIENTATION_VERTICAL);
	  break;
	case REL_HWHEEL:
	  notify_scroll (source, _time, e->value, CLUTTER_ORIENTATION_HORIZONTAL);
	  break;
	}
      break;

    case EV_ABS:
    default:
      g_warning ("Unhandled event of type %d", e->type);
      break;
    }
}

static void
sync_source (ClutterEventSource *source)
{
  struct input_event ev;
  int err;
  const gchar *device_path;

  /* We read a SYN_DROPPED, ignore it and sync the device */
  err = libevdev_next_event (source->dev, LIBEVDEV_READ_SYNC, &ev);
  while (err == 1)
    {
      dispatch_one_event (source, &ev);
      err = libevdev_next_event (source->dev, LIBEVDEV_READ_SYNC, &ev);
    }

  if (err != -EAGAIN && CLUTTER_HAS_DEBUG (EVENT))
    {
      device_path = _clutter_input_device_evdev_get_device_path (source->device);

      CLUTTER_NOTE (EVENT, "Could not sync device (%s).", device_path);
    }
}

static void
fail_source (ClutterEventSource *source,
	     int                 error)
{
  ClutterDeviceManager *manager;
  ClutterInputDevice *device;
  const gchar *device_path;

  device = CLUTTER_INPUT_DEVICE (source->device);

  if (CLUTTER_HAS_DEBUG (EVENT))
    {
      device_path = _clutter_input_device_evdev_get_device_path (source->device);

      CLUTTER_NOTE (EVENT, "Could not read device (%s): %s. Removing.",
		    device_path, strerror (error));
    }

  /* remove the faulty device */
  manager = clutter_device_manager_get_default ();
  _clutter_device_manager_remove_device (manager, device);
}

static gboolean
clutter_event_dispatch (GSource     *g_source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterEventSource *source = (ClutterEventSource *) g_source;
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  struct input_event ev;
  ClutterEvent *event;
  ClutterStage *stage;
  int err;

  _clutter_threads_acquire_lock ();

  stage = _clutter_input_device_get_stage (input_device);

  /* Don't queue more events if we haven't finished handling the previous batch
   */
  if (clutter_events_pending ())
    goto queue_event;

  err = libevdev_next_event (source->dev, LIBEVDEV_READ_NORMAL, &ev);
  while (err != -EAGAIN)
    {
      if (err == 1)
	sync_source (source);
      else if (err == 0)
	dispatch_one_event (source, &ev);
      else
	{
	  fail_source (source, -err);
	  goto out;
	}

      err = libevdev_next_event (source->dev, LIBEVDEV_READ_NORMAL, &ev);
    }

 queue_event:
  /* Drop events if we don't have any stage to forward them to */
  if (stage == NULL)
    goto out;

  /* Pop an event off the queue if any */
  event = clutter_event_get ();

  if (event)
    {
      ClutterModifierType event_state;
      ClutterInputDevice *input_device;
      ClutterDeviceManagerEvdev *manager_evdev;

      input_device = CLUTTER_INPUT_DEVICE (source->device);
      manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (input_device->device_manager);

      /* forward the event into clutter for emission etc. */
      clutter_do_event (event);
      clutter_event_free (event);

      /* update the device states *after* the event */
      event_state = xkb_state_serialize_mods (manager_evdev->priv->xkb, XKB_STATE_MODS_EFFECTIVE);
      _clutter_input_device_set_state (manager_evdev->priv->core_pointer, event_state);
      _clutter_input_device_set_state (manager_evdev->priv->core_keyboard, event_state);
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

static GSource *
clutter_event_source_new (ClutterInputDeviceEvdev *input_device)
{
  GSource *source = g_source_new (&event_funcs, sizeof (ClutterEventSource));
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  const gchar *node_path;
  gint fd, clkid;
  GError *error;

  /* grab the udev input device node and open it */
  node_path = _clutter_input_device_evdev_get_device_path (input_device);

  CLUTTER_NOTE (EVENT, "Creating GSource for device %s", node_path);

  if (open_callback)
    {
      error = NULL;
      fd = open_callback (node_path, O_RDWR | O_NONBLOCK, open_callback_data, &error);

      if (fd < 0)
	{
	  g_warning ("Could not open device %s: %s", node_path, error->message);
	  g_error_free (error);
	  return NULL;
	}
    }
  else
    {
      fd = open (node_path, O_RDWR | O_NONBLOCK);
      if (fd < 0)
	{
	  g_warning ("Could not open device %s: %s", node_path, strerror (errno));
	  return NULL;
	}
    }

  /* Tell evdev to use the monotonic clock for its timestamps */
  clkid = CLOCK_MONOTONIC;
  ioctl (fd, EVIOCSCLOCKID, &clkid);

  /* setup the source */
  event_source->device = input_device;
  event_source->event_poll_fd.fd = fd;
  event_source->event_poll_fd.events = G_IO_IN;
  libevdev_new_from_fd (fd, &event_source->dev);

  /* and finally configure and attach the GSource */
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  return source;
}

static void
clutter_event_source_free (ClutterEventSource *source)
{
  GSource *g_source = (GSource *) source;
  const gchar *node_path;

  node_path = _clutter_input_device_evdev_get_device_path (source->device);

  CLUTTER_NOTE (EVENT, "Removing GSource for device %s", node_path);

  /* ignore the return value of close, it's not like we can do something
   * about it */
  close (source->event_poll_fd.fd);
  libevdev_free (source->dev);

  g_source_destroy (g_source);
  g_source_unref (g_source);
}

static ClutterEventSource *
find_source_by_device (ClutterDeviceManagerEvdev *manager,
                       ClutterInputDevice        *device)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager->priv;
  GSList *l;

  for (l = priv->event_sources; l; l = g_slist_next (l))
    {
      ClutterEventSource *source = l->data;

      if (source->device == (ClutterInputDeviceEvdev *) device)
        return source;
    }

  return NULL;
}

static gboolean
is_evdev (const gchar *sysfs_path)
{
  GRegex *regex;
  gboolean match;

  regex = g_regex_new ("/input[0-9]+/event[0-9]+$", 0, 0, NULL);
  match = g_regex_match (regex, sysfs_path, 0, NULL);

  g_regex_unref (regex);
  return match;
}

static void
evdev_add_device (ClutterDeviceManagerEvdev *manager_evdev,
                  GUdevDevice               *udev_device)
{
  ClutterDeviceManager *manager = (ClutterDeviceManager *) manager_evdev;
  ClutterInputDeviceType type = CLUTTER_EXTENSION_DEVICE;
  ClutterInputDevice *device;
  const gchar *device_file, *sysfs_path, *device_name;
  int id, ok;

  device_file = g_udev_device_get_device_file (udev_device);
  sysfs_path = g_udev_device_get_sysfs_path (udev_device);
  device_name = g_udev_device_get_name (udev_device);

  if (device_file == NULL || sysfs_path == NULL)
    return;

  if (g_udev_device_get_property (udev_device, "ID_INPUT") == NULL)
    return;

  /* Make sure to only add evdev devices, ie the device with a sysfs path that
   * finishes by input%d/event%d (We don't rely on the node name as this
   * policy is enforced by udev rules Vs API/ABI guarantees of sysfs) */
  if (!is_evdev (sysfs_path))
    return;

  /* Clutter assumes that device types are exclusive in the
   * ClutterInputDevice API */
  if (g_udev_device_has_property (udev_device, "ID_INPUT_KEYBOARD"))
    type = CLUTTER_KEYBOARD_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_MOUSE"))
    type = CLUTTER_POINTER_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_JOYSTICK"))
    type = CLUTTER_JOYSTICK_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_TABLET"))
    type = CLUTTER_TABLET_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_TOUCHPAD"))
    type = CLUTTER_TOUCHPAD_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_TOUCHSCREEN"))
    type = CLUTTER_TOUCHSCREEN_DEVICE;

  ok = sscanf (device_file, "/dev/input/event%d", &id);
  if (ok == 1)
    id += FIRST_SLAVE_ID;
  else
    id = INVALID_SLAVE_ID;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", id,
                         "name", device_name,
			 "device-manager", manager,
                         "device-type", type,
			 "device-mode", CLUTTER_INPUT_MODE_SLAVE,
                         "sysfs-path", sysfs_path,
                         "device-path", device_file,
                         "enabled", TRUE,
                         NULL);

  _clutter_input_device_set_stage (device, manager_evdev->priv->stage);

  _clutter_device_manager_add_device (manager, device);
  if (type == CLUTTER_KEYBOARD_DEVICE)
    {
      _clutter_input_device_set_associated_device (device, manager_evdev->priv->core_keyboard);
      _clutter_input_device_add_slave (manager_evdev->priv->core_keyboard, device);
    }
  else
    {
      _clutter_input_device_set_associated_device (device, manager_evdev->priv->core_pointer);
      _clutter_input_device_add_slave (manager_evdev->priv->core_pointer, device);
    }

  CLUTTER_NOTE (EVENT, "Added device %s, type %d, sysfs %s",
                device_file, type, sysfs_path);
}

static ClutterInputDeviceEvdev *
find_device_by_udev_device (ClutterDeviceManagerEvdev *manager_evdev,
                            GUdevDevice               *udev_device)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  GSList *l;
  const gchar *sysfs_path;

  sysfs_path = g_udev_device_get_sysfs_path (udev_device);
  if (sysfs_path == NULL)
    {
      g_message ("device file is NULL");
      return NULL;
    }

  for (l = priv->devices; l; l = g_slist_next (l))
    {
      ClutterInputDeviceEvdev *device = l->data;

      if (strcmp (sysfs_path,
                  _clutter_input_device_evdev_get_sysfs_path (device)) == 0)
        {
          return device;
        }
    }

  return NULL;
}

static void
evdev_remove_device (ClutterDeviceManagerEvdev *manager_evdev,
                     GUdevDevice               *device)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (manager_evdev);
  ClutterInputDeviceEvdev *device_evdev;
  ClutterInputDevice *input_device;

  device_evdev = find_device_by_udev_device (manager_evdev, device);
  if (device_evdev == NULL)
      return;

  input_device = CLUTTER_INPUT_DEVICE (device_evdev);
  _clutter_device_manager_remove_device (manager, input_device);
}

static void
on_uevent (GUdevClient *client,
           gchar       *action,
           GUdevDevice *device,
           gpointer     data)
{
  ClutterDeviceManagerEvdev *manager = CLUTTER_DEVICE_MANAGER_EVDEV (data);
  ClutterDeviceManagerEvdevPrivate *priv = manager->priv;

  if (priv->released)
    return;

  if (g_strcmp0 (action, "add") == 0)
    evdev_add_device (manager, device);
  else if (g_strcmp0 (action, "remove") == 0)
    evdev_remove_device (manager, device);
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
  GSource *source;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);

  priv->devices = g_slist_prepend (priv->devices, device);

  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_SLAVE)
    {
      /* Install the GSource for this device */
      source = clutter_event_source_new (device_evdev);
      g_assert (source != NULL);

      priv->event_sources = g_slist_prepend (priv->event_sources, source);
    }
}

static void
clutter_device_manager_evdev_remove_device (ClutterDeviceManager *manager,
                                            ClutterInputDevice   *device)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterEventSource *source;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  /* Remove the device */
  priv->devices = g_slist_remove (priv->devices, device);

  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_SLAVE)
    {
      /* Remove the source */
      source = find_source_by_device (manager_evdev, device);
      if (G_UNLIKELY (source == NULL))
	{
	  g_warning ("Trying to remove a device without a source installed ?!");
	  return;
	}

      clutter_event_source_free (source);
      priv->event_sources = g_slist_remove (priv->event_sources, source);
    }
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
      return priv->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return priv->core_keyboard;

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

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  for (l = priv->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id)
        return device;
    }

  return NULL;
}

static void
clutter_device_manager_evdev_probe_devices (ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv = self->priv;
  GList *devices, *l;

  devices = g_udev_client_query_by_subsystem (priv->udev_client, subsystems[0]);
  for (l = devices; l; l = g_list_next (l))
    {
      GUdevDevice *device = l->data;

      evdev_add_device (self, device);
      g_object_unref (device);
    }
  g_list_free (devices);
}

/*
 * GObject implementation
 */

static void
clutter_device_manager_evdev_constructed (GObject *gobject)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterInputDevice *device;
  struct xkb_context *ctx;
  struct xkb_keymap *keymap;
  struct xkb_rule_names names;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (gobject);
  priv = manager_evdev->priv;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", CORE_POINTER_ID,
                         "name", "input/core_pointer",
			 "device-manager", manager_evdev,
                         "device-type", CLUTTER_POINTER_DEVICE,
			 "device-mode", CLUTTER_INPUT_MODE_MASTER,
                         "enabled", TRUE,
                         NULL);  
  _clutter_input_device_set_stage (device, priv->stage);
  _clutter_device_manager_add_device (CLUTTER_DEVICE_MANAGER (manager_evdev), device);
  priv->core_pointer = device;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", CORE_KEYBOARD_ID,
                         "name", "input/core_keyboard",
			 "device-manager", manager_evdev,
                         "device-type", CLUTTER_KEYBOARD_DEVICE,
			 "device-mode", CLUTTER_INPUT_MODE_MASTER,
                         "enabled", TRUE,
                         NULL);
  _clutter_input_device_set_stage (device, priv->stage);
  _clutter_device_manager_add_device (CLUTTER_DEVICE_MANAGER (manager_evdev), device);
  priv->core_keyboard = device;

  priv->keys = g_array_new (FALSE, FALSE, sizeof (guint32));

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
      priv->xkb = xkb_state_new (keymap);

      priv->caps_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_CAPS);
      priv->num_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_NUM);
      priv->scroll_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_SCROLL);

      xkb_keymap_unref (keymap);
    }

  priv->udev_client = g_udev_client_new (subsystems);

  clutter_device_manager_evdev_probe_devices (manager_evdev);

  /* subcribe for events on input devices */
  g_signal_connect (priv->udev_client, "uevent",
                    G_CALLBACK (on_uevent), manager_evdev);
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

  g_object_unref (priv->udev_client);

  for (l = priv->devices; l; l = g_slist_next (l))
    {
      ClutterInputDevice *device = l->data;

      g_object_unref (device);
    }
  g_slist_free (priv->devices);

  for (l = priv->event_sources; l; l = g_slist_next (l))
    {
      ClutterEventSource *source = l->data;

      clutter_event_source_free (source);
    }
  g_slist_free (priv->event_sources);

  if (priv->constrain_data_notify)
    priv->constrain_data_notify (priv->constrain_data);

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
  for (l = priv->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (_clutter_input_device_get_stage (device) == NULL)
        _clutter_input_device_set_stage (device, stage);
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
  for (l = priv->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (_clutter_input_device_get_stage (device) == stage)
        _clutter_input_device_set_stage (device, NULL);
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
  ClutterDeviceManagerEvdev *evdev_manager;
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *l, *next;
  uint32_t time_;
  unsigned i;

  if (!manager)
    {
      g_warning ("clutter_evdev_release_devices shouldn't be called "
                 "before clutter_init()");
      return;
    }

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (manager));

  evdev_manager = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = evdev_manager->priv;

  if (priv->released)
    {
      g_warning ("clutter_evdev_release_devices() shouldn't be called "
                 "multiple times without a corresponding call to "
                 "clutter_evdev_reclaim_devices() first");
      return;
    }

  /* Fake release events for all currently pressed keys */
  time_ = g_get_monotonic_time () / 1000;
  for (i = 0; i < priv->keys->len; i++)
    notify_key_device (priv->core_keyboard, time_,
		       g_array_index (priv->keys, uint32_t, i) - 8, 0, FALSE);
  g_array_set_size (priv->keys, 0);

  for (l = priv->devices; l; l = next)
    {
      ClutterInputDevice *device = l->data;

      /* Be careful about the list we're iterating being modified... */
      next = l->next;

      _clutter_device_manager_remove_device (manager, device);
    }

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
  ClutterDeviceManagerEvdev *evdev_manager;
  ClutterDeviceManagerEvdevPrivate *priv;
#define LONG_BITS (sizeof(long) * 8)
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)
  unsigned long key_bits[NLONGS(KEY_CNT)];
  unsigned long source_key_bits[NLONGS(KEY_CNT)];
  GSList *iter;
  int i, j, rc;
  guint32 time_;

  if (!manager)
    {
      g_warning ("clutter_evdev_reclaim_devices shouldn't be called "
                 "before clutter_init()");
      return;
    }

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (manager));

  evdev_manager = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = evdev_manager->priv;

  if (!priv->released)
    {
      g_warning ("Spurious call to clutter_evdev_reclaim_devices() without "
                 "previous call to clutter_evdev_release_devices");
      return;
    }

  priv->released = FALSE;
  clutter_device_manager_evdev_probe_devices (evdev_manager);

  memset (key_bits, 0, sizeof (key_bits));
  for (iter = priv->event_sources; iter; iter++)
    {
      ClutterEventSource *source = iter->data;
      ClutterInputDevice *slave = CLUTTER_INPUT_DEVICE (source->device);

      if (clutter_input_device_get_device_type (slave) == CLUTTER_KEYBOARD_DEVICE)
	{
	  rc = ioctl (source->event_poll_fd.fd, EVIOCGBIT(EV_KEY, sizeof (source_key_bits)), source_key_bits);
	  if (rc < 0)
	    continue;

	  for (i = 0; i < NLONGS(KEY_CNT); i++)
	    key_bits[i] |= source_key_bits[i];
	}
    }

  /* Fake press events for all currently pressed keys */
  time_ = g_get_monotonic_time () / 1000;
  for (i = 0; i < NLONGS(KEY_CNT); i++)
    {
      for (j = 0; j < 8; j++)
	{
	  if (key_bits[i] & (1 << j))
	    notify_key_device (priv->core_keyboard, time_, i * 8 + j, 1, TRUE);
	}
    }

#undef LONG_BITS
#undef NLONGS
}

/**
 * clutter_evdev_set_open_callback: (skip)
 * @callback: the user replacement for open()
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
 */
void
clutter_evdev_set_open_callback (ClutterOpenDeviceCallback callback,
                                 gpointer                  user_data)
{
  open_callback = callback;
  open_callback_data = user_data;
}

/**
 * clutter_evdev_set_keyboard_map: (skip)
 * @evdev: the #ClutterDeviceManager created by the evdev backend
 * @keymap: the new keymap
 *
 * Instructs @evdev to use the speficied keyboard map. This will cause
 * the backend to drop the state and create a new one with the new map.
 */
void
clutter_evdev_set_keyboard_map (ClutterDeviceManager *evdev,
				struct xkb_keymap    *keymap)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  unsigned int i;

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (evdev));

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (evdev);
  priv = manager_evdev->priv;

  xkb_state_unref (priv->xkb);
  priv->xkb = xkb_state_new (keymap);

  priv->caps_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_CAPS);
  priv->num_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_NUM);
  priv->scroll_lock_led = xkb_keymap_led_get_index (keymap, XKB_LED_NAME_SCROLL);

  for (i = 0; i < priv->keys->len; i++)
    xkb_state_update_key (priv->xkb, g_array_index (priv->keys, guint32, i), XKB_KEY_DOWN);

  sync_leds (manager_evdev);
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
