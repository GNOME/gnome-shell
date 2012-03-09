/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 *  Kristian Høgsberg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <wayland-util.h>
#include <wayland-client.h>

#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-private.h"
#include "clutter-keysyms.h"
#include "evdev/clutter-xkb-utils.h"
#include "clutter-input-device-wayland.h"
#include "clutter-backend-wayland.h"
#include "clutter-stage-private.h"
#include "clutter-wayland.h"

#include "cogl/clutter-stage-cogl.h"

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceWaylandClass;

G_DEFINE_TYPE (ClutterInputDeviceWayland,
               clutter_input_device_wayland,
               CLUTTER_TYPE_INPUT_DEVICE);

static void
clutter_wayland_handle_motion (void *data,
                               struct wl_input_device *input_device,
                               uint32_t _time,
                               int32_t x, int32_t y,
                               int32_t sx, int32_t sy)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl = device->pointer_focus;
  ClutterEvent              *event;

  event = clutter_event_new (CLUTTER_MOTION);
  event->motion.stage = stage_cogl->wrapper;
  event->motion.device = CLUTTER_INPUT_DEVICE (device);
  event->motion.time = _time;
  event->motion.modifier_state = 0;
  event->motion.x = sx;
  event->motion.y = sy;

  device->surface_x = sx;
  device->surface_y = sy;
  device->x = x;
  device->y = y;

  _clutter_event_push (event, FALSE);
}

static void
clutter_wayland_handle_button (void *data,
                               struct wl_input_device *input_device,
                               uint32_t _time,
                               uint32_t button, uint32_t state)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl = device->pointer_focus;
  ClutterEvent              *event;
  ClutterEventType           type;

  if (state)
    type = CLUTTER_BUTTON_PRESS;
  else
    type = CLUTTER_BUTTON_RELEASE;

  event = clutter_event_new (type);
  event->button.stage = stage_cogl->wrapper;
  event->button.device = CLUTTER_INPUT_DEVICE (device);
  event->button.time = _time;
  event->button.x = device->surface_x;
  event->button.y = device->surface_y;
  event->button.modifier_state = device->modifier_state;

  /* evdev button codes */
  switch (button) {
  case 272:
    event->button.button = 1;
    break;
  case 273:
    event->button.button = 3;
    break;
  case 274:
    event->button.button = 2;
    break;
  }

  _clutter_event_push (event, FALSE);
}

static void
clutter_wayland_handle_key (void *data,
                            struct wl_input_device *input_device,
                            uint32_t _time,
                            uint32_t key, uint32_t state)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl = device->keyboard_focus;
  ClutterEvent              *event;

  event = _clutter_key_event_new_from_evdev ((ClutterInputDevice *) device,
                                             stage_cogl->wrapper,
                                             device->xkb,
                                             _time, key, state,
                                             &device->modifier_state);

  _clutter_event_push (event, FALSE);
}

