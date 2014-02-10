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
#include <unistd.h>
#include <sys/mman.h>
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
#include "clutter-backend-wayland-priv.h"
#include "clutter-stage-private.h"
#include "clutter-stage-wayland.h"
#include "clutter-wayland.h"

#include "cogl/clutter-stage-cogl.h"

typedef struct _ClutterInputDeviceClass         ClutterInputDeviceWaylandClass;

#define clutter_input_device_wayland_get_type \
    _clutter_input_device_wayland_get_type

G_DEFINE_TYPE (ClutterInputDeviceWayland,
               clutter_input_device_wayland,
               CLUTTER_TYPE_INPUT_DEVICE);

/* This gives us a fake time source for higher level abstractions to have an
 * understanding of when an event happens. All that matters are that this is a
 * monotonic increasing millisecond accurate time for events to be compared with.
 */
static guint32
_clutter_wayland_get_time (void)
{
  return g_get_monotonic_time () / 1000;
}

static void
clutter_wayland_handle_motion (void *data,
                               struct wl_pointer *pointer,
                               uint32_t _time,
                               wl_fixed_t x, wl_fixed_t y)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;
  ClutterEvent              *event;

  if (!device->pointer_focus)
    return;

  stage_cogl = device->pointer_focus;

  event = clutter_event_new (CLUTTER_MOTION);
  event->motion.stage = stage_cogl->wrapper;
  event->motion.device = CLUTTER_INPUT_DEVICE (device);
  event->motion.time = _clutter_wayland_get_time ();
  event->motion.modifier_state = 0;
  event->motion.x = wl_fixed_to_double (x);
  event->motion.y = wl_fixed_to_double (y);

  device->x = event->motion.x;
  device->y = event->motion.y;

  _clutter_event_push (event, FALSE);
}

static void
clutter_wayland_handle_button (void *data,
                               struct wl_pointer *pointer,
                               uint32_t serial, uint32_t _time,
                               uint32_t button, uint32_t state)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;
  ClutterEvent              *event;
  ClutterEventType           type;
  ClutterModifierType        modifier_mask = 0;

  if (!device->pointer_focus)
    return;

  stage_cogl = device->pointer_focus;

  if (state)
    type = CLUTTER_BUTTON_PRESS;
  else
    type = CLUTTER_BUTTON_RELEASE;

  event = clutter_event_new (type);
  event->button.stage = stage_cogl->wrapper;
  event->button.device = CLUTTER_INPUT_DEVICE (device);
  event->button.time = _clutter_wayland_get_time ();
  event->button.x = device->x;
  event->button.y = device->y;
  _clutter_xkb_translate_state (event, device->xkb, 0);

  /* evdev button codes */
  switch (button) {
  case 272:
    event->button.button = 1;
    modifier_mask = CLUTTER_BUTTON1_MASK;
    break;
  case 273:
    event->button.button = 3;
    modifier_mask = CLUTTER_BUTTON2_MASK;
    break;
  case 274:
    event->button.button = 2;
    modifier_mask = CLUTTER_BUTTON3_MASK;
    break;
  }

  if (modifier_mask)
    {
      if (state)
        device->button_modifier_state |= modifier_mask;
      else
        device->button_modifier_state &= ~modifier_mask;
    }

  event->button.modifier_state = device->button_modifier_state;

  _clutter_event_push (event, FALSE);
}

static void
clutter_wayland_handle_axis (void *data,
                             struct wl_pointer *pointer,
                             uint32_t time,
                             uint32_t axis,
                             wl_fixed_t value)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;
  ClutterEvent              *event;
  gdouble                    delta_x, delta_y;

  if (!device->pointer_focus)
    return;

  stage_cogl = device->pointer_focus;
  event = clutter_event_new (CLUTTER_SCROLL);
  event->scroll.time = _clutter_wayland_get_time ();
  event->scroll.stage = stage_cogl->wrapper;
  event->scroll.direction = CLUTTER_SCROLL_SMOOTH;
  event->scroll.x = device->x;
  event->scroll.y = device->y;

  if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    {
      delta_x = -wl_fixed_to_double(value) * 23;
      delta_y = 0;
    }
  else
    {
      delta_x = 0;
      delta_y = -wl_fixed_to_double(value) * 23; /* XXX: based on my bcm5794 */
    }
  clutter_event_set_scroll_delta (event, delta_x, delta_y);

  _clutter_xkb_translate_state (event, device->xkb, 0);

  _clutter_event_push (event, FALSE);
}

