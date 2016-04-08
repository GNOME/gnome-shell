/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2011  Intel Corp.
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

#include "clutter-input-device-xi2.h"

#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "clutter-backend-x11.h"
#include "clutter-stage-x11.h"

#include <X11/extensions/XInput2.h>

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceXI2Class;

/* a specific XI2 input device */
struct _ClutterInputDeviceXI2
{
  ClutterInputDevice device;

  gint device_id;
};

#define N_BUTTONS       5

#define clutter_input_device_xi2_get_type       _clutter_input_device_xi2_get_type

G_DEFINE_TYPE (ClutterInputDeviceXI2,
               clutter_input_device_xi2,
               CLUTTER_TYPE_INPUT_DEVICE);

static void
clutter_input_device_xi2_constructed (GObject *gobject)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (gobject);

  g_object_get (gobject, "id", &device_xi2->device_id, NULL);

  if (G_OBJECT_CLASS (clutter_input_device_xi2_parent_class)->constructed)
    G_OBJECT_CLASS (clutter_input_device_xi2_parent_class)->constructed (gobject);
}

static gboolean
clutter_input_device_xi2_keycode_to_evdev (ClutterInputDevice *device,
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
clutter_input_device_xi2_class_init (ClutterInputDeviceXI2Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterInputDeviceClass *device_class = CLUTTER_INPUT_DEVICE_CLASS (klass);

  gobject_class->constructed = clutter_input_device_xi2_constructed;

  device_class->keycode_to_evdev = clutter_input_device_xi2_keycode_to_evdev;
}

static void
clutter_input_device_xi2_init (ClutterInputDeviceXI2 *self)
{
}

static ClutterModifierType
get_modifier_for_button (int i)
{
  switch (i)
    {
    case 1:
      return CLUTTER_BUTTON1_MASK;
    case 2:
      return CLUTTER_BUTTON2_MASK;
    case 3:
      return CLUTTER_BUTTON3_MASK;
    case 4:
      return CLUTTER_BUTTON4_MASK;
    case 5:
      return CLUTTER_BUTTON5_MASK;
    default:
      return 0;
    }
}

void
_clutter_input_device_xi2_translate_state (ClutterEvent    *event,
					   XIModifierState *modifiers_state,
                                           XIButtonState   *buttons_state,
                                           XIGroupState    *group_state)
{
  guint button = 0;
  guint base = 0;
  guint latched = 0;
  guint locked = 0;
  guint effective;

  if (modifiers_state)
    {
      base = (guint) modifiers_state->base;
      latched = (guint) modifiers_state->latched;
      locked = (guint) modifiers_state->locked;
    }

  if (buttons_state)
    {
      int len, i;

      len = MIN (N_BUTTONS, buttons_state->mask_len * 8);

      for (i = 0; i < len; i++)
        {
          if (!XIMaskIsSet (buttons_state->mask, i))
            continue;

          button |= get_modifier_for_button (i);
        }
    }

  /* The XIButtonState sent in the event specifies the
   * state of the buttons before the event. In order to
   * get the current state of the buttons, we need to
   * filter out the current button.
   */
  switch (event->type)
    {
    case CLUTTER_BUTTON_PRESS:
      button |=  (get_modifier_for_button (event->button.button));
      break;
    case CLUTTER_BUTTON_RELEASE:
      button &= ~(get_modifier_for_button (event->button.button));
      break;
    default:
      break;
    }

  effective = button | base | latched | locked;
  if (group_state)
    effective |= (group_state->effective) << 13;

  _clutter_event_set_state_full (event, button, base, latched, locked, effective);
}
