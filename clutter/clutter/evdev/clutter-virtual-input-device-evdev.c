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
#include <linux/input.h>

#include "clutter-private.h"
#include "clutter-virtual-input-device.h"
#include "evdev/clutter-input-device-evdev.h"
#include "evdev/clutter-seat-evdev.h"
#include "evdev/clutter-virtual-input-device-evdev.h"

enum
{
  PROP_0,

  PROP_SEAT,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

struct _ClutterVirtualInputDeviceEvdev
{
  ClutterVirtualInputDevice parent;

  ClutterInputDevice *device;
  ClutterSeatEvdev *seat;
  int button_count[KEY_CNT];
};

G_DEFINE_TYPE (ClutterVirtualInputDeviceEvdev,
               clutter_virtual_input_device_evdev,
               CLUTTER_TYPE_VIRTUAL_INPUT_DEVICE)

typedef enum _EvdevButtonType
{
  EVDEV_BUTTON_TYPE_NONE,
  EVDEV_BUTTON_TYPE_KEY,
  EVDEV_BUTTON_TYPE_BUTTON,
} EvdevButtonType;

static int
update_button_count (ClutterVirtualInputDeviceEvdev *virtual_evdev,
                     uint32_t                        button,
                     uint32_t                        state)
{
  if (state)
    return ++virtual_evdev->button_count[button];
  else
    return --virtual_evdev->button_count[button];
}

static EvdevButtonType
get_button_type (uint16_t code)
{
  switch (code)
    {
    case BTN_TOOL_PEN:
    case BTN_TOOL_RUBBER:
    case BTN_TOOL_BRUSH:
    case BTN_TOOL_PENCIL:
    case BTN_TOOL_AIRBRUSH:
    case BTN_TOOL_MOUSE:
    case BTN_TOOL_LENS:
    case BTN_TOOL_QUINTTAP:
    case BTN_TOOL_DOUBLETAP:
    case BTN_TOOL_TRIPLETAP:
    case BTN_TOOL_QUADTAP:
    case BTN_TOOL_FINGER:
    case BTN_TOUCH:
      return EVDEV_BUTTON_TYPE_NONE;
    }

  if (code >= KEY_ESC && code <= KEY_MICMUTE)
    return EVDEV_BUTTON_TYPE_KEY;
  if (code >= BTN_MISC && code <= BTN_GEAR_UP)
    return EVDEV_BUTTON_TYPE_BUTTON;
  if (code >= KEY_OK && code <= KEY_LIGHTS_TOGGLE)
    return EVDEV_BUTTON_TYPE_KEY;
  if (code >= BTN_DPAD_UP && code <= BTN_DPAD_RIGHT)
    return EVDEV_BUTTON_TYPE_BUTTON;
  if (code >= KEY_ALS_TOGGLE && code <= KEY_KBDINPUTASSIST_CANCEL)
    return EVDEV_BUTTON_TYPE_KEY;
  if (code >= BTN_TRIGGER_HAPPY && code <= BTN_TRIGGER_HAPPY40)
    return EVDEV_BUTTON_TYPE_BUTTON;
  return EVDEV_BUTTON_TYPE_NONE;
}

static void
release_pressed_buttons (ClutterVirtualInputDevice *virtual_device)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);
  int code;
  uint64_t time_us;

  time_us = g_get_monotonic_time ();

  for (code = 0; code < G_N_ELEMENTS (virtual_evdev->button_count); code++)
    {
      if (virtual_evdev->button_count[code] == 0)
        continue;

      switch (get_button_type (code))
        {
        case EVDEV_BUTTON_TYPE_KEY:
          clutter_virtual_input_device_notify_key (virtual_device,
                                                   time_us,
                                                   code,
                                                   CLUTTER_KEY_STATE_RELEASED);
          break;
        case EVDEV_BUTTON_TYPE_BUTTON:
          clutter_virtual_input_device_notify_button (virtual_device,
                                                      time_us,
                                                      code,
                                                      CLUTTER_BUTTON_STATE_RELEASED);
          break;
        case EVDEV_BUTTON_TYPE_NONE:
          g_assert_not_reached ();
        }
    }
}