static void
clutter_wayland_handle_keymap (void *data,
                               struct wl_keyboard *keyboard,
                               uint32_t format,
                               int32_t fd,
                               uint32_t size)
{
  ClutterInputDeviceWayland *device = data;
  struct xkb_context *ctx;
  struct xkb_keymap *keymap;
  char *map_str;

  if (device->xkb)
    {
      xkb_state_unref (device->xkb);
      device->xkb = NULL;
    }

  ctx = xkb_context_new (0);
  if (!ctx)
    {
      close (fd);
      return;
    }

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    {
      close (fd);
      return;
    }

  map_str = mmap (NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  if (map_str == MAP_FAILED)
    {
      close (fd);
      return;
    }

  keymap = xkb_map_new_from_string (ctx,
                                    map_str,
                                    XKB_KEYMAP_FORMAT_TEXT_V1,
                                    0);
  xkb_context_unref (ctx);
  munmap (map_str, size);
  close (fd);

  if (!keymap)
    {
      g_warning ("failed to compile keymap\n");
      return;
    }

  device->xkb = xkb_state_new (keymap);
  xkb_map_unref (keymap);
  if (!device->xkb)
    {
      g_warning ("failed to create XKB state object\n");
      return;
    }
}

/* XXX: Need a wl_keyboard event to harmonise these across clients. */
#define KEY_REPEAT_DELAY 660
#define KEY_REPEAT_INTERVAL 40

static gboolean
clutter_wayland_repeat_key (void *data)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl = device->keyboard_focus;
  ClutterEvent              *event;

  event = _clutter_key_event_new_from_evdev ((ClutterInputDevice *) device,
					     (ClutterInputDevice*) device,
                                             stage_cogl->wrapper,
                                             device->xkb, 0,
                                             device->repeat_time,
                                             device->repeat_key,
                                             1);
  device->repeat_time += KEY_REPEAT_INTERVAL;
  _clutter_event_push (event, FALSE);

  if (!device->is_initial_repeat)
    return TRUE;

  g_source_remove (device->repeat_source);
  device->repeat_source = g_timeout_add (KEY_REPEAT_INTERVAL,
                                         clutter_wayland_repeat_key,
                                         device);
  device->is_initial_repeat = FALSE;

  return FALSE;
}

static void
clutter_wayland_handle_key (void *data,
                            struct wl_keyboard *keyboard,
                            uint32_t serial, uint32_t _time,
                            uint32_t key, uint32_t state)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl = device->keyboard_focus;
  ClutterEvent              *event;

  if (!device->keyboard_focus)
    return;
  if (!device->xkb)
    return;

  event = _clutter_key_event_new_from_evdev ((ClutterInputDevice *) device,
					     (ClutterInputDevice *) device,
                                             stage_cogl->wrapper,
                                             device->xkb, 0,
                                             _clutter_wayland_get_time (),
                                             key, state);

  _clutter_event_push (event, FALSE);

  if (!xkb_key_repeats (xkb_state_get_map (device->xkb), key))
    return;

  if (state)
    {
      if (device->repeat_key != XKB_KEYCODE_INVALID)
        g_source_remove (device->repeat_source);
      device->repeat_key = key;
      device->repeat_time = _time + KEY_REPEAT_DELAY;
      device->repeat_source = g_timeout_add (KEY_REPEAT_DELAY,
                                             clutter_wayland_repeat_key,
                                             device);
      device->is_initial_repeat = TRUE;
    }
  else if (device->repeat_key == key)
    {
      g_source_remove (device->repeat_source);
      device->repeat_key = XKB_KEYCODE_INVALID;
    }
}

