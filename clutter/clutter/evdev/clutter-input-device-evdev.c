/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Intel Corp.
 * Copyright (C) 2014 Jonas Ådahl
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

#include "clutter/clutter-device-manager-private.h"
#include "clutter-private.h"
#include "clutter-evdev.h"
#include "clutter-input-device-tool-evdev.h"

#include "clutter-input-device-evdev.h"
#include "clutter-device-manager-evdev.h"

#include "cairo-gobject.h"

typedef struct _ClutterInputDeviceClass        ClutterInputDeviceEvdevClass;

#define clutter_input_device_evdev_get_type _clutter_input_device_evdev_get_type

G_DEFINE_TYPE (ClutterInputDeviceEvdev,
               clutter_input_device_evdev,
               CLUTTER_TYPE_INPUT_DEVICE)

enum {
  PROP_0,
  PROP_DEVICE_MATRIX,
  PROP_OUTPUT_ASPECT_RATIO,
  N_PROPS
};

static GParamSpec *obj_props[N_PROPS] = { 0 };

typedef struct _SlowKeysEventPending
{
  ClutterInputDeviceEvdev *device;
  ClutterEvent *event;
  ClutterEmitInputDeviceEvent emit_event_func;
  guint timer;
} SlowKeysEventPending;

static void clear_slow_keys      (ClutterInputDeviceEvdev *device);
static void stop_bounce_keys     (ClutterInputDeviceEvdev *device);

static void
clutter_input_device_evdev_finalize (GObject *object)
{
  ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (object);
  ClutterInputDeviceEvdev *device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (object);
  ClutterDeviceManagerEvdev *manager_evdev =
    CLUTTER_DEVICE_MANAGER_EVDEV (device->device_manager);

  if (device_evdev->libinput_device)
    libinput_device_unref (device_evdev->libinput_device);

  _clutter_device_manager_evdev_release_device_id (manager_evdev, device);

  clear_slow_keys (device_evdev);
  stop_bounce_keys (device_evdev);

  G_OBJECT_CLASS (clutter_input_device_evdev_parent_class)->finalize (object);
}