static void
clutter_wayland_handle_pointer_focus (void *data,
                                      struct wl_input_device *input_device,
                                      uint32_t _time,
                                      struct wl_surface *surface,
                                      int32_t x, int32_t y, int32_t sx, int32_t sy)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;
  ClutterEvent              *event;

  if (!surface)
    {
      stage_cogl = device->pointer_focus;

      event = clutter_event_new (CLUTTER_LEAVE);
      event->crossing.stage = stage_cogl->wrapper;
      event->crossing.time = _time;
      event->crossing.x = sx;
      event->crossing.y = sy;
      event->crossing.source = CLUTTER_ACTOR (stage_cogl->wrapper);
      event->crossing.device = CLUTTER_INPUT_DEVICE (device);

      _clutter_event_push (event, FALSE);

      device->pointer_focus = NULL;
      _clutter_input_device_set_stage (CLUTTER_INPUT_DEVICE (device), NULL);
    }

  if (surface)
    {
      ClutterBackend        *backend;
      ClutterBackendWayland *backend_wayland;

      stage_cogl = wl_surface_get_user_data (surface);

      device->pointer_focus = stage_cogl;
      _clutter_input_device_set_stage (CLUTTER_INPUT_DEVICE (device),
				       stage_cogl->wrapper);

      event = clutter_event_new (CLUTTER_ENTER);
      event->crossing.stage = stage_cogl->wrapper;
      event->crossing.time = _time;
      event->crossing.x = sx;
      event->crossing.y = sy;
      event->crossing.source = CLUTTER_ACTOR (stage_cogl->wrapper);
      event->crossing.device = CLUTTER_INPUT_DEVICE (device);

      _clutter_event_push (event, FALSE);

      device->surface_x = sx;
      device->surface_y = sy;
      device->x = x;
      device->y = y;

      /* Set the cursor to the cursor loaded at backend initialisation */
      backend = clutter_get_default_backend ();
      backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);

      wl_input_device_attach (input_device,
                              _time,
                              backend_wayland->cursor_buffer,
                              backend_wayland->cursor_x,
                              backend_wayland->cursor_y);
    }
}

static void
clutter_wayland_handle_keyboard_focus (void *data,
                                       struct wl_input_device *input_device,
                                       uint32_t _time,
                                       struct wl_surface *surface,
                                       struct wl_array *keys)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;
  uint32_t                  *k, *end;

  if (device->keyboard_focus)
    {
      stage_cogl = device->keyboard_focus;
      device->keyboard_focus = NULL;

      _clutter_stage_update_state (stage_cogl->wrapper,
                                   CLUTTER_STAGE_STATE_ACTIVATED,
                                   0);
    }

  if (surface)
    {
      stage_cogl = wl_surface_get_user_data (surface);
      device->keyboard_focus = stage_cogl;

      _clutter_stage_update_state (stage_cogl->wrapper,
                                   0,
                                   CLUTTER_STAGE_STATE_ACTIVATED);

      end = (uint32_t *)((guint8 *)keys->data + keys->size);
      device->modifier_state = 0;
      for (k = keys->data; k < end; k++)
	device->modifier_state |= device->xkb->map->modmap[*k];
    }
}

const struct wl_input_device_listener _clutter_input_device_wayland_listener = {
  clutter_wayland_handle_motion,
  clutter_wayland_handle_button,
  clutter_wayland_handle_key,
  clutter_wayland_handle_pointer_focus,
  clutter_wayland_handle_keyboard_focus,
};

static gboolean
clutter_input_device_wayland_keycode_to_evdev (ClutterInputDevice *device,
                                               guint hardware_keycode,
                                               guint *evdev_keycode)
{
  /* The hardware keycodes from the wayland backend are already evdev
     keycodes */
  *evdev_keycode = hardware_keycode;
  return TRUE;
}

static void
clutter_input_device_wayland_class_init (ClutterInputDeviceWaylandClass *klass)
{
  klass->keycode_to_evdev = clutter_input_device_wayland_keycode_to_evdev;
}

static void
clutter_input_device_wayland_init (ClutterInputDeviceWayland *self)
{
}

/**
 * clutter_wayland_input_device_get_wl_input_device: (skip)
 *
 * @device: a #ClutterInputDevice
 *
 * Access the underlying data structure representing the Wayland device that is
 * backing this #ClutterInputDevice.
 *
 * Note: this function can only be called when running on the Wayland platform.
 * Calling this function at any other time will return %NULL.
 *
 * Returns: (transfer none): the Wayland input device associated with the
 * @device
 *
 * Since: 1.10
 */
struct wl_input_device *
clutter_wayland_input_device_get_wl_input_device (ClutterInputDevice *device)
{
  ClutterInputDeviceWayland *wayland_device;

  if (!CLUTTER_INPUT_DEVICE_WAYLAND (device))
    return NULL;

  wayland_device = CLUTTER_INPUT_DEVICE_WAYLAND (device);

  return wayland_device->input_device;
}
