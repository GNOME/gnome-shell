/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2009, 2010, 2011  Intel Corp.
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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_INPUT_DEVICE_H__
#define __CLUTTER_INPUT_DEVICE_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_INPUT_DEVICE               (clutter_input_device_get_type ())
#define CLUTTER_INPUT_DEVICE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE, ClutterInputDevice))
#define CLUTTER_IS_INPUT_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE))
#define CLUTTER_INPUT_DEVICE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_INPUT_DEVICE, ClutterInputDeviceClass))
#define CLUTTER_IS_INPUT_DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_INPUT_DEVICE))
#define CLUTTER_INPUT_DEVICE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_INPUT_DEVICE, ClutterInputDeviceClass))

/**
 * ClutterInputDevice:
 *
 * Generic representation of an input device. The actual contents of this
 * structure depend on the backend used.
 */
typedef struct _ClutterInputDevice      ClutterInputDevice;
typedef struct _ClutterInputDeviceClass ClutterInputDeviceClass;

/**
 * ClutterInputDeviceType:
 * @CLUTTER_POINTER_DEVICE: A pointer device
 * @CLUTTER_KEYBOARD_DEVICE: A keyboard device
 * @CLUTTER_EXTENSION_DEVICE: A generic extension device
 * @CLUTTER_JOYSTICK_DEVICE: A joystick device
 * @CLUTTER_TABLET_DEVICE: A tablet device
 * @CLUTTER_TOUCHPAD_DEVICE: A touchpad device
 * @CLUTTER_TOUCHSCREEN_DEVICE: A touch screen device
 * @CLUTTER_PEN_DEVICE: A pen device
 * @CLUTTER_ERASER_DEVICE: An eraser device
 * @CLUTTER_CURSOR_DEVICE: A cursor device
 * @CLUTTER_N_DEVICE_TYPES: The number of device types
 *
 * The types of input devices available.
 *
 * The #ClutterInputDeviceType enumeration can be extended at later
 * date; not every platform supports every input device type.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_POINTER_DEVICE,
  CLUTTER_KEYBOARD_DEVICE,
  CLUTTER_EXTENSION_DEVICE,
  CLUTTER_JOYSTICK_DEVICE,
  CLUTTER_TABLET_DEVICE,
  CLUTTER_TOUCHPAD_DEVICE,
  CLUTTER_TOUCHSCREEN_DEVICE,
  CLUTTER_PEN_DEVICE,
  CLUTTER_ERASER_DEVICE,
  CLUTTER_CURSOR_DEVICE,

  CLUTTER_N_DEVICE_TYPES
} ClutterInputDeviceType;

/**
 * ClutterInputMode:
 * @CLUTTER_INPUT_MODE_MASTER: A master, virtual device
 * @CLUTTER_INPUT_MODE_SLAVE: A slave, physical device, attached to
 *   a master device
 * @CLUTTER_INPUT_MODE_FLOATING: A slave, physical device, not attached
 *   to a master device
 *
 * The mode for input devices available.
 *
 * Since: 1.6
 */
typedef enum {
  CLUTTER_INPUT_MODE_MASTER,
  CLUTTER_INPUT_MODE_SLAVE,
  CLUTTER_INPUT_MODE_FLOATING
} ClutterInputMode;

/**
 * ClutterInputAxis:
 * @CLUTTER_INPUT_AXIS_IGNORE: Unused axis
 * @CLUTTER_INPUT_AXIS_X: The position on the X axis
 * @CLUTTER_INPUT_AXIS_Y: The position of the Y axis
 * @CLUTTER_INPUT_AXIS_PRESSURE: The pressure information
 * @CLUTTER_INPUT_AXIS_XTILT: The tilt on the X axis
 * @CLUTTER_INPUT_AXIS_YTILT: The tile on the Y axis
 * @CLUTTER_INPUT_AXIS_WHEEL: A wheel
 *
 * The type of axes Clutter recognizes on a #ClutterInputDevice
 *
 * Since: 1.6
 */
typedef enum {
  CLUTTER_INPUT_AXIS_IGNORE,
  CLUTTER_INPUT_AXIS_X,
  CLUTTER_INPUT_AXIS_Y,
  CLUTTER_INPUT_AXIS_PRESSURE,
  CLUTTER_INPUT_AXIS_XTILT,
  CLUTTER_INPUT_AXIS_YTILT,
  CLUTTER_INPUT_AXIS_WHEEL
} ClutterInputAxis;

GType clutter_input_device_get_type (void) G_GNUC_CONST;

ClutterInputDeviceType  clutter_input_device_get_device_type    (ClutterInputDevice  *device);
gint                    clutter_input_device_get_device_id      (ClutterInputDevice  *device);
void                    clutter_input_device_get_device_coords  (ClutterInputDevice  *device,
                                                                 gint                *x,
                                                                 gint                *y);
ClutterActor *          clutter_input_device_get_pointer_actor  (ClutterInputDevice  *device);
ClutterStage *          clutter_input_device_get_pointer_stage  (ClutterInputDevice  *device);
const gchar *           clutter_input_device_get_device_name    (ClutterInputDevice  *device);
ClutterInputMode        clutter_input_device_get_device_mode    (ClutterInputDevice  *device);
gboolean                clutter_input_device_get_has_cursor     (ClutterInputDevice  *device);
void                    clutter_input_device_set_enabled        (ClutterInputDevice  *device,
                                                                 gboolean             enabled);
gboolean                clutter_input_device_get_enabled        (ClutterInputDevice  *device);

guint                   clutter_input_device_get_n_axes         (ClutterInputDevice  *device);
ClutterInputAxis        clutter_input_device_get_axis           (ClutterInputDevice  *device,
                                                                 guint                index_);
gboolean                clutter_input_device_get_axis_value     (ClutterInputDevice  *device,
                                                                 gdouble             *axes,
                                                                 ClutterInputAxis     axis,
                                                                 gdouble             *value);

guint                   clutter_input_device_get_n_keys         (ClutterInputDevice  *device);
void                    clutter_input_device_set_key            (ClutterInputDevice  *device,
                                                                 guint                index_,
                                                                 guint                keyval,
                                                                 ClutterModifierType  modifiers);
gboolean                clutter_input_device_get_key            (ClutterInputDevice  *device,
                                                                 guint                index_,
                                                                 guint               *keyval,
                                                                 ClutterModifierType *modifiers);

ClutterInputDevice *    clutter_input_device_get_associated_device (ClutterInputDevice *device);
GList *                 clutter_input_device_get_slave_devices  (ClutterInputDevice  *device);

void                    clutter_input_device_update_from_event  (ClutterInputDevice  *device,
                                                                 ClutterEvent        *event,
                                                                 gboolean             update_stage);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_H__ */