static void
clutter_input_device_evdev_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ClutterInputDeviceEvdev *device = CLUTTER_INPUT_DEVICE_EVDEV (object);

  switch (prop_id)
    {
    case PROP_DEVICE_MATRIX:
      {
        const cairo_matrix_t *matrix = g_value_get_boxed (value);
        cairo_matrix_init_identity (&device->device_matrix);
        cairo_matrix_multiply (&device->device_matrix,
                               &device->device_matrix, matrix);
        break;
      }
    case PROP_OUTPUT_ASPECT_RATIO:
      device->output_ratio = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clutter_input_device_evdev_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ClutterInputDeviceEvdev *device = CLUTTER_INPUT_DEVICE_EVDEV (object);

  switch (prop_id)
    {
    case PROP_DEVICE_MATRIX:
      g_value_set_boxed (value, &device->device_matrix);
      break;
    case PROP_OUTPUT_ASPECT_RATIO:
      g_value_set_double (value, device->output_ratio);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
clutter_input_device_evdev_keycode_to_evdev (ClutterInputDevice *device,
                                             guint hardware_keycode,
                                             guint *evdev_keycode)
{
  /* The hardware keycodes from the evdev backend are almost evdev
     keycodes: we use the evdev keycode file, but xkb rules have an
     offset by 8. See the comment in _clutter_key_event_new_from_evdev()
  */
  *evdev_keycode = hardware_keycode - 8;
  return TRUE;
}

static void
clutter_input_device_evdev_update_from_tool (ClutterInputDevice     *device,
                                             ClutterInputDeviceTool *tool)
{
  ClutterInputDeviceToolEvdev *evdev_tool;

  evdev_tool = CLUTTER_INPUT_DEVICE_TOOL_EVDEV (tool);

  g_object_freeze_notify (G_OBJECT (device));

  _clutter_input_device_reset_axes (device);

  _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_X, 0, 0, 0);
  _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_Y, 0, 0, 0);

  if (libinput_tablet_tool_has_distance (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_DISTANCE, 0, 1, 0);

  if (libinput_tablet_tool_has_pressure (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_PRESSURE, 0, 1, 0);

  if (libinput_tablet_tool_has_tilt (evdev_tool->tool))
    {
      _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_XTILT, -90, 90, 0);
      _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_YTILT, -90, 90, 0);
    }

  if (libinput_tablet_tool_has_rotation (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_ROTATION, 0, 360, 0);

  if (libinput_tablet_tool_has_slider (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_SLIDER, -1, 1, 0);

  if (libinput_tablet_tool_has_wheel (evdev_tool->tool))
    _clutter_input_device_add_axis (device, CLUTTER_INPUT_AXIS_WHEEL, -180, 180, 0);

  g_object_thaw_notify (G_OBJECT (device));
}

static gboolean
clutter_input_device_evdev_is_mode_switch_button (ClutterInputDevice *device,
                                                  guint               group,
                                                  guint               button)
{
  struct libinput_device *libinput_device;
  struct libinput_tablet_pad_mode_group *mode_group;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  mode_group = libinput_device_tablet_pad_get_mode_group (libinput_device, group);

  return libinput_tablet_pad_mode_group_button_is_toggle (mode_group, button) != 0;
}

static gint
clutter_input_device_evdev_get_group_n_modes (ClutterInputDevice *device,
                                              gint                group)
{
  struct libinput_device *libinput_device;
  struct libinput_tablet_pad_mode_group *mode_group;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  mode_group = libinput_device_tablet_pad_get_mode_group (libinput_device, group);

  return libinput_tablet_pad_mode_group_get_num_modes (mode_group);
}

static gboolean
clutter_input_device_evdev_is_grouped (ClutterInputDevice *device,
                                       ClutterInputDevice *other_device)
{
  struct libinput_device *libinput_device, *other_libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  other_libinput_device = clutter_evdev_input_device_get_libinput_device (other_device);

  return libinput_device_get_device_group (libinput_device) ==
    libinput_device_get_device_group (other_libinput_device);
}

static void
clutter_input_device_evdev_bell_notify (void)
{
  ClutterBackend *backend;

  backend = clutter_get_default_backend ();
  clutter_backend_bell_notify (backend);
}

static void
clutter_input_device_evdev_free_pending_slow_key (gpointer data)
{
  SlowKeysEventPending *slow_keys_event = data;

  clutter_event_free (slow_keys_event->event);
  if (slow_keys_event->timer)
    g_source_remove (slow_keys_event->timer);
  g_free (slow_keys_event);
}

static void
clear_slow_keys (ClutterInputDeviceEvdev *device)
{
  g_list_free_full (device->slow_keys_list, clutter_input_device_evdev_free_pending_slow_key);
  g_list_free (device->slow_keys_list);
  device->slow_keys_list = NULL;
}

static guint
get_slow_keys_delay (ClutterInputDevice *device)
{
  ClutterKbdA11ySettings a11y_settings;

  clutter_device_manager_get_kbd_a11y_settings (device->device_manager,
                                                &a11y_settings);
  /* Settings use int, we use uint, make sure we dont go negative */
  return MAX (0, a11y_settings.slowkeys_delay);
}

static gboolean
trigger_slow_keys (gpointer data)
{
  SlowKeysEventPending *slow_keys_event = data;
  ClutterInputDeviceEvdev *device = slow_keys_event->device;
  ClutterKeyEvent *key_event = (ClutterKeyEvent *) slow_keys_event->event;

  /* Alter timestamp and emit the event */
  key_event->time = us2ms (g_get_monotonic_time ());
  slow_keys_event->emit_event_func (slow_keys_event->event,
                                    CLUTTER_INPUT_DEVICE (device));

  /* Then remote the pending event */
  device->slow_keys_list = g_list_remove (device->slow_keys_list, slow_keys_event);
  clutter_input_device_evdev_free_pending_slow_key (slow_keys_event);

  if (device->a11y_flags & CLUTTER_A11Y_SLOW_KEYS_BEEP_ACCEPT)
    clutter_input_device_evdev_bell_notify ();

  return G_SOURCE_REMOVE;
}

static gint
find_pending_event_by_keycode (gconstpointer a,
                               gconstpointer b)
{
  const SlowKeysEventPending *pa = a;
  const ClutterKeyEvent *ka = (ClutterKeyEvent *) pa->event;
  const ClutterKeyEvent *kb = b;

  return kb->hardware_keycode - ka->hardware_keycode;
}

static void
start_slow_keys (ClutterEvent               *event,
                 ClutterInputDeviceEvdev    *device,
                 ClutterEmitInputDeviceEvent emit_event_func)
{
  SlowKeysEventPending *slow_keys_event;
  ClutterKeyEvent *key_event = (ClutterKeyEvent *) event;

  /* Synthetic key events are for autorepeat, ignore those... */
  if (key_event->flags & CLUTTER_EVENT_FLAG_SYNTHETIC)
    return;

  slow_keys_event = g_new0 (SlowKeysEventPending, 1);
  slow_keys_event->device = device;
  slow_keys_event->event = clutter_event_copy (event);
  slow_keys_event->emit_event_func = emit_event_func;
  slow_keys_event->timer =
    clutter_threads_add_timeout (get_slow_keys_delay (CLUTTER_INPUT_DEVICE (device)),
                                 trigger_slow_keys,
                                 slow_keys_event);
  device->slow_keys_list = g_list_append (device->slow_keys_list, slow_keys_event);

  if (device->a11y_flags & CLUTTER_A11Y_SLOW_KEYS_BEEP_PRESS)
    clutter_input_device_evdev_bell_notify ();
}

static void
stop_slow_keys (ClutterEvent               *event,
                ClutterInputDeviceEvdev    *device,
                ClutterEmitInputDeviceEvent emit_event_func)
{
  GList *item;

  /* Check if we have a slow key event queued for this key event */
  item = g_list_find_custom (device->slow_keys_list, event, find_pending_event_by_keycode);
  if (item)
    {
      SlowKeysEventPending *slow_keys_event = item->data;

      device->slow_keys_list = g_list_delete_link (device->slow_keys_list, item);
      clutter_input_device_evdev_free_pending_slow_key (slow_keys_event);

      if (device->a11y_flags & CLUTTER_A11Y_SLOW_KEYS_BEEP_REJECT)
        clutter_input_device_evdev_bell_notify ();

      return;
    }

  /* If no key press event was pending, just emit the key release as-is */
  emit_event_func (event, CLUTTER_INPUT_DEVICE (device));
}

static guint
get_debounce_delay (ClutterInputDevice *device)
{
  ClutterKbdA11ySettings a11y_settings;

  clutter_device_manager_get_kbd_a11y_settings (device->device_manager,
                                                &a11y_settings);
  /* Settings use int, we use uint, make sure we dont go negative */
  return MAX (0, a11y_settings.debounce_delay);
}

static gboolean
clear_bounce_keys (gpointer data)
{
  ClutterInputDeviceEvdev *device = data;

  device->debounce_key = 0;
  device->debounce_timer = 0;

  return G_SOURCE_REMOVE;
}

static void
start_bounce_keys (ClutterEvent            *event,
                   ClutterInputDeviceEvdev *device)
{
  stop_bounce_keys (device);

  device->debounce_key = ((ClutterKeyEvent *) event)->hardware_keycode;
  device->debounce_timer =
    clutter_threads_add_timeout (get_debounce_delay (CLUTTER_INPUT_DEVICE (device)),
                                 clear_bounce_keys,
                                 device);
}

static void
stop_bounce_keys (ClutterInputDeviceEvdev *device)
{
  if (device->debounce_timer)
    {
      g_source_remove (device->debounce_timer);
      device->debounce_timer = 0;
    }
}

static void
notify_bounce_keys_reject (ClutterInputDeviceEvdev *device)
{
  if (device->a11y_flags & CLUTTER_A11Y_BOUNCE_KEYS_BEEP_REJECT)
    clutter_input_device_evdev_bell_notify ();
}

static gboolean
debounce_key (ClutterEvent            *event,
              ClutterInputDeviceEvdev *device)
{
  return (device->debounce_key == ((ClutterKeyEvent *) event)->hardware_keycode);
}

static void
clutter_input_device_evdev_process_kbd_a11y_event (ClutterEvent               *event,
                                                   ClutterInputDevice         *device,
                                                   ClutterEmitInputDeviceEvent emit_event_func)
{
  ClutterInputDeviceEvdev *device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);

  if (!device_evdev->a11y_flags & CLUTTER_A11Y_KEYBOARD_ENABLED)
    goto emit_event;

  if ((device_evdev->a11y_flags & CLUTTER_A11Y_BOUNCE_KEYS_ENABLED) &&
      (get_debounce_delay (device) != 0))
    {
      if ((event->type == CLUTTER_KEY_PRESS) && debounce_key (event, device_evdev))
        {
          notify_bounce_keys_reject (device_evdev);

          return;
        }
      else if (event->type == CLUTTER_KEY_RELEASE)
        start_bounce_keys (event, device_evdev);
    }

  if ((device_evdev->a11y_flags & CLUTTER_A11Y_SLOW_KEYS_ENABLED) &&
      (get_slow_keys_delay (device) != 0))
    {
      if (event->type == CLUTTER_KEY_PRESS)
        start_slow_keys (event, device_evdev, emit_event_func);
      else if (event->type == CLUTTER_KEY_RELEASE)
        stop_slow_keys (event, device_evdev, emit_event_func);

      return;
    }

emit_event:
  emit_event_func (event, device);
}

void
clutter_input_device_evdev_apply_kbd_a11y_settings (ClutterInputDeviceEvdev *device,
                                                    ClutterKbdA11ySettings  *settings)
{
  ClutterKeyboardA11yFlags changed_flags = (device->a11y_flags ^ settings->controls);

  if (changed_flags & (CLUTTER_A11Y_KEYBOARD_ENABLED | CLUTTER_A11Y_SLOW_KEYS_ENABLED))
    clear_slow_keys (device);

  if (changed_flags & (CLUTTER_A11Y_KEYBOARD_ENABLED | CLUTTER_A11Y_BOUNCE_KEYS_ENABLED))
    device->debounce_key = 0;

  /* Keep our own copy of keyboard a11y features flags to see what changes */
  device->a11y_flags = settings->controls;
}

static void
clutter_input_device_evdev_class_init (ClutterInputDeviceEvdevClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = clutter_input_device_evdev_finalize;
  object_class->set_property = clutter_input_device_evdev_set_property;
  object_class->get_property = clutter_input_device_evdev_get_property;

  klass->keycode_to_evdev = clutter_input_device_evdev_keycode_to_evdev;
  klass->update_from_tool = clutter_input_device_evdev_update_from_tool;
  klass->is_mode_switch_button = clutter_input_device_evdev_is_mode_switch_button;
  klass->get_group_n_modes = clutter_input_device_evdev_get_group_n_modes;
  klass->is_grouped = clutter_input_device_evdev_is_grouped;
  klass->process_kbd_a11y_event = clutter_input_device_evdev_process_kbd_a11y_event;

  obj_props[PROP_DEVICE_MATRIX] =
    g_param_spec_boxed ("device-matrix",
			P_("Device input matrix"),
			P_("Device input matrix"),
			CAIRO_GOBJECT_TYPE_MATRIX,
			CLUTTER_PARAM_READWRITE);
  obj_props[PROP_OUTPUT_ASPECT_RATIO] =
    g_param_spec_double ("output-aspect-ratio",
                         P_("Output aspect ratio"),
                         P_("Output aspect ratio"),
                         0, G_MAXDOUBLE, 0,
                         CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, obj_props);
}

static void
clutter_input_device_evdev_init (ClutterInputDeviceEvdev *self)
{
  cairo_matrix_init_identity (&self->device_matrix);
  self->device_aspect_ratio = 0;
  self->output_ratio = 0;
}

/*
 * _clutter_input_device_evdev_new:
 * @manager: the device manager
 * @seat: the seat the device will belong to
 * @libinput_device: the libinput device
 *
 * Create a new ClutterInputDevice given a libinput device and associate
 * it with the provided seat.
 */
ClutterInputDevice *
_clutter_input_device_evdev_new (ClutterDeviceManager *manager,
                                 ClutterSeatEvdev *seat,
                                 struct libinput_device *libinput_device)
{
  ClutterInputDeviceEvdev *device;
  ClutterInputDeviceType type;
  ClutterDeviceManagerEvdev *manager_evdev;
  gchar *vendor, *product;
  gint device_id, n_rings = 0, n_strips = 0, n_groups = 1;
  gchar *node_path;
  gdouble width, height;

  type = _clutter_input_device_evdev_determine_type (libinput_device);
  vendor = g_strdup_printf ("%.4x", libinput_device_get_id_vendor (libinput_device));
  product = g_strdup_printf ("%.4x", libinput_device_get_id_product (libinput_device));
  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  device_id = _clutter_device_manager_evdev_acquire_device_id (manager_evdev);
  node_path = g_strdup_printf ("/dev/input/%s", libinput_device_get_sysname (libinput_device));

  if (libinput_device_has_capability (libinput_device,
                                      LIBINPUT_DEVICE_CAP_TABLET_PAD))
    {
      n_rings = libinput_device_tablet_pad_get_num_rings (libinput_device);
      n_strips = libinput_device_tablet_pad_get_num_strips (libinput_device);
      n_groups = libinput_device_tablet_pad_get_num_mode_groups (libinput_device);
    }

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", device_id,
                         "name", libinput_device_get_name (libinput_device),
                         "device-manager", manager,
                         "device-type", type,
                         "device-mode", CLUTTER_INPUT_MODE_SLAVE,
                         "enabled", TRUE,
                         "vendor-id", vendor,
                         "product-id", product,
                         "n-rings", n_rings,
                         "n-strips", n_strips,
                         "n-mode-groups", n_groups,
                         "device-node", node_path,
                         NULL);

  device->seat = seat;
  device->libinput_device = libinput_device;

  libinput_device_set_user_data (libinput_device, device);
  libinput_device_ref (libinput_device);
  g_free (vendor);
  g_free (product);
  g_free (node_path);

  if (libinput_device_get_size (libinput_device, &width, &height) == 0)
    device->device_aspect_ratio = width / height;

  return CLUTTER_INPUT_DEVICE (device);
}

