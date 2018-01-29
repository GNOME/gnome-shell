/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
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
 * Author: Jonas Ådahl <jadahl@gmail.com>
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include <glib-object.h>

#include "clutter-x11.h"
#include "X11/extensions/XTest.h"

#include "clutter-virtual-input-device.h"
#include "x11/clutter-virtual-input-device-x11.h"

struct _ClutterVirtualInputDeviceX11
{
  ClutterVirtualInputDevice parent;
};

G_DEFINE_TYPE (ClutterVirtualInputDeviceX11,
               clutter_virtual_input_device_x11,
               CLUTTER_TYPE_VIRTUAL_INPUT_DEVICE)

static void
clutter_virtual_input_device_x11_notify_relative_motion (ClutterVirtualInputDevice *virtual_device,
                                                         uint64_t                   time_us,
                                                         double                     dx,
                                                         double                     dy)
{
  XTestFakeRelativeMotionEvent (clutter_x11_get_default_display (),
                                (int) dx,
                                (int) dy,
                                0);
}

static void
clutter_virtual_input_device_x11_notify_absolute_motion (ClutterVirtualInputDevice *virtual_device,
                                                         uint64_t                   time_us,
                                                         double                     x,
                                                         double                     y)
{
  XTestFakeMotionEvent (clutter_x11_get_default_display (),
                        clutter_x11_get_default_screen (),
                        (int) x,
                        (int) y,
                        0);
}

static void
clutter_virtual_input_device_x11_notify_button (ClutterVirtualInputDevice *virtual_device,
                                                uint64_t                   time_us,
                                                uint32_t                   button,
                                                ClutterButtonState         button_state)
{
  XTestFakeButtonEvent (clutter_x11_get_default_display (),
                        button, button_state == CLUTTER_BUTTON_STATE_PRESSED, 0);
}

static void
clutter_virtual_input_device_x11_notify_discrete_scroll (ClutterVirtualInputDevice *virtual_device,
                                                         uint64_t                   time_us,
                                                         ClutterScrollDirection     direction,
                                                         ClutterScrollSource        scroll_source)
{
  Display *xdisplay = clutter_x11_get_default_display ();
  int button;

  switch (direction)
    {
    case CLUTTER_SCROLL_UP:
      button = 4;
      break;
    case CLUTTER_SCROLL_DOWN:
      button = 5;
      break;
    case CLUTTER_SCROLL_LEFT:
      button = 6;
      break;
    case CLUTTER_SCROLL_RIGHT:
      button = 7;
      break;
    default:
      g_warn_if_reached ();
      return;
    }

  XTestFakeButtonEvent (xdisplay, button, True, 0);
  XTestFakeButtonEvent (xdisplay, button, False, 0);
}

static void
clutter_virtual_input_device_x11_notify_scroll_continuous (ClutterVirtualInputDevice *virtual_device,
                                                           uint64_t                   time_us,
                                                           double                     dx,
                                                           double                     dy,
                                                           ClutterScrollSource        scroll_source,
                                                           ClutterScrollFinishFlags   finish_flags)
{
}

static void
clutter_virtual_input_device_x11_notify_key (ClutterVirtualInputDevice *virtual_device,
                                             uint64_t                   time_us,
                                             uint32_t                   key,
                                             ClutterKeyState            key_state)
{
  XTestFakeKeyEvent (clutter_x11_get_default_display (),
                     key, key_state == CLUTTER_KEY_STATE_PRESSED, 0);
}

static void
clutter_virtual_input_device_x11_notify_keyval (ClutterVirtualInputDevice *virtual_device,
						uint64_t                   time_us,
						uint32_t                   keyval,
						ClutterKeyState            key_state)
{
  KeyCode keycode;

  keycode = XKeysymToKeycode (clutter_x11_get_default_display (), keyval);
  XTestFakeKeyEvent (clutter_x11_get_default_display (),
                     keycode, key_state == CLUTTER_KEY_STATE_PRESSED, 0);
}

static void
clutter_virtual_input_device_x11_notify_touch_down (ClutterVirtualInputDevice *virtual_device,
                                                    uint64_t                   time_us,
                                                    int                        device_slot,
                                                    double                     x,
                                                    double                     y)
{
  g_warning ("Virtual touch motion not implemented under X11");
}

static void
clutter_virtual_input_device_x11_notify_touch_motion (ClutterVirtualInputDevice *virtual_device,
                                                      uint64_t                   time_us,
                                                      int                        device_slot,
                                                      double                     x,
                                                      double                     y)
{
  g_warning ("Virtual touch motion not implemented under X11");
}

static void
clutter_virtual_input_device_x11_notify_touch_up (ClutterVirtualInputDevice *virtual_device,
                                                  uint64_t                   time_us,
                                                  int                        device_slot)
{
  g_warning ("Virtual touch motion not implemented under X11");
}

static void
clutter_virtual_input_device_x11_init (ClutterVirtualInputDeviceX11 *virtual_device_x11)
{
}

static void
clutter_virtual_input_device_x11_class_init (ClutterVirtualInputDeviceX11Class *klass)
{
  ClutterVirtualInputDeviceClass *virtual_input_device_class =
    CLUTTER_VIRTUAL_INPUT_DEVICE_CLASS (klass);

  virtual_input_device_class->notify_relative_motion = clutter_virtual_input_device_x11_notify_relative_motion;
  virtual_input_device_class->notify_absolute_motion = clutter_virtual_input_device_x11_notify_absolute_motion;
  virtual_input_device_class->notify_button = clutter_virtual_input_device_x11_notify_button;
  virtual_input_device_class->notify_discrete_scroll = clutter_virtual_input_device_x11_notify_discrete_scroll;
  virtual_input_device_class->notify_scroll_continuous = clutter_virtual_input_device_x11_notify_scroll_continuous;
  virtual_input_device_class->notify_key = clutter_virtual_input_device_x11_notify_key;
  virtual_input_device_class->notify_keyval = clutter_virtual_input_device_x11_notify_keyval;
  virtual_input_device_class->notify_touch_down = clutter_virtual_input_device_x11_notify_touch_down;
  virtual_input_device_class->notify_touch_motion = clutter_virtual_input_device_x11_notify_touch_motion;
  virtual_input_device_class->notify_touch_up = clutter_virtual_input_device_x11_notify_touch_up;
}
