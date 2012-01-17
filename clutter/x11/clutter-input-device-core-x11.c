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

#include "config.h"

#include "clutter-input-device-core-x11.h"

#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "clutter-backend-x11.h"
#include "clutter-stage-x11.h"

#ifdef HAVE_XINPUT
#include <X11/extensions/XInput.h>
#endif

#define MAX_DEVICE_CLASSES      13

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceX11Class;

/* a specific X11 input device */
struct _ClutterInputDeviceX11
{
  ClutterInputDevice device;

#ifdef HAVE_XINPUT
  XDevice *xdevice;

  XEventClass event_classes[MAX_DEVICE_CLASSES];
  int num_classes;

  int button_press_type;
  int button_release_type;
  int motion_notify_type;
  int state_notify_type;
  int key_press_type;
  int key_release_type;
#endif /* HAVE_XINPUT */

  gint *axis_data;

  int min_keycode;
  int max_keycode;
};

#define clutter_input_device_x11_get_type       _clutter_input_device_x11_get_type

G_DEFINE_TYPE (ClutterInputDeviceX11,
               clutter_input_device_x11,
               CLUTTER_TYPE_INPUT_DEVICE);

static void
clutter_input_device_x11_select_stage_events (ClutterInputDevice *device,
                                              ClutterStage       *stage,
                                              gint                event_mask)
{
#if HAVE_XINPUT
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (device->backend);
  ClutterInputDeviceX11 *device_x11;
  ClutterStageX11 *stage_x11;
  XEventClass class;
  gint i;

  device_x11 = CLUTTER_INPUT_DEVICE_X11 (device);

  stage_x11 = CLUTTER_STAGE_X11 (_clutter_stage_get_window (stage));

  i = 0;

  if (event_mask & ButtonPressMask)
    {
      DeviceButtonPress (device_x11->xdevice,
                         device_x11->button_press_type,
                         class);
      if (class != 0)
        device_x11->event_classes[i++] = class;

      DeviceButtonPressGrab (device_x11->xdevice, 0, class);
      if (class != 0)
        device_x11->event_classes[i++] = class;
    }

  if (event_mask & ButtonReleaseMask)
    {
      DeviceButtonRelease (device_x11->xdevice,
                           device_x11->button_release_type,
                           class);
      if (class != 0)
        device_x11->event_classes[i++] = class;
    }

  if (event_mask & PointerMotionMask)
    {
      DeviceMotionNotify (device_x11->xdevice,
                          device_x11->motion_notify_type,
                          class);
      if (class != 0)
        device_x11->event_classes[i++] = class;

      DeviceStateNotify (device_x11->xdevice,
                         device_x11->state_notify_type,
                         class);
      if (class != 0)
        device_x11->event_classes[i++] = class;
    }

  if (event_mask & KeyPressMask)
    {
      DeviceKeyPress (device_x11->xdevice,
                      device_x11->key_press_type,
                      class);
      if (class != 0)
        device_x11->event_classes[i++] = class;
    }

  if (event_mask & KeyReleaseMask)
    {
      DeviceKeyRelease (device_x11->xdevice,
                        device_x11->key_release_type,
                        class);
      if (class != 0)
        device_x11->event_classes[i++] = class;
    }

  device_x11->num_classes = i;

  XSelectExtensionEvent (backend_x11->xdpy,
                         stage_x11->xwin,
                         device_x11->event_classes,
                         device_x11->num_classes);
#endif /* HAVE_XINPUT */
}

static void
clutter_input_device_x11_dispose (GObject *gobject)
{
  ClutterInputDeviceX11 *device_x11 = CLUTTER_INPUT_DEVICE_X11 (gobject);

#ifdef HAVE_XINPUT
  if (device_x11->xdevice)
    {
      ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (gobject);
      ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (device->backend);

      XCloseDevice (backend_x11->xdpy, device_x11->xdevice);
      device_x11->xdevice = NULL;
    }
#endif /* HAVE_XINPUT */

  g_free (device_x11->axis_data);

  G_OBJECT_CLASS (clutter_input_device_x11_parent_class)->dispose (gobject);
}