/*
 * _clutter_input_device_evdev_new_virtual:
 * @manager: the device manager
 * @seat: the seat the device will belong to
 * @type: the input device type
 *
 * Create a new virtual ClutterInputDevice of the given type.
 */
ClutterInputDevice *
_clutter_input_device_evdev_new_virtual (ClutterDeviceManager *manager,
                                         ClutterSeatEvdev *seat,
                                         ClutterInputDeviceType type,
                                         ClutterInputMode mode)
{
  ClutterInputDeviceEvdev *device;
  ClutterDeviceManagerEvdev *manager_evdev;
  const char *name;
  gint device_id;

  switch (type)
    {
    case CLUTTER_KEYBOARD_DEVICE:
      name = "Virtual keyboard device for seat";
      break;
    case CLUTTER_POINTER_DEVICE:
      name = "Virtual pointer device for seat";
      break;
    default:
      name = "Virtual device for seat";
      break;
    };

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  device_id = _clutter_device_manager_evdev_acquire_device_id (manager_evdev);
  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", device_id,
                         "name", name,
                         "device-manager", manager,
                         "device-type", type,
                         "device-mode", mode,
                         "enabled", TRUE,
                         NULL);

  device->seat = seat;

  return CLUTTER_INPUT_DEVICE (device);
}