static void
clutter_wayland_handle_modifiers (void *data,
                                  struct wl_keyboard *keyboard,
                                  uint32_t serial,
                                  uint32_t mods_depressed,
                                  uint32_t mods_latched,
                                  uint32_t mods_locked,
                                  uint32_t group)
{
  ClutterInputDeviceWayland *device = data;

  if (!device->xkb)
    return;

  xkb_state_update_mask (device->xkb,
                         mods_depressed,
                         mods_latched,
                         mods_locked,
                         0,
                         0,
                         group);
}

static void
clutter_wayland_handle_pointer_enter (void *data,
                                      struct wl_pointer *pointer,
                                      uint32_t serial,
                                      struct wl_surface *surface,
                                      wl_fixed_t x, wl_fixed_t y)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageWayland       *stage_wayland;
  ClutterStageCogl          *stage_cogl;
  ClutterEvent              *event;
  ClutterBackend            *backend;
  ClutterBackendWayland     *backend_wayland;

  stage_wayland = wl_surface_get_user_data (surface);

  if (!CLUTTER_IS_STAGE_COGL (stage_wayland))
    return;
  stage_cogl = CLUTTER_STAGE_COGL (stage_wayland);

  device->pointer_focus = stage_cogl;
  _clutter_input_device_set_stage (CLUTTER_INPUT_DEVICE (device),
       stage_cogl->wrapper);

  event = clutter_event_new (CLUTTER_ENTER);
  event->crossing.stage = stage_cogl->wrapper;
  event->crossing.time = _clutter_wayland_get_time ();
  event->crossing.x = wl_fixed_to_double (x);
  event->crossing.y = wl_fixed_to_double (y);
  event->crossing.source = CLUTTER_ACTOR (stage_cogl->wrapper);
  event->crossing.device = CLUTTER_INPUT_DEVICE (device);

  device->x = event->crossing.x;
  device->y = event->crossing.y;

  _clutter_event_push (event, FALSE);

  if (stage_wayland->cursor_visible)
    {
      /* Set the cursor to the cursor loaded at backend initialisation */
      backend = clutter_get_default_backend ();
      backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);

      _clutter_backend_wayland_ensure_cursor (backend_wayland);

      wl_pointer_set_cursor (pointer,
                             serial,
                             backend_wayland->cursor_surface,
                             backend_wayland->cursor_x,
                             backend_wayland->cursor_y);
      wl_surface_attach (backend_wayland->cursor_surface,
                         backend_wayland->cursor_buffer,
                         0,
                         0);
      wl_surface_damage (backend_wayland->cursor_surface,
                         0,
                         0,
                         32, /* XXX: FFS */
                         32);

      wl_surface_commit (backend_wayland->cursor_surface);
    }
  else
    {
      wl_pointer_set_cursor (pointer, serial, NULL, 0, 0);
    }
}

static void
clutter_wayland_handle_pointer_leave (void *data,
                                      struct wl_pointer *pointer,
                                      uint32_t serial,
                                      struct wl_surface *surface)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;
  ClutterEvent              *event;

  if (surface == NULL)
    return;

  if (!CLUTTER_IS_STAGE_COGL (wl_surface_get_user_data (surface)))
    return;

  stage_cogl = wl_surface_get_user_data (surface);
  g_assert (device->pointer_focus == stage_cogl);

  event = clutter_event_new (CLUTTER_LEAVE);
  event->crossing.stage = stage_cogl->wrapper;
  event->crossing.time = _clutter_wayland_get_time ();
  event->crossing.x = device->x;
  event->crossing.y = device->y;
  event->crossing.source = CLUTTER_ACTOR (stage_cogl->wrapper);
  event->crossing.device = CLUTTER_INPUT_DEVICE (device);

  _clutter_event_push (event, FALSE);

  device->pointer_focus = NULL;
  _clutter_input_device_set_stage (CLUTTER_INPUT_DEVICE (device), NULL);
}