static void
clutter_input_device_x11_constructed (GObject *gobject)
{
#ifdef HAVE_XINPUT
  ClutterInputDeviceX11 *device_x11 = CLUTTER_INPUT_DEVICE_X11 (gobject);
  ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (gobject);
  ClutterBackendX11 *backend_x11;

  backend_x11 = CLUTTER_BACKEND_X11 (device->backend);

  clutter_x11_trap_x_errors ();

  device_x11->xdevice = XOpenDevice (backend_x11->xdpy, device->id);

  if (clutter_x11_untrap_x_errors ())
    {
      g_warning ("Device '%s' cannot be opened",
                 clutter_input_device_get_device_name (device));
    }
#endif /* HAVE_XINPUT */

  if (G_OBJECT_CLASS (clutter_input_device_x11_parent_class)->constructed)
    G_OBJECT_CLASS (clutter_input_device_x11_parent_class)->constructed (gobject);
}

static gboolean
clutter_input_device_x11_keycode_to_evdev (ClutterInputDevice *device,
                                           guint hardware_keycode,
                                           guint *evdev_keycode)
{
  /* When using evdev under X11 the hardware keycodes are the evdev
     keycodes plus 8. I haven't been able to find any documentation to
     know what the +8 is for. FIXME: This should probably verify that
     X server is using evdev. */
  *evdev_keycode = hardware_keycode - 8;

  return TRUE;
}

static void
clutter_input_device_x11_class_init (ClutterInputDeviceX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterInputDeviceClass *device_class = CLUTTER_INPUT_DEVICE_CLASS (klass);

  gobject_class->constructed = clutter_input_device_x11_constructed;
  gobject_class->dispose = clutter_input_device_x11_dispose;

  device_class->select_stage_events = clutter_input_device_x11_select_stage_events;
  device_class->keycode_to_evdev = clutter_input_device_x11_keycode_to_evdev;
}

static void
clutter_input_device_x11_init (ClutterInputDeviceX11 *self)
{
}

void
_clutter_input_device_x11_set_keycodes (ClutterInputDeviceX11 *device_x11,
                                        int                    min_keycode,
                                        int                    max_keycode)
{
  device_x11->min_keycode = min_keycode;
  device_x11->max_keycode = max_keycode;
}

int
_clutter_input_device_x11_get_min_keycode (ClutterInputDeviceX11 *device_x11)
{
  return device_x11->min_keycode;
}

int
_clutter_input_device_x11_get_max_keycode (ClutterInputDeviceX11 *device_x11)
{
  return device_x11->max_keycode;
}

#ifdef HAVE_XINPUT
static void
update_axes (ClutterInputDeviceX11 *device_x11,
             guint                  n_axes,
             gint                   first_axis,
             gint                  *axes_data)
{
  ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (device_x11);
  gint i;

  if (device_x11->axis_data == NULL)
    {
      device_x11->axis_data =
        g_new0 (gint, clutter_input_device_get_n_axes (device));
    }

  for (i = 0; i < n_axes; i++)
    device_x11->axis_data[first_axis + i] = axes_data[i];
}

static gdouble *
translate_axes (ClutterInputDeviceX11 *device_x11,
                ClutterStageX11       *stage_x11,
                gfloat                *event_x,
                gfloat                *event_y)
{
  ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (device_x11);
  gint root_x, root_y;
  gint n_axes, i;
  gdouble x, y;
  gdouble *retval;

  if (!_clutter_stage_x11_get_root_coords (stage_x11, &root_x, &root_y))
    return NULL;

  x = y = 0.0f;
  n_axes = clutter_input_device_get_n_axes (device);

  retval = g_new0 (gdouble, n_axes);

  for (i = 0; i < n_axes; i++)
    {
      ClutterInputAxis axis;

      axis = clutter_input_device_get_axis (device, i);
      switch (axis)
        {
        case CLUTTER_INPUT_AXIS_X:
        case CLUTTER_INPUT_AXIS_Y:
          _clutter_x11_input_device_translate_screen_coord (device,
                                                            root_x, root_y,
                                                            i,
                                                            device_x11->axis_data[i],
                                                            &retval[i]);
          if (axis == CLUTTER_INPUT_AXIS_X)
            x = retval[i];
          else if (axis == CLUTTER_INPUT_AXIS_Y)
            y = retval[i];
          break;

        default:
          _clutter_input_device_translate_axis (device, i,
                                                device_x11->axis_data[i],
                                                &retval[i]);
          break;
        }
    }

  if (event_x)
    *event_x = x;

  if (event_y)
    *event_y = y;

  return retval;
}