static void
clutter_virtual_input_device_evdev_notify_relative_motion (ClutterVirtualInputDevice *virtual_device,
                                                           uint64_t                   time_us,
                                                           double                     dx,
                                                           double                     dy)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);

  if (time_us == CLUTTER_CURRENT_TIME)
    time_us = g_get_monotonic_time ();

  clutter_seat_evdev_notify_relative_motion (virtual_evdev->seat,
                                             virtual_evdev->device,
                                             time_us,
                                             dx, dy,
                                             dx, dy);
}

static void
clutter_virtual_input_device_evdev_notify_absolute_motion (ClutterVirtualInputDevice *virtual_device,
                                                           uint64_t                   time_us,
                                                           double                     x,
                                                           double                     y)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);

  if (time_us == CLUTTER_CURRENT_TIME)
    time_us = g_get_monotonic_time ();

  clutter_seat_evdev_notify_absolute_motion (virtual_evdev->seat,
                                             virtual_evdev->device,
                                             time_us,
                                             x, y,
                                             NULL);
}

static void
clutter_virtual_input_device_evdev_notify_button (ClutterVirtualInputDevice *virtual_device,
                                                  uint64_t                   time_us,
                                                  uint32_t                   button,
                                                  ClutterButtonState         button_state)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);
  int button_count;

  if (time_us == CLUTTER_CURRENT_TIME)
    time_us = g_get_monotonic_time ();

  if (get_button_type (button) != EVDEV_BUTTON_TYPE_BUTTON)
    {
      g_warning ("Unknown/invalid virtual device button 0x%x pressed",
                 button);
      return;
    }

  button_count = update_button_count (virtual_evdev, button, button_state);
  if (button_count < 0 || button_count > 1)
    {
      g_warning ("Received multiple virtual 0x%x button %s (ignoring)", button,
                 button_state == CLUTTER_BUTTON_STATE_PRESSED ? "presses" : "releases");
      update_button_count (virtual_evdev, button, 1 - button_state);
      return;
    }

  clutter_seat_evdev_notify_button (virtual_evdev->seat,
                                    virtual_evdev->device,
                                    time_us,
                                    button,
                                    button_state);
}

static void
clutter_virtual_input_device_evdev_notify_key (ClutterVirtualInputDevice *virtual_device,
                                               uint64_t                   time_us,
                                               uint32_t                   key,
                                               ClutterKeyState            key_state)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);
  int key_count;

  if (time_us == CLUTTER_CURRENT_TIME)
    time_us = g_get_monotonic_time ();

  if (get_button_type (key) != EVDEV_BUTTON_TYPE_KEY)
    {
      g_warning ("Unknown/invalid virtual device key 0x%x pressed\n", key);
      return;
    }

  key_count = update_button_count (virtual_evdev, key, key_state);
  if (key_count < 0 || key_count > 1)
    {
      g_warning ("Received multiple virtual 0x%x key %s (ignoring)", key,
		 key_state == CLUTTER_KEY_STATE_PRESSED ? "presses" : "releases");
      update_button_count (virtual_evdev, key, 1 - key_state);
      return;
    }

  clutter_seat_evdev_notify_key (virtual_evdev->seat,
                                 virtual_evdev->device,
                                 time_us,
                                 key,
                                 key_state,
                                 TRUE);
}

static gboolean
pick_keycode_for_keyval_in_current_group (ClutterVirtualInputDevice *virtual_device,
                                          guint                      keyval,
                                          guint                     *keycode_out,
                                          guint                     *level_out)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);
  ClutterDeviceManager *manager;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state  *state;
  guint keycode, layout;
  xkb_keycode_t min_keycode, max_keycode;

  manager = clutter_virtual_input_device_get_manager (virtual_device);
  xkb_keymap = _clutter_device_manager_evdev_get_keymap (CLUTTER_DEVICE_MANAGER_EVDEV (manager));
  state = virtual_evdev->seat->xkb;

  layout = xkb_state_serialize_layout (state, XKB_STATE_LAYOUT_EFFECTIVE);
  min_keycode = xkb_keymap_min_keycode (xkb_keymap);
  max_keycode = xkb_keymap_max_keycode (xkb_keymap);
  for (keycode = min_keycode; keycode < max_keycode; keycode++)
    {
      gint num_levels, level;
      num_levels = xkb_keymap_num_levels_for_key (xkb_keymap, keycode, layout);
      for (level = 0; level < num_levels; level++)
        {
          const xkb_keysym_t *syms;
          gint num_syms, sym;
          num_syms = xkb_keymap_key_get_syms_by_level (xkb_keymap, keycode, layout, level, &syms);
          for (sym = 0; sym < num_syms; sym++)
            {
              if (syms[sym] == keyval)
                {
                  *keycode_out = keycode;
                  if (level_out)
                    *level_out = level;
                  return TRUE;
                }
            }
        }
    }

  return FALSE;
}