static void
clutter_wayland_handle_keyboard_enter (void *data,
                                       struct wl_keyboard *keyboard,
                                       uint32_t serial,
                                       struct wl_surface *surface,
                                       struct wl_array *keys)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;

  if (!CLUTTER_IS_STAGE_COGL (wl_surface_get_user_data (surface)))
    return;

  stage_cogl = wl_surface_get_user_data (surface);
  g_assert (device->keyboard_focus == NULL);
  device->keyboard_focus = stage_cogl;

  _clutter_stage_update_state (stage_cogl->wrapper,
                               0,
                               CLUTTER_STAGE_STATE_ACTIVATED);
}

static void
clutter_wayland_handle_keyboard_leave (void *data,
                                       struct wl_keyboard *keyboard,
                                       uint32_t serial,
                                       struct wl_surface *surface)
{
  ClutterInputDeviceWayland *device = data;
  ClutterStageCogl          *stage_cogl;

  if (!surface)
    return;
  if (!CLUTTER_IS_STAGE_COGL (wl_surface_get_user_data (surface)))
    return;

  stage_cogl = wl_surface_get_user_data (surface);
  g_assert (device->keyboard_focus == stage_cogl);

  _clutter_stage_update_state (stage_cogl->wrapper,
                               CLUTTER_STAGE_STATE_ACTIVATED,
                               0);

  if (device->repeat_key != XKB_KEYCODE_INVALID)
    {
      g_source_remove (device->repeat_source);
      device->repeat_key = XKB_KEYCODE_INVALID;
    }

  device->keyboard_focus = NULL;
}

static const struct wl_keyboard_listener _clutter_keyboard_wayland_listener = {
  clutter_wayland_handle_keymap,
  clutter_wayland_handle_keyboard_enter,
  clutter_wayland_handle_keyboard_leave,
  clutter_wayland_handle_key,
  clutter_wayland_handle_modifiers,
};

static const struct wl_pointer_listener _clutter_pointer_wayland_listener = {
  clutter_wayland_handle_pointer_enter,
  clutter_wayland_handle_pointer_leave,
  clutter_wayland_handle_motion,
  clutter_wayland_handle_button,
  clutter_wayland_handle_axis,
};

static void
clutter_wayland_handle_seat (void *data,
                             struct wl_seat *seat,
                             uint32_t capabilities)
{
  ClutterInputDeviceWayland *device = data;

  /* XXX: Needs to handle removals too. */

  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !device->has_pointer)
    {
      struct wl_pointer *pointer;

      pointer = wl_seat_get_pointer (seat);
      if (pointer)
        {
          wl_pointer_add_listener (pointer,
                                   &_clutter_pointer_wayland_listener,
                                   device);
          wl_pointer_set_user_data (pointer, device);
          device->has_pointer = 1;
        }
    }

  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !device->has_keyboard)
    {
      struct wl_keyboard *keyboard;

      keyboard = wl_seat_get_keyboard (seat);
      if (keyboard)
        {
          wl_keyboard_add_listener (keyboard,
                                    &_clutter_keyboard_wayland_listener,
                                    device);
          wl_keyboard_set_user_data (keyboard, device);
          device->has_keyboard = 1;
        }
    }
}

const struct wl_seat_listener _clutter_seat_wayland_listener = {
  clutter_wayland_handle_seat,
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
  self->repeat_key = XKB_KEYCODE_INVALID;
}

/**
 * clutter_wayland_input_device_get_wl_seat: (skip)
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
struct wl_seat *
clutter_wayland_input_device_get_wl_seat (ClutterInputDevice *device)
{
  ClutterInputDeviceWayland *wayland_device;

  if (!CLUTTER_INPUT_DEVICE_WAYLAND (device))
    return NULL;

  wayland_device = CLUTTER_INPUT_DEVICE_WAYLAND (device);

  return wayland_device->input_device;
}
