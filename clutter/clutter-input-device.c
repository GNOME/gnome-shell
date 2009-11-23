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

  pspec = g_param_spec_int ("id",
                            "Id",
                            "Unique identifier of the device",
                            -1, G_MAXINT,
                            0,
                            CLUTTER_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_ID, pspec);

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

void
_clutter_input_device_set_coords (ClutterInputDevice *device,
                                  gfloat              x,
                                  gfloat              y)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  device->previous_x = floorf (x) + 0.5;
  device->previous_y = floorf (y) + 0.5;
}

void
_clutter_input_device_set_state (ClutterInputDevice  *device,
                                 ClutterModifierType  state)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  device->previous_state = state;
}

void
_clutter_input_device_set_time (ClutterInputDevice *device,
                                guint32             time_)
{
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  device->previous_time = time_;
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
