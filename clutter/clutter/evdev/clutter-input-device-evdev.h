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

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_EVDEV_H__ */
