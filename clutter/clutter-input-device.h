/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009 Intel Corp.
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

  CLUTTER_N_DEVICE_TYPES
} ClutterInputDeviceType;

/**
 * ClutterInputDeviceClass:
 *
 * The #ClutterInputDeviceClass structure contains only private
 * data and should not be accessed directly
 *
 * Since: 1.2
 */
struct _ClutterInputDeviceClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GType clutter_input_device_get_type (void) G_GNUC_CONST;

ClutterInputDeviceType clutter_input_device_get_device_type   (ClutterInputDevice *device);
gint                   clutter_input_device_get_device_id     (ClutterInputDevice *device);
void                   clutter_input_device_get_device_coords (ClutterInputDevice *device,
                                                               gint               *x,
                                                               gint               *y);
ClutterActor *         clutter_input_device_get_pointer_actor (ClutterInputDevice *device);
G_CONST_RETURN gchar * clutter_input_device_get_device_name   (ClutterInputDevice *device);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_H__ */
