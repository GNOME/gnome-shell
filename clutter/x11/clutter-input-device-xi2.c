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
clutter_input_device_xi2_select_stage_events (ClutterInputDevice *device,
                                              ClutterStage       *stage)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11;
  XIEventMask xi_event_mask;
  unsigned char *mask;
  int len;

  backend_x11 = CLUTTER_BACKEND_X11 (device->backend);
  stage_x11 = CLUTTER_STAGE_X11 (_clutter_stage_get_window (stage));

  len = XIMaskLen (XI_LASTEVENT);
  mask = g_new0 (unsigned char, len);

  XISetMask (mask, XI_Motion);
  XISetMask (mask, XI_ButtonPress);
  XISetMask (mask, XI_ButtonRelease);
  XISetMask (mask, XI_KeyPress);
  XISetMask (mask, XI_KeyRelease);
  XISetMask (mask, XI_Enter);
  XISetMask (mask, XI_Leave);

#ifdef HAVE_XINPUT_2_2
  /* enable touch event support if we're running on XInput 2.2 */
  if (backend_x11->xi_minor >= 2)
    {
      XISetMask (mask, XI_TouchBegin);
      XISetMask (mask, XI_TouchUpdate);
      XISetMask (mask, XI_TouchEnd);
    }
#endif /* HAVE_XINPUT_2_2 */

  xi_event_mask.deviceid = device_xi2->device_id;
  xi_event_mask.mask = mask;
  xi_event_mask.mask_len = len;

  CLUTTER_NOTE (BACKEND, "Selecting device id '%d' events",
                device_xi2->device_id);

  XISelectEvents (backend_x11->xdpy, stage_x11->xwin, &xi_event_mask, 1);

  g_free (mask);
}

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

  device_class->select_stage_events = clutter_input_device_xi2_select_stage_events;
  device_class->keycode_to_evdev = clutter_input_device_xi2_keycode_to_evdev;
}

static void
clutter_input_device_xi2_init (ClutterInputDeviceXI2 *self)
{
}

guint
_clutter_input_device_xi2_translate_state (XIModifierState *modifiers_state,
                                           XIButtonState   *buttons_state,
                                           XIGroupState    *group_state)
{
  guint retval = 0;

  if (modifiers_state)
    retval = (guint) modifiers_state->effective;

  if (buttons_state)
    {
      int len, i;

      len = MIN (N_BUTTONS, buttons_state->mask_len * 8);

      for (i = 0; i < len; i++)
        {
          if (!XIMaskIsSet (buttons_state->mask, i))
            continue;

          switch (i)
            {
            case 1:
              retval |= CLUTTER_BUTTON1_MASK;
              break;

            case 2:
              retval |= CLUTTER_BUTTON2_MASK;
              break;

            case 3:
              retval |= CLUTTER_BUTTON3_MASK;
              break;

            case 4:
              retval |= CLUTTER_BUTTON4_MASK;
              break;

            case 5:
              retval |= CLUTTER_BUTTON5_MASK;
              break;

            default:
              break;
            }
        }
    }

  if (group_state)
    retval |= (group_state->effective) << 13;

  return retval;
}
