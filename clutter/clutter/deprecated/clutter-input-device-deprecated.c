#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include <glib-object.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "clutter-device-manager-private.h"
#include "deprecated/clutter-input-device.h"

/**
 * clutter_input_device_get_device_coords:
 * @device: a #ClutterInputDevice of type %CLUTTER_POINTER_DEVICE
 * @x: (out): return location for the X coordinate
 * @y: (out): return location for the Y coordinate
 *
 * Retrieves the latest coordinates of the pointer of @device
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: Use clutter_input_device_get_coords() instead.
 */
void
clutter_input_device_get_device_coords (ClutterInputDevice *device,
                                        gint               *x,
                                        gint               *y)
{
  ClutterPoint point;

  clutter_input_device_get_coords (device, NULL, &point);

  if (x)
    *x = point.x;

  if (y)
    *y = point.y;
}
