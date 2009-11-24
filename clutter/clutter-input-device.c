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

/**
 * SECTION:clutter-input-device
 * @short_description: An input device managed by Clutter
 *
 * #ClutterInputDevice represents an input device known to Clutter.
 *
 * The #ClutterInputDevice class holds the state of the device, but
 * its contents are usually defined by the Clutter backend in use.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-input-device.h"
#include "clutter-private.h"

enum
{
  PROP_0,

  PROP_ID,
  PROP_DEVICE_TYPE
};

G_DEFINE_TYPE (ClutterInputDevice, clutter_input_device, G_TYPE_OBJECT);

static void
clutter_input_device_set_property (GObject      *gobject,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ClutterInputDevice *self = CLUTTER_INPUT_DEVICE (gobject);

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_value_get_int (value);
      break;

    case PROP_DEVICE_TYPE:
      self->device_type = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_input_device_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ClutterInputDevice *self = CLUTTER_INPUT_DEVICE (gobject);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int (value, self->id);
      break;

    case PROP_DEVICE_TYPE:
      g_value_set_enum (value, self->device_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_input_device_class_init (ClutterInputDeviceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_input_device_set_property;
  gobject_class->get_property = clutter_input_device_get_property;

  /**
   * ClutterInputDevice:id:
   *
   * The unique identifier of the device
   *
   * Since: 1.2
   */
  pspec = g_param_spec_int ("id",
                            "Id",
                            "Unique identifier of the device",
                            -1, G_MAXINT,
                            0,
                            CLUTTER_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_ID, pspec);

  /**
   * ClutterInputDevice:device-type:
   *
   * The type of the device
   *
   * Since: 1.2
   */
  pspec = g_param_spec_enum ("device-type",
                             "Device Type",
                             "The type of the device",
                             CLUTTER_TYPE_INPUT_DEVICE_TYPE,
                             CLUTTER_POINTER_DEVICE,
                             CLUTTER_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_DEVICE_TYPE, pspec);
}

static void
clutter_input_device_init (ClutterInputDevice *self)
{
  self->id = -1;
  self->device_type = CLUTTER_POINTER_DEVICE;

  self->click_count = 0;

  self->previous_time = CLUTTER_CURRENT_TIME;
  self->previous_x = -1;
  self->previous_y = -1;
  self->previous_button_number = -1;
  self->previous_state = 0;
}

/*
 * _clutter_input_device_set_coords:
 * @device: a #ClutterInputDevice
 * @x: X coordinate of the device
 * @y: Y coordinate of the device
 *
 * Stores the last known coordinates of the device
 */
void
_clutter_input_device_set_coords (ClutterInputDevice *device,
                                  gint                x,
                                  gint                y)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  if (device->previous_x != x)
    device->previous_x = x;

  if (device->previous_y != y)
    device->previous_y = y;
}

/*
 * _clutter_input_device_set_state:
 * @device: a #ClutterInputDevice
 * @state: a bitmask of modifiers
 *
 * Stores the last known modifiers state of the device
 */
void
_clutter_input_device_set_state (ClutterInputDevice  *device,
                                 ClutterModifierType  state)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  device->previous_state = state;
}

/*
 * _clutter_input_device_set_time:
 * @device: a #ClutterInputDevice
 * @time_: the time
 *
 * Stores the last known event time of the device
 */
void
_clutter_input_device_set_time (ClutterInputDevice *device,
                                guint32             time_)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  if (device->previous_time != time_)
    device->previous_time = time_;
}

/*
 * _clutter_input_device_set_stage:
 * @device: a #ClutterInputDevice
 * @stage: a #ClutterStage
 *
 * Stores the stage under the device
 */
void
_clutter_input_device_set_stage (ClutterInputDevice *device,
                                 ClutterStage       *stage)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  device->stage = stage;
}

/*
 * cursor_weak_unref:
 *
 * #ClutterInputDevice keeps a weak reference on the actor
 * under its pointer; this function unsets the reference on
 * the actor to avoid keeping around stale pointers
 */
static void
cursor_weak_unref (gpointer  user_data,
                   GObject  *object_pointer)
{
  ClutterInputDevice *device = user_data;

  device->cursor_actor = NULL;
}

void
_clutter_input_device_set_actor (ClutterInputDevice *device,
                                 ClutterActor       *actor)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  if (device->cursor_actor != NULL)
    {
      _clutter_actor_set_has_pointer (device->cursor_actor, FALSE);
      g_object_weak_unref (G_OBJECT (device->cursor_actor),
                           cursor_weak_unref,
                           device);
    }

  device->cursor_actor = actor;
  g_object_weak_ref (G_OBJECT (device->cursor_actor),
                     cursor_weak_unref,
                     device);
  _clutter_actor_set_has_pointer (device->cursor_actor, TRUE);
}

/**
 * clutter_input_device_get_device_type:
 * @device: a #ClutterInputDevice
 *
 * Retrieves the type of @device
 *
 * Return value: the type of the device
 *
 * Since: 1.0
 */
ClutterInputDeviceType
clutter_input_device_get_device_type (ClutterInputDevice *device)
{
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (device),
                        CLUTTER_POINTER_DEVICE);

  return device->device_type;
}

/**
 * clutter_input_device_get_device_id:
 * @device: a #ClutterInputDevice
 *
 * Retrieves the unique identifier of @device
 *
 * Return value: the identifier of the device
 *
 * Since: 1.0
 */
gint
clutter_input_device_get_device_id (ClutterInputDevice *device)
{
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (device), -1);

  return device->id;
}

/**
 * clutter_input_device_get_device_coords:
 * @device: a #ClutterInputDevice of type %CLUTTER_POINTER_DEVICE
 * @x: (out): return location for the X coordinate
 * @y: (out): return location for the Y coordinate
 *
 * Retrieves the latest coordinates of the pointer of @device
 *
 * Since: 1.2
 */
void
clutter_input_device_get_device_coords (ClutterInputDevice *device,
                                        gint               *x,
                                        gint               *y)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));
  g_return_if_fail (device->device_type == CLUTTER_POINTER_DEVICE);

  if (x)
    *x = device->previous_x;

  if (y)
    *y = device->previous_y;
}

ClutterActor *
_clutter_input_device_update (ClutterInputDevice *device)
{
  ClutterStage *stage;
  ClutterActor *new_cursor_actor;
  ClutterActor *old_cursor_actor;
  gint x, y;

  clutter_input_device_get_device_coords (device, &x, &y);

  stage = device->stage;
  old_cursor_actor = device->cursor_actor;
  new_cursor_actor = _clutter_do_pick (stage, x, y, CLUTTER_PICK_REACTIVE);

  /* if the pick could not find an actor then we do not update the
   * input device, to avoid ghost enter/leave events; the pick should
   * never fail, except for bugs in the glReadPixels() implementation
   * in which case this is the safest course of action anyway
   */
  if (new_cursor_actor == NULL)
    new_cursor_actor = CLUTTER_ACTOR (stage);

  CLUTTER_NOTE (EVENT,
                "Actor under cursor (device %d, at %d, %d): %s",
                clutter_input_device_get_device_id (device),
                x, y,
                clutter_actor_get_name (new_cursor_actor) != NULL
                  ? clutter_actor_get_name (new_cursor_actor)
                  : G_OBJECT_TYPE_NAME (new_cursor_actor));

  _clutter_input_device_set_actor (device, new_cursor_actor);

  return device->cursor_actor;
}