/*
 * translate_state:
 * @state: the keyboard state of the core device
 * @device_state: the button state of the device
 *
 * Trivially translates the state and the device state into a
 * single bitmask.
 */
static guint
translate_state (guint state,
                 guint device_state)
{
  return device_state | (state & 0xff);
}
#endif /* HAVE_XINPUT */

gboolean
_clutter_input_device_x11_translate_xi_event (ClutterInputDeviceX11 *device_x11,
                                              ClutterStageX11       *stage_x11,
                                              XEvent                *xevent,
                                              ClutterEvent          *event)
{
#ifdef HAVE_XINPUT
  ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (device_x11);

  if ((xevent->type == device_x11->button_press_type) ||
      (xevent->type == device_x11->button_release_type))
    {
      XDeviceButtonEvent *xdbe = (XDeviceButtonEvent *) xevent;

      event->button.type = event->type =
        (xdbe->type == device_x11->button_press_type) ? CLUTTER_BUTTON_PRESS
                                                      : CLUTTER_BUTTON_RELEASE;
      event->button.device = device;
      event->button.time = xdbe->time;
      event->button.button = xdbe->button;
      event->button.modifier_state =
        translate_state (xdbe->state, xdbe->device_state);

      update_axes (device_x11,
                   xdbe->axes_count,
                   xdbe->first_axis,
                   xdbe->axis_data);

      event->button.axes = translate_axes (device_x11, stage_x11,
                                           &event->button.x,
                                           &event->button.y);

      _clutter_stage_x11_set_user_time (stage_x11, event->button.time);

      return TRUE;
    }

  if ((xevent->type == device_x11->key_press_type) ||
      (xevent->type == device_x11->key_release_type))
    {
      XDeviceKeyEvent *xdke = (XDeviceKeyEvent *) xevent;

      if (xdke->keycode < device_x11->min_keycode ||
          xdke->keycode >= device_x11->max_keycode)
        {
          g_warning ("Invalid device key code received: %d", xdke->keycode);
          return FALSE;
        }

      clutter_input_device_get_key (device,
                                    xdke->keycode - device_x11->min_keycode,
                                    &event->key.keyval,
                                    &event->key.modifier_state);
      if (event->key.keyval == 0)
        return FALSE;

      event->key.type = event->type =
        (xdke->type == device_x11->key_press_type) ? CLUTTER_KEY_PRESS
                                                   : CLUTTER_KEY_RELEASE;
      event->key.time = xdke->time;
      event->key.modifier_state |=
        translate_state (xdke->state, xdke->device_state);
      event->key.device = device;

#if 0
      if ((event->key.keyval >= 0x20) && (event->key.keyval <= 0xff))
        {
          event->key.unicode = (gunichar) event->key.keyval;
        }
#endif

      _clutter_stage_x11_set_user_time (stage_x11, event->key.time);

      return TRUE;
    }

  if (xevent->type == device_x11->motion_notify_type)
    {
      XDeviceMotionEvent *xdme = (XDeviceMotionEvent *) xevent;

      event->motion.type = event->type = CLUTTER_MOTION;
      event->motion.time = xdme->time;
      event->motion.modifier_state =
        translate_state (xdme->state, xdme->device_state);
      event->motion.device = device;

      event->motion.axes =
        g_new0 (gdouble, clutter_input_device_get_n_axes (device));

      update_axes (device_x11,
                   xdme->axes_count,
                   xdme->first_axis,
                   xdme->axis_data);

      event->motion.axes = translate_axes (device_x11, stage_x11,
                                           &event->motion.x,
                                           &event->motion.y);

      return TRUE;
    }

  if (xevent->type == device_x11->state_notify_type)
    {
      XDeviceStateNotifyEvent *xdse = (XDeviceStateNotifyEvent *) xevent;
      XInputClass *input_class = (XInputClass *) xdse->data;
      gint n_axes = clutter_input_device_get_n_axes (device);
      int i;

      for (i = 0; i < xdse->num_classes; i++)
        {
          if (input_class->class == ValuatorClass)
            {
              int *axis_data = ((XValuatorState *) input_class)->valuators;

              update_axes (device_x11, n_axes, 0, axis_data);
            }

          input_class =
            (XInputClass *)(((char *) input_class) + input_class->length);
        }
    }
#endif /* HAVE_XINPUT */

  return FALSE;
}
