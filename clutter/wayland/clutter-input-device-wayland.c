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
#include "clutter-private.h"
#include "clutter-keysyms.h"
#include "clutter-xkb-utils.h"

#include "clutter-stage-wayland.h"

#define CLUTTER_TYPE_INPUT_DEVICE_WAYLAND       (clutter_input_device_wayland_get_type ())
#define CLUTTER_INPUT_DEVICE_WAYLAND(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INPUT_DEVICE_WAYLAND, ClutterInputDeviceWayland))
#define CLUTTER_IS_INPUT_DEVICE_WAYLAND(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INPUT_DEVICE_WAYLAND))

typedef struct _ClutterInputDeviceWayland           ClutterInputDeviceWayland;

GType clutter_input_device_wayland_get_type (void) G_GNUC_CONST;

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceWaylandClass;

struct _ClutterInputDeviceWayland
{
  ClutterInputDevice      device;
  struct wl_input_device *input_device;
  ClutterStageWayland    *pointer_focus;
  ClutterStageWayland    *keyboard_focus;
  uint32_t                modifier_state;
  int32_t                 x, y, surface_x, surface_y;
  struct xkb_desc        *xkb;
};

G_DEFINE_TYPE (ClutterInputDeviceWayland,
               clutter_input_device_wayland,
               CLUTTER_TYPE_INPUT_DEVICE);

static void
clutter_backend_wayland_handle_motion (void *data,
				       struct wl_input_device *input_device,
				       uint32_t _time,
				       int32_t x, int32_t y,
				       int32_t sx, int32_t sy)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageWayland       *stage_wayland = device->pointer_focus;
  ClutterMainContext        *clutter_context;
  ClutterEvent              *event;

  event = clutter_event_new (CLUTTER_MOTION);
  event->motion.stage = stage_wayland->wrapper;
  event->motion.device = CLUTTER_INPUT_DEVICE (device);
  event->motion.time = _time;
  event->motion.modifier_state = 0;
  event->motion.x = sx;
  event->motion.y = sy;

  device->surface_x = sx;
  device->surface_y = sy;
  device->x = x;
  device->y = y;

  clutter_context = _clutter_context_get_default ();
  g_queue_push_head (clutter_context->events_queue, event);
}

static void
clutter_backend_wayland_handle_button (void *data,
				       struct wl_input_device *input_device,
				       uint32_t _time,
				       uint32_t button, uint32_t state)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageWayland       *stage_wayland = device->pointer_focus;
  ClutterMainContext        *clutter_context;
  ClutterEvent              *event;
  ClutterEventType           type;

  if (state)
    type = CLUTTER_BUTTON_PRESS;
  else
    type = CLUTTER_BUTTON_RELEASE;

  event = clutter_event_new (type);
  event->button.stage = stage_wayland->wrapper;
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

  clutter_context = _clutter_context_get_default ();
  g_queue_push_head (clutter_context->events_queue, event);
}

static void
clutter_backend_wayland_handle_key (void *data,
				    struct wl_input_device *input_device,
				    uint32_t _time,
				    uint32_t key, uint32_t state)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageWayland       *stage_wayland = device->keyboard_focus;
  ClutterMainContext        *clutter_context;
  ClutterEvent              *event;

  event = _clutter_key_event_new_from_evdev ((ClutterInputDevice *) device,
                                             stage_wayland->wrapper,
                                             device->xkb,
                                             _time, key, state,
                                             &device->modifier_state);

  clutter_context = _clutter_context_get_default ();
  g_queue_push_head (clutter_context->events_queue, event);
}