static void
apply_level_modifiers (ClutterVirtualInputDevice *virtual_device,
                       uint64_t                   time_us,
                       uint32_t                   level,
                       uint32_t                   key_state)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);
  guint keysym, keycode, evcode;

  if (level == 0)
    return;

  if (level == 1)
    {
      keysym = XKB_KEY_Shift_L;
    }
  else if (level == 2)
    {
      keysym = XKB_KEY_ISO_Level3_Shift;
    }
  else
    {
      g_warning ("Unhandled level: %d\n", level);
      return;
    }

  if (!pick_keycode_for_keyval_in_current_group (virtual_device, keysym,
                                                 &keycode, NULL))
    return;

  clutter_input_device_keycode_to_evdev (virtual_evdev->device,
                                         keycode, &evcode);
  clutter_seat_evdev_notify_key (virtual_evdev->seat,
                                 virtual_evdev->device,
                                 time_us,
                                 evcode,
                                 key_state,
                                 TRUE);
}

static void
clutter_virtual_input_device_evdev_notify_keyval (ClutterVirtualInputDevice *virtual_device,
                                                  uint64_t                   time_us,
                                                  uint32_t                   keyval,
                                                  ClutterKeyState            key_state)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);
  int key_count;
  guint keycode = 0, level = 0, evcode = 0;

  if (time_us == CLUTTER_CURRENT_TIME)
    time_us = g_get_monotonic_time ();

  if (!pick_keycode_for_keyval_in_current_group (virtual_device,
                                                 keyval, &keycode, &level))
    {
      g_warning ("No keycode found for keyval %x in current group", keyval);
      return;
    }

  clutter_input_device_keycode_to_evdev (virtual_evdev->device,
                                         keycode, &evcode);

  if (get_button_type (evcode) != EVDEV_BUTTON_TYPE_KEY)
    {
      g_warning ("Unknown/invalid virtual device key 0x%x pressed\n", evcode);
      return;
    }

  key_count = update_button_count (virtual_evdev, evcode, key_state);
  if (key_count < 0 || key_count > 1)
    {
      g_warning ("Received multiple virtual 0x%x key %s (ignoring)", keycode,
		 key_state == CLUTTER_KEY_STATE_PRESSED ? "presses" : "releases");
      update_button_count (virtual_evdev, evcode, 1 - key_state);
      return;
    }

  if (key_state)
    apply_level_modifiers (virtual_device, time_us, level, key_state);

  clutter_seat_evdev_notify_key (virtual_evdev->seat,
                                 virtual_evdev->device,
                                 time_us,
                                 evcode,
                                 key_state,
                                 TRUE);

  if (!key_state)
    apply_level_modifiers (virtual_device, time_us, level, key_state);
}

static void
direction_to_discrete (ClutterScrollDirection direction,
                       double                *discrete_dx,
                       double                *discrete_dy)
{
  switch (direction)
    {
    case CLUTTER_SCROLL_UP:
      *discrete_dx = 0.0;
      *discrete_dy = -1.0;
      break;
    case CLUTTER_SCROLL_DOWN:
      *discrete_dx = 0.0;
      *discrete_dy = 1.0;
      break;
    case CLUTTER_SCROLL_LEFT:
      *discrete_dx = -1.0;
      *discrete_dy = 0.0;
      break;
    case CLUTTER_SCROLL_RIGHT:
      *discrete_dx = 1.0;
      *discrete_dy = 0.0;
      break;
    case CLUTTER_SCROLL_SMOOTH:
      g_assert_not_reached ();
      break;
    }
}

