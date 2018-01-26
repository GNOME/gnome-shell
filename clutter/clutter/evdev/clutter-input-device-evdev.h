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

#ifndef __CLUTTER_INPUT_DEVICE_EVDEV_H__
#define __CLUTTER_INPUT_DEVICE_EVDEV_H__

#include <glib-object.h>
#include <libinput.h>

#include "clutter/clutter-device-manager-private.h"
#include "evdev/clutter-seat-evdev.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE_EVDEV _clutter_input_device_evdev_get_type()

#define CLUTTER_INPUT_DEVICE_EVDEV(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV, ClutterInputDeviceEvdev))

#define CLUTTER_INPUT_DEVICE_EVDEV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV, ClutterInputDeviceEvdevClass))

#define CLUTTER_IS_INPUT_DEVICE_EVDEV(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV))

#define CLUTTER_IS_INPUT_DEVICE_EVDEV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV))

#define CLUTTER_INPUT_DEVICE_EVDEV_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_INPUT_DEVICE_EVDEV, ClutterInputDeviceEvdevClass))

typedef struct _ClutterInputDeviceEvdev ClutterInputDeviceEvdev;
typedef struct _ClutterEventEvdev ClutterEventEvdev;

struct _ClutterInputDeviceEvdev
{
  ClutterInputDevice parent;

  struct libinput_device *libinput_device;
  ClutterSeatEvdev *seat;
  ClutterInputDeviceTool *last_tool;

  cairo_matrix_t device_matrix;
  gdouble device_aspect_ratio; /* w:h */
  gdouble output_ratio;        /* w:h */

  GHashTable *touches;

  /* Keyboard a11y */
  ClutterKeyboardA11yFlags a11y_flags;
  GList *slow_keys_list;
  guint debounce_timer;
  guint16 debounce_key;
  xkb_mod_mask_t stickykeys_depressed_mask;
  xkb_mod_mask_t stickykeys_latched_mask;
  xkb_mod_mask_t stickykeys_locked_mask;
  guint toggle_slowkeys_timer;
  guint16 shift_count;
  guint32 last_shift_time;
  gint mousekeys_btn;
  gboolean mousekeys_btn_states[3];
  guint32 mousekeys_first_motion_time; /* ms */
  guint32 mousekeys_last_motion_time; /* ms */
  guint mousekeys_init_delay;
  guint mousekeys_accel_time;
  guint mousekeys_max_speed;
  gdouble mousekeys_curve_factor;
  guint move_mousekeys_timer;
  guint16 last_mousekeys_key;
  ClutterVirtualInputDevice *mousekeys_virtual_device;
};

GType                     _clutter_input_device_evdev_get_type        (void) G_GNUC_CONST;

ClutterInputDevice *      _clutter_input_device_evdev_new             (ClutterDeviceManager    *manager,
                                                                       ClutterSeatEvdev        *seat,
                                                                       struct libinput_device  *libinput_device);

ClutterInputDevice *      _clutter_input_device_evdev_new_virtual     (ClutterDeviceManager    *manager,
                                                                       ClutterSeatEvdev        *seat,
                                                                       ClutterInputDeviceType   type,
                                                                       ClutterInputMode         mode);

ClutterSeatEvdev *        _clutter_input_device_evdev_get_seat        (ClutterInputDeviceEvdev *device);

void                      _clutter_input_device_evdev_update_leds     (ClutterInputDeviceEvdev *device,
                                                                       enum libinput_led        leds);

ClutterInputDeviceType    _clutter_input_device_evdev_determine_type  (struct libinput_device  *libinput_device);


ClutterEventEvdev *       _clutter_event_evdev_copy                   (ClutterEventEvdev *event_evdev);
void                      _clutter_event_evdev_free                   (ClutterEventEvdev *event_evdev);

void                      _clutter_evdev_event_set_event_code         (ClutterEvent      *event,
                                                                       guint32            evcode);

void                      _clutter_evdev_event_set_time_usec       (ClutterEvent *event,
								    guint64       time_usec);

void  			  _clutter_evdev_event_set_relative_motion (ClutterEvent *event,
								    double        dx,
								    double        dy,
								    double        dx_unaccel,
								    double        dy_unaccel);

void                      clutter_input_device_evdev_translate_coordinates (ClutterInputDevice *device,
                                                                            ClutterStage       *stage,
                                                                            gfloat             *x,
                                                                            gfloat             *y);

void                      clutter_input_device_evdev_apply_kbd_a11y_settings (ClutterInputDeviceEvdev *device,
                                                                              ClutterKbdA11ySettings  *settings);

ClutterTouchState *       clutter_input_device_evdev_acquire_touch_state (ClutterInputDeviceEvdev *device,
                                                                          int                      device_slot);

ClutterTouchState *       clutter_input_device_evdev_lookup_touch_state (ClutterInputDeviceEvdev *device,
                                                                         int                      device_slot);

void                      clutter_input_device_evdev_release_touch_state (ClutterInputDeviceEvdev *device,
                                                                          ClutterTouchState       *touch_state);

void                      clutter_input_device_evdev_release_touch_slots (ClutterInputDeviceEvdev *device_evdev,
                                                                          uint64_t                 time_us);


G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_EVDEV_H__ */