static void
clutter_backend_wayland_handle_pointer_focus (void *data,
					      struct wl_input_device *input_device,
					      uint32_t _time,
					      struct wl_surface *surface,
					      int32_t x, int32_t y, int32_t sx, int32_t sy)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageWayland       *stage_wayland;
  ClutterMainContext        *clutter_context;
  ClutterEvent              *event;

  if (device->pointer_focus)
    {
      stage_wayland = device->pointer_focus;

      event = clutter_event_new (CLUTTER_LEAVE);
      event->crossing.stage = stage_wayland->wrapper;
      event->crossing.time = _time;
      event->crossing.x = sx;
      event->crossing.y = sy;
      event->crossing.source = CLUTTER_ACTOR (stage_wayland->wrapper);
      event->crossing.device = CLUTTER_INPUT_DEVICE (device);

      clutter_context = _clutter_context_get_default ();
      g_queue_push_head (clutter_context->events_queue, event);

      device->pointer_focus = NULL;
      _clutter_input_device_set_stage (CLUTTER_INPUT_DEVICE (device), NULL);
    }

  if (surface)
    {
      stage_wayland = wl_surface_get_user_data (surface);

      device->pointer_focus = stage_wayland;
      _clutter_input_device_set_stage (CLUTTER_INPUT_DEVICE (device),
				       stage_wayland->wrapper);

      event = clutter_event_new (CLUTTER_MOTION);
      event->motion.time = _time;
      event->motion.x = sx;
      event->motion.y = sy;
      event->motion.modifier_state = device->modifier_state;
      event->motion.source = CLUTTER_ACTOR (stage_wayland->wrapper);
      event->motion.device = CLUTTER_INPUT_DEVICE (device);

      clutter_context = _clutter_context_get_default ();
      g_queue_push_head (clutter_context->events_queue, event);

      device->surface_x = sx;
      device->surface_y = sy;
      device->x = x;
      device->y = y;

      /* Revert back to default pointer for now. */
      wl_input_device_attach (input_device, _time, NULL, 0, 0);
    }
}

static void
clutter_backend_wayland_handle_keyboard_focus (void *data,
					       struct wl_input_device *input_device,
					       uint32_t _time,
					       struct wl_surface *surface,
					       struct wl_array *keys)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageWayland       *stage_wayland;
  ClutterMainContext        *clutter_context;
  ClutterEvent              *event;
  uint32_t                  *k, *end;

  if (device->keyboard_focus)
    {
      stage_wayland = device->keyboard_focus;
      device->keyboard_focus = NULL;

      event = clutter_event_new (CLUTTER_STAGE_STATE);
      event->stage_state.time = _time;
      event->stage_state.stage = stage_wayland->wrapper;
      event->stage_state.stage = stage_wayland->wrapper;
      event->stage_state.changed_mask = CLUTTER_STAGE_STATE_ACTIVATED;
      event->stage_state.new_state = 0;

      clutter_context = _clutter_context_get_default ();
      g_queue_push_head (clutter_context->events_queue, event);
    }

  if (surface)
    {
      stage_wayland = wl_surface_get_user_data (surface);
      device->keyboard_focus = stage_wayland;

      event = clutter_event_new (CLUTTER_STAGE_STATE);
      event->stage_state.stage = stage_wayland->wrapper;
      event->stage_state.changed_mask = CLUTTER_STAGE_STATE_ACTIVATED;
      event->stage_state.new_state = CLUTTER_STAGE_STATE_ACTIVATED;

      end = keys->data + keys->size;
      device->modifier_state = 0;
      for (k = keys->data; k < end; k++)
	device->modifier_state |= device->xkb->map->modmap[*k];

      clutter_context = _clutter_context_get_default ();
      g_queue_push_head (clutter_context->events_queue, event);
    }
}

static const struct wl_input_device_listener input_device_listener = {
  clutter_backend_wayland_handle_motion,
  clutter_backend_wayland_handle_button,
  clutter_backend_wayland_handle_key,
  clutter_backend_wayland_handle_pointer_focus,
  clutter_backend_wayland_handle_keyboard_focus,
};

static void
clutter_input_device_wayland_class_init (ClutterInputDeviceWaylandClass *klass)
{
}

static void
clutter_input_device_wayland_init (ClutterInputDeviceWayland *self)
{
}

const char *option_xkb_layout = "us";
const char *option_xkb_variant = "";
const char *option_xkb_options = "";

void
_clutter_backend_add_input_device (ClutterBackendWayland *backend_wayland,
				   uint32_t id)
{
  ClutterInputDeviceWayland *device;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_WAYLAND,
			 "id", id,
			 "device-type", CLUTTER_POINTER_DEVICE,
			 "name", "wayland device",
			 NULL);

  device->input_device =
    wl_input_device_create (backend_wayland->wayland_display, id);
  wl_input_device_add_listener (device->input_device,
				&input_device_listener, device);
  wl_input_device_set_user_data (device->input_device, device);

  device->xkb = _clutter_xkb_desc_new (NULL,
                                       option_xkb_layout,
                                       option_xkb_variant,
                                       option_xkb_options);
  if (!device->xkb)
    CLUTTER_NOTE (BACKEND, "Failed to compile keymap");
}