ClutterSeatEvdev *
_clutter_input_device_evdev_get_seat (ClutterInputDeviceEvdev *device)
{
  return device->seat;
}

void
_clutter_input_device_evdev_update_leds (ClutterInputDeviceEvdev *device,
                                         enum libinput_led leds)
{
  if (!device->libinput_device)
    return;

  libinput_device_led_update (device->libinput_device, leds);
}

ClutterInputDeviceType
_clutter_input_device_evdev_determine_type (struct libinput_device *ldev)
{
  /* This setting is specific to touchpads and alike, only in these
   * devices there is this additional layer of touch event interpretation.
   */
  if (libinput_device_config_tap_get_finger_count (ldev) > 0)
    return CLUTTER_TOUCHPAD_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
    return CLUTTER_TABLET_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_TABLET_PAD))
    return CLUTTER_PAD_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_POINTER))
    return CLUTTER_POINTER_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_TOUCH))
    return CLUTTER_TOUCHSCREEN_DEVICE;
  else if (libinput_device_has_capability (ldev, LIBINPUT_DEVICE_CAP_KEYBOARD))
    return CLUTTER_KEYBOARD_DEVICE;
  else
    return CLUTTER_EXTENSION_DEVICE;
}

/**
 * clutter_evdev_input_device_get_libinput_device:
 * @device: a #ClutterInputDevice
 *
 * Retrieves the libinput_device struct held in @device.
 *
 * Returns: The libinput_device struct
 *
 * Since: 1.20
 * Stability: unstable
 **/