static void
clutter_virtual_input_device_evdev_notify_discrete_scroll (ClutterVirtualInputDevice *virtual_device,
                                                           uint64_t                   time_us,
                                                           ClutterScrollDirection     direction,
                                                           ClutterScrollSource        scroll_source)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (virtual_device);
  double discrete_dx = 0.0, discrete_dy = 0.0;

  if (time_us == CLUTTER_CURRENT_TIME)
    time_us = g_get_monotonic_time ();

  direction_to_discrete (direction, &discrete_dx, &discrete_dy);

  clutter_seat_evdev_notify_discrete_scroll (virtual_evdev->seat,
                                             virtual_evdev->device,
                                             time_us,
                                             discrete_dx, discrete_dy,
                                             scroll_source);
}

static void
clutter_virtual_input_device_evdev_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);

  switch (prop_id)
    {
    case PROP_SEAT:
      g_value_set_pointer (value, virtual_evdev->seat);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_virtual_input_device_evdev_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);

  switch (prop_id)
    {
    case PROP_SEAT:
      virtual_evdev->seat = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_virtual_input_device_evdev_constructed (GObject *object)
{
  ClutterVirtualInputDevice *virtual_device =
    CLUTTER_VIRTUAL_INPUT_DEVICE (object);
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);
  ClutterDeviceManager *manager;
  ClutterInputDeviceType device_type;
  ClutterStage *stage;

  manager = clutter_virtual_input_device_get_manager (virtual_device);
  device_type = clutter_virtual_input_device_get_device_type (virtual_device);

  virtual_evdev->device =
    _clutter_input_device_evdev_new_virtual (manager,
                                             virtual_evdev->seat,
                                             device_type,
                                             CLUTTER_INPUT_MODE_SLAVE);

  stage = _clutter_device_manager_evdev_get_stage (CLUTTER_DEVICE_MANAGER_EVDEV (manager));
  _clutter_input_device_set_stage (virtual_evdev->device, stage);
}

static void
clutter_virtual_input_device_evdev_finalize (GObject *object)
{
  ClutterVirtualInputDevice *virtual_device =
    CLUTTER_VIRTUAL_INPUT_DEVICE (object);
  ClutterVirtualInputDeviceEvdev *virtual_evdev =
    CLUTTER_VIRTUAL_INPUT_DEVICE_EVDEV (object);
  GObjectClass *object_class;

  release_pressed_buttons (virtual_device);
  g_clear_object (&virtual_evdev->device);

  object_class =
    G_OBJECT_CLASS (clutter_virtual_input_device_evdev_parent_class);
  object_class->finalize (object);
}

static void
clutter_virtual_input_device_evdev_init (ClutterVirtualInputDeviceEvdev *virtual_device_evdev)
{
}

static void
clutter_virtual_input_device_evdev_class_init (ClutterVirtualInputDeviceEvdevClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterVirtualInputDeviceClass *virtual_input_device_class =
    CLUTTER_VIRTUAL_INPUT_DEVICE_CLASS (klass);

  object_class->get_property = clutter_virtual_input_device_evdev_get_property;
  object_class->set_property = clutter_virtual_input_device_evdev_set_property;
  object_class->constructed = clutter_virtual_input_device_evdev_constructed;
  object_class->finalize = clutter_virtual_input_device_evdev_finalize;

  virtual_input_device_class->notify_relative_motion = clutter_virtual_input_device_evdev_notify_relative_motion;
  virtual_input_device_class->notify_absolute_motion = clutter_virtual_input_device_evdev_notify_absolute_motion;
  virtual_input_device_class->notify_button = clutter_virtual_input_device_evdev_notify_button;
  virtual_input_device_class->notify_key = clutter_virtual_input_device_evdev_notify_key;
  virtual_input_device_class->notify_keyval = clutter_virtual_input_device_evdev_notify_keyval;
  virtual_input_device_class->notify_discrete_scroll = clutter_virtual_input_device_evdev_notify_discrete_scroll;

  obj_props[PROP_SEAT] = g_param_spec_pointer ("seat",
                                               P_("ClutterSeatEvdev"),
                                               P_("ClutterSeatEvdev"),
                                               CLUTTER_PARAM_READWRITE |
                                               G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}
