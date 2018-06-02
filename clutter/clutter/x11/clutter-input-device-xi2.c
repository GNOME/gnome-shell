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

#include "clutter-build-config.h"

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
  ClutterInputDeviceTool *current_tool;

#ifdef HAVE_LIBWACOM
  WacomDevice *wacom_device;
  GArray *group_modes;
#endif
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

#ifdef HAVE_LIBWACOM
  if (clutter_input_device_get_device_type (CLUTTER_INPUT_DEVICE (gobject)) == CLUTTER_PAD_DEVICE)
    {
      device_xi2->group_modes = g_array_new (FALSE, TRUE, sizeof (guint));
      g_array_set_size (device_xi2->group_modes,
                        clutter_input_device_get_n_mode_groups (CLUTTER_INPUT_DEVICE (gobject)));
    }
#endif
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

static gboolean
clutter_input_device_xi2_is_grouped (ClutterInputDevice *device,
                                     ClutterInputDevice *other_device)
{
  return FALSE;
}

static void
clutter_input_device_xi2_finalize (GObject *object)
{
#ifdef HAVE_LIBWACOM
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (object);

  if (device_xi2->wacom_device)
    libwacom_destroy (device_xi2->wacom_device);

  if (device_xi2->group_modes)
    g_array_unref (device_xi2->group_modes);
#endif

  G_OBJECT_CLASS (clutter_input_device_xi2_parent_class)->finalize (object);
}

static gint
clutter_input_device_xi2_get_group_n_modes (ClutterInputDevice *device,
                                            gint                group)
{
#ifdef HAVE_LIBWACOM
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);

  if (device_xi2->wacom_device)
    {
      if (group == 0)
        {
          if (libwacom_has_ring (device_xi2->wacom_device))
            return libwacom_get_ring_num_modes (device_xi2->wacom_device);
          else if (libwacom_get_num_strips (device_xi2->wacom_device) >= 1)
            return libwacom_get_strips_num_modes (device_xi2->wacom_device);
        }
      else if (group == 1)
        {
          if (libwacom_has_ring2 (device_xi2->wacom_device))
            return libwacom_get_ring2_num_modes (device_xi2->wacom_device);
          else if (libwacom_get_num_strips (device_xi2->wacom_device) >= 2)
            return libwacom_get_strips_num_modes (device_xi2->wacom_device);
        }
    }
#endif

  return -1;
}

#ifdef HAVE_LIBWACOM
static int
clutter_input_device_xi2_get_button_group (ClutterInputDevice *device,
                                           guint               button)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);

  if (device_xi2->wacom_device)
    {
      if (button >= libwacom_get_num_buttons (device_xi2->wacom_device))
        return -1;

      return libwacom_get_button_led_group (device_xi2->wacom_device,
                                            'A' + button);
    }
  else
    return -1;
}
#endif

static gboolean
clutter_input_device_xi2_is_mode_switch_button (ClutterInputDevice *device,
                                                guint               group,
                                                guint               button)
{
  int button_group = -1;

#ifdef HAVE_LIBWACOM
  button_group = clutter_input_device_xi2_get_button_group (device, button);
#endif

  return button_group == (int) group;
}

static void
clutter_input_device_xi2_class_init (ClutterInputDeviceXI2Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterInputDeviceClass *device_class = CLUTTER_INPUT_DEVICE_CLASS (klass);

  gobject_class->constructed = clutter_input_device_xi2_constructed;
  gobject_class->finalize = clutter_input_device_xi2_finalize;

  device_class->keycode_to_evdev = clutter_input_device_xi2_keycode_to_evdev;
  device_class->is_grouped = clutter_input_device_xi2_is_grouped;
  device_class->get_group_n_modes = clutter_input_device_xi2_get_group_n_modes;
  device_class->is_mode_switch_button = clutter_input_device_xi2_is_mode_switch_button;
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

void
clutter_input_device_xi2_update_tool (ClutterInputDevice     *device,
                                      ClutterInputDeviceTool *tool)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);
  g_set_object (&device_xi2->current_tool, tool);
}

ClutterInputDeviceTool *
clutter_input_device_xi2_get_current_tool (ClutterInputDevice *device)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);
  return device_xi2->current_tool;
}

#ifdef HAVE_LIBWACOM
void
clutter_input_device_xi2_ensure_wacom_info (ClutterInputDevice  *device,
                                            WacomDeviceDatabase *wacom_db)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);
  const gchar *node_path;

  node_path = clutter_input_device_get_device_node (device);
  device_xi2->wacom_device = libwacom_new_from_path (wacom_db, node_path,
                                                     WFALLBACK_NONE, NULL);
}

guint
clutter_input_device_xi2_get_pad_group_mode (ClutterInputDevice *device,
                                             guint               group)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);

  if (group >= device_xi2->group_modes->len)
    return 0;

  return g_array_index (device_xi2->group_modes, guint, group);
}

void
clutter_input_device_xi2_update_pad_state (ClutterInputDevice *device,
                                           guint               button,
                                           guint               state,
                                           guint              *group,
                                           guint              *mode)
{
  ClutterInputDeviceXI2 *device_xi2 = CLUTTER_INPUT_DEVICE_XI2 (device);
  guint button_group, *group_mode;
  gboolean is_mode_switch = FALSE;

  button_group = clutter_input_device_xi2_get_button_group (device, button);
  is_mode_switch = button_group >= 0;

  /* Assign all non-mode-switch buttons to group 0 so far */
  button_group = MAX (0, button_group);

  if (button_group >= device_xi2->group_modes->len)
    return;

  group_mode = &g_array_index (device_xi2->group_modes, guint, button_group);

  if (is_mode_switch && state)
    {
      guint next, n_modes;

      n_modes = clutter_input_device_get_group_n_modes (device, button_group);
      next = (*group_mode + 1) % n_modes;
      *group_mode = next;
    }

  if (group)
    *group = button_group;
  if (mode)
    *mode = *group_mode;
}
#endif