struct libinput_device *
clutter_evdev_input_device_get_libinput_device (ClutterInputDevice *device)
{
  ClutterInputDeviceEvdev *device_evdev;

  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_EVDEV (device), NULL);

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);

  return device_evdev->libinput_device;
}

/**
 * clutter_evdev_event_sequence_get_slot:
 * @sequence: a #ClutterEventSequence
 *
 * Retrieves the touch slot triggered by this @sequence
 *
 * Returns: the libinput touch slot.
 *
 * Since: 1.20
 * Stability: unstable
 **/
gint32
clutter_evdev_event_sequence_get_slot (const ClutterEventSequence *sequence)
{
  if (!sequence)
    return -1;

  return GPOINTER_TO_INT (sequence) - 1;
}

void
clutter_input_device_evdev_translate_coordinates (ClutterInputDevice *device,
                                                  ClutterStage       *stage,
                                                  gfloat             *x,
                                                  gfloat             *y)
{
  ClutterInputDeviceEvdev *device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);
  double min_x = 0, min_y = 0, max_x = 1, max_y = 1;
  gdouble stage_width, stage_height;
  double x_d, y_d;

  stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
  stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));
  x_d = *x / stage_width;
  y_d = *y / stage_height;

  /* Apply aspect ratio */
  if (device_evdev->output_ratio > 0 &&
      device_evdev->device_aspect_ratio > 0)
    {
      gdouble ratio = device_evdev->device_aspect_ratio / device_evdev->output_ratio;

      if (ratio > 1)
        x_d *= ratio;
      else if (ratio < 1)
        y_d *= 1 / ratio;
    }

  cairo_matrix_transform_point (&device_evdev->device_matrix, &min_x, &min_y);
  cairo_matrix_transform_point (&device_evdev->device_matrix, &max_x, &max_y);
  cairo_matrix_transform_point (&device_evdev->device_matrix, &x_d, &y_d);

  *x = CLAMP (x_d, MIN (min_x, max_x), MAX (min_x, max_x)) * stage_width;
  *y = CLAMP (y_d, MIN (min_y, max_y), MAX (min_y, max_y)) * stage_height;
}
